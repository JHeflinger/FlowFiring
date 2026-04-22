"""Benchmark: C++ sequential vs C++ colored (OpenMP) vs CUDA colored.

Produces two plots:
  bench_scaling.png — total time and throughput vs lattice size
  bench_steps.png   — per-step firings and time vs step number
"""

import argparse
import statistics
import time

import numpy as np

from flowfiring import build_lattice_3d, has_cuda
from flowfiring.configs import make_triangle_circulation
from flowfiring.firing import fire_sequential, fire, fire_sequential_step, fire_step


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def make_config(d):
    """Initial flow config at lattice center, scaled to saturate."""
    edge_verts = d["edge_verts"]
    pts = edge_verts.reshape(-1, 3)
    center = (pts.min(axis=0) + pts.max(axis=0)) / 2
    flow = d["num_edges"] * 2
    return make_triangle_circulation(d, center, flow)


def build_and_print(size):
    """Build lattice, print stats, return (d, build_time)."""
    t0 = time.perf_counter()
    d = build_lattice_3d(size, with_colors=True)
    t_build = time.perf_counter() - t0
    num_colors = len(d["color_offsets"]) - 1
    print(f"  Edges: {d['num_edges']}, Faces: {d['num_faces']}, "
          f"Colors: {num_colors}, Build: {t_build:.3f}s")
    return d, t_build


# ---------------------------------------------------------------------------
# Scaling benchmark
# ---------------------------------------------------------------------------

def timed_run(config_template, d, max_steps, backend):
    """Single timed run. Returns (elapsed, fired)."""
    config = config_template.copy()
    if backend == "sequential":
        t0 = time.perf_counter()
        fired = fire_sequential(config, d, max_steps=max_steps)
        elapsed = time.perf_counter() - t0
    else:
        t0 = time.perf_counter()
        fired = fire(config, d, max_steps=max_steps, backend=backend)
        elapsed = time.perf_counter() - t0
    return elapsed, fired


def run_scaling_benchmark(sizes, trials, max_steps, backends):
    """Run scaling benchmark. Returns {backend: {size: (med_time, fired, throughput)}}."""
    results = {b: {} for b in backends}

    for size in sizes:
        print(f"\n{'='*60}")
        print(f"Size {size}")
        d, _ = build_and_print(size)
        config_template = make_config(d)

        for backend in backends:
            # Warmup
            timed_run(config_template, d, max_steps, backend)

            times, firings = [], []
            for _ in range(trials):
                t, f = timed_run(config_template, d, max_steps, backend)
                times.append(t)
                firings.append(f)

            med_t = statistics.median(times)
            std_t = statistics.stdev(times) if len(times) > 1 else 0.0
            fired = firings[0]
            throughput = fired / med_t if med_t > 0 else 0
            results[backend][size] = (med_t, fired, throughput)

            # Check consistency
            if len(set(firings)) > 1:
                print(f"  WARNING: {backend} fired counts vary: {firings}")

            print(f"  {backend:20s} {med_t:8.3f}s ±{std_t:.3f}  "
                  f"fired={fired:>12,}  {throughput:>12,.0f} fire/s")

    return results


# ---------------------------------------------------------------------------
# Per-step benchmark
# ---------------------------------------------------------------------------

def run_step_benchmark(size, max_steps, backends):
    """Per-step timing at a single lattice size.

    Returns {backend: list of (step, fired_this_step, elapsed)}.
    """
    print(f"\n{'='*60}")
    print(f"Per-step benchmark (size {size})")
    d, _ = build_and_print(size)

    results = {}

    for backend in backends:
        config = make_config(d)
        steps = []

        if backend == "cuda":
            from flowfiring._native._native import CudaFiringSession
            sess = CudaFiringSession(
                config, d["degrees"], d["indptr"], d["indices"], d["data"],
                d["color_offsets"], d["color_edges"])
            for i in range(max_steps):
                t0 = time.perf_counter()
                fired = sess.step()
                elapsed = time.perf_counter() - t0
                steps.append((i, fired, elapsed))
                if fired == 0:
                    break
            sess.read_config(config)
        elif backend == "cpu":
            for i in range(max_steps):
                t0 = time.perf_counter()
                fired = fire_step(config, d, use_cuda=False)
                elapsed = time.perf_counter() - t0
                steps.append((i, fired, elapsed))
                if fired == 0:
                    break
        else:  # sequential
            for i in range(max_steps):
                t0 = time.perf_counter()
                fired = fire_sequential_step(config, d)
                elapsed = time.perf_counter() - t0
                steps.append((i, fired, elapsed))
                if fired == 0:
                    break

        total_fired = sum(s[1] for s in steps)
        total_time = sum(s[2] for s in steps)
        print(f"  {backend:20s} {len(steps):>6} steps, "
              f"fired={total_fired:>12,}, {total_time:.3f}s")
        results[backend] = steps

    return results


# ---------------------------------------------------------------------------
# Plotting
# ---------------------------------------------------------------------------

def plot_scaling(results, output_dir):
    import matplotlib.pyplot as plt

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

    for backend, data in results.items():
        sizes = sorted(data.keys())
        times = [data[s][0] for s in sizes]
        throughputs = [data[s][2] for s in sizes]

        ax1.plot(sizes, times, "o-", label=backend)
        ax2.plot(sizes, throughputs, "o-", label=backend)

    ax1.set_xlabel("Lattice size")
    ax1.set_ylabel("Time (s)")
    ax1.set_title("Total firing time vs lattice size")
    ax1.set_yscale("log")
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    ax2.set_xlabel("Lattice size")
    ax2.set_ylabel("Firings / sec")
    ax2.set_title("Throughput vs lattice size")
    ax2.set_yscale("log")
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    fig.tight_layout()
    path = f"{output_dir}/bench_scaling.png"
    fig.savefig(path, dpi=150)
    print(f"\nSaved {path}")
    plt.close(fig)


def plot_steps(results, output_dir):
    import matplotlib.pyplot as plt

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

    for backend, steps in results.items():
        step_nums = [s[0] for s in steps]
        fired = [s[1] for s in steps]
        elapsed = [s[2] for s in steps]

        ax1.plot(step_nums, fired, label=backend, alpha=0.8)
        ax2.plot(step_nums, elapsed, label=backend, alpha=0.8)

    ax1.set_xlabel("Step")
    ax1.set_ylabel("Edges fired")
    ax1.set_title("Firings per step")
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    ax2.set_xlabel("Step")
    ax2.set_ylabel("Time (s)")
    ax2.set_title("Time per step")
    ax2.set_yscale("log")
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    fig.tight_layout()
    path = f"{output_dir}/bench_steps.png"
    fig.savefig(path, dpi=150)
    print(f"Saved {path}")
    plt.close(fig)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Benchmark flow firing")
    parser.add_argument("--sizes", nargs="+", type=int,
                        default=[10, 20, 50, 100, 200])
    parser.add_argument("--trials", type=int, default=3)
    parser.add_argument("--max-steps", type=int, default=10000)
    parser.add_argument("--step-size", type=int, default=50,
                        help="Lattice size for per-step benchmark")
    parser.add_argument("--output-dir", type=str, default=".")
    parser.add_argument("--no-cuda", action="store_true")
    parser.add_argument("--no-plot", action="store_true")
    args = parser.parse_args()

    backends = ["sequential", "cpu"]
    if has_cuda() and not args.no_cuda:
        backends.append("cuda")
    print(f"Backends: {backends}")

    # Scaling benchmark
    scaling = run_scaling_benchmark(
        args.sizes, args.trials, args.max_steps, backends)

    # Per-step benchmark
    step_results = run_step_benchmark(
        args.step_size, args.max_steps, backends)

    # Plots
    if not args.no_plot:
        plot_scaling(scaling, args.output_dir)
        plot_steps(step_results, args.output_dir)


if __name__ == "__main__":
    main()
