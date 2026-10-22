"""The OpenLocoland icon: a golden cog in the brass look of the game (the golden cog wheel the
enemy steals in mission 1) - our own drawing, nothing of the game.

    python tools/make_icon.py

writes tools/icon/openlocoland.svg (the drawing: the cog with its rings and the axle, no windows -
readable down to 16 px), renders it with a headless Chrome or Edge and packs
tools/icon/openlocoland.ico with the Windows sizes. release.py uses the .ico (Setup.exe's file and
window icon); run this only to change the drawing."""
import math
import os
import subprocess
import tempfile

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "tools", "icon")
SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
C = 512


def cog_path(teeth=12, r_tip=486, r_root=404, tip=0.30, base=0.50):
    """A cog outline: each tooth a trapezoid, its tip `tip` and its base `base` of the pitch wide,
    joined by arcs of the root circle."""
    pitch = 2 * math.pi / teeth
    p = lambda r, a: f"{C + r * math.cos(a):.1f} {C + r * math.sin(a):.1f}"
    d = []
    for i in range(teeth):
        a = i * pitch - math.pi / 2
        b0, b1 = a - base * pitch / 2, a + base * pitch / 2
        t0, t1 = a - tip * pitch / 2, a + tip * pitch / 2
        d.append(("M " if i == 0 else "L ") + p(r_root, b0))
        d.append("L " + p(r_tip, t0))
        d.append(f"A {r_tip} {r_tip} 0 0 1 " + p(r_tip, t1))
        d.append("L " + p(r_root, b1))
        d.append(f"A {r_root} {r_root} 0 0 1 " + p(r_root, a + pitch - base * pitch / 2))
    return " ".join(d) + " Z"


def window_path(i, n=6, r0=196, r1=318, spoke=0.36):
    """The i-th window between the hub and the rim: a ring sector with the spokes left between."""
    sector = 2 * math.pi / n
    a = i * sector - math.pi / 2 + sector / 2
    g0 = spoke * sector / 2 * 318 / r1
    p = lambda r, ang: f"{C + r * math.cos(ang):.1f} {C + r * math.sin(ang):.1f}"
    a0o, a1o = a - sector / 2 + spoke * sector * 0.5 * (318 / r1), a + sector / 2 - spoke * sector * 0.5 * (318 / r1)
    a0i, a1i = a - sector / 2 + spoke * sector * 0.5 * (318 / r0), a + sector / 2 - spoke * sector * 0.5 * (318 / r0)
    del g0
    return (f"M {p(r0, a0i)} L {p(r1, a0o)} A {r1} {r1} 0 0 1 {p(r1, a1o)} "
            f"L {p(r0, a1i)} A {r0} {r0} 0 0 0 {p(r0, a0i)} Z")


def svg(windows=True):
    body = cog_path()
    holes = " ".join(window_path(i) for i in range(6)) if windows else ""
    bolts = "".join(f'<circle cx="{C + 112 * math.cos(math.radians(a)):.1f}" cy="{C + 112 * math.sin(math.radians(a)):.1f}" '
                    f'r="15" fill="url(#bolt)" stroke="#3a2306" stroke-width="5"/>'
                    for a in range(-90, 270, 60)) if windows else ""
    edge = 12 if windows else 22
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" width="1024" height="1024">
  <!-- The OpenLocoland icon: a golden cog in the brass look of the game. Made by tools/make_icon.py. -->
  <defs>
    <linearGradient id="gold" x1="0.15" y1="0.05" x2="0.85" y2="0.95">
      <stop offset="0" stop-color="#fff3b8"/>
      <stop offset="0.25" stop-color="#f6cf5c"/>
      <stop offset="0.6" stop-color="#d39a2a"/>
      <stop offset="1" stop-color="#7a4d0e"/>
    </linearGradient>
    <linearGradient id="rim" x1="0.85" y1="0.95" x2="0.15" y2="0.05">
      <stop offset="0" stop-color="#ffe9a0"/>
      <stop offset="0.5" stop-color="#c48a22"/>
      <stop offset="1" stop-color="#6a420b"/>
    </linearGradient>
    <radialGradient id="hub" cx="0.38" cy="0.32" r="0.75">
      <stop offset="0" stop-color="#fff6c8"/>
      <stop offset="0.45" stop-color="#f0c450"/>
      <stop offset="1" stop-color="#8a5a12"/>
    </radialGradient>
    <radialGradient id="bolt" cx="0.35" cy="0.3" r="0.8">
      <stop offset="0" stop-color="#fff8d8"/>
      <stop offset="1" stop-color="#9c6a1a"/>
    </radialGradient>
    <radialGradient id="axle" cx="0.6" cy="0.65" r="0.8">
      <stop offset="0" stop-color="#4a2f08"/>
      <stop offset="1" stop-color="#140b02"/>
    </radialGradient>
    <filter id="shadow" x="-10%" y="-10%" width="125%" height="125%">
      <feDropShadow dx="6" dy="14" stdDeviation="10" flood-color="#000" flood-opacity="0.5"/>
    </filter>
    <clipPath id="body"><path d="{body} {holes}" clip-rule="evenodd"/></clipPath>
  </defs>
  <g filter="url(#shadow)">
    <path d="{body} {holes}" fill-rule="evenodd" fill="url(#gold)" stroke="#3a2306" stroke-width="{edge}" stroke-linejoin="round"/>
  </g>
  <!-- a light edge on the top left of every face, a dark one on the bottom right: the bevel -->
  <g clip-path="url(#body)" fill="none" stroke-linejoin="round">
    <path d="{body} {holes}" transform="translate(-7 -7)" stroke="#fff4c4" stroke-width="14" opacity="0.55"/>
    <path d="{body} {holes}" transform="translate(7 7)" stroke="#5a3608" stroke-width="14" opacity="0.45"/>
  </g>
  <!-- the inner rim and the hub -->
  <circle cx="{C}" cy="{C}" r="{350 if windows else 300}" fill="none" stroke="url(#rim)" stroke-width="{18 if windows else 28}" opacity="0.9"/>
  <circle cx="{C}" cy="{C}" r="{162 if windows else 190}" fill="url(#hub)" stroke="#3a2306" stroke-width="{edge}"/>
  {bolts}
  <circle cx="{C}" cy="{C}" r="{58 if windows else 92}" fill="url(#axle)" stroke="#3a2306" stroke-width="{edge}"/>
</svg>
"""


def find_browser():
    for p in (r"C:\Program Files\Google\Chrome\Application\chrome.exe",
              r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
              r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
              r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"):
        if os.path.exists(p):
            return p
    raise SystemExit("needs Chrome or Edge to render the SVG")


def render(svg_text, png, browser):
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, "icon.svg")
        with open(src, "w", encoding="utf-8") as f:
            f.write(svg_text)
        subprocess.run([browser, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                        "--default-background-color=00000000", "--window-size=1024,1024",
                        f"--screenshot={png}", "file:///" + src.replace("\\", "/")],
                       check=True, capture_output=True)
    return Image.open(png).convert("RGBA")


def main():
    os.makedirs(OUT, exist_ok=True)
    full_svg = svg(windows=False)
    with open(os.path.join(OUT, "openlocoland.svg"), "w", encoding="utf-8", newline="\n") as f:
        f.write(full_svg)
    browser = find_browser()
    with tempfile.TemporaryDirectory() as tmp:
        big = render(full_svg, os.path.join(tmp, "icon.png"), browser)
    images = [big.resize((s, s), Image.LANCZOS) for s in SIZES]
    ico = os.path.join(OUT, "openlocoland.ico")
    images[-1].save(ico, sizes=[(s, s) for s in SIZES], append_images=images[:-1])
    print(f"{ico}: {len(SIZES)} sizes")


if __name__ == "__main__":
    main()
