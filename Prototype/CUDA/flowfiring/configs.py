"""Initial configuration helpers for grid experiments."""

import numpy as np


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
