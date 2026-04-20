# FlowFiring — CUDA Prototype

GPU-accelerated higher-dimensional chip-firing on simplicial complexes.
Edges carry integer flow and fire when `|flow| >= degree` in the coboundary
Laplacian, redistributing flow to neighbors. Graph coloring enables parallel
firing of independent edge sets.

## Install

Requires Python >= 3.13 and [uv](https://docs.astral.sh/uv/).

```bash
# CPU only (OpenMP parallel + sequential firing)
uv sync
uv pip install -e .

# With CUDA (requires nvcc in PATH)
export PATH="/usr/local/cuda/bin:$PATH"
CMAKE_ARGS="-DWITH_CUDA=ON -DCMAKE_BUILD_TYPE=Release" uv sync
CMAKE_ARGS="-DWITH_CUDA=ON -DCMAKE_BUILD_TYPE=Release" uv pip install -e . 
```

## Viewer

Blender visualization with per-frame animated firing.

```bash
blender viewer.blend --python viewer.py
blender viewer.blend --python viewer.py -- --init quad --size 10
blender viewer.blend --python viewer.py -- --init cubic --size 5 --gpu
blender viewer.blend --python viewer.py -- --shuffle --seed 42
```

Options: `--init {triangle,quad,cubic}`, `--size N`, `--initial N`,
`--gpu`, `--shuffle`, `--seed N`, `--prefire N` (`-1` = until stable),
`--hollow-face` (cubic only).

## Firing Backends

All firing is in C++/CUDA — no Python firing code.

| Backend | Function | Parallelism |
|---------|----------|-------------|
| C++ sequential | `fire_sequential()` | Single-threaded, custom edge order |
| C++ colored | `fire(backend="cpu")` | OpenMP, parallel per color class |
| CUDA colored | `fire(backend="cuda")` | GPU, persistent `CudaFiringSession` |

Sequential fires edges one at a time with immediate updates. Colored fires
all eligible edges per color class simultaneously (two-phase: check then
scatter). CPU and CUDA colored are bit-identical for the same coloring.
