r"""Posts the project's GitHub releases to a Discord channel through a webhook.

Two ways to use it:

1. A new release, from .github/workflows/discord-release.yml:

       python tools/discord_release.py notify --event "$GITHUB_EVENT_PATH"

   The workflow runs on `release: published`, so a draft is announced when it is published,
   never when it is saved. A manual run (`workflow_dispatch` with a tag) re-announces a release.

2. Every release published so far, from a developer's machine:

       python tools/discord_release.py backfill --dry-run
       python tools/discord_release.py backfill

   The releases go oldest first, so the channel reads like a changelog. `--dry-run` prints what
   would be sent and writes the list of links, without touching Discord. Both commands write
   build/releases.md (every release with its link), which is handy on its own: the channel and
   the file say the same thing.

A release's message is its name, the lead of its notes (what stands before the first section),
a "Full changelog info" link to the release on GitHub in place of the sections, the tag, the
date and the files. Discord has no collapsible text, so the sections are not in the message;
`--spoiler` puts them there under a spoiler, `--full` as they are.

The webhook URL is a secret: it comes from DISCORD_WEBHOOK_URL (the repository secret in GitHub
Actions, an environment variable locally) and is never printed. Create it in Discord: the
channel's settings -> Integrations -> Webhooks -> New Webhook -> Copy Webhook URL.
"""
import argparse
import datetime
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request

DEFAULT_REPO = "Dysprohol/OpenLocoland"
API = "https://api.github.com"
USER_AGENT = "OpenLocoland-release-bot (https://github.com/Dysprohol/OpenLocoland)"
GREEN = 0x2ECC71    # a release
ORANGE = 0xE67E22   # a pre-release
GREY = 0x95A5A6     # a draft, or a release with no body

# Discord's limits for one embed (a value above them is rejected, not trimmed).
DESCRIPTION = 4096
FIELD_VALUE = 1024
FIELD_NAME = 256
TITLE = 256
ASSETS = 20          # a field lists at most this many files; the rest are counted
BODY = 3000          # the release notes, short enough to leave room for the fields


def log(message):
    print(message, flush=True)


def human_size(size):
    for unit in ("B", "KB", "MB", "GB"):
        if size < 1024 or unit == "GB":
            return f"{size:.0f} {unit}" if unit == "B" else f"{size:.1f} {unit}"
        size /= 1024


# The start of a line Discord must keep on a line of its own.
BLOCK = re.compile(r"^\s{0,3}(#{1,6}\s|[-*+]\s|\d+[.)]\s|>|\||```|~~~)")
LIST = re.compile(r"^\s*([-*+]|\d+[.)])\s")


def is_block(line):
    return bool(BLOCK.match(line)) or line.startswith("    ")


def unwrap(body):
    """Joins the hard wraps of prose into paragraphs; lists, headings, quotes, tables and code
    keep their lines. GitHub keeps every newline its author typed, Discord would show all of
    them, and the notes were wrapped for a browser's width, not a chat window's."""
    lines, fence = [], None
    for line in body.split("\n"):
        stripped = line.strip()
        if fence:
            lines.append(line)
            if stripped.startswith(fence):
                fence = None
            continue
        if stripped.startswith(("```", "~~~")):
            fence = stripped[:3]
            lines.append(line)
        elif lines and stripped and not is_block(line) and (
                (lines[-1].strip() and not is_block(lines[-1]))
                or (line[:1].isspace() and LIST.match(lines[-1]))):
            lines[-1] = lines[-1].rstrip() + " " + stripped
        else:
            lines.append(line)
    return "\n".join(lines)


def clean_body(body):
    """The release body as Discord shows it: no HTML comments, no Windows line ends."""
    body = re.sub(r"<!--.*?-->", "", body, flags=re.S)
    body = body.replace("\r\n", "\n").replace("\r", "\n")
    body = re.sub(r"\n{3,}", "\n\n", body)
    body = unwrap(body)
    body = body.strip()
    if not body:
        return "_No release notes._"
    if len(body) > BODY:
        # cut at a paragraph's end, not in the middle of a sentence, list or table
        cut = body[:BODY]
        boundary = cut.rfind("\n\n")
        if boundary > BODY // 2:
            cut = cut[:boundary]
        elif "\n" in cut:
            cut = cut[: cut.rfind("\n")]
        body = cut.rstrip() + "\n\n… (full notes on GitHub)"
    return body


def assets_field(release):
    assets = release.get("assets") or []
    lines = []
    for asset in assets[:ASSETS]:
        lines.append(f"[{asset['name']}]({asset['browser_download_url']}) · {human_size(asset['size'])}")
    if len(assets) > ASSETS:
        lines.append(f"… and {len(assets) - ASSETS} more")
    return "\n".join(lines)


def split_notes(body):
    """The notes split into the lead that stays visible and the rest.
    The lead is everything before the first section heading (or the first section, when the
    notes open with one); a heading repeating the title is dropped."""
    cut = body
    head = re.match(r"(?:#{1,6}\s.*\n?)+", cut)
    if head:
        cut = cut[head.end():]
    cut = cut.lstrip("\n")
    if re.match(r"#{1,6}\s", cut):
        after = re.search(r"\n#{1,6}\s", cut)
        lead = cut[: after.start()] if after else cut
    else:
        heading = re.search(r"\n#{2,6}\s", cut)
        lead = cut[: heading.start()] if heading else cut.split("\n\n", 1)[0]
    lead = lead.strip()
    rest = cut[len(lead):].lstrip("\n")
    return lead, rest


LINK = "Full changelog info"


def notes_text(body, url, mode):
    """The notes as Discord shows them: the lead and a link to the whole notes in place of
    their sections, the lead and the sections under a spoiler, or all of it."""
    lead, rest = split_notes(body)
    if mode == "full":
        return body
    if mode == "link" or not rest:
        return f"{lead}\n\n[{LINK}]({url})" if lead else f"[{LINK}]({url})"
    room = DESCRIPTION - len(lead) - 8                  # "\n\n||" and "||"
    if len(rest) > room:
        part = rest[:room]
        boundary = part.rfind("\n\n")
        part = part[:boundary] if boundary > room // 2 else part[: part.rfind("\n")]
        rest = part.rstrip() + f"\n\n… [full notes on GitHub]({url})"
    return f"{lead}\n\n||{rest}||" if lead else f"||{rest}||"


def embed(release, repo, mode="link"):
    """One Discord embed for one release: name, notes, tag, date and the files."""
    tag = release["tag_name"]
    title = (release.get("name") or tag).strip()
    fields = [{"name": "Tag", "value": f"`{tag}`"[:FIELD_VALUE], "inline": True}]

    published = release.get("published_at") or release.get("created_at")
    if published:
        stamp = int(datetime.datetime.fromisoformat(published.replace("Z", "+00:00")).timestamp())
        fields.append({"name": "Published", "value": f"<t:{stamp}:D> (<t:{stamp}:R>)", "inline": True})

    field = assets_field(release)
    if field:
        fields.append({"name": "Files", "value": field[:FIELD_VALUE], "inline": False})

    notes = clean_body(release.get("body") or "")
    color = ORANGE if release.get("prerelease") else (GREY if release.get("draft") else GREEN)
    result = {
        "author": {"name": repo},
        "title": title[:TITLE],
        "url": release["html_url"],
        "description": notes_text(notes, release["html_url"], mode)[:DESCRIPTION],
        "color": color,
        "fields": fields,
        "footer": {"text": "GitHub release" if not release.get("prerelease") else "GitHub pre-release"},
    }
    if published:
        result["timestamp"] = published
    return result


def payload(release, repo, mode="link"):
    return {"username": "OpenLocoland", "embeds": [embed(release, repo, mode)]}


def mode_of(args):
    if getattr(args, "full", False):
        return "full"
    if getattr(args, "spoiler", False):
        return "spoiler"
    return "link"


def request(url, data=None, headers=None, method=None):
    body = None if data is None else json.dumps(data).encode("utf-8")
    head = {"User-Agent": USER_AGENT, "Accept": "application/vnd.github+json"}
    if data is not None:
        head["Content-Type"] = "application/json"
    head.update(headers or {})
    return urllib.request.Request(url, data=body, headers=head, method=method or ("POST" if body else "GET"))


def github(url, token):
    headers = {"X-GitHub-Api-Version": "2022-11-28"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    with urllib.request.urlopen(request(url, headers=headers), timeout=30) as reply:
        return json.load(reply)


def releases(repo, token, per_page=100):
    """Every published release of the repository, oldest first; drafts are left out."""
    found, page = [], 1
    while True:
        batch = github(f"{API}/repos/{repo}/releases?per_page={per_page}&page={page}", token)
        found.extend(batch)
        if len(batch) < per_page:
            break
        page += 1
    found = [r for r in found if not r.get("draft")]
    found.sort(key=lambda r: r.get("published_at") or r.get("created_at") or "")
    return found


def release_by_tag(repo, tag, token):
    for release in releases(repo, token):
        if release["tag_name"] == tag:
            return release
    sys.exit(f"{repo}: no published release with the tag {tag!r}")


def post(webhook, data, attempts=3):
    """Sends one message; Discord's 429 says how long to wait, so wait that long and retry."""
    url = webhook + ("&" if "?" in webhook else "?") + "wait=true"
    for attempt in range(1, attempts + 1):
        try:
            with urllib.request.urlopen(request(url, data), timeout=30) as reply:
                return json.load(reply)
        except urllib.error.HTTPError as e:
            detail = e.read().decode("utf-8", "replace")
            if e.code == 429 and attempt < attempts:
                wait = 1.0
                try:
                    wait = float(json.loads(detail).get("retry_after", wait))
                except ValueError:
                    pass
                log(f"rate limited, waiting {wait:.1f}s")
                time.sleep(wait + 0.5)
                continue
            sys.exit(f"Discord said {e.code}: {detail}")


def write_links(path, repo, found):
    """build/releases.md: every release in the channel with its link."""
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    lines = [f"# {repo} releases", "",
             "Every release announced on Discord, newest first.", "",
             "| Published | Release | Tag | Link |", "| --- | --- | --- | --- |"]
    for release in reversed(found):
        date = (release.get("published_at") or release.get("created_at") or "")[:10]
        name = (release.get("name") or release["tag_name"]).replace("|", "\\|")
        lines.append(f"| {date} | {name} | `{release['tag_name']}` | {release['html_url']} |")
    lines += ["", "## Links", ""]
    lines += [f"- {release['html_url']} — {release.get('name') or release['tag_name']}" for release in reversed(found)]
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    log(f"wrote {path} ({len(found)} releases)")


def need_webhook(given):
    webhook = given or os.environ.get("DISCORD_WEBHOOK_URL", "")
    if not webhook:
        sys.exit("no webhook: pass --webhook or set DISCORD_WEBHOOK_URL")
    if not webhook.startswith("https://discord.com/api/webhooks/") and not webhook.startswith("https://discordapp.com/api/webhooks/"):
        log("warning: this does not look like a Discord webhook URL")
    return webhook


def notify(args):
    repo = args.repo or os.environ.get("GITHUB_REPOSITORY") or DEFAULT_REPO
    token = args.token or os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN") or ""
    if args.tag:
        release = release_by_tag(repo, args.tag, token)
    elif args.event:
        with open(args.event, encoding="utf-8-sig") as f:
            event = json.load(f)
        release = event.get("release")
        if not release:
            log("the event has no release (not a release event) - nothing to announce")
            return 0
        if release.get("draft"):
            log("the release is a draft - nothing to announce")
            return 0
    else:
        sys.exit("pass --event (GitHub Actions) or --tag")

    message = payload(release, repo, mode_of(args))
    if args.dry_run:
        log(json.dumps(message, indent=2, ensure_ascii=False))
        return 0
    post(need_webhook(args.webhook), message)
    log(f"announced {release['tag_name']}")
    return 0


def backfill(args):
    repo = args.repo or DEFAULT_REPO
    token = args.token or os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN") or ""
    found = releases(repo, token)
    if args.limit:
        found = found[-args.limit:]
    if args.newest_first:
        found.reverse()
    write_links(args.out, repo, found)

    if args.dry_run:
        for release in found:
            log(f"would announce {release['tag_name']}: {release['html_url']}")
        log(f"{len(found)} releases; nothing sent (--dry-run)")
        return 0

    webhook = need_webhook(args.webhook)
    for number, release in enumerate(found, 1):
        post(webhook, payload(release, repo, mode_of(args)))
        log(f"[{number}/{len(found)}] {release['tag_name']}")
        if number < len(found):
            time.sleep(args.delay)
    return 0


def shared(parser, suppress=False):
    """--repo/--webhook/--token: before the command (the main parser) or after it (each
    subparser); the subparsers' copies are SUPPRESS so they do not overwrite a value given
    before the command."""
    default = argparse.SUPPRESS if suppress else None
    parser.add_argument("--repo", default=default, help=f"owner/name (default {DEFAULT_REPO})")
    parser.add_argument("--webhook", default=default, help="Discord webhook URL (default DISCORD_WEBHOOK_URL)")
    parser.add_argument("--token", default=default, help="GitHub token for the API (default GH_TOKEN/GITHUB_TOKEN)")
    parser.add_argument("--full", action="store_true", default=default,
                        help="the whole release notes in the message instead of the link")
    parser.add_argument("--spoiler", action="store_true", default=default,
                        help="the notes' sections under a spoiler in the message instead of the link")


def main():
    parser = argparse.ArgumentParser(description="Posts GitHub releases to a Discord webhook.")
    shared(parser)
    commands = parser.add_subparsers(dest="command", required=True)

    one = commands.add_parser("notify", help="announce one release")
    shared(one, suppress=True)
    one.add_argument("--event", help="GitHub event JSON (the release event's path)")
    one.add_argument("--tag", help="announce this tag instead of the event's release")
    one.add_argument("--dry-run", action="store_true", help="print the message, send nothing")
    one.set_defaults(run=notify)

    many = commands.add_parser("backfill", help="announce every published release, oldest first")
    shared(many, suppress=True)
    many.add_argument("--dry-run", action="store_true", help="list what would be sent, send nothing")
    many.add_argument("--limit", type=int, help="only the newest N releases")
    many.add_argument("--newest-first", action="store_true", help="newest first instead of oldest first")
    many.add_argument("--delay", type=float, default=2.0, help="seconds between messages (default 2)")
    many.add_argument("--out", default=os.path.join("build", "releases.md"), help="the list of links")
    many.set_defaults(run=backfill)

    args = parser.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    return args.run(args)


if __name__ == "__main__":
    sys.exit(main())
