const vscode = require("vscode");
const fs = require("fs");
const path = require("path");

const functionCatalog = require("../data/functions.json");
const constantCatalog = require("../data/constants.json");
const { formatLgcText } = require("./formatter");

const CONTROL_WORDS = new Set(["if", "iff", "else", "while", "for", "do", "switch", "case", "return", "sizeof"]);
const TYPE_WORDS = new Set(["int", "string", "char", "void"]);
const KEYWORDS = [
    "if", "else", "while", "for", "do", "switch", "case", "default",
    "break", "continue", "return", "static", "extern", "int", "string", "char", "void"
];
const MULTI_OPERATORS = ["==", "!=", "<=", ">=", "++", "--", "&&", "||", "<<", ">>", "+=", "-=", "*=", "/=", "%=", "->"];

let diagnosticCollection;
let workspaceFunctions = new Map();
let workspaceConstants = new Map();
let workspaceFiles = [];
let refreshPromise;
let refreshTimer;

function tokenize(text) {
    const tokens = [];
    let i = 0;
    let line = 0;

    while (i < text.length) {
        const ch = text[i];
        if (ch === "\n") {
            line++;
            i++;
            continue;
        }
        if (/\s/.test(ch)) {
            i++;
            continue;
        }
        if (ch === "/" && text[i + 1] === "/") {
            while (i < text.length && text[i] !== "\n") i++;
            continue;
        }
        if (ch === "/" && text[i + 1] === "*") {
            i += 2;
            while (i < text.length) {
                if (text[i] === "\n") line++;
                if (text[i] === "*" && text[i + 1] === "/") {
                    i += 2;
                    break;
                }
                i++;
            }
            continue;
        }

        const start = i;
        const startLine = line;

        if (ch === '"' || ch === "'") {
            const quote = ch;
            i++;
            while (i < text.length) {
                if (text[i] === "\\") {
                    i += 2;
                    continue;
                }
                if (text[i] === quote) {
                    i++;
                    break;
                }
                if (text[i] === "\n") line++;
                i++;
            }
            tokens.push({ type: quote === '"' ? "string" : "char", value: text.slice(start, i), start, end: i, line: startLine });
            continue;
        }

        if (/[A-Za-z_]/.test(ch)) {
            i++;
            while (i < text.length && /[A-Za-z0-9_]/.test(text[i])) i++;
            tokens.push({ type: "identifier", value: text.slice(start, i), start, end: i, line: startLine });
            continue;
        }

        if (/[0-9]/.test(ch)) {
            i++;
            if (ch === "0" && (text[i] === "x" || text[i] === "X")) {
                i++;
                while (i < text.length && /[0-9A-Fa-f]/.test(text[i])) i++;
            } else {
                while (i < text.length && /[0-9.]/.test(text[i])) i++;
            }
            tokens.push({ type: "number", value: text.slice(start, i), start, end: i, line: startLine });
            continue;
        }

        const op = MULTI_OPERATORS.find(candidate => text.startsWith(candidate, i));
        if (op) {
            i += op.length;
            tokens.push({ type: "operator", value: op, start, end: i, line: startLine });
            continue;
        }

        i++;
        tokens.push({ type: "punctuation", value: ch, start, end: i, line: startLine });
    }
    return tokens;
}

function findMatching(tokens, openIndex, openValue, closeValue) {
    let depth = 0;
    for (let i = openIndex; i < tokens.length; i++) {
        if (tokens[i].value === openValue) depth++;
        else if (tokens[i].value === closeValue) {
            depth--;
            if (depth === 0) return i;
        }
    }
    return -1;
}

function splitTokenArguments(tokens, start, end) {
    if (start >= end) return [];
    const parts = [];
    let partStart = start;
    let paren = 0;
    let bracket = 0;
    let brace = 0;

    for (let i = start; i < end; i++) {
        const v = tokens[i].value;
        if (v === "(") paren++;
        else if (v === ")") paren--;
        else if (v === "[") bracket++;
        else if (v === "]") bracket--;
        else if (v === "{") brace++;
        else if (v === "}") brace--;
        else if (v === "," && paren === 0 && bracket === 0 && brace === 0) {
            parts.push(tokens.slice(partStart, i));
            partStart = i + 1;
        }
    }
    parts.push(tokens.slice(partStart, end));
    return parts.filter(part => part.length > 0);
}

function parseParameter(tokens) {
    if (!tokens.length) return null;
    const eq = tokens.findIndex(token => token.value === "=");
    const core = eq >= 0 ? tokens.slice(0, eq) : tokens;
    const identifiers = core.filter(token => token.type === "identifier");
    if (!identifiers.length) return null;
    const nameToken = identifiers[identifiers.length - 1];
    const typeToken = identifiers.find(token => TYPE_WORDS.has(token.value));
    const type = typeToken ? typeToken.value : "unknown";
    const isPointer = core.some(token => token.value === "*" || token.value === "&");
    return {
        name: nameToken.value,
        type: isPointer ? "unknown" : type,
        optional: eq >= 0,
        default: eq >= 0 ? tokens.slice(eq + 1).map(token => token.value).join(" ") : null
    };
}

function parseFunctionDeclarations(tokens, text, uri) {
    const functions = [];
    const declarationPositions = new Set();

    for (let i = 0; i < tokens.length - 1; i++) {
        const nameToken = tokens[i];
        if (nameToken.type !== "identifier" || tokens[i + 1].value !== "(" || CONTROL_WORDS.has(nameToken.value)) continue;
        const close = findMatching(tokens, i + 1, "(", ")");
        if (close < 0) continue;

        const after = tokens[close + 1];
        if (!after) continue;

        const lineStart = text.lastIndexOf("\n", nameToken.start - 1) + 1;
        const prefix = text.slice(lineStart, nameToken.start).trim();
        if (!/^(?:(?:extern|static|inline|const)(?:\s+|$))*(?:(?:int|string|char|void)\s*\*?\s*)?$/.test(prefix)) continue;
        const hasDeclarationPrefix = /\b(?:extern|int|string|char|void)\b/.test(prefix);
        const hasExternPrefix = /\bextern\b/.test(prefix);
        const isFunctionBody = after.value === "{";
        const isPrototype = (after.value === ";" && hasDeclarationPrefix) ||
            (hasExternPrefix && after.type === "number" && tokens[close + 2] && tokens[close + 2].value === ";");
        if (!isFunctionBody && !isPrototype) continue;

        const args = splitTokenArguments(tokens, i + 2, close);
        const params = args.map(parseParameter).filter(Boolean);
        const bodyClose = isFunctionBody ? findMatching(tokens, close + 1, "{", "}") : -1;
        const returnMatch = prefix.match(/\b(int|string|char|void)\b/);
        const returnType = returnMatch ? returnMatch[1] : "unknown";
        const signature = `${nameToken.value}(${params.map(p => `${p.type} ${p.name}${p.optional ? ` = ${p.default}` : ""}`).join(", ")})`;
        const item = {
            name: nameToken.value,
            params,
            signature,
            returnType,
            description: "Function declared in the current LGC workspace.",
            category: "Workspace function",
            source: uri ? vscode.workspace.asRelativePath(uri, false) : "current document",
            uri,
            position: {
                line: nameToken.line,
                character: nameToken.start - (text.lastIndexOf("\n", nameToken.start - 1) + 1)
            },
            offset: nameToken.start,
            range: nameToken.start,
            paramStartOffset: tokens[i + 2] ? tokens[i + 2].start : tokens[i + 1].end,
            paramEndOffset: tokens[close - 1] ? tokens[close - 1].end : tokens[i + 1].end,
            bodyOpenOffset: isFunctionBody ? after.start : null,
            bodyCloseOffset: bodyClose >= 0 ? tokens[bodyClose].end : null,
            declaration: true,
            external: hasExternPrefix
        };
        functions.push(item);
        declarationPositions.add(nameToken.start);
    }
    return { functions, declarationPositions };
}

function buildLexicalScopes(tokens, text) {
    const root = { id: 0, parent: null, start: 0, end: text.length, depth: 0 };
    const scopes = [root];
    const stack = [root];

    for (const token of tokens) {
        if (token.value === "{") {
            const parent = stack[stack.length - 1];
            const scope = {
                id: scopes.length,
                parent,
                start: token.start,
                end: text.length,
                depth: parent.depth + 1
            };
            scopes.push(scope);
            stack.push(scope);
        } else if (token.value === "}" && stack.length > 1) {
            stack[stack.length - 1].end = token.end;
            stack.pop();
        }
    }
    return scopes;
}

function innermostScopeAt(scopes, offset) {
    let result = scopes[0];
    for (const scope of scopes) {
        if (scope.start <= offset && offset <= scope.end && scope.depth > result.depth) result = scope;
    }
    return result;
}

function addVariable(variables, name, declaration) {
    if (!variables.has(name)) variables.set(name, []);
    variables.get(name).push(declaration);
}

function resolveVariable(variables, name, offset) {
    if (!variables || !variables.has(name)) return null;
    const entries = variables.get(name);
    if (!Array.isArray(entries)) return entries || null;
    const visible = entries.filter(entry =>
        offset >= entry.visibleFrom &&
        offset >= entry.scope.start &&
        offset <= entry.scope.end
    );
    if (!visible.length) return null;
    visible.sort((a, b) =>
        b.scope.depth - a.scope.depth ||
        b.visibleFrom - a.visibleFrom ||
        b.token.start - a.token.start
    );
    return visible[0];
}

function isDeclarationTypePosition(tokens, index, text, parameterRanges) {
    const token = tokens[index];
    if (!token || token.type !== "identifier" || !TYPE_WORDS.has(token.value) || token.value === "void") return false;
    if (parameterRanges.some(range => token.start >= range.start && token.start <= range.end)) return false;

    const lineStart = text.lastIndexOf("\n", token.start - 1) + 1;
    if (/^\s*#/.test(text.slice(lineStart, token.start))) return false;

    let previous = index - 1;
    const qualifiers = new Set(["static", "extern", "const", "volatile", "register", "auto"]);
    while (previous >= 0 && qualifiers.has(tokens[previous].value)) previous--;
    const boundary = previous < 0 ? null : tokens[previous].value;
    if (boundary !== null && ![";", "{", "}", "(", ":", "else", "do"].includes(boundary)) return false;

    let nameIndex = index + 1;
    while (tokens[nameIndex] && ["*", "&"].includes(tokens[nameIndex].value)) nameIndex++;
    if (!tokens[nameIndex] || tokens[nameIndex].type !== "identifier") return false;
    if (tokens[nameIndex + 1] && tokens[nameIndex + 1].value === "(") return false;
    return true;
}

function declarationEnd(tokens, start) {
    let paren = 0;
    let bracket = 0;
    let brace = 0;
    for (let i = start; i < tokens.length; i++) {
        const value = tokens[i].value;
        if (value === "(") paren++;
        else if (value === ")") {
            if (paren === 0) return i;
            paren--;
        } else if (value === "[") bracket++;
        else if (value === "]") {
            if (bracket === 0) return i;
            bracket--;
        } else if (value === "{") brace++;
        else if (value === "}") {
            if (brace === 0) return i;
            brace--;
        } else if (value === ";" && paren === 0 && bracket === 0 && brace === 0) {
            return i;
        }
    }
    return tokens.length;
}

function parseVariableDeclarator(part, type, scope, visibleFrom, declarationOffset) {
    if (!part.length) return null;
    let index = 0;
    let pointer = false;
    while (part[index] && ["*", "&"].includes(part[index].value)) {
        pointer = true;
        index++;
    }
    while (part[index] && (part[index].value === "const" || part[index].value === "volatile")) index++;
    const nameToken = part[index];
    if (!nameToken || nameToken.type !== "identifier") return null;
    return {
        name: nameToken.value,
        type: pointer ? "unknown" : type,
        token: nameToken,
        scope,
        visibleFrom,
        declarationOffset
    };
}

function parseVariables(tokens, text, functions) {
    const variables = new Map();
    const scopes = buildLexicalScopes(tokens, text);
    const parameterRanges = [];

    for (const fn of functions) {
        if (fn.paramStartOffset !== undefined && fn.paramEndOffset !== undefined) {
            parameterRanges.push({ start: fn.paramStartOffset, end: fn.paramEndOffset });
        }
        if (fn.bodyOpenOffset === null || fn.bodyOpenOffset === undefined) continue;
        const bodyScope = scopes.find(scope => scope.start === fn.bodyOpenOffset);
        if (!bodyScope) continue;
        for (const param of fn.params || []) {
            if (!param.name) continue;
            const token = tokens.find(item => item.value === param.name && item.start >= fn.paramStartOffset && item.end <= fn.paramEndOffset);
            if (!token) continue;
            addVariable(variables, param.name, {
                name: param.name,
                type: param.type || "unknown",
                token,
                scope: bodyScope,
                visibleFrom: bodyScope.start,
                declarationOffset: fn.offset
            });
        }
    }

    for (let i = 0; i < tokens.length; i++) {
        if (!isDeclarationTypePosition(tokens, i, text, parameterRanges)) continue;
        const type = tokens[i].value;
        const nameIndex = i + 1;
        const end = declarationEnd(tokens, nameIndex);
        const parts = splitTokenArguments(tokens, nameIndex, end);
        const declarationScope = innermostScopeAt(scopes, tokens[i].start);
        for (const part of parts) {
            const declaration = parseVariableDeclarator(part, type, declarationScope, tokens[i].start, tokens[i].start);
            if (declaration) addVariable(variables, declaration.name, declaration);
        }
        i = Math.max(i, nameIndex - 1);
    }
    return variables;
}

function parseDefines(text) {
    const result = new Map();
    const definePattern = /^\s*#\s*define\s+([A-Za-z_]\w*)(?:\(([^)]*)\))?\s*(.*)$/gm;
    let match;
    while ((match = definePattern.exec(text)) !== null) {
        const name = match[1];
        const params = match[2];
        const value = (match[3] || "").replace(/\/\/.*$/, "").trim();
        result.set(name, {
            name,
            value,
            params,
            description: `Workspace preprocessor definition${value ? ` (value: ${value})` : ""}.`,
            source: "workspace"
        });
    }
    return result;
}

function parseSource(text, uri) {
    const tokens = tokenize(text);
    const parsedFunctions = parseFunctionDeclarations(tokens, text, uri);
    const variables = parseVariables(tokens, text, parsedFunctions.functions);
    const constants = parseDefines(text);
    inferFunctionReturnTypes(parsedFunctions.functions, tokens, variables, constants);
    return {
        tokens,
        functions: parsedFunctions.functions,
        declarationPositions: parsedFunctions.declarationPositions,
        variables,
        constants
    };
}

function makeFunctionMap(document, parsed) {
    const result = new Map();
    for (const item of functionCatalog) result.set(item.name, item);
    for (const [name, item] of workspaceFunctions) result.set(name, item);
    for (const item of parsed.functions) {
        if (!item.external || !result.has(item.name)) result.set(item.name, item);
    }
    return result;
}

function makeConstantMap(parsed) {
    const result = new Map();
    for (const item of constantCatalog) result.set(item.name, item);
    for (const [name, item] of workspaceConstants) result.set(name, item);
    for (const [name, item] of parsed.constants) result.set(name, item);
    return result;
}

function splitTopLevel(tokens, operators) {
    const parts = [];
    let start = 0;
    let paren = 0;
    let bracket = 0;
    let brace = 0;
    for (let i = 0; i < tokens.length; i++) {
        const value = tokens[i].value;
        if (value === "(") paren++;
        else if (value === ")") paren--;
        else if (value === "[") bracket++;
        else if (value === "]") bracket--;
        else if (value === "{") brace++;
        else if (value === "}") brace--;
        else if (operators.has(value) && paren === 0 && bracket === 0 && brace === 0) {
            parts.push(tokens.slice(start, i));
            start = i + 1;
        }
    }
    if (parts.length) parts.push(tokens.slice(start));
    return parts;
}

function constantExpressionType(constant) {
    if (!constant || constant.value === undefined || constant.value === "") return "unknown";
    const value = String(constant.value).trim();
    if (/^(?:"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')$/.test(value)) {
        return value[0] === '"' ? "string" : "char";
    }
    if (/^[+-]?(?:0[xX][0-9A-Fa-f]+|\d+(?:\.\d+)?)$/.test(value)) return "int";
    return "unknown";
}

function inferFunctionReturnTypes(functions, tokens, variables, constants) {
    const functionMap = new Map(functionCatalog.map(fn => [fn.name, fn]));
    for (const fn of functions) {
        if (!fn.external || !functionMap.has(fn.name)) functionMap.set(fn.name, fn);
    }

    for (let pass = 0; pass < 3; pass++) {
        let changed = false;
        for (const fn of functions) {
            if (fn.returnType !== "unknown" || fn.bodyOpenOffset === null || fn.bodyOpenOffset === undefined ||
                fn.bodyCloseOffset === null || fn.bodyCloseOffset === undefined) continue;
            const returns = [];
            for (let i = 0; i < tokens.length; i++) {
                if (tokens[i].start <= fn.bodyOpenOffset || tokens[i].start >= fn.bodyCloseOffset || tokens[i].value !== "return") continue;
                let end = i + 1;
                let paren = 0;
                let bracket = 0;
                for (; end < tokens.length; end++) {
                    const value = tokens[end].value;
                    if (value === "(") paren++;
                    else if (value === ")") paren--;
                    else if (value === "[") bracket++;
                    else if (value === "]") bracket--;
                    if (value === ";" && paren === 0 && bracket === 0) break;
                }
                const expression = tokens.slice(i + 1, end);
                returns.push(expression.length
                    ? expressionType(expression, variables, functionMap, constants, expression[0].start)
                    : "void");
            }
            if (!returns.length || returns.some(type => type === "unknown")) continue;
            const inferred = returns[0];
            if (returns.every(type => type === inferred)) {
                fn.returnType = inferred;
                fn.returnTypeSource = "inferred from return statements";
                functionMap.set(fn.name, fn);
                changed = true;
            }
        }
        if (!changed) break;
    }
}

function expressionType(tokens, variables, functions, constants, offset) {
    if (!tokens || !tokens.length) return "unknown";
    let list = tokens.slice();
    const expressionOffset = offset === undefined ? list[0].start : offset;

    while (list.length >= 2 && list[0].value === "(" && findMatching(list, 0, "(", ")") === list.length - 1) {
        list = list.slice(1, -1);
    }
    if (!list.length) return "unknown";

    if (list[0].value === "&" || list[0].value === "*") return "unknown";

    // Recognize simple C-style casts without attempting general pointer analysis.
    if (list[0].value === "(" && list[1] && TYPE_WORDS.has(list[1].value) && list[2] && list[2].value === ")" && list.length > 3) {
        return list[1].value;
    }

    // Array indexing preserves the declared element type for primitive LGC arrays.
    if (list[0].type === "identifier" && list[1] && list[1].value === "[") {
        const close = findMatching(list, 1, "[", "]");
        if (close === list.length - 1) {
            const variable = resolveVariable(variables, list[0].value, list[0].start);
            return variable ? (variable.type || "unknown") : "unknown";
        }
    }

    if (list.length >= 2 && list[0].type === "identifier" && list[1].value === "(") {
        const close = findMatching(list, 1, "(", ")");
        if (close === list.length - 1) {
            const fn = functions.get(list[0].value);
            return fn ? (fn.returnType || "unknown") : "unknown";
        }
    }

    if (list.length === 1) {
        const token = list[0];
        if (token.type === "string") return "string";
        if (token.type === "char") return "char";
        if (token.type === "number") return "int";
        if (token.type === "identifier") {
            const variable = resolveVariable(variables, token.value, token.start === undefined ? expressionOffset : token.start);
            if (variable) return variable.type || "unknown";
            const constant = constants.get(token.value);
            if (constant) return constantExpressionType(constant);
            const fn = functions.get(token.value);
            if (fn) return fn.returnType || "unknown";
            if (token.value === "sizeof") return "int";
        }
        return "unknown";
    }

    if (["+", "-", "!", "~", "++", "--"].includes(list[0].value) && list.length > 1) {
        if (list[0].value === "!" || list[0].value === "~") return "int";
        const operandType = expressionType(list.slice(1), variables, functions, constants, list[1].start);
        return operandType === "int" || operandType === "char" ? "int" : "unknown";
    }

    const comparisonParts = splitTopLevel(list, new Set(["==", "!=", "<", ">", "<=", ">=", "&&", "||"]));
    if (comparisonParts.length) return "int";

    const plusParts = splitTopLevel(list, new Set(["+"]));
    if (plusParts.length) {
        const types = plusParts.map(part => expressionType(part, variables, functions, constants, part[0] ? part[0].start : expressionOffset));
        if (types.includes("string")) return "string";
        if (types.every(type => type === "int" || type === "char")) return "int";
        return "unknown";
    }

    const arithmeticParts = splitTopLevel(list, new Set(["-", "*", "/", "%", "<<", ">>", "&", "|", "^"]));
    if (arithmeticParts.length) {
        const types = arithmeticParts.map(part => expressionType(part, variables, functions, constants, part[0] ? part[0].start : expressionOffset));
        return types.every(type => type === "int" || type === "char") ? "int" : "unknown";
    }

    if (list[0].value === "!" || list[0].value === "~" || list[0].value === "++" || list[0].value === "--" ||
        list[list.length - 1].value === "++" || list[list.length - 1].value === "--") return "int";

    if (list.some(token => token.type === "string")) return "string";
    if (list.some(token => token.type === "number")) return "unknown";
    return "unknown";
}

function areTypesCompatible(expected, actual) {
    if (expected === actual) return true;
    // LGC uses integer storage for character values, as in C integer promotions.
    return (expected === "int" && actual === "char") || (expected === "char" && actual === "int");
}

function diagnostic(document, token, message, severity) {
    const start = document.positionAt(token.start);
    const end = document.positionAt(token.end);
    const result = new vscode.Diagnostic(new vscode.Range(start, end), message, severity);
    result.source = "Locoland LGC";
    return result;
}

function analyzeDocument(document, parsed, functions, constants) {
    const config = vscode.workspace.getConfiguration("lgc.analysis", document.uri);
    if (!config.get("enabled", true)) return [];

    const severityValue = config.get("severity", "Warning");
    const severity = severityValue === "Error" ? vscode.DiagnosticSeverity.Error :
        severityValue === "Information" ? vscode.DiagnosticSeverity.Information : vscode.DiagnosticSeverity.Warning;
    const diagnostics = [];
    const tokens = parsed.tokens;
    const variables = parsed.variables;

    const brackets = [];
    const closingToOpening = { ")": "(", "]": "[", "}": "{" };
    const matching = { "(": ")", "[": "]", "{": "}" };
    for (const token of tokens) {
        if (["(", "[", "{"].includes(token.value)) brackets.push(token);
        else if (closingToOpening[token.value]) {
            const top = brackets[brackets.length - 1];
            if (!top || top.value !== closingToOpening[token.value]) {
                diagnostics.push(diagnostic(document, token, `Unmatched closing '${token.value}'.`, vscode.DiagnosticSeverity.Error));
            } else {
                brackets.pop();
            }
        }
    }
    for (const token of brackets) {
        diagnostics.push(diagnostic(document, token, `Unclosed '${token.value}'.`, vscode.DiagnosticSeverity.Error));
    }

    const checkArity = config.get("checkFunctionArity", true);
    const checkTypes = config.get("checkArgumentTypes", true);
    const strictUnknown = config.get("strictUnknownFunctions", false);
    const knownFunctionNames = new Set(functions.keys());

    for (let i = 0; i < tokens.length - 1; i++) {
        const token = tokens[i];
        if (token.type !== "identifier" || tokens[i + 1].value !== "(") continue;
        if (parsed.declarationPositions.has(token.start) || CONTROL_WORDS.has(token.value)) continue;

        const lineStart = document.getText().lastIndexOf("\n", token.start - 1) + 1;
        const prefix = document.getText().slice(lineStart, token.start);
        if (/^\s*#\s*define\b/.test(prefix)) continue;

        const close = findMatching(tokens, i + 1, "(", ")");
        if (close < 0) continue;
        const args = splitTokenArguments(tokens, i + 2, close);
        const fn = functions.get(token.value);

        if (!fn) {
            if (strictUnknown && !knownFunctionNames.has(token.value)) {
                diagnostics.push(diagnostic(document, token, `Unknown function '${token.value}'. Add its script to the workspace or declare it before using strict mode.`, severity));
            }
            continue;
        }

        const required = (fn.params || []).filter(param => !param.optional).length;
        const maximum = (fn.params || []).length;
        if (checkArity && (args.length < required || args.length > maximum)) {
            diagnostics.push(diagnostic(document, token,
                `${token.value} expects ${required === maximum ? required : `${required}-${maximum}`} argument(s), but ${args.length} supplied.`,
                severity));
        }

        if (checkTypes && fn.params) {
            for (let argIndex = 0; argIndex < Math.min(args.length, fn.params.length); argIndex++) {
                const expected = fn.params[argIndex].type;
                if (expected !== "int" && expected !== "string" && expected !== "char") continue;
                const argument = args[argIndex];
                const actual = expressionType(argument, variables, functions, constants, argument[0] ? argument[0].start : token.start);
                if (actual !== "unknown" && !areTypesCompatible(expected, actual)) {
                    const arg = args[argIndex][0];
                    diagnostics.push(diagnostic(document, arg,
                        `Argument ${argIndex + 1} of ${token.value} expects ${expected}, but the expression is ${actual}.`,
                        severity));
                }
            }
        }
    }

    if (config.get("checkAssignments", true)) {
        for (let i = 1; i < tokens.length - 1; i++) {
            if (tokens[i].value !== "=" || tokens[i - 1].type !== "identifier") continue;
            const previous = tokens[i - 2] ? tokens[i - 2].value : "";
            const next = tokens[i + 1] ? tokens[i + 1].value : "";
            if (["=", "!", "<", ">"].includes(previous) || next === "=") continue;
            const target = resolveVariable(variables, tokens[i - 1].value, tokens[i - 1].start);
            if (!target || !["int", "string", "char"].includes(target.type)) continue;

            let end = i + 1;
            let nesting = 0;
            for (; end < tokens.length; end++) {
                const v = tokens[end].value;
                if (["(", "["].includes(v)) nesting++;
                else if ([")", "]"].includes(v)) {
                    if (nesting === 0) break;
                    nesting--;
                }
                if (nesting === 0 && (v === ";" || v === ",")) break;
            }
            const rhsTokens = tokens.slice(i + 1, end);
            const actual = expressionType(rhsTokens, variables, functions, constants, rhsTokens[0] ? rhsTokens[0].start : tokens[i].start);
            if (actual !== "unknown" && !areTypesCompatible(target.type, actual)) {
                diagnostics.push(diagnostic(document, tokens[i - 1],
                    `Cannot assign ${actual} to ${target.type} variable '${tokens[i - 1].value}'.`,
                    severity));
            }
        }
    }

    return diagnostics;
}

function markdownForFunction(fn) {
    const lines = [`**\`${fn.signature || `${fn.name}()`}\`**`, "", fn.description || "LGC function."];
    if (fn.returnType && fn.returnType !== "unknown") lines.push("", `**Returns:** \`${fn.returnType}\``);
    if (fn.returnTypeSource) lines.push("**Return type note:** inferred from consistent return statements in the workspace script.");
    if (fn.source) lines.push("", `**Source:** \`${fn.source}\`${fn.id !== null && fn.id !== undefined ? ` (engine function ID ${fn.id})` : ""}`);
    if (fn.params && fn.params.length) {
        lines.push("", "**Parameters:**");
        for (const param of fn.params) {
            lines.push(`- \`${param.name}\` (\`${param.type}\`)${param.optional ? ` — optional, default \`${param.default}\`` : ""}`);
        }
    }
    return lines.join("\n");
}

function completionForFunction(fn) {
    const item = new vscode.CompletionItem(fn.name, vscode.CompletionItemKind.Function);
    item.detail = fn.signature || `${fn.name}()`;
    item.documentation = new vscode.MarkdownString(markdownForFunction(fn));
    const requiredParams = (fn.params || []).filter(param => !param.optional);
    if (requiredParams.length === 0) {
        item.insertText = new vscode.SnippetString(`${fn.name}()`);
    } else {
        const placeholders = requiredParams.map((param, index) => `\${${index + 1}:${param.name}}`);
        item.insertText = new vscode.SnippetString(`${fn.name}(${placeholders.join(", ")})`);
    }
    item.sortText = fn.category === "Workspace function" ? `0_${fn.name}` :
        fn.category === "Engine API" ? `1_${fn.name}` : `2_${fn.name}`;
    return item;
}

function wordRangeAt(document, position) {
    const line = document.lineAt(position.line).text;
    const before = line.slice(0, position.character);
    const match = before.match(/[A-Za-z_]\w*$/);
    if (!match) return null;
    return new vscode.Range(position.line, position.character - match[0].length, position.line, position.character);
}

function currentFunctionAt(document, position, functions) {
    const before = document.getText(new vscode.Range(new vscode.Position(0, 0), position));
    const tokens = tokenize(before);
    let openIndex = -1;
    const stack = [];
    for (let i = 0; i < tokens.length; i++) {
        if (tokens[i].value === "(") stack.push(i);
        else if (tokens[i].value === ")" && stack.length) stack.pop();
    }
    if (!stack.length) return null;
    openIndex = stack[stack.length - 1];
    const nameToken = tokens[openIndex - 1];
    if (!nameToken || nameToken.type !== "identifier") return null;
    const fn = functions.get(nameToken.value);
    if (!fn) return null;

    let activeParameter = 0;
    let paren = 0;
    let bracket = 0;
    for (let i = openIndex + 1; i < tokens.length; i++) {
        const v = tokens[i].value;
        if (v === "(") paren++;
        else if (v === ")") {
            if (paren === 0) break;
            paren--;
        } else if (v === "[") bracket++;
        else if (v === "]") bracket--;
        else if (v === "," && paren === 0 && bracket === 0) activeParameter++;
    }
    return { fn, activeParameter };
}

async function refreshWorkspaceIndex() {
    if (refreshPromise) return refreshPromise;
    refreshPromise = (async () => {
        const limit = vscode.workspace.getConfiguration("lgc.analysis").get("maxWorkspaceFiles", 500);
        const files = await vscode.workspace.findFiles("**/*.lgc", "**/{node_modules,.git,dist,out}/**", limit);
        const nextFunctions = new Map();
        const nextConstants = new Map();
        for (const uri of files) {
            try {
                const openDoc = vscode.workspace.textDocuments.find(doc => doc.uri.toString() === uri.toString());
                const text = openDoc ? openDoc.getText() : Buffer.from(await vscode.workspace.fs.readFile(uri)).toString("utf8");
                const parsed = parseSource(text, uri);
                for (const fn of parsed.functions) {
                    const isBundledEngineFunction = fn.external && functionCatalog.some(item => item.name === fn.name);
                    if (!isBundledEngineFunction && !nextFunctions.has(fn.name)) nextFunctions.set(fn.name, fn);
                }
                for (const [name, value] of parsed.constants) nextConstants.set(name, value);
            } catch (_error) {
                // Ignore files that cannot be read; other workspace scripts remain available.
            }
        }
        workspaceFunctions = nextFunctions;
        workspaceConstants = nextConstants;
        workspaceFiles = files;
    })().finally(() => { refreshPromise = undefined; });
    return refreshPromise;
}

function scheduleWorkspaceRefresh() {
    if (refreshTimer) clearTimeout(refreshTimer);
    refreshTimer = setTimeout(() => {
        refreshTimer = undefined;
        refreshWorkspaceIndex().then(() => {
            for (const document of vscode.workspace.textDocuments) {
                if (document.languageId === "lgc") updateDiagnostics(document);
            }
        }).catch(() => undefined);
    }, 250);
}

function updateDiagnostics(document) {
    if (!diagnosticCollection || document.languageId !== "lgc") return;
    const parsed = parseSource(document.getText(), document.uri);
    const functions = makeFunctionMap(document, parsed);
    const constants = makeConstantMap(parsed);
    diagnosticCollection.set(document.uri, analyzeDocument(document, parsed, functions, constants));
}

function activate(context) {
    diagnosticCollection = vscode.languages.createDiagnosticCollection("lgc");
    context.subscriptions.push(diagnosticCollection);

    const selector = { language: "lgc", scheme: "file" };

    context.subscriptions.push(vscode.languages.registerDocumentFormattingEditProvider(selector, {
        provideDocumentFormattingEdits(document, formattingOptions) {
            const config = vscode.workspace.getConfiguration("lgc.format", document.uri);
            const source = document.getText();
            const configuredIndentStyle = config.get("indentStyle", "editor");
            const configuredIndentSize = config.get("indentSize", 0);
            const formatted = formatLgcText(source, {
                indentStyle: configuredIndentStyle === "editor"
                    ? (formattingOptions && formattingOptions.insertSpaces === false ? "tabs" : "spaces")
                    : configuredIndentStyle,
                indentSize: configuredIndentSize > 0
                    ? configuredIndentSize
                    : (formattingOptions && formattingOptions.tabSize ? formattingOptions.tabSize : 4),
                braceStyle: config.get("braceStyle", "sameLine"),
                spaceBeforeParens: config.get("spaceBeforeParens", "control"),
                spaceInsideParentheses: config.get("spaceInsideParentheses", false),
                spaceInsideBrackets: config.get("spaceInsideBrackets", false),
                spaceAroundOperators: config.get("spaceAroundOperators", true),
                spaceAfterComma: config.get("spaceAfterComma", true),
                spaceBeforeColon: config.get("spaceBeforeColon", true),
                spaceAfterColon: config.get("spaceAfterColon", true),
                blankLinesBetweenFunctions: config.get("blankLinesBetweenFunctions", 1),
                preserveBlankLines: config.get("preserveBlankLines", true),
                maxBlankLines: config.get("maxBlankLines", 1),
                indentCaseLabels: config.get("indentCaseLabels", false),
                keepEmptyBlocksSingleLine: config.get("keepEmptyBlocksSingleLine", true),
                insertFinalNewline: config.get("insertFinalNewline", true)
            });
            if (formatted === source) return [];
            return [vscode.TextEdit.replace(
                new vscode.Range(document.positionAt(0), document.positionAt(source.length)),
                formatted
            )];
        }
    }));

    context.subscriptions.push(vscode.languages.registerCompletionItemProvider(selector, {
        provideCompletionItems(document, position) {
            const parsed = parseSource(document.getText(), document.uri);
            const functions = makeFunctionMap(document, parsed);
            const constants = makeConstantMap(parsed);
            const items = [];
            const prefixRange = wordRangeAt(document, position);
            const prefix = prefixRange ? document.getText(prefixRange) : "";

            for (const fn of functions.values()) {
                if (!prefix || fn.name.toLowerCase().startsWith(prefix.toLowerCase())) items.push(completionForFunction(fn));
            }
            for (const constant of constants.values()) {
                if (!prefix || constant.name.toLowerCase().startsWith(prefix.toLowerCase())) {
                    const item = new vscode.CompletionItem(constant.name, vscode.CompletionItemKind.Constant);
                    item.detail = constant.value ? `LGC constant = ${constant.value}` : "LGC constant";
                    item.documentation = new vscode.MarkdownString(constant.description || "Game-script constant.");
                    item.sortText = `3_${constant.name}`;
                    items.push(item);
                }
            }
            if (!prefix) {
                for (const keyword of KEYWORDS) {
                    const item = new vscode.CompletionItem(keyword, vscode.CompletionItemKind.Keyword);
                    item.sortText = `9_${keyword}`;
                    items.push(item);
                }
            }
            return new vscode.CompletionList(items, false);
        }
    }, ".", "#"));

    context.subscriptions.push(vscode.languages.registerHoverProvider(selector, {
        provideHover(document, position) {
            const range = document.getWordRangeAtPosition(position, /[A-Za-z_]\w*/);
            if (!range) return null;
            const name = document.getText(range);
            const parsed = parseSource(document.getText(), document.uri);
            const functions = makeFunctionMap(document, parsed);
            const constants = makeConstantMap(parsed);
            const variable = resolveVariable(parsed.variables, name, document.offsetAt(position));
            if (variable) return new vscode.Hover(new vscode.MarkdownString(`**\`${name}\`**\n\nType: \`${variable.type}\``), range);
            const fn = functions.get(name);
            if (fn) return new vscode.Hover(new vscode.MarkdownString(markdownForFunction(fn)), range);
            const constant = constants.get(name);
            if (constant) {
                return new vscode.Hover(new vscode.MarkdownString(`**\`${name}\`**\n\n${constant.description || "LGC constant."}${constant.value ? `\n\n**Value:** \`${constant.value}\`` : ""}\n\n**Source:** \`${constant.source || "workspace"}\``), range);
            }
            return null;
        }
    }));

    context.subscriptions.push(vscode.languages.registerSignatureHelpProvider(selector, {
        provideSignatureHelp(document, position) {
            const parsed = parseSource(document.getText(), document.uri);
            const functions = makeFunctionMap(document, parsed);
            const active = currentFunctionAt(document, position, functions);
            if (!active) return null;
            const info = new vscode.SignatureInformation(active.fn.signature || `${active.fn.name}()`, active.fn.description || "");
            info.parameters = (active.fn.params || []).map(param => new vscode.ParameterInformation(
                `${param.type} ${param.name}${param.optional ? ` = ${param.default}` : ""}`,
                ""
            ));
            const help = new vscode.SignatureHelp();
            help.signatures = [info];
            help.activeSignature = 0;
            help.activeParameter = Math.min(active.activeParameter, Math.max(0, info.parameters.length - 1));
            return help;
        }
    }, "(", ","));

    context.subscriptions.push(vscode.languages.registerDefinitionProvider(selector, {
        provideDefinition(document, position) {
            const range = document.getWordRangeAtPosition(position, /[A-Za-z_]\w*/);
            if (!range) return null;
            const name = document.getText(range);
            const fn = workspaceFunctions.get(name);
            if (fn && fn.uri && fn.position !== undefined) {
                return new vscode.Location(fn.uri, documentPositionFromOffset(fn.uri, fn.position));
            }
            return null;
        }
    }));

    context.subscriptions.push(vscode.languages.registerWorkspaceSymbolProvider({
        provideWorkspaceSymbols(query) {
            const symbols = [];
            for (const fn of workspaceFunctions.values()) {
                if (!query || fn.name.toLowerCase().includes(query.toLowerCase())) {
                    if (fn.uri && fn.position !== undefined) {
                        symbols.push(new vscode.SymbolInformation(
                            fn.name,
                            vscode.SymbolKind.Function,
                            fn.category || "LGC",
                            new vscode.Location(fn.uri, documentPositionFromOffset(fn.uri, fn.position))
                        ));
                    }
                }
            }
            return symbols;
        }
    }));

    context.subscriptions.push(vscode.workspace.onDidOpenTextDocument(document => {
        if (document.languageId === "lgc") {
            updateDiagnostics(document);
            scheduleWorkspaceRefresh();
        }
    }));
    context.subscriptions.push(vscode.workspace.onDidChangeTextDocument(event => {
        if (event.document.languageId === "lgc") updateDiagnostics(event.document);
    }));
    context.subscriptions.push(vscode.workspace.onDidSaveTextDocument(document => {
        if (document.languageId === "lgc") {
            updateDiagnostics(document);
            scheduleWorkspaceRefresh();
        }
    }));
    context.subscriptions.push(vscode.workspace.onDidCloseTextDocument(document => {
        if (document.languageId === "lgc") diagnosticCollection.delete(document.uri);
    }));

    context.subscriptions.push(vscode.commands.registerCommand("lgc.formatDocument", () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || editor.document.languageId !== "lgc") {
            vscode.window.showInformationMessage("Open an .lgc file to format it.");
            return;
        }
        return vscode.commands.executeCommand("editor.action.formatDocument");
    }));

    context.subscriptions.push(vscode.commands.registerCommand("lgc.analyzeCurrentFile", () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || editor.document.languageId !== "lgc") {
            vscode.window.showInformationMessage("Open an .lgc file to analyze it.");
            return;
        }
        updateDiagnostics(editor.document);
        const count = diagnosticCollection.get(editor.document.uri) || [];
        vscode.window.showInformationMessage(`LGC analysis completed: ${count.length} diagnostic(s).`);
    }));

    refreshWorkspaceIndex().then(() => {
        for (const document of vscode.workspace.textDocuments) {
            if (document.languageId === "lgc") updateDiagnostics(document);
        }
    }).catch(() => undefined);

    for (const document of vscode.workspace.textDocuments) {
        if (document.languageId === "lgc") updateDiagnostics(document);
    }
}

function documentPositionFromOffset(uri, position) {
    if (position && typeof position.line === "number" && typeof position.character === "number") {
        return new vscode.Position(position.line, position.character);
    }
    const document = vscode.workspace.textDocuments.find(doc => doc.uri.toString() === uri.toString());
    if (!document) return new vscode.Position(0, 0);
    return document.positionAt(typeof position === "number" ? position : 0);
}

function deactivate() {
    if (refreshTimer) clearTimeout(refreshTimer);
}

module.exports = {
    activate,
    deactivate,
    __test: {
        tokenize,
        parseSource,
        makeFunctionMap,
        makeConstantMap,
        analyzeDocument,
        expressionType,
        resolveVariable,
        parseVariables,
        inferFunctionReturnTypes,
        splitTokenArguments,
        formatLgcText
    }
};
