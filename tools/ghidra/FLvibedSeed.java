// Seeds a Freelancer.exe program with FLvibed's verified symbols (core\flexe.h, as a TSV from flexe_tsv.js): each
// symbol is resolved the way core\hook.cpp Resolve does it (the v1.1 address with its signature bytes verified, else
// a unique signature scan), then labelled and commented; a Code symbol at its signature's start names the function
// there (created if analysis did not find it). Prints one line per symbol (v11 / scan / missing / ambiguous).
// Headless: analyzeHeadless <proj> <name> -process Freelancer.exe -postScript FLvibedSeed.java <tsv> (tools\ghidra\ghidra.ps1).
// @category FLvibed
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class FLvibedSeed extends GhidraScript {
    private byte[] bytes, mask;

    private boolean parse(String sig) {
        String[] t = sig.trim().split("\\s+");
        bytes = new byte[t.length];
        mask = new byte[t.length];
        for (int i = 0; i < t.length; i++) {
            if (t[i].equals("??")) continue;
            if (t[i].length() != 2) return false;
            bytes[i] = (byte) Integer.parseInt(t[i], 16);
            mask[i] = (byte) 0xFF;
        }
        return t.length > 0;
    }

    private boolean matchAt(Address a) {
        byte[] got = new byte[bytes.length];
        try {
            if (currentProgram.getMemory().getBytes(a, got) != got.length) return false;
        } catch (Exception e) {
            return false;
        }
        for (int i = 0; i < got.length; i++)
            if ((got[i] & mask[i]) != (bytes[i] & mask[i])) return false;
        return true;
    }

    private long readU32(Address a) throws Exception {
        return currentProgram.getMemory().getInt(a) & 0xFFFFFFFFL;
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) throw new IllegalArgumentException("usage: FLvibedSeed.java <flexe.tsv>");
        if (!currentProgram.getName().equalsIgnoreCase("Freelancer.exe")) {
            println("FLvibedSeed: skipped " + currentProgram.getName() + " (Freelancer.exe only)");
            return;
        }
        List<String> lines = Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8);
        Memory mem = currentProgram.getMemory();
        int v11 = 0, scan = 0, missing = 0;
        for (String line : lines) {
            if (line.isBlank()) continue;
            String[] c = line.split("\t", -1);
            String name = c[0], kind = c[1], comment = c.length > 6 ? c[6] : "";
            long va = Long.decode(c[2]), site = Long.decode(c[3]);
            int off = Integer.parseInt(c[4]);
            boolean code = kind.equals("Code");
            if (!parse(c[5]) || off < 0 || off >= bytes.length) {
                println("FLvibedSeed\tmissing\t" + name + "\tbad signature");
                missing++;
                continue;
            }
            String how = null;
            long addr = 0;
            Address at = toAddr((code ? va : site) - off);
            if (matchAt(at) && (code || readU32(toAddr(site)) == va)) {
                addr = va;
                how = "v11";
            } else {
                Address first = mem.findBytes(mem.getMinAddress(), bytes, mask, true, monitor);
                Address second = first == null ? null : mem.findBytes(first.add(1), bytes, mask, true, monitor);
                if (first == null || second != null) {
                    println("FLvibedSeed\t" + (first == null ? "missing" : "ambiguous") + "\t" + name);
                    missing++;
                    continue;
                }
                addr = code ? first.getOffset() + off : readU32(first.add(off));
                how = "scan";
            }
            Address a = toAddr(addr);
            if (code && off == 0) {
                Function f = getFunctionAt(a);
                if (f == null && getFunctionContaining(a) == null) f = createFunction(a, name);
                if (f != null && f.getEntryPoint().equals(a)) f.setName(name, SourceType.USER_DEFINED);
                else createLabel(a, name, true, SourceType.USER_DEFINED);
            } else {
                Symbol s = createLabel(a, name, true, SourceType.USER_DEFINED);
                if (s == null) println("FLvibedSeed\tlabel failed\t" + name);
            }
            String text = "FLvibed exe::" + name + " (" + kind + ", core\\flexe.h): " + comment;
            setPlateComment(a, text);
            if (how.equals("v11")) v11++;
            else scan++;
            println("FLvibedSeed\t" + how + "\t" + name + "\t0x" + Long.toHexString(addr));
        }
        println("FLvibedSeed: " + v11 + " at the v1.1 address, " + scan + " by scan, " + missing + " not found");
    }
}
