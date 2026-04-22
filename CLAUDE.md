# CLAUDE.md

See `README.md` for project overview, architecture, and configuration reference.

## Build & Flash

```bash
pio run -e m5stack-atom-lite                                                    # build
pio run -e m5stack-atom-lite --target upload && pio device monitor              # flash + monitor
```

## Running Tests

Tests run on the native platform — no hardware required.

```bash
pio test -e native       # run all tests
pio test -e native -v    # verbose
```

Tests live in `test/test_scarfnet/`. `test_main.cpp` is the entry point.

### Native test constraints

`Mesh.h` pulls in `painlessMesh.h → Arduino.h` and cannot compile natively. Logic that needs unit testing must be extracted into a standalone header with no hardware deps (see `include/mesh_time.h`, `include/sync.h`, `include/swarm_ema.h` as examples). Test files include these directly. `src/` files are not compiled in native test mode.

## Pattern Simulator

```bash
cd tools/sim && make run    # build and run (no dependencies)
```

Controls: `SPACE` next pattern, `R` new seed, `T` tap tempo, `+`/`-` scarves, `Q` quit.

The sim compiles the real pattern engine natively. Source files included: `src/patterns/*.cpp`, `src/PatternManager.cpp`, `src/palettes.cpp`, `src/patterns.cpp`, `src/log.cpp`.

## Code Rules

- **Timing constants**: All in `include/config.h`. Never hard-code timing values elsewhere.
- **Hardware-dependent code**: Wrap in `#if SCARFNET_EMBEDDED` (set via `-DSCARFNET_EMBEDDED=1` in `platformio.ini` for the embedded target; absent in native/sim builds).
- **Python scripts**: Use `uv` — never bare `python3`. Add PEP 723 inline metadata and a `#!/usr/bin/env -S uv run --script` shebang. Scripts go in `tools/`.
