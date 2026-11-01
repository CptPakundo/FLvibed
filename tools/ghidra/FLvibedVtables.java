// Creates the functions that auto-analysis misses because only a vtable points at them (the game's C++ classes: e.g.
// Common.dll's LaneAction update RVA 0x619D0, 29 Oct 2026). A vtable here = a vftable symbol (MSVC "??_7..." /
// Ghidra's "vftable" labels) or any address in non-executable data that an instruction references (a constructor's
// "mov [obj], table") holding at least two consecutive pointers into executable memory. Each such slot gets a
// function if none contains it; the walk stops at the first slot that is no code pointer or at the next referenced /
// named address (the next table). Then analysis runs on the new functions. Any program; prints what it made.
// Headless: analyzeHeadless <proj> <name> -process <file> -noanalysis -postScript FLvibedVtables.java (ghidra.ps1 -Vtables).
// @category FLvibed
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import java.util.LinkedHashSet;
import java.util.Set;

public class FLvibedVtables extends GhidraScript {
    private boolean exec(Address a) {
        MemoryBlock b = currentProgram.getMemory().getBlock(a);
        return b != null && b.isExecute() && b.isInitialized();
    }

    private Address target(Address slot) {
        try {
            Address t = toAddr(currentProgram.getMemory().getInt(slot) & 0xFFFFFFFFL);
            return exec(t) ? t : null;
        } catch (Exception e) {
            return null;
        }
    }

    private boolean fromCode(Address a) {
        for (Reference r : getReferencesTo(a))
            if (getInstructionAt(r.getFromAddress()) != null) return true;
        return false;
    }

    private boolean tableStart(Address a) { // a later slot that begins another table
        Symbol s = currentProgram.getSymbolTable().getPrimarySymbol(a);
        return (s != null && s.getSource() != SourceType.DEFAULT) || fromCode(a);
    }

    @Override
    public void run() throws Exception {
        Set<Address> tables = new LinkedHashSet<>();
        SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
        while (it.hasNext()) {
            Symbol s = it.next();
            String n = s.getName();
            if (n.startsWith("??_7") || n.startsWith("vftable") || n.endsWith("_vftable_")) tables.add(s.getAddress());
        }
        ReferenceManager rm = currentProgram.getReferenceManager();
        AddressIterator dests = rm.getReferenceDestinationIterator(currentProgram.getMemory().getLoadedAndInitializedAddressSet(), true);
        while (dests.hasNext() && !monitor.isCancelled()) {
            Address a = dests.next();
            MemoryBlock b = currentProgram.getMemory().getBlock(a);
            if (b == null || b.isExecute() || a.getOffset() % 4 != 0) continue;
            if (target(a) != null && target(a.add(4)) != null && fromCode(a)) tables.add(a);
        }
        int made = 0;
        for (Address start : tables) {
            if (monitor.isCancelled()) break;
            Address slot = start;
            for (int i = 0; i < 512; i++, slot = slot.add(4)) {
                if (i > 0 && tableStart(slot)) break;
                Address t = target(slot);
                if (t == null) break;
                if (getFunctionContaining(t) != null) continue;
                Instruction ins = getInstructionAt(t);
                if (ins == null) {
                    if (getDataAt(t) != null) continue; // a pointer into data inside the code block: not code
                    new DisassembleCommand(t, null, true).applyTo(currentProgram, monitor);
                    if (getInstructionAt(t) == null) continue;
                }
                if (createFunction(t, null) != null) made++;
            }
        }
        println("FLvibedVtables: " + currentProgram.getName() + ": " + tables.size() + " vtables, " + made + " functions created");
        if (made > 0) analyzeChanges(currentProgram);
    }
}
