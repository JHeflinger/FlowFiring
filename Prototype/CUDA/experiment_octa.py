"""
Convergence experiment: hollow face vs hollow volume on lattice_3d.

Usage:
    python experiment_octa.py --phase verify      # visual verification
    python experiment_octa.py --phase experiment   # convergence test
    python experiment_octa.py --phase all          # both

    python experiment_octa.py --phase verify --size 3
    python experiment_octa.py --phase experiment --size 4 --seeds 10 --flow 1000
"""

import argparse
from collections import defaultdict

import numpy as np
import scipy.sparse as sp
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Line3DCollection

from flowfiring import build_lattice_3d, fire_sequential
from flowfiring.configs import vkey


Z_SCALE = 0.7


# ---------------------------------------------------------------------------
# Step 1: Octahedron finding
# ---------------------------------------------------------------------------

def _build_edge_lookups(d):
    """Build vertex->edges and vertex-pair->edge lookups."""
    edge_verts = d["edge_verts"]
    num_edges = d["num_edges"]

    # vkey -> list of edge indices
    vert_edges = defaultdict(list)
    # (vkey_lo, vkey_hi) -> edge index  (sorted pair)
    pair_to_edge = {}

    for ei in range(num_edges):
        k0 = vkey(edge_verts[ei, 0])
        k1 = vkey(edge_verts[ei, 1])
        vert_edges[k0].append(ei)
        vert_edges[k1].append(ei)
        pair = tuple(sorted([k0, k1]))
        pair_to_edge[pair] = ei

    return vert_edges, pair_to_edge


def find_octahedra(d):
    """Find all octahedra in the lattice.

    Returns list of dicts with keys:
        bottom, top: vertex keys (tuples)
        equatorial: list of 4 vertex keys
        edge_indices: list of 12 edge indices
        face_indices: list of 8 face indices
        center: (x, y, z) centroid
    """
    edge_verts = d["edge_verts"]
    num_edges = d["num_edges"]
    vert_edges, pair_to_edge = _build_edge_lookups(d)

    # Collect all unique vertices and group by z-coordinate
    all_verts = set()
    for ei in range(num_edges):
        all_verts.add(vkey(edge_verts[ei, 0]))
        all_verts.add(vkey(edge_verts[ei, 1]))

    z_groups = defaultdict(list)
    for v in all_verts:
        z_groups[v[2]].append(v)

    # Sort z-levels
    z_levels = sorted(z_groups.keys())

    # Build face edge set for quick lookup
    faces = d["faces"]
    num_faces = faces.shape[0]
    face_edge_sets = []
    for fi in range(num_faces):
        face_edge_sets.append(frozenset(faces[fi].tolist()))

    # For each pair of z-levels separated by 2*Z_SCALE (≈1.4),
    # the equatorial level is at z_bottom + Z_SCALE
    octahedra = []

    for zi, z_bot in enumerate(z_levels):
        z_eq = round(z_bot + Z_SCALE, 4)
        z_top = round(z_bot + 2 * Z_SCALE, 4)

        if z_eq not in z_groups or z_top not in z_groups:
            continue

        for bottom in z_groups[z_bot]:
            bx, by, bz = bottom
            top = (bx, by, z_top)

            if top not in vert_edges:
                continue

            # 4 equatorial vertices
            eq = [
                (round(bx - 0.5, 4), round(by - 0.5, 4), z_eq),
                (round(bx + 0.5, 4), round(by - 0.5, 4), z_eq),
                (round(bx + 0.5, 4), round(by + 0.5, 4), z_eq),
                (round(bx - 0.5, 4), round(by + 0.5, 4), z_eq),
            ]

            # Check all equatorial vertices exist
            if not all(v in vert_edges for v in eq):
                continue

            # Check all 12 edges exist
            edges_12 = []
            ok = True

            # 4 bottom-equatorial edges
            for v in eq:
                pair = tuple(sorted([bottom, v]))
                if pair not in pair_to_edge:
                    ok = False
                    break
                edges_12.append(pair_to_edge[pair])

            if not ok:
                continue

            # 4 top-equatorial edges
            for v in eq:
                pair = tuple(sorted([top, v]))
                if pair not in pair_to_edge:
                    ok = False
                    break
                edges_12.append(pair_to_edge[pair])

            if not ok:
                continue

            # 4 equatorial-equatorial edges (adjacent pairs in the square)
            for i in range(4):
                v0 = eq[i]
                v1 = eq[(i + 1) % 4]
                pair = tuple(sorted([v0, v1]))
                if pair not in pair_to_edge:
                    ok = False
                    break
                edges_12.append(pair_to_edge[pair])

            if not ok:
                continue

            # Find the 8 face indices
            edge_set = set(edges_12)
            face_indices = []
            for fi in range(num_faces):
                if face_edge_sets[fi].issubset(edge_set):
                    face_indices.append(fi)

            if len(face_indices) != 8:
                continue

            center = (
                (bottom[0] + top[0]) / 2,
                (bottom[1] + top[1]) / 2,
                (bottom[2] + top[2]) / 2,
            )

            octahedra.append({
                "bottom": bottom,
                "top": top,
                "equatorial": eq,
                "edge_indices": edges_12,
                "face_indices": face_indices,
                "center": center,
            })

    return octahedra


def pick_central(octahedra, d):
    """Select octahedron nearest the lattice center."""
    edge_verts = d["edge_verts"]
    num_edges = d["num_edges"]

    # Compute lattice bounding box center
    all_pts = edge_verts.reshape(-1, 3)
    lattice_center = (all_pts.min(axis=0) + all_pts.max(axis=0)) / 2

    best, best_dist = None, float("inf")
    for o in octahedra:
        c = np.array(o["center"])
        dist = np.linalg.norm(c - lattice_center)
        if dist < best_dist:
            best_dist = dist
            best = o

    return best


# ---------------------------------------------------------------------------
# Step 2: Visual verification
# ---------------------------------------------------------------------------

def plot_lattice_with_highlight(d, highlight_edges, title="Lattice"):
    """3D plot: all edges gray, highlighted edges red."""
    edge_verts = d["edge_verts"]
    num_edges = d["num_edges"]
    highlight_set = set(highlight_edges)

    fig = plt.figure(figsize=(10, 8))
    ax = fig.add_subplot(111, projection="3d")

    # Regular edges
    reg_lines = []
    hi_lines = []
    for ei in range(num_edges):
        v0 = edge_verts[ei, 0]
        v1 = edge_verts[ei, 1]
        seg = [v0.tolist(), v1.tolist()]
        if ei in highlight_set:
            hi_lines.append(seg)
        else:
            reg_lines.append(seg)

    if reg_lines:
        lc_reg = Line3DCollection(reg_lines, colors="gray", linewidths=0.3, alpha=0.3)
        ax.add_collection3d(lc_reg)

    if hi_lines:
        lc_hi = Line3DCollection(hi_lines, colors="red", linewidths=2.5, alpha=1.0)
        ax.add_collection3d(lc_hi)

    # Set axis limits
    all_pts = edge_verts.reshape(-1, 3)
    for i, label in enumerate(["X", "Y", "Z"]):
        lo, hi = all_pts[:, i].min(), all_pts[:, i].max()
        pad = (hi - lo) * 0.05
        getattr(ax, f"set_{label.lower()}lim")(lo - pad, hi + pad)
        getattr(ax, f"set_{label.lower()}label")(label)

    ax.set_title(title)
    plt.tight_layout()
    plt.show()


# ---------------------------------------------------------------------------
# Step 3: Face removal + Laplacian recomputation
# ---------------------------------------------------------------------------

def verify_laplacian_scipy(d):
    """Verify scipy Laplacian matches C++ Laplacian exactly."""
    bnd_row = np.array(d["bnd_row"], dtype=np.int64)
    bnd_col = np.array(d["bnd_col"], dtype=np.int64)
    bnd_val = np.array(d["bnd_val"], dtype=np.int64)

    B = sp.coo_matrix(
        (bnd_val, (bnd_row, bnd_col)),
        shape=(d["num_edges"], d["num_faces"]),
    ).tocsc()

    L_scipy = (B @ B.T).tocsc()

    L_cpp = sp.csc_matrix(
        (d["data"].astype(np.int64), d["indices"], d["indptr"]),
        shape=(d["num_edges"], d["num_edges"]),
    )

    diff = L_scipy - L_cpp
    if diff.nnz == 0:
        print("  Laplacian verification: PASS")
        return True
    else:
        print(f"  Laplacian verification: FAIL ({diff.nnz} differing entries)")
        print(f"  Max abs difference: {np.max(np.abs(diff.data))}")
        return False


def remove_faces(d, face_indices_to_remove):
    """Remove faces from complex, recompute Laplacian.

    Returns:
        d_new: modified dict with updated Laplacian
        removed_bnd: list of dicts with 'edges' and 'signs' for each removed face
    """
    bnd_row = np.array(d["bnd_row"], dtype=np.int64)
    bnd_col = np.array(d["bnd_col"], dtype=np.int64)
    bnd_val = np.array(d["bnd_val"], dtype=np.int64)
    num_edges = d["num_edges"]
    num_faces = d["num_faces"]

    remove_set = set(face_indices_to_remove)

    # Save boundary data for removed faces
    removed_bnd = []
    for fi in face_indices_to_remove:
        mask = bnd_col == fi
        removed_bnd.append({
            "edges": np.array(d["bnd_row"])[mask].copy(),
            "signs": np.array(d["bnd_val"])[mask].copy(),
        })

    # Filter out removed faces
    keep_mask = np.array([c not in remove_set for c in bnd_col])
    new_bnd_row = bnd_row[keep_mask]
    new_bnd_col = bnd_col[keep_mask]
    new_bnd_val = bnd_val[keep_mask]

    # Re-index face columns
    new_num_faces = num_faces - len(face_indices_to_remove)
    old_to_new = np.full(num_faces, -1, dtype=np.int64)
    new_fi = 0
    for fi in range(num_faces):
        if fi not in remove_set:
            old_to_new[fi] = new_fi
            new_fi += 1
    new_bnd_col = old_to_new[new_bnd_col]

    # Build scipy sparse B and compute L = B * B^T
    B = sp.coo_matrix(
        (new_bnd_val, (new_bnd_row, new_bnd_col)),
        shape=(num_edges, new_num_faces),
    ).tocsc()

    L = (B @ B.T).tocsc()

    # Update dict
    d_new = dict(d)
    d_new["indptr"] = np.array(L.indptr, dtype=np.int32)
    d_new["indices"] = np.array(L.indices, dtype=np.int32)
    d_new["data"] = np.array(L.data, dtype=np.int32)
    d_new["degrees"] = np.array(L.diagonal(), dtype=np.int32)
    d_new["num_faces"] = new_num_faces
    d_new["bnd_row"] = new_bnd_row.astype(np.int32)
    d_new["bnd_col"] = new_bnd_col.astype(np.int32)
    d_new["bnd_val"] = new_bnd_val.astype(np.int32)

    return d_new, removed_bnd


# ---------------------------------------------------------------------------
# Step 4: Eulerian circuit
# ---------------------------------------------------------------------------

def find_eulerian_circuit(octa, d):
    """Find Eulerian circuit where every face has circulation ±3.

    The octahedron's face adjacency graph is a cube (bipartite). We assign
    each face a "flip" factor c_f ∈ {+1, -1} such that for any shared edge e
    between faces f and g:  c_f * s_{f,e} = c_g * s_{g,e}.
    Then x_e = c_f * s_{f,e} gives all faces circulation ±3 and balanced
    vertex in/out degrees (Eulerian).

    Returns list of (edge_index, sign) where sign is +1/-1 indicating
    traversal direction relative to the edge's v0->v1 orientation.
    """
    edge_indices = octa["edge_indices"]
    face_indices = octa["face_indices"]
    edge_verts = d["edge_verts"]
    bnd_row = np.array(d["bnd_row"])
    bnd_col = np.array(d["bnd_col"])
    bnd_val = np.array(d["bnd_val"])

    face_set = set(face_indices)

    # Get boundary signs for each face: face_idx -> {edge_idx: sign}
    face_bnd = defaultdict(dict)
    for k in range(len(bnd_row)):
        fi = int(bnd_col[k])
        if fi in face_set:
            face_bnd[fi][int(bnd_row[k])] = int(bnd_val[k])

    # Build face adjacency with shared edge info
    edge_to_faces = defaultdict(list)
    for fi, edges in face_bnd.items():
        for ei in edges:
            edge_to_faces[ei].append(fi)

    # BFS to assign flip factors c_f
    # For shared edge e: c_f * s_{f,e} = c_g * s_{g,e}
    # => c_g = c_f * s_{f,e} / s_{g,e}  (signs are ±1 so division = multiplication)
    c = {}
    c[face_indices[0]] = 1
    queue = [face_indices[0]]

    while queue:
        fi = queue.pop(0)
        for ei, s_fi in face_bnd[fi].items():
            for fj in edge_to_faces[ei]:
                if fj == fi or fj in c:
                    continue
                s_fj = face_bnd[fj][ei]
                c[fj] = c[fi] * s_fi * s_fj  # c_g = c_f * s_f / s_g = c_f * s_f * s_g
                queue.append(fj)

    # Derive edge directions: x_e = c_f * s_{f,e}
    edge_dir = {}
    for fi in face_indices:
        for ei, s in face_bnd[fi].items():
            x = c[fi] * s
            if ei in edge_dir:
                assert edge_dir[ei] == x, \
                    f"Inconsistent direction for edge {ei}"
            else:
                edge_dir[ei] = x

    # Verify all faces have circulation ±3
    for fi in face_indices:
        circ = sum(edge_dir[ei] * s for ei, s in face_bnd[fi].items())
        assert abs(circ) == 3, f"Face {fi} has circulation {circ}, expected ±3"

    # Build directed adjacency and find Euler circuit (Hierholzer's)
    adj = defaultdict(list)
    for ei in edge_indices:
        v0 = vkey(edge_verts[ei, 0])
        v1 = vkey(edge_verts[ei, 1])
        if edge_dir[ei] > 0:
            adj[v0].append((ei, v1))
        else:
            adj[v1].append((ei, v0))

    start = next(iter(adj))
    stack = [start]
    circuit_verts = []
    used = set()
    adj_ptr = {v: 0 for v in adj}

    while stack:
        v = stack[-1]
        found = False
        while adj_ptr[v] < len(adj[v]):
            ei, w = adj[v][adj_ptr[v]]
            adj_ptr[v] += 1
            if ei not in used:
                used.add(ei)
                stack.append(w)
                found = True
                break
        if not found:
            stack.pop()
            circuit_verts.append(v)

    # Convert vertex sequence to (edge_index, sign) pairs
    directed = {}
    for ei in edge_indices:
        v0 = vkey(edge_verts[ei, 0])
        v1 = vkey(edge_verts[ei, 1])
        directed[(v0, v1)] = (ei, +1)
        directed[(v1, v0)] = (ei, -1)

    circuit = []
    for i in range(len(circuit_verts) - 1):
        v_from = circuit_verts[i]
        v_to = circuit_verts[i + 1]
        ei, sign = directed[(v_from, v_to)]
        circuit.append((ei, sign))

    return circuit


def circuit_to_config(circuit, num_edges, flow):
    """Convert Eulerian circuit to flow configuration."""
    config = np.zeros(num_edges, dtype=np.int32)
    for ei, sign in circuit:
        config[ei] += sign * flow
    return config


# ---------------------------------------------------------------------------
# Step 5: Convergence experiment
# ---------------------------------------------------------------------------

def run_convergence(d_modified, config_template, num_seeds, label):
    """Run fire_sequential with multiple seeds, check convergence."""
    print(f"\n--- {label} ---")
    print(f"  Initial |flow| = {np.sum(np.abs(config_template))}")

    finals = []
    for seed in range(num_seeds):
        cfg = config_template.copy()
        total = fire_sequential(cfg, d_modified, max_steps=10_000_000,
                                shuffle=True, seed=seed)
        finals.append(cfg.copy())
        print(f"  seed={seed}: fired={total}, final |flow|={np.sum(np.abs(cfg))}")

    # Compare all to seed 0
    all_same = True
    for i in range(1, num_seeds):
        diff = finals[i] - finals[0]
        if np.any(diff != 0):
            all_same = False
            ndiff = np.count_nonzero(diff)
            maxdiff = np.max(np.abs(diff))
            print(f"  seed {i} differs from seed 0: {ndiff} edges, max diff={maxdiff}")

    if all_same:
        print(f"  RESULT: All {num_seeds} seeds converge to SAME configuration")
    else:
        print(f"  RESULT: Seeds converge to DIFFERENT configurations")

    return finals, all_same


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def phase_verify(size):
    print(f"Building lattice_3d(size={size})...")
    d = build_lattice_3d(size)
    print(f"  {d['num_edges']} edges, {d['num_faces']} faces")

    print("Verifying Laplacian (scipy vs C++)...")
    verify_laplacian_scipy(d)

    print("Finding octahedra...")
    octahedra = find_octahedra(d)
    print(f"  Found {len(octahedra)} octahedra")

    if not octahedra:
        print("  No octahedra found! Try a larger size.")
        return

    octa = pick_central(octahedra, d)
    print(f"  Central octahedron at {octa['center']}")
    print(f"    bottom = {octa['bottom']}")
    print(f"    top    = {octa['top']}")
    print(f"    equatorial = {octa['equatorial']}")
    print(f"    12 edges, 8 faces")

    # Verify Eulerian circuit
    circuit = find_eulerian_circuit(octa, d)
    print(f"  Eulerian circuit: {len(circuit)} edges")

    # Compute per-face circulations
    bnd_row = np.array(d["bnd_row"])
    bnd_col = np.array(d["bnd_col"])
    bnd_val = np.array(d["bnd_val"])
    circ_signs = {}
    for ei, sign in circuit:
        circ_signs[ei] = sign
    for fi in octa["face_indices"]:
        mask = bnd_col == fi
        edges_f = bnd_row[mask]
        signs_f = bnd_val[mask]
        circ = sum(circ_signs[ei] * s for ei, s in zip(edges_f, signs_f))
        print(f"    face {fi}: circulation = {circ}")

    plot_lattice_with_highlight(
        d, octa["edge_indices"],
        title=f"Lattice size={size}, central octahedron (red)"
    )


def phase_experiment(size, num_seeds, flow):
    print(f"Building lattice_3d(size={size})...")
    d = build_lattice_3d(size)
    print(f"  {d['num_edges']} edges, {d['num_faces']} faces")

    print("Finding octahedra...")
    octahedra = find_octahedra(d)
    octa = pick_central(octahedra, d)
    print(f"  Central octahedron at {octa['center']}")

    # --- Hollow face: remove 1 face of the octahedron ---
    face_to_remove = [octa["face_indices"][0]]
    d_hf, removed_bnd_hf = remove_faces(d, face_to_remove)

    config_hf = np.zeros(d["num_edges"], dtype=np.int32)
    config_hf[removed_bnd_hf[0]["edges"]] = removed_bnd_hf[0]["signs"] * flow

    finals_hf, same_hf = run_convergence(
        d_hf, config_hf, num_seeds, f"Hollow face (1 triangle, flow={flow})")

    # --- Hollow volume: remove all 8 octahedron faces ---
    d_hv, removed_bnd_hv = remove_faces(d, octa["face_indices"])

    circuit = find_eulerian_circuit(octa, d)
    config_hv = circuit_to_config(circuit, d["num_edges"], flow)

    finals_hv, same_hv = run_convergence(
        d_hv, config_hv, num_seeds, f"Hollow volume (octahedron, flow={flow})")

    # Summary
    print("\n=== Summary ===")
    print(f"  Hollow face:   {'confluent' if same_hf else 'NOT confluent'}")
    print(f"  Hollow volume: {'confluent' if same_hv else 'NOT confluent'}")


def main():
    parser = argparse.ArgumentParser(description="Octahedron convergence experiment")
    parser.add_argument("--phase", choices=["verify", "experiment", "all"],
                        default="all")
    parser.add_argument("--size", type=int, default=4)
    parser.add_argument("--seeds", type=int, default=10)
    parser.add_argument("--flow", type=int, default=1000)
    args = parser.parse_args()

    if args.phase in ("verify", "all"):
        phase_verify(args.size)

    if args.phase in ("experiment", "all"):
        phase_experiment(args.size, args.seeds, args.flow)


if __name__ == "__main__":
    main()
