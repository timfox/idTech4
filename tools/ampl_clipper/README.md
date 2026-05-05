# AMPL / Clipper (id Tech 5–style offline toolchain)

[id Tech 5](https://en.wikipedia.org/wiki/Id_Tech_5) was described as using **C++** for the runtime plus auxiliary languages for tools and data pipelines—including **AMPL** (algebraic modeling for optimization) and **Clipper** (Angus Johnson’s polygon clipping), alongside Python for some AI tooling.

This repository is **id Tech 4–era C++**; we do **not** embed a commercial AMPL runtime in the game. Instead we mirror the **same separation of concerns**:

| id Tech 5 idea | This fork |
|-----------------|-----------|
| AMPL for offline numerical models | Example **MathProg** `.mod` files solved with **`glpsol`** (GNU MathProg, AMPL-compatible syntax) or your own AMPL installation |
| Clipper for 2D boolean ops | **Clipper2** vendored as submodule `third_party/clipper2`, used from the small **`ampl_clipper`** C++ tool |

## Build

**Linux / macOS**

```bash
make -C tools/ampl_clipper clean all
./tools/ampl_clipper/ampl_clipper
```

**Windows (Developer Command Prompt)**

```cmd
cd tools\ampl_clipper
cl /nologo /EHsc /O2 /W3 /I "..\..\third_party\clipper2\CPP\Clipper2Lib\include" /Fe:ampl_clipper.exe main.cpp ^
  "..\..\third_party\clipper2\CPP\Clipper2Lib\src\clipper.engine.cpp" ^
  "..\..\third_party\clipper2\CPP\Clipper2Lib\src\clipper.offset.cpp" ^
  "..\..\third_party\clipper2\CPP\Clipper2Lib\src\clipper.rectclip.cpp" ^
  "..\..\third_party\clipper2\CPP\Clipper2Lib\src\clipper.triangulation.cpp"
ampl_clipper.exe
```

## Optional: run the LP demo with GLPK

On Debian/Ubuntu:

```bash
sudo apt-get install -y glpk-utils
glpsol -m tools/ampl_clipper/examples/page_budget.mod
```

The `ampl_clipper` binary will invoke `glpsol` automatically when it is on `PATH` and report the optimum; otherwise it prints the path to the `.mod` and continues with the Clipper demo only.

## Clipper2 submodule

```bash
git submodule update --init --recursive
```

Headers live under `third_party/clipper2/CPP/Clipper2Lib/include` (Boost-licensed).
