// Puts docs\FREELANCER_RE.md into Freelancer.exe's listing (a TSV from re_notes.js): at every address the doc mentions,
// "FREELANCER_RE.md:<line>: <the doc's line>" - in the plate comment at a function's entry (a decompile shows it), else
// as a pre comment (the listing shows it); rewritten from the TSV each run, so an edited doc line replaces the old text; a function the doc's tables name gets that name while it still has Ghidra's default
// one (a flexe.h name from FLvibedSeed.java wins). Prints the counts.
// Headless: analyzeHeadless <proj> <name> -process Freelancer.exe -noanalysis -postScript FLvibedNotes.java <tsv>
// @category FLvibed
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public class FLvibedNotes extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) throw new IllegalArgumentException("usage: FLvibedNotes.java <re_notes.tsv>");
        if (!currentProgram.getName().equalsIgnoreCase("Freelancer.exe")) {
            println("FLvibedNotes: skipped " + currentProgram.getName() + " (Freelancer.exe only)");
            return;
        }
        List<String> lines = Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8);
        Map<Long, StringBuilder> notes = new LinkedHashMap<>();
        Map<Long, String> names = new LinkedHashMap<>();
        for (String line : lines) {
            if (line.isBlank()) continue;
            String[] c = line.split("\t", -1);
            long va = Long.decode(c[0]);
            notes.computeIfAbsent(va, k -> new StringBuilder())
                 .append(notes.get(va).length() > 0 ? "\n" : "").append("FREELANCER_RE.md:").append(c[1]).append(": ").append(c.length > 3 ? c[3] : "");
            if (c.length > 2 && !c[2].isEmpty()) names.putIfAbsent(va, c[2]);
        }
        int commented = 0, named = 0, outside = 0;
        for (Map.Entry<Long, StringBuilder> e : notes.entrySet()) {
            Address a;
            try {
                a = toAddr(e.getKey());
            } catch (Exception ex) {
                outside++;
                continue;
            }
            if (currentProgram.getMemory().getBlock(a) == null) {
                outside++;
                continue;
            }
            String name = names.get(e.getKey());
            Function f = getFunctionAt(a);
            if (f != null) {
                // a function's entry: the plate comment (what a decompile shows), after flexe.h's line from FLvibedSeed;
                // the doc lines of an earlier run are replaced
                String plate = getPlateComment(a);
                StringBuilder keep = new StringBuilder();
                if (plate != null)
                    for (String l : plate.split("\n"))
                        if (!l.startsWith("FREELANCER_RE.md:")) keep.append(keep.length() > 0 ? "\n" : "").append(l);
                setPlateComment(a, (keep.length() > 0 ? keep + "\n" : "") + e.getValue());
            } else
                setPreComment(a, e.getValue().toString());
            commented++;
            if (name != null && f != null && f.getSymbol().getSource() == SourceType.DEFAULT) {
                f.setName(name, SourceType.USER_DEFINED);
                named++;
            }
        }
        println("FLvibedNotes: " + commented + " addresses commented, " + named + " functions named, " + outside + " outside the image");
    }
}
