"""Initial configuration helpers for grid experiments."""

import numpy as np
from collections import defaultdict
import scipy.sparse as sp


Z_SCALE = 0.7


def vkey(v):
    """Vertex coordinate key (rounded to 4 decimals)."""
    return tuple(np.round(v, 4))


def make_rect_cycle(d, corners, initial):
    """Set flow=initial on rectangular cycle through corners.

    Args:
        d: builder dict with "edge_verts" and "num_edges"
        corners: list of (x, y) corner coordinates
        initial: flow magnitude
    """
    edge_verts = d["edge_verts"]
    num_edges = d["num_edges"]

    edge_map = {}
    for ei in range(num_edges):
        k0 = vkey(edge_verts[ei, 0])
        k1 = vkey(edge_verts[ei, 1])
        edge_map[(k0, k1)] = ei
        edge_map[(k1, k0)] = ei

    cfg = np.zeros(num_edges, dtype=np.int32)

    for ci in range(len(corners)):
        x0, y0 = corners[ci]
        x1, y1 = corners[(ci + 1) % len(corners)]

        if y0 == y1:  # horizontal side
            step = 1 if x1 > x0 else -1
            for x in range(x0, x1, step):
                v0 = (float(x), float(y0), 0.0)
                v1 = (float(x + step), float(y0), 0.0)
                k0, k1 = vkey(np.array(v0)), vkey(np.array(v1))
                ei = edge_map[(k0, k1)]
                if vkey(edge_verts[ei, 0]) == k0:
                    cfg[ei] += initial
                else:
                    cfg[ei] -= initial
        else:  # vertical side
            step = 1 if y1 > y0 else -1
            for y in range(y0, y1, step):
                v0 = (float(x0), float(y), 0.0)
                v1 = (float(x0), float(y + step), 0.0)
                k0, k1 = vkey(np.array(v0)), vkey(np.array(v1))
                ei = edge_map[(k0, k1)]
                if vkey(edge_verts[ei, 0]) == k0:
                    cfg[ei] += initial
                else:
                    cfg[ei] -= initial

    return cfg


def make_triangle_circulation(d, center, flow):
    """Find face nearest to center, return boundary column * flow as config.

    Args:
        d: builder dict with "faces", "edge_verts", "bnd_row", "bnd_col", "bnd_val", "num_edges"
        center: (x, y, z) center point
        flow: integer flow magnitude
    """
    faces = d["faces"]
    edge_verts = d["edge_verts"]
    num_edges = d["num_edges"]
    center = np.asarray(center, dtype=np.float64)

    # faces is (num_faces, 3) for triangular or (num_faces, 4) for quad
    num_faces = faces.shape[0]
    face_width = faces.shape[1]

    best_f, best_d = 0, float('inf')
    for fi in range(num_faces):
        edge_indices = faces[fi]
        pts = edge_verts[edge_indices].reshape(-1, 3)
        dist = np.linalg.norm(pts.mean(axis=0) - center)
        if dist < best_d:
            best_d = dist
            best_f = fi

    # Extract boundary column for this face
    bnd_row = d["bnd_row"]
    bnd_col = d["bnd_col"]
    bnd_val = d["bnd_val"]
    mask = bnd_col == best_f

    config = np.zeros(num_edges, dtype=np.int32)
    config[bnd_row[mask]] = bnd_val[mask] * flow
    return config


def hollow_for_grid(r):
    """2x2 hollow block centered in an r x r grid."""
    return (r // 2 - 1, r // 2 - 1, r // 2 + 1, r // 2 + 1)


def cycle_corners_for_hollow(hollow):
    """Cycle corners walking around the hollow boundary."""
    r0, c0, r1, c1 = hollow
    return [(r0, c0), (r1, c0), (r1, c1), (r0, c1)]


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

