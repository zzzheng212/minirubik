Starting with `mywork/solver_led.s`. This is the final combined assembly program intended to be loaded directly into Ripes. It contains the solver, required tables, and LED rendering code. No C compilation or manual concatenation of the other assembly files is required for this demonstration.

The other files document individual components, development work, and validation. They are not additional programs that must all be loaded into Ripes together. Loading duplicate components alongside the combined file can introduce duplicate labels.


Expected output

For the default input, the important checks are:

```text
Solution: <an 11-move solution>
Length: 11
PASS: replay solved
Program exited with code: 0
```

The program replays its returned moves and checks that the resulting cube is solved. A different move sequence is acceptable if it has the correct shortest length and passes replay.

The LED display presents the cube state during replay. To inspect an intermediate frame, place a breakpoint on the instruction immediately after the replay loop's `jal ra, render_cube`. Run to the breakpoint, view the LED Matrix, and continue to inspect subsequent frames. Fast execution may not visibly pause on every frame.


Supporting files

| File in `mywork/` | Role |
|---|---|
| `solver_led.s` | Final combined program and primary Ripes entry point. |
| `solver.s` | Solver source retained separately for inspection and development. |
| `renderer.s` | LED drawing routines. |
| `table.s` | Precomputed table data retained separately. |
| `hueristic.s` | Heuristic component; filename spelling follows the submitted listing. |
| `ida.s` | IDA* search component. |
| `heuristic_test.c` | Native C implementation and validation/development support. |
| `memory_small.s`, `larger_memory.s` | Stage 1 memory measurement programs, not inputs to the final solver. |
| `host_audit.c` | Host-side BFS and table-audit support; not a Ripes assembly input. |
| `validate.py` | Automated Ripes test driver; requires its supporting source and test-data files. |

The combined program is the authoritative entry point for the LED demonstration. Individual assembly components need not be independently executable.
