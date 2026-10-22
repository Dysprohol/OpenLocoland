// Creates struct types from re/types.txt and applies them to globals, vtables and functions.
// Runs after ApplySymbols (function names come from symbols.csv). File format: see the header
// of re/types.txt. Every problem is printed as "types.txt:<line>: ..." and counted; the run
// goes on, so one bad line does not stop the decompilation.
// @category Locoland

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.lang.PrototypeModel;
import ghidra.program.model.listing.*;
import ghidra.program.model.listing.Function.FunctionUpdateType;
import ghidra.program.model.pcode.HighFunctionDBUtil;
import ghidra.program.model.pcode.HighFunctionDBUtil.ReturnCommitOption;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.util.CodeUnitInsertionException;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class ApplyTypes extends GhidraScript {
    static final CategoryPath CATEGORY = new CategoryPath("/Locoland");

    /** One "struct" or "vtable" block of the file. */
    static class Block {
        String kind, name;
        int line, size = -1, count;
        long address;
        List<String[]> lines = new ArrayList<>();   // {lineNo, text, comment}
    }

    DataTypeManager dtm;
    String file;
    int errors;
    Map<String, Structure> structs = new LinkedHashMap<>();

    void error(int line, String message) {
        printerr("types.txt:" + line + ": " + message);
        errors++;
    }

    @Override
    protected void run() throws Exception {
        file = getScriptArgs()[0];
        dtm = currentProgram.getDataTypeManager();
        List<String> text = Files.readAllLines(Paths.get(file), StandardCharsets.UTF_8);

        List<Block> blocks = new ArrayList<>();
        List<String[]> globals = new ArrayList<>(), funcs = new ArrayList<>(), methods = new ArrayList<>(),
            locals = new ArrayList<>();
        Block open = null;
        for (int i = 0; i < text.size(); i++) {
            String raw = text.get(i);
            int hash = raw.indexOf('#');
            String comment = hash >= 0 ? raw.substring(hash + 1).strip() : "";
            String line = (hash >= 0 ? raw.substring(0, hash) : raw).strip();
            if (line.isEmpty()) continue;
            String no = String.valueOf(i + 1);
            String[] w = line.split("\\s+", 2);
            if (open != null) {
                if (line.equals("end")) { open = null; continue; }
                open.lines.add(new String[] { no, line, comment });
                continue;
            }
            switch (w[0]) {
                case "struct": {
                    Block b = new Block();
                    String[] p = w[1].split("\\s+");
                    b.kind = "struct"; b.name = p[0]; b.line = i + 1;
                    if (p.length > 1) b.size = Long.decode(p[1]).intValue();
                    blocks.add(b); open = b;
                    break;
                }
                case "vtable": {
                    Block b = new Block();
                    String[] p = w[1].split("\\s+");
                    b.kind = "vtable"; b.name = p[0]; b.line = i + 1;
                    b.address = Long.decode(p[1]); b.count = Integer.decode(p[2]);
                    blocks.add(b); open = b;
                    break;
                }
                case "global": globals.add(new String[] { no, w[1] }); break;
                case "func": funcs.add(new String[] { no, w[1] }); break;
                case "method": methods.add(new String[] { no, w[1] }); break;
                case "local": locals.add(new String[] { no, w[1] }); break;
                default: error(i + 1, "unknown keyword " + w[0]);
            }
        }

        // 1. empty structs first, so that fields and prototypes can point to any of them
        for (Block b : blocks) {
            StructureDataType s = new StructureDataType(CATEGORY, b.name, 0, dtm);
            structs.put(b.name, (Structure) dtm.addDataType(s, DataTypeConflictHandler.REPLACE_HANDLER));
        }
        // 2. fields of the structs, in file order (a struct embedded by value must come earlier)
        for (Block b : blocks) if (b.kind.equals("struct")) fillStruct(b);
        // 3. function prototypes
        for (String[] f : methods) applyMethod(Integer.parseInt(f[0]), f[1]);
        for (String[] f : funcs) applyPrototype(Integer.parseInt(f[0]), f[1]);
        for (String[] l : locals) applyLocal(Integer.parseInt(l[0]), l[1]);
        // 4. vtables: function pointer fields typed from the functions in the table
        for (Block b : blocks) if (b.kind.equals("vtable")) fillVtable(b);
        // 5. global variables
        for (String[] g : globals) applyGlobal(Integer.parseInt(g[0]), g[1]);

        println("Types: " + structs.size() + " structs, " + methods.size() + " methods, "
            + funcs.size() + " prototypes, " + locals.size() + " locals, " + globals.size() + " globals, errors: " + errors);
    }

    // ---- types

    static final Pattern TYPE = Pattern.compile("^([A-Za-z_][A-Za-z0-9_]*)\\s*(\\**)\\s*((?:\\[[^\\]]+\\])*)$");

    /** Parses "base", "base*", "base**", "base[16]", "base*[0x800]", "base[4][4]". */
    DataType parseType(String spec, int line) {
        Matcher m = TYPE.matcher(spec.strip());
        if (!m.matches()) { error(line, "bad type '" + spec + "'"); return null; }
        DataType dt = baseType(m.group(1));
        if (dt == null) { error(line, "unknown type '" + m.group(1) + "'"); return null; }
        for (int i = 0; i < m.group(2).length(); i++) dt = dtm.getPointer(dt);
        String dims = m.group(3);
        if (!dims.isEmpty()) {
            List<Integer> n = new ArrayList<>();
            for (String d : dims.substring(1, dims.length() - 1).split("\\]\\[")) n.add(Long.decode(d.strip()).intValue());
            for (int i = n.size() - 1; i >= 0; i--) {
                if (dt.getLength() <= 0) { error(line, "array of a type without size: " + spec); return null; }
                dt = new ArrayDataType(dt, n.get(i), dt.getLength(), dtm);
            }
        }
        return dt;
    }

    DataType baseType(String name) {
        switch (name) {
            case "void": return VoidDataType.dataType;
            case "char": return CharDataType.dataType;
            case "byte": case "uchar": return ByteDataType.dataType;
            case "bool": return BooleanDataType.dataType;
            case "short": return ShortDataType.dataType;
            case "ushort": return UnsignedShortDataType.dataType;
            case "int": return IntegerDataType.dataType;
            case "uint": return UnsignedIntegerDataType.dataType;
            case "float": return FloatDataType.dataType;
            case "double": return DoubleDataType.dataType;
            case "undefined": case "undefined1": return Undefined1DataType.dataType;
            case "undefined2": return Undefined2DataType.dataType;
            case "undefined4": return Undefined4DataType.dataType;
            case "code": return new PointerDataType(VoidDataType.dataType, dtm).getDataType();
        }
        if (structs.containsKey(name)) return structs.get(name);
        List<DataType> found = new ArrayList<>();
        dtm.findDataTypes(name, found);   // Windows types known to the program (RECT, HWND...)
        return found.isEmpty() ? null : found.get(0);
    }

    // ---- structs

    static final Pattern FIELD = Pattern.compile("^(0x[0-9a-fA-F]+|\\d+)\\s+(.+?)\\s+([A-Za-z_][A-Za-z0-9_]*)$");

    void fillStruct(Block b) {
        Structure s = structs.get(b.name);
        if (b.size > 0) s.growStructure(b.size);
        for (String[] l : b.lines) {
            int line = Integer.parseInt(l[0]);
            Matcher m = FIELD.matcher(l[1]);
            if (!m.matches()) { error(line, "expected '<offset> <type> <name>'"); continue; }
            int offset = Long.decode(m.group(1)).intValue();
            DataType dt = parseType(m.group(2), line);
            if (dt == null) continue;
            int len = dt.getLength();
            if (len <= 0) { error(line, "type without size: " + m.group(2)); continue; }
            if (b.size > 0 && offset + len > b.size) {
                error(line, String.format("field %s ends at 0x%x, past the struct size 0x%x", m.group(3), offset + len, b.size));
                continue;
            }
            if (offset + len > s.getLength()) s.growStructure(offset + len - s.getLength());
            for (DataTypeComponent c : s.getDefinedComponents()) {
                if (c.getOffset() < offset + len && offset < c.getOffset() + c.getLength()) {
                    error(line, String.format("field %s at 0x%x overlaps %s", m.group(3), offset, c.getFieldName()));
                    dt = null;
                    break;
                }
            }
            if (dt == null) continue;
            s.replaceAtOffset(offset, dt, len, m.group(3), l[2].isEmpty() ? null : l[2]);
        }
        if (b.size < 0 && s.getLength() == 0) error(b.line, "struct " + b.name + " has no size and no fields");
    }

    // ---- functions

    Function functionAt(long address, int line) {
        Address a = toAddr(address);
        Function f = getFunctionAt(a);
        if (f == null) {
            disassemble(a);
            f = createFunction(a, null);
        }
        if (f == null) error(line, "no function at " + a);
        return f;
    }

    /** "method <address> <Struct>": __thiscall with a typed this; return and stack parameters stay as found. */
    void applyMethod(int line, String spec) throws Exception {
        String[] p = spec.split("\\s+");
        if (p.length != 2) { error(line, "expected 'method <address> <Struct>'"); return; }
        Function f = functionAt(Long.decode(p[0]), line);
        DataType cls = parseType(p[1] + "*", line);
        if (f == null || cls == null) return;
        if (f.getSignatureSource() == SourceType.DEFAULT) commitDecompiledSignature(f, line);
        List<DataType> types = new ArrayList<>();
        List<String> names = new ArrayList<>();
        types.add(f.getReturnType());
        types.add(cls); names.add("this");
        for (Parameter par : f.getParameters()) {
            if (!par.isStackVariable()) continue;     // the old ECX/EDX parameter is replaced by this
            types.add(par.getDataType()); names.add(par.getName());
        }
        setSignature(f, "__thiscall", types, names, line);
    }

    DecompInterface decompiler;

    /** Stores the parameters the decompiler finds (until then the function has none in the database). */
    void commitDecompiledSignature(Function f, int line) throws Exception {
        if (decompiler == null) {
            decompiler = new DecompInterface();
            decompiler.openProgram(currentProgram);
        }
        DecompileResults r = decompiler.decompileFunction(f, 60, monitor);
        if (r == null || r.getHighFunction() == null) { error(line, "cannot decompile " + f.getName()); return; }
        HighFunctionDBUtil.commitParamsToDatabase(r.getHighFunction(), true, ReturnCommitOption.COMMIT, SourceType.ANALYSIS);
    }

    static final Pattern PROTO = Pattern.compile("^(.+?)\\s+(__thiscall|__cdecl|__stdcall|__fastcall)\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*\\((.*)\\)$");

    /** "func <address> <return> <convention> <Name>(<type> <name>, ...)". */
    void applyPrototype(int line, String spec) throws Exception {
        String[] p = spec.split("\\s+", 2);
        Matcher m = PROTO.matcher(p.length > 1 ? p[1].strip() : "");
        if (!m.matches()) { error(line, "expected 'func <address> <return> <convention> <Name>(<params>)'"); return; }
        Function f = functionAt(Long.decode(p[0]), line);
        if (f == null) return;
        if (!f.getName().equals(m.group(3))) error(line, "name " + m.group(3) + " differs from the function name " + f.getName());
        List<DataType> types = new ArrayList<>();
        List<String> names = new ArrayList<>();
        types.add(parseType(m.group(1), line));
        String params = m.group(4).strip();
        boolean varargs = false;
        if (!params.isEmpty() && !params.equals("void")) {
            for (String par : params.split(",")) {
                par = par.strip();
                if (par.equals("...")) { varargs = true; continue; }
                int cut = Math.max(par.lastIndexOf(' '), par.lastIndexOf('*'));
                if (cut < 0) { error(line, "parameter without a name: " + par); return; }
                types.add(parseType(par.substring(0, cut + 1), line));
                names.add(par.substring(cut + 1).strip());
            }
        }
        if (types.contains(null)) return;
        setSignature(f, m.group(2), types, names, line);
        f.setVarArgs(varargs);
    }

    /** Storage is computed from the convention with the explicit `this` as an ordinary first parameter. */
    void setSignature(Function f, String convention, List<DataType> types, List<String> names, int line) throws Exception {
        PrototypeModel model = currentProgram.getCompilerSpec().getCallingConvention(convention);
        VariableStorage[] storage = model.getStorageLocations(currentProgram, types.toArray(new DataType[0]), false);
        if (storage.length != types.size()) { error(line, "cannot assign parameter storage for " + f.getName()); return; }
        Parameter ret = new ReturnParameterImpl(types.get(0), storage[0], currentProgram);
        List<Parameter> params = new ArrayList<>();
        for (int i = 1; i < types.size(); i++)
            params.add(new ParameterImpl(names.get(i - 1), types.get(i), storage[i], currentProgram, SourceType.USER_DEFINED));
        f.updateFunction(convention, ret, params, FunctionUpdateType.CUSTOM_STORAGE, true, SourceType.USER_DEFINED);
    }

    /** "local <function address> <stack offset> <type> <name>": a typed local variable on the stack. */
    void applyLocal(int line, String spec) throws Exception {
        String[] p = spec.split("\\s+");
        if (p.length < 4) { error(line, "expected 'local <function address> <stack offset> <type> <name>'"); return; }
        Function f = functionAt(Long.decode(p[0]), line);
        String o = p[1];
        int offset = o.startsWith("-") ? -Long.decode(o.substring(1)).intValue() : Long.decode(o).intValue();
        DataType dt = parseType(String.join("", Arrays.copyOfRange(p, 2, p.length - 1)), line);
        if (f == null || dt == null) return;
        String name = p[p.length - 1];
        for (Variable v : f.getLocalVariables()) {
            if (v.isStackVariable() && v.getStackOffset() < offset + dt.getLength() && offset < v.getStackOffset() + v.getLength())
                f.removeVariable(v);
        }
        f.addLocalVariable(new LocalVariableImpl(name, dt, offset, currentProgram, SourceType.USER_DEFINED), SourceType.USER_DEFINED);
    }

    // ---- vtables

    void fillVtable(Block b) throws Exception {
        Structure s = structs.get(b.name);
        Map<Integer, String> slotNames = new HashMap<>();
        for (String[] l : b.lines) {
            String[] p = l[1].split("\\s+");
            if (p.length != 2) { error(Integer.parseInt(l[0]), "expected '<offset> <name>'"); continue; }
            slotNames.put(Long.decode(p[0]).intValue(), p[1]);
        }
        CategoryPath fnCategory = new CategoryPath(CATEGORY, "functions");
        s.growStructure(b.count * 4);
        for (int i = 0; i < b.count; i++) {
            Address slot = toAddr(b.address + 4L * i);
            long target = getInt(slot) & 0xffffffffL;
            Function f = functionAt(target, b.line);
            String name = slotNames.get(i * 4);
            if (name == null) name = f != null && !f.getName().startsWith("FUN_") ? f.getName() : String.format("vf_%02x", i * 4);
            DataType dt;
            if (f != null) {
                FunctionDefinitionDataType fd = new FunctionDefinitionDataType(f, false);
                fd.setNameAndCategory(fnCategory, b.name + "_" + name);
                dt = dtm.getPointer(dtm.addDataType(fd, DataTypeConflictHandler.REPLACE_HANDLER));
            } else {
                dt = dtm.getPointer(VoidDataType.dataType);
            }
            s.replaceAtOffset(i * 4, dt, 4, name, f != null ? f.getName() : null);
        }
        Address a = toAddr(b.address);
        try {
            DataUtilities.createData(currentProgram, a, s, -1, DataUtilities.ClearDataMode.CLEAR_ALL_CONFLICT_DATA);
        } catch (CodeUnitInsertionException e) {
            error(b.line, "cannot place " + b.name + " at " + a + ": " + e.getMessage());
        }
        if (getSymbolAt(a) == null || getSymbolAt(a).getSource() == SourceType.DEFAULT)
            createLabel(a, b.name, true, SourceType.USER_DEFINED);
    }

    // ---- globals

    /** "global <address> <type> <name>": the name must be the one from symbols.csv. */
    void applyGlobal(int line, String spec) {
        String[] p = spec.split("\\s+");
        if (p.length < 3) { error(line, "expected 'global <address> <type> <name>'"); return; }
        String name = p[p.length - 1];
        String type = String.join("", Arrays.copyOfRange(p, 1, p.length - 1));
        DataType dt = parseType(type, line);
        if (dt == null) return;
        Address a = toAddr(Long.decode(p[0]));
        Symbol sym = getSymbolAt(a);
        if (sym == null || !sym.getName().equals(name)) error(line, "name " + name + " differs from the symbol at " + a);
        try {
            DataUtilities.createData(currentProgram, a, dt, -1, DataUtilities.ClearDataMode.CLEAR_ALL_CONFLICT_DATA);
        } catch (CodeUnitInsertionException e) {
            error(line, "cannot place " + type + " at " + a + ": " + e.getMessage());
        }
    }
}
