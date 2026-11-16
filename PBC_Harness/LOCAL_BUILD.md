# Local build settings

`build/release` is configured with these non-default options. They live in
`build/release/CMakeCache.txt`, so rerun this after deleting that directory:

```bash
CC=clang-23 CXX=clang++-23 cmake --preset release \
  -DENABLE_IPO=OFF -DCMAKE_LINKER_TYPE=MOLD -DLLVM_PARALLEL_LINK_JOBS=2
```

| Option                          | Default   | Why                                          |
| ------------------------------- | --------- | -------------------------------------------- |
| `CC=clang-23`, `CXX=clang++-23` | GCC 16    | About half the memory per compile            |
| `ENABLE_IPO=OFF`                | `ON`      | Avoids slow LTO code generation at each link |
| `CMAKE_LINKER_TYPE=MOLD`        | GNU `ld`  | Faster linking                               |
| `LLVM_PARALLEL_LINK_JOBS=2`     | unlimited | Caps memory used by parallel links           |

Build with `CMAKE_BUILD_PARALLEL_LEVEL=5` (4 if other heavy programs are open).

## Running the harness

To build the harness with `-DMQT_BUILD_PBC_HARNESS=ON`, `pbc: configure` task is
run.

`pbc-simple-setup` translates a qasm program to the QC dialect. `--emit` selects
what it writes:

| Argument                  | Output                                                    |
| ------------------------- | --------------------------------------------------------- |
| `--emit=stdout` (default) | IR on stdout                                              |
| `--emit=file`             | IR in `outputs/qc-output.qc.mlir`                         |
| `--emit=dot`              | `view-op-graph` Graphviz graph in `outputs/qc-output.dot` |

Files always go to `PBC_Harness/outputs`. It is excluded from Git locally
through `.git/info/exclude`.

Both launch configs in `.vscode/launch.json` build the harness first, then run
it under `gdb`.

`pbc: render op graph` task converts `qc-output.dot` to `outputs/qc-output.png`
with `dot` and opens the image in VS Code.
