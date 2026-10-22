// Applies names and descriptions from re/symbols.csv to the program in Ghidra.
// File format (separator ;, lines starting with # are comments):
//   address;kind;name;description
// kind: f = function, d = data (global variable / array),
//       c = comment on an instruction inside a function (shown above that statement in the
//           decompiled C): "<name>: <description>", e.g. the cases of a switch
// @category Locoland

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class ApplySymbols extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String file = getScriptArgs()[0];
        List<String> lines = Files.readAllLines(Paths.get(file), StandardCharsets.UTF_8);
        int lineNo = 0, applied = 0;
        for (String line : lines) {
            lineNo++;
            line = line.strip();
            if (line.isEmpty() || line.startsWith("#")) continue;
            String[] p = line.split(";", 4);
            if (p.length < 3) {
                printerr(file + ":" + lineNo + ": too few fields");
                continue;
            }
            Address a = toAddr(Long.decode(p[0].strip()));
            String kind = p[1].strip();
            String name = p[2].strip();
            String description = p.length > 3 ? p[3].strip() : "";

            if (kind.equals("f")) {
                Function f = getFunctionAt(a);
                if (f == null) {
                    disassemble(a);
                    f = createFunction(a, name);
                }
                if (f == null) {
                    printerr(file + ":" + lineNo + ": cannot create a function at " + a);
                    continue;
                }
                f.setName(name, SourceType.USER_DEFINED);
                if (!description.isEmpty()) f.setComment(description);
            } else if (kind.equals("c")) {
                setPreComment(a, description.isEmpty() ? name : name + ": " + description);
            } else {
                createLabel(a, name, true, SourceType.USER_DEFINED);
                if (!description.isEmpty()) setPlateComment(a, description);
            }
            applied++;
        }
        println("Symbols: " + applied);
    }
}
