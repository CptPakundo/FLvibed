# Ghidra for FLvibed (headless, with an MCP bridge)

Decompilation, cross-references and searches on demand over the game's own code, instead of grepping the static
listings in `research\disasm\`. Set up 29 Oct 2026 (the owner approved the downloads that day). Everything Ghidra is
local and git-ignored in `research\ghidra\`; the scripts that drive it are here.

| What | Where (`research\ghidra\`) |
|---|---|
| Ghidra 12.1.4 (NSA release zip, SHA-256 checked against the release notes) | `ghidra_12.1.4\ghidra_12.1.4_PUBLIC\` (zip in `dl_ghidra_12.1.4\`) |
| JDK 21 (Temurin 21.0.12.1+1 zip, SHA-256 checked against Adoptium's API) | `jdk21\` (zip in `dl_jdk21\`) |
| GhidraMCP 7.0.0 (bethington/ghidra-mcp: the extension jar, built for exactly 12.1.4, holds the headless server) + its Python bridge wheel | `dl_ghidramcp_7.0.0\` (jar unpacked in `x\`), bridge in `bridge_venv\` (uv, Python 3.12) |
| Read-only copies of the test install's binaries (HDE 0.7.1): `Freelancer.exe` + 17 game DLLs | `bin_hde071\` (never an install itself) |
| The Ghidra project: all 18 analysed; Freelancer.exe seeded with `core\flexe.h` | `project\FLvibed.gpr` |
| Server log, import / seed logs, the bearer token (`.token`) | `logs\`, `.token` |

## Scripts (here)
| File | What |
|---|---|
| `ghidra.ps1` | `-Import` (copy the binaries, import + analyse what is missing, seed, vtable pass), `-Seed` (re-seed after new flexe.h symbols), `-Vtables`, `-Serve`, `-Stop`, `-Status`, `-Check` (the seed is today's flexe.h: drift_check's `ghidra` line). One owner of the project at a time: the import / seed / vtable runs stop the server first. |
| `flexe_tsv.js` | `core\flexe.h` -> `research\ghidra\flexe.tsv` (the seed's input; fails when it parses fewer entries than there are `X(` lines) |
| `FLvibedSeed.java` | resolves every flexe.h symbol the way `core\hook.cpp` does (the v1.1 address with its signature verified, else a unique scan), labels it, names the function, writes the flexe.h comment as a plate comment. 29 Oct 2026: 211 / 211 at the v1.1 address in HDE 0.7.1's exe. |
| `re_notes.js` + `FLvibedNotes.java` | `docs\FREELANCER_RE.md` into the listing: every Freelancer.exe address it mentions (634 on 29 Oct 2026) gets the doc's lines as a comment - at a function's entry in the plate comment, so a decompile shows the verified facts; the tables' function names are applied where Ghidra had none. Part of `-Seed` (re-run after doc edits; `-Check` notices a changed doc). |
| `FLvibedVtables.java` | creates the functions only a vtable points at (analysis misses them: Common.dll's LaneAction update was one). 29 Oct: ~6,200 over the 18 programs (Freelancer.exe +1,200, Common.dll +464, Alchemy.dll +2,034). A vtable here is also any code-referenced run of two or more code pointers in data, so a few of these may be jump / callback tables rather than classes - harmless for reading, but a function that decompiles to nonsense may be one. |
| `mcp_launch.js` | the MCP server Claude Code starts (`.mcp.json` from the private MCP settings): starts the headless server when it does not answer (loopback 127.0.0.1:8089, the token, file access limited to `research\ghidra\`), then runs the bridge on stdio with program selectors required |
| `gq.js` | the same server from a shell: `node tools\ghidra\gq.js <program> dec|asm|callers|xto|xfrom|find|str|ins|mem|mkfn|name|note ...` (header) - for the `game-facts` agent (shell only) and sessions started before the MCP registration |

## Use
- **MCP** (sessions started after 29 Oct 2026 ~12:30): the `ghidra` server's tools (`get_functions` with
  `fields=decompiled_code`, `get_xrefs_to`, `find_functions`, `search_strings`, ...; 75 listed at first (67 of the default groups + 8 bridge tools), the rest by
  `search_tools` / `load_tool_group`). Always pass `program=` (Freelancer.exe, Common.dll, ...): the bridge refuses
  calls without it.
- **Shell**: `node tools\ghidra\gq.js common dec rva:0x619D0` (an RVA is added to the program's base), `gq.js exe dec
  0x5B2890`, `gq.js common find tradelane`, `gq.js exe xto 0x67DCF0`.
- Code with no function yet: `gq.js <program> mkfn <addr> [name]` (writes the research project only).
- Names learned: `gq.js <program> name <fn> <new>` / `note <addr> <text>` - the project keeps them; the facts go
  into `docs\FREELANCER_RE.md` with standing "from decompiled code, not verified in game" until a probe confirms them.

## Standing of what it says
Decompiled code is a reading of the binary, not of the game at run time: it ranks below "verified in game" and game
data, above inference (a private session agent). Ghidra's types and names for unnamed code are guesses;
vtable calls (`(**(code **)(*p + 0x150))()`) need the class to read them - the exports (`CShip::...`) and flexe.h's
names are the anchors.

## Rebuild from nothing
`tools\ghidra\ghidra.ps1 -Import` (needs the three downloads unpacked as above; ~6 min for all 18 on this machine:
Freelancer.exe 64 s of analysis). Ghidra and the extension must match exactly (the extension's `extension.properties`
says `version=12.1.4`); a new GhidraMCP release may need a new Ghidra.
