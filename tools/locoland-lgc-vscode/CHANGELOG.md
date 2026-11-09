# Changelog

## 0.1.2

- Add an LGC document formatter and the `LGC: Format Document` command.
- Add settings for indentation style/size, brace placement, parenthesis and bracket padding, operator/comma/ternary spacing, blank lines, `switch` labels, empty blocks and final newlines.
- Follow VS Code editor indentation settings by default and support format-on-save through `editor.formatOnSave`.
- Preserve lexical token order, string/character literals, comments and preprocessor directives while formatting.
- Add formatter regression tests, including idempotency and source-token preservation checks.

## 0.1.1

- Fixed type resolution for variables with the same name in different functions and nested blocks.
- Added lexical-scope-aware variable lookup and function-parameter typing.
- Improved declaration parsing for comma-separated variables, arrays, and storage qualifiers.
- Added conservative return-type inference for script functions with consistent, explicitly typed return expressions.
- Type checks now validate known function-call return types when used as arguments or assignment values.
- Added regression coverage for shadowing, function parameters, character values, inferred returns and cross-scope name reuse.

## 0.1.0

- Initial language support for `.lgc` files.
- Added TextMate syntax highlighting, snippets, completions, hover documentation, signature help and workspace navigation.
- Added static diagnostics for delimiters, known function arity and conservative type mismatches.
- Bundled an API and helper catalogue derived from the supplied Locoland scripts.
