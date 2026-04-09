"""
Interactive matplotlib GUI to debug flow-firing.

Each frame shows a single edge firing. The firing edge is highlighted green,
edges are colored by flow magnitude, and flow values are printed.

Usage (interactive - default):
    python debug_firing.py --init hollow-face --size 5 --flow 100
    python debug_firing.py --init hollow-octa --size 5 --flow 100

Usage (save MP4):
    python debug_firing.py --init hollow-face --size 5 --flow 100 --save
"""

import argparse
import sys
import numpy as np

from flowfiring import build_lattice_3d
from flowfiring.configs import vkey
from experiment_octa import (
    find_octahedra, pick_central, find_eulerian_circuit,
    circuit_to_config, remove_faces,
)


# ---------------------------------------------------------------------------
# Single-edge firing (Python implementation of the C++ rule)
# ---------------------------------------------------------------------------

def find_next_firable(config, d, order, start_idx):
    """Find the next firable edge starting from start_idx in order.

    Returns (order_position, edge_index) or (None, None) if none found.
    """
    deg = d["degrees"]
    for k in range(start_idx, len(order)):
        i = order[k]
        if deg[i] > 0 and abs(config[i]) >= deg[i]:
            return k, i
    return None, None


def fire_single_edge(config, d, edge_idx):
    """Fire a single edge. Modifies config in-place."""
    indptr = d["indptr"]
    indices = d["indices"]
    data = d["data"]

    sign = 1 if config[edge_idx] > 0 else -1
    col_start = indptr[edge_idx]
    col_end = indptr[edge_idx + 1]
    for p in range(col_start, col_end):
        config[indices[p]] -= sign * data[p]


# ---------------------------------------------------------------------------
# Color mapping
# ---------------------------------------------------------------------------

def flow_to_color(val, max_flow):
    """Map flow value to RGBA color."""
    if val == 0:
        return (0.2, 0.2, 0.2, 0.15)  # dim gray
    mag = min(abs(val) / max(max_flow, 1), 1.0)
    if val > 0:
        return (1.0, 0.2, 0.2, 0.3 + 0.7 * mag)  # red
    else:
        return (0.2, 0.2, 1.0, 0.3 + 0.7 * mag)  # blue


# ---------------------------------------------------------------------------
# Build the simulation state
# ---------------------------------------------------------------------------

def build_sim(args):
    """Build lattice and initial config based on args."""
    d = build_lattice_3d(args.size)
    octahedra = find_octahedra(d)
    octa = pick_central(octahedra, d)
    print(f"Central octahedron at {octa['center']}")

    if args.init == "hollow-octa":
        circuit = find_eulerian_circuit(octa, d)
        d, _ = remove_faces(d, octa["face_indices"])
        config = circuit_to_config(circuit, d["num_edges"], args.flow)
        print(f"Hollow octa: 8 faces removed, |flow|={np.sum(np.abs(config))}")
    else:  # hollow-face
        d, removed = remove_faces(d, [octa["face_indices"][0]])
        config = np.zeros(d["num_edges"], dtype=np.int32)
        config[removed[0]["edges"]] = removed[0]["signs"] * args.flow
        print(f"Hollow face: 1 face removed, |flow|={np.sum(np.abs(config))}")

    return d, config, octa


# ---------------------------------------------------------------------------
# Pre-compute all firing steps
# ---------------------------------------------------------------------------

# def compute_firing_sequence_(config, d, max_fires, shuffle, seed):
#     """Compute sequence of individual edge firings.

#     Returns list of (edge_fired, config_snapshot) tuples.
#     First entry is the initial state (edge_fired=-1).
#     """
#     rng = np.random.default_rng(seed) if shuffle else None
#     config = config.copy()
#     num_edges = d["num_edges"]

#     frames = [(-1, config.copy())]  # initial state

#     order = np.arange(num_edges, dtype=np.int32)
#     sweep = 0
#     total_fired = 0

#     while total_fired < max_fires:
#         if rng is not None:
#             rng.shuffle(order)

#         fired_this_sweep = 0
#         scan_idx = 0
#         while scan_idx < len(order) and total_fired < max_fires:
#             k, ei = find_next_firable(config, d, order, scan_idx)
#             if k is None:
#                 break
#             fire_single_edge(config, d, ei)
#             frames.append((ei, config.copy()))
#             total_fired += 1
#             fired_this_sweep += 1
#             # scan_idx = k + 1

#         sweep += 1
#         if fired_this_sweep == 0:
#             print(f"Stable after {sweep} sweeps, {total_fired} total firings")
#             break

#     if total_fired >= max_fires:
#         print(f"Stopped at {max_fires} firings ({sweep} sweeps)")

#     return frames

def compute_firing_sequence(config, d, max_fires, shuffle, seed):
    """ Shuffle the entire edge set every fire, find the next ready and fire """
    rng = np.random.default_rng(seed) if shuffle else None
    config = config.copy()
    num_edges = d["num_edges"]
    frames = [(-1, config.copy())]

    order = np.arange(num_edges, dtype=np.int32)
    total_fired = 0
    while total_fired < max_fires:
        if rng is not None:
            rng.shuffle(order)
        k, ei = find_next_firable(config, d, order, 0)
        if k is None: 
            break
        fire_single_edge(config, d, ei)
        frames.append((ei, config.copy()))
        total_fired += 1
    
    if total_fired >= max_fires:
        print(f"Stopped at {max_fires} firings")

    return frames



# ---------------------------------------------------------------------------
# Rendering helper
# ---------------------------------------------------------------------------

def compute_view_bounds(d, frames, octa):
    """Compute bounding box around active region."""
    edge_verts = d["edge_verts"]

    active_edges = set()
    for _, cfg in frames:
        active_edges.update(np.nonzero(cfg)[0].tolist())
    active_edges.update(octa["edge_indices"])

    if active_edges:
        active_pts = edge_verts[list(active_edges)].reshape(-1, 3)
        center = (active_pts.min(axis=0) + active_pts.max(axis=0)) / 2
        span = (active_pts.max(axis=0) - active_pts.min(axis=0)).max() / 2 + 1.0
    else:
        all_pts = edge_verts.reshape(-1, 3)
        center = (all_pts.min(axis=0) + all_pts.max(axis=0)) / 2
        span = 3.0

    return center, span


def draw_frame(ax, d, frames, frame_idx, octa, center, span, max_flow,
               elev=None, azim=None):
    """Draw a single frame on the given axes."""
    from mpl_toolkits.mplot3d.art3d import Line3DCollection

    # Preserve camera if not specified
    if elev is None:
        elev = ax.elev
    if azim is None:
        azim = ax.azim

    ax.clear()

    edge_verts = d["edge_verts"]
    num_edges = d["num_edges"]
    octa_edge_set = set(octa["edge_indices"])

    ei_fired, config = frames[frame_idx]

    segments = []
    colors = []
    linewidths = []

    for ei in range(num_edges):
        v0 = edge_verts[ei, 0].tolist()
        v1 = edge_verts[ei, 1].tolist()
        segments.append([v0, v1])

        if ei == ei_fired:
            colors.append((0.0, 1.0, 0.0, 1.0))  # green = just fired
            linewidths.append(4.0)
        elif ei in octa_edge_set:
            c = flow_to_color(int(config[ei]), max_flow)
            colors.append(c)
            linewidths.append(2.0)
        elif config[ei] != 0:
            colors.append(flow_to_color(int(config[ei]), max_flow))
            linewidths.append(1.5)
        else:
            colors.append((0.5, 0.5, 0.5, 0.08))
            linewidths.append(0.2)

    lc = Line3DCollection(segments, colors=colors, linewidths=linewidths)
    ax.add_collection3d(lc)

    # Arrows and labels on nonzero-flow edges
    nonzero = np.nonzero(config)[0]
    for ei in nonzero:
        v0 = edge_verts[ei, 0]
        v1 = edge_verts[ei, 1]
        val = int(config[ei])
        # Arrow points in flow direction (v0->v1 if positive, v1->v0 if negative)
        if val > 0:
            src, dst = v0, v1
        else:
            src, dst = v1, v0
        direction = dst - src
        dlen = np.linalg.norm(direction)
        if dlen > 0:
            direction = direction / dlen * 0.12
            tip = src * 0.3 + dst * 0.7
            ax.quiver(tip[0], tip[1], tip[2],
                        direction[0], direction[1], direction[2],
                        color='black', arrow_length_ratio=2.0,
                        linewidth=1.0, alpha=0.8)
        mid = (v0 + v1) / 2
        ax.text(mid[0], mid[1], mid[2], f'{val}',
                fontsize=10, color='black', ha='center', va='bottom')

    ax.set_xlim(center[0] - span, center[0] + span)
    ax.set_ylim(center[1] - span, center[1] + span)
    ax.set_zlim(center[2] - span, center[2] + span)
    ax.set_xlabel('X'); ax.set_ylabel('Y'); ax.set_zlabel('Z')
    ax.view_init(elev=elev, azim=azim)

    total_flow = np.sum(np.abs(config))
    nz = np.count_nonzero(config)
    if ei_fired >= 0:
        ax.set_title(
            f'Step {frame_idx}/{len(frames)-1}  |  '
            f'Fired edge {ei_fired} (deg={d["degrees"][ei_fired]})  |  '
            f'|flow|={total_flow}  |  {nz} nonzero edges',
            fontsize=10)
    else:
        ax.set_title(
            f'Initial state  |  |flow|={total_flow}  |  {nz} nonzero edges',
            fontsize=10)


# ---------------------------------------------------------------------------
# Interactive GUI
# ---------------------------------------------------------------------------

def run_interactive(d, frames, octa, args):
    """Interactive stepper: arrow keys / buttons to step, mouse to rotate."""
    import matplotlib.pyplot as plt
    from matplotlib.widgets import Button

    center, span = compute_view_bounds(d, frames, octa)
    max_flow = max(np.max(np.abs(f[1])) for f in frames)
    n_frames = len(frames)

    fig = plt.figure(figsize=(13, 9))
    ax = fig.add_subplot(111, projection='3d')
    fig.subplots_adjust(bottom=0.12)

    state = {"idx": 0, "playing": False, "timer": None, "zoom": 1.0}

    def show(idx):
        state["idx"] = max(0, min(idx, n_frames - 1))
        s = span * state["zoom"]
        draw_frame(ax, d, frames, state["idx"], octa, center, s, max_flow)
        fig.canvas.draw_idle()

    def on_next(_=None):
        show(state["idx"] + 1)

    def on_prev(_=None):
        show(state["idx"] - 1)

    def on_home(_=None):
        show(0)

    def on_play(_=None):
        if state["playing"]:
            # Stop
            state["playing"] = False
            if state["timer"] is not None:
                state["timer"].stop()
                state["timer"] = None
            btn_play.label.set_text("Play")
        else:
            # Start
            state["playing"] = True
            btn_play.label.set_text("Pause")

            def tick():
                if state["idx"] < n_frames - 1:
                    on_next()
                else:
                    on_play()  # auto-stop at end

            timer = fig.canvas.new_timer(interval=150)
            timer.add_callback(tick)
            timer.start()
            state["timer"] = timer
        fig.canvas.draw_idle()

    def on_scroll(event):
        factor = 0.9 if event.button == 'up' else 1.1
        state["zoom"] *= factor
        s = span * state["zoom"]
        ax.set_xlim(center[0] - s, center[0] + s)
        ax.set_ylim(center[1] - s, center[1] + s)
        ax.set_zlim(center[2] - s, center[2] + s)
        fig.canvas.draw_idle()

    def on_key(event):
        if event.key == 'right':
            on_next()
        elif event.key == 'left':
            on_prev()
        elif event.key == ' ':
            on_play()
        elif event.key == 'home':
            on_home()

    # Buttons
    ax_prev = fig.add_axes([0.25, 0.02, 0.1, 0.04])
    ax_play = fig.add_axes([0.40, 0.02, 0.1, 0.04])
    ax_next = fig.add_axes([0.55, 0.02, 0.1, 0.04])
    ax_home = fig.add_axes([0.70, 0.02, 0.1, 0.04])

    btn_prev = Button(ax_prev, 'Prev')
    btn_play = Button(ax_play, 'Play')
    btn_next = Button(ax_next, 'Next')
    btn_home = Button(ax_home, 'Home')

    btn_prev.on_clicked(on_prev)
    btn_play.on_clicked(on_play)
    btn_next.on_clicked(on_next)
    btn_home.on_clicked(on_home)

    fig.canvas.mpl_connect('key_press_event', on_key)
    fig.canvas.mpl_connect('scroll_event', on_scroll)

    # Draw initial state
    show(0)
    print(f"Interactive mode: {n_frames} frames. Use arrow keys or buttons.")
    plt.show()


# ---------------------------------------------------------------------------
# Save MP4
# ---------------------------------------------------------------------------

def save_animation(d, frames, octa, args):
    """Save pre-rendered animation as MP4."""
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation, FFMpegWriter

    center, span = compute_view_bounds(d, frames, octa)
    max_flow = max(np.max(np.abs(f[1])) for f in frames)

    fig = plt.figure(figsize=(12, 9))
    ax = fig.add_subplot(111, projection='3d')

    def render(frame_idx):
        draw_frame(ax, d, frames, frame_idx, octa, center, span, max_flow,
                   elev=25, azim=45)

    print(f"Creating animation with {len(frames)} frames...")
    anim = FuncAnimation(fig, render, frames=len(frames),
                         interval=200, repeat=True)

    outfile = f'debug_{args.init}_s{args.size}_f{args.flow}.mp4'
    writer = FFMpegWriter(fps=5, bitrate=2000)
    anim.save(outfile, writer=writer)
    print(f"Saved {outfile}")
    plt.close()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Step-by-step firing debugger")
    parser.add_argument("--init", choices=["hollow-face", "hollow-octa"],
                        default="hollow-face")
    parser.add_argument("--size", type=int, default=3)
    parser.add_argument("--flow", type=int, default=100)
    parser.add_argument("--max-frames", type=int, default=500)
    parser.add_argument("--shuffle", action="store_true", default=False)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--save", action="store_true",
                        help="Save MP4 instead of interactive GUI")
    args = parser.parse_args()

    d, config, octa = build_sim(args)
    frames = compute_firing_sequence(config, d, args.max_frames,
                                     args.shuffle, args.seed)

    if args.save:
        save_animation(d, frames, octa, args)
    else:
        run_interactive(d, frames, octa, args)


if __name__ == "__main__":
    main()
