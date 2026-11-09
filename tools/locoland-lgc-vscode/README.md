# Locoland Logic Script for Visual Studio Code

Language support for Locoland `.lgc` scripts.

## Features

- TextMate syntax highlighting for comments, strings, numeric literals, preprocessor directives, keywords, types, functions and engine constants.
- IntelliSense for the engine API extracted from the supplied `export.lgc`, including function signatures, optional parameters and English hover documentation.
- Completion and hover data for commonly used helpers from the supplied script set (`basefunctions.lgc`, `ai_functions.lgc`, capture, environment, weather and reinforcement scripts).
- Workspace indexing of `.lgc` files for user-defined functions and `#define` constants.
- Signature help, navigation to workspace function declarations and workspace symbol search.
- Static diagnostics for unmatched delimiters, known function arity, inferable `int`/`string`/`char` argument mismatches and assignments to explicitly typed variables. Variable lookup respects function and nested-block scopes, including shadowing and function parameters; consistent script-function return types are inferred where safe.
- Snippets for functions, control flow, unit iteration, train iteration and common `Action()` patterns.
- Configurable source formatting with indentation, brace placement, operator spacing, parenthesis spacing, comma spacing, blank-line and `switch` label options. Formatting preserves string literals, comments and preprocessor directives.

## Install from source

1. Open this folder in Visual Studio Code.
2. Run `npm install` only if you want to use the standard VS Code extension development workflow. This extension has no runtime npm dependencies.
3. Press `F5` to launch an Extension Development Host.
4. Open a `.lgc` file. The language is registered automatically.

To package it as a VSIX, install `vsce` and run:

```sh
npx @vscode/vsce package --out "./builds"
```

The runtime itself uses only the VS Code API and Node.js built-ins.

## Code formatting

The extension registers an LGC document formatter. Use **LGC: Format Document** from the Command Palette or the standard VS Code **Format Document** command (`Shift+Alt+F` on Windows/Linux, `Shift+Option+F` on macOS). To format every saved `.lgc` file automatically, enable:

```json
{
    "[lgc]": {
        "editor.formatOnSave": true
    }
}
```

The formatter is deliberately lexical rather than a full compiler: it does not rewrite expressions or rename symbols. It preserves string and character literal contents, comments, preprocessor lines and token order. Trailing whitespace is removed as part of formatting. The formatter has been checked against all 35 `.lgc` files in the supplied script archive; formatting preserved the token sequence and was idempotent on that corpus.

Formatting options are available under **Locoland Logic Script**:

| Setting | Default | Purpose |
| --- | --- | --- |
| `lgc.format.indentStyle` | `editor` | Follow the editor indentation settings, or force `spaces` / `tabs`. |
| `lgc.format.indentSize` | `0` | Spaces per level. `0` follows the editor tab size. |
| `lgc.format.braceStyle` | `sameLine` | Opening braces on the declaration/control line (`sameLine`) or the next line (`nextLine`). |
| `lgc.format.spaceBeforeParens` | `control` | `control`, `always` or `never`. |
| `lgc.format.spaceInsideParentheses` | `false` | Add padding inside parentheses. |
| `lgc.format.spaceInsideBrackets` | `false` | Add padding inside array brackets. |
| `lgc.format.spaceAroundOperators` | `true` | Add spaces around binary and assignment operators. |
| `lgc.format.spaceAfterComma` | `true` | Add a space after commas. |
| `lgc.format.spaceBeforeColon` / `spaceAfterColon` | `true` | Control ternary-expression colon spacing. |
| `lgc.format.blankLinesBetweenFunctions` | `1` | Blank lines between top-level braced declarations. |
| `lgc.format.preserveBlankLines` | `true` | Retain blank lines found in the source. |
| `lgc.format.maxBlankLines` | `1` | Maximum consecutive blank lines preserved. |
| `lgc.format.indentCaseLabels` | `false` | Indent `case` and `default` labels inside `switch` blocks. |
| `lgc.format.keepEmptyBlocksSingleLine` | `true` | Keep empty blocks as `{}`. |
| `lgc.format.insertFinalNewline` | `true` | Ensure the formatted file ends with a newline. |

Example configuration using Allman braces and tabs:

```json
{
    "[lgc]": {
        "editor.formatOnSave": true,
        "editor.insertSpaces": false,
        "editor.tabSize": 4
    },
    "lgc.format.braceStyle": "nextLine",
    "lgc.format.indentStyle": "editor",
    "lgc.format.spaceAroundOperators": true
}
```

## Static analysis settings

The following settings are available under **Locoland Logic Script**:

| Setting | Default | Purpose |
| --- | --- | --- |
| `lgc.analysis.enabled` | `true` | Enable or disable LGC diagnostics. |
| `lgc.analysis.checkFunctionArity` | `true` | Check calls against known required and optional parameter counts. |
| `lgc.analysis.checkArgumentTypes` | `true` | Check known `int`, `string` and `char` parameter types when the expression type is clear. |
| `lgc.analysis.checkAssignments` | `true` | Check assignments to explicitly typed variables when both sides have known types. |
| `lgc.analysis.strictUnknownFunctions` | `false` | Warn about calls not found in the bundled catalog or workspace scripts. |
| `lgc.analysis.severity` | `Warning` | Default severity for static-analysis findings. |
| `lgc.analysis.maxWorkspaceFiles` | `500` | Maximum `.lgc` files indexed in a workspace. |

Run **LGC: Analyze Current File** from the Command Palette to refresh diagnostics for the active document.

## API data and language assumptions

`data/functions.json` contains 154 engine/compatibility API entries and 69 project helper entries. `data/constants.json` contains 471 constants collected from the supplied `export.lgc` and `obsolete.lgc`.

The engine declarations in `export.lgc` do not consistently spell out return types. Parameter types are taken from the declarations; script-function return types are inferred only when all observed return expressions have the same known type. Uncertain return types are intentionally marked as `unknown` so the checker does not invent certainty.

LGC resembles a small C-like language, but it is not treated as standard C. The checker is deliberately conservative: it checks only known signatures and expressions with inferable types. In particular, pointer-like expressions, engine-specific values and unresolved expressions are not assigned guessed types. Strict unknown-function checking is off by default because game scripts frequently depend on conditional includes and project-specific declarations.

## Data sources

- The API catalogue and constants were extracted from the user-supplied script archive, primarily `export.lgc` and `obsolete.lgc`.
- The helper catalogue was derived from the supplied scripts, especially `basefunctions.lgc`, `ai_functions.lgc`, `Captures.lgc`, `civil.lgc`, `daynight.lgc`, `environmenttact.lgc`, `reinforcement.lgc` and `setarmygamma.lgc`.
- Project context: [Dysprohol/OpenLocoland](https://github.com/Dysprohol/OpenLocoland), particularly its [developer documentation](https://github.com/Dysprohol/OpenLocoland/blob/main/DEVELOPERS.md). The repository describes how OpenLocoland documents and restores the original Locoland engine; it is not presented as an official language specification.

## Limitations

- There is no formal LGC grammar or compiler specification in the supplied materials. Syntax coverage is based on the supplied scripts and engine declarations.
- The static checker is not a game runtime or full compiler and does not execute preprocessor branches.
- `Action()` and `AddCommand()` argument semantics depend on the selected `ACT_*` opcode; the checker validates their declared integer parameter types but cannot validate every action-specific payload.
- The bundled helper catalogue documents common functions, not every mission-specific callback or function used by every map. Workspace indexing discovers additional functions from `.lgc` files included in the opened workspace.

## License

MIT. This extension is an independent fan-made development aid and is not affiliated with Gromada, ESDigital Games or the OpenLocoland project.
