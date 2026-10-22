// Decompiles all functions and writes the C code to the directory given as the argument.
// Functions go into files by address range (0x4000 bytes of code each), in address
// order, so consecutive exports give comparable diffs in git. The struct types from
// re/types.txt (category /Locoland) go to types.h in the same directory.
// @category Locoland

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.io.File;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.TreeMap;

public class ExportC extends GhidraScript {
    private static final long RANGE = 0x4000;

    @Override
    protected void run() throws Exception {
        File dir = new File(getScriptArgs()[0]);
        dir.mkdirs();
        for (File old : dir.listFiles((d, n) -> n.endsWith(".c"))) old.delete();
        writeTypes(new File(dir, "types.h"));

        DecompInterface decompiler = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        options.grabFromProgram(currentProgram);
        decompiler.setOptions(options);
        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(false);
        decompiler.openProgram(currentProgram);

        Map<Long, StringBuilder> files = new TreeMap<>();
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        int count = 0, failed = 0;
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            if (f.isThunk() || f.isExternal()) continue;
            long address = f.getEntryPoint().getOffset();
            StringBuilder sb = files.computeIfAbsent(address / RANGE * RANGE, k -> new StringBuilder());
            sb.append("\n// ---- ").append(f.getName()).append(" @ 0x")
              .append(Long.toHexString(address)).append("\n");
            DecompileResults r = decompiler.decompileFunction(f, 60, monitor);
            if (r != null && r.decompileCompleted()) {
                sb.append(r.getDecompiledFunction().getC());
            } else {
                sb.append("/* decompilation failed: ")
                  .append(r == null ? "?" : r.getErrorMessage()).append(" */\n");
                failed++;
            }
            count++;
        }
        decompiler.dispose();

        for (Map.Entry<Long, StringBuilder> e : files.entrySet()) {
            String name = String.format("%08x.c", e.getKey());
            try (PrintWriter pw = new PrintWriter(new File(dir, name), StandardCharsets.UTF_8)) {
                pw.print("// Locoland - code decompiled by Ghidra, range from 0x" + name.substring(0, 8) + "\n");
                pw.print(e.getValue());
            }
        }
        println("Functions: " + count + ", failed: " + failed + ", files: " + files.size());
    }

    /** Writes the structs as C with offsets; runs of undefined bytes become unknown_XXX arrays. */
    private void writeTypes(File out) throws Exception {
        Category cat = currentProgram.getDataTypeManager().getCategory(new CategoryPath("/Locoland"));
        try (PrintWriter pw = new PrintWriter(out, StandardCharsets.UTF_8)) {
            pw.print("// Locoland - struct types from re/types.txt as Ghidra applied them (generated).\n");
            pw.print("// unknown_XXX = bytes nobody has identified yet.\n");
            if (cat == null) return;
            DataType[] types = cat.getDataTypes();
            java.util.Arrays.sort(types, (a, b) -> a.getName().compareTo(b.getName()));
            for (DataType dt : types) {
                if (!(dt instanceof Structure s)) continue;
                pw.printf("%nstruct %s {   // size 0x%x%n", s.getName(), s.getLength());
                int unknownStart = -1;
                for (DataTypeComponent c : s.getComponents()) {
                    boolean undefined = c.getDataType() instanceof DefaultDataType;
                    if (undefined) {
                        if (unknownStart < 0) unknownStart = c.getOffset();
                        continue;
                    }
                    if (unknownStart >= 0) {
                        unknown(pw, unknownStart, c.getOffset());
                        unknownStart = -1;
                    }
                    pw.printf("    /* 0x%03x */ %s;%s%n", c.getOffset(), declaration(c.getDataType(), c.getFieldName()),
                        c.getComment() == null ? "" : "   // " + c.getComment());
                }
                if (unknownStart >= 0) unknown(pw, unknownStart, s.getLength());
                pw.print("};\n");
            }
        }
    }

    private void unknown(PrintWriter pw, int from, int to) {
        pw.printf("    /* 0x%03x */ undefined unknown_%x[0x%x];%n", from, from, to - from);
    }

    private String declaration(DataType dt, String name) {
        if (dt instanceof Array a) return declaration(a.getDataType(), name + "[" + a.getNumElements() + "]");
        if (dt instanceof Pointer p && p.getDataType() instanceof FunctionDefinition f) {
            StringBuilder sb = new StringBuilder(f.getReturnType().getDisplayName());
            sb.append(" (").append(f.getCallingConventionName()).append(" *").append(name).append(")(");
            ParameterDefinition[] args = f.getArguments();
            for (int i = 0; i < args.length; i++) {
                if (i > 0) sb.append(", ");
                sb.append(args[i].getDataType().getDisplayName()).append(' ').append(args[i].getName());
            }
            return sb.append(')').toString();
        }
        return dt.getDisplayName() + " " + name;
    }
}
