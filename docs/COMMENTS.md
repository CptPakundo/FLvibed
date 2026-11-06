# How FLvibed's code comments

The standard for the source, from `docs\MODULES_PLAN.md` section 5. Each file is read once, when it is moved: the module headers, public
APIs and the 20 largest files are rewritten by hand; the rest improves when touched. A whole-tree rewrite is not worth its tokens or
its diff noise. `tools\comment_lint.py` reports what breaks this; from a module's cut on, that module is what it counts first.

1. **Every file opens with a header:** what the file is and what it owns, in a line or three, before the first `#include`.
2. **Say what and why, not who and when.** A comment explains what the code cannot: the reason, the game fact behind it, the trap. It
   does not narrate how it came to be.
3. **No dated narrative, version history or owner quotes in code.** Those belong to the docs and git. A date in a comment is
   allowed where it is evidence for a measured fact a reader must weigh, and then the fact comes first.
4. **Point, do not repeat.** A game fact lives in `docs\FREELANCER_RE.md` or `docs\kb`; the comment names the section.
5. **Public interfaces are the documentation:** a header says what each entry does, what it needs, and how it fails.
6. **Plain words.** Name things as the code does; no abbreviation a third party would have to guess.
