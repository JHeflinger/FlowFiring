"""
Blender visualization for animated flow-firing.

Usage:
    blender viewer.blend --python viewer.py
    blender viewer.blend --background --python viewer.py

    # Pass script args after "--":
    blender viewer.blend --python viewer.py -- --init quad --size 10
    blender viewer.blend --python viewer.py -- --init cubic --size 5
    blender viewer.blend --python viewer.py -- --shuffle --seed 42

    # Render paper-quality PNG:
    blender viewer.blend --background --python viewer.py -- --init cubic --size 5 --prefire -1 --render-frame 1 --output stable.png --transparent-bg

    # Render animation MP4:
    blender viewer.blend --background --python viewer.py -- --init cubic --size 5 --render-video --output firing.mp4

    # Directional arrows:
    blender viewer.blend --python viewer.py -- --init cubic --size 5 --arrows
"""

import sys
import math
import argparse
from pathlib import Path

import numpy as np
import bpy  # type: ignore
import mathutils  # type: ignore

# Add current working dir and env site-packages to sys path
# since Blender uses its own bundled python
DIR = Path(__file__).parent
sys.path.insert(0, str(DIR))
sys.path.insert(0, str(DIR / ".venv/lib/python3.13/site-packages"))

import scipy.sparse as sp  # noqa: E402
from scipy.optimize import linprog  # noqa: E402

from flowfiring import build_lattice_3d, build_grid_3d, build_grid_2d, build_grid_2d_quad  # type: ignore  # noqa: E402
from flowfiring.firing import fire_sequential_step, fire_step, fire_sequential, fire  # noqa: E402
from flowfiring.configs import (  # noqa: E402
    make_triangle_circulation,
    vkey,
    find_octahedra,
    pick_central,
    find_eulerian_circuit,
    circuit_to_config,
    remove_faces,
)


# Color palette: |flow| -> RGBA
SIM_COLORS = [
    (0, 0, 0, 0.0),        # 0: black transparent
    (1.0, 0, 0, 1.0),      # 1: red
    (0, 1.0, 0, 1.0),      # 2: green
    (0, 0, 1.0, 1.0),      # 3: blue
    (1.0, 1.0, 0, 1.0),    # 4: yellow
    (1.0, 0, 1.0, 1.0),    # 5: magenta
    (0, 1.0, 1.0, 1.0),    # 6: cyan
]


def edge_color(value):
    idx = min(abs(value), len(SIM_COLORS) - 1)
    return SIM_COLORS[idx]


FACE_COLOR_CAP = 4


def face_color(value):
    if value == 0:
        return (0.5, 0.5, 0.5, 0.0)
    mag = min(abs(value), FACE_COLOR_CAP) / FACE_COLOR_CAP
    alpha = 0.25 + 0.6 * mag
    if value > 0:
        return (1.0, 0.15 + 0.55 * (1 - mag), 0.15 + 0.55 * (1 - mag), alpha)
    else:
        return (0.15 + 0.55 * (1 - mag), 0.15 + 0.55 * (1 - mag), 1.0, alpha)


# Module state
sim_d = None
sim_config = None
sim_z_layers = []          # per-edge z-layer (int or float)
sim_unique_layers = []     # sorted unique layer values
sim_layer_indices = []     # per-edge index into sim_unique_layers
sim_label_objs = []        # per-edge FONT curve objects for numeric labels
_label_draw_handle = None  # SpaceView3D draw handler for viewport billboarding

sim_B = None
sim_face_centroids = None
sim_face_z_layers = None
sim_face_euler = None
_face_residual_warned = False

_edge_obj = None
_face_obj = None
_glyph_obj = None
_update_mesh_fn = None


def _apply_view_mode():
    if _edge_obj is None or _face_obj is None or _glyph_obj is None:
        return
    props = bpy.context.scene.flowfire_props
    _edge_obj.hide_viewport = not props.show_edges
    _face_obj.hide_viewport = not props.show_faces
    _glyph_obj.hide_viewport = not props.show_faces


def _request_mesh_update():
    if _update_mesh_fn is not None:
        _update_mesh_fn()


def parse_args():
    try:
        idx = sys.argv.index("--")
        script_args = sys.argv[idx + 1:]
    except ValueError:
        script_args = []
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=10)
    parser.add_argument("--init", choices=["triangle", "quad", "cubic", "Z2tess", "Z2",
                                          "hollow-face", "hollow-octa"],
                        default="quad")
    parser.add_argument("--initial", type=int, default=1000)
    parser.add_argument("--shuffle", action="store_true", default=False)
    parser.add_argument("--seed", type=int, default=None)
    parser.add_argument("--gpu", action="store_true", help="Use CUDA colored firing")
    parser.add_argument("--hollow-face", action="store_true",
                        help="Remove center XY-face (cubic/Z2 only)")
    parser.add_argument("--prefire", type=int, default=0,
                        help="Fire N steps before showing (-1 = until stable)")
    parser.add_argument("--single", action="store_true",
                        help="Fire one edge per frame (step through individual firings)")
    # Rendering
    parser.add_argument("--render-frame", type=int, default=None,
                        help="Render a single frame as PNG (Cycles), then exit")
    parser.add_argument("--render-video", action="store_true",
                        help="Render animation as MP4, then exit")
    parser.add_argument("--output", type=str, default=None,
                        help="Output path (default: render.png or render.mp4)")
    parser.add_argument("--render-samples", type=int, default=128,
                        help="Cycles render samples")
    parser.add_argument("--render-res", type=str, default="1920x1080",
                        help="Render resolution WxH")
    parser.add_argument("--camera-elev", type=float, default=30.0,
                        help="Camera elevation angle in degrees")
    parser.add_argument("--camera-azim", type=float, default=45.0,
                        help="Camera azimuth angle in degrees")
    parser.add_argument("--camera-distance", type=float, default=1.8,
                        help="Camera distance multiplier (default: 1.8)")
    parser.add_argument("--transparent-bg", action="store_true",
                        help="Use transparent background (for paper figures)")
    # Arrows
    parser.add_argument("--arrows", action="store_true",
                        help="Show directional arrow cones on edges")
    # Numeric labels
    parser.add_argument("--flow-labels", action="store_true",
                        help="Show numeric flow-value labels on edges")
    # Visualization mode
    parser.add_argument("--packet-only", action="store_true",
                        help="Show only the flowing packet "
                             "(tubes/arrows hidden). Default shows "
                             "tubes + arrows with no packet animation.")
    return parser.parse_args(script_args)


def _attach_viz_boundary(d, d_full):
    for k in ("faces", "bnd_row", "bnd_col", "bnd_val", "num_faces"):
        d[k + "_viz"] = d_full[k]
    return d


def build_complex(args):
    """Build simplicial complex based on --init mode."""
    global sim_z_layers, sim_unique_layers, sim_layer_indices

    if args.init == "Z2tess":
        n = args.size
        mid = n // 2
        d_full = build_grid_2d(n, n, with_colors=args.gpu)
        if args.hollow_face:
            d = build_grid_2d(
                n, n,
                has_hollow=True,
                x_lo=mid, x_hi=mid + 1,
                y_lo=mid, y_hi=mid + 1,
                with_colors=args.gpu,
            )
        else:
            d = d_full
        _attach_viz_boundary(d, d_full)
        sim_z_layers = [0] * d["num_edges"]
        return d
    elif args.init == "Z2":
        n = args.size
        mid = n // 2
        d_full = build_grid_2d_quad(n, n, with_colors=args.gpu)
        if args.hollow_face:
            d = build_grid_2d_quad(
                n, n,
                has_hollow=True,
                x_lo=mid, x_hi=mid + 1,
                y_lo=mid, y_hi=mid + 1,
                with_colors=args.gpu,
            )
        else:
            d = d_full
        _attach_viz_boundary(d, d_full)
        sim_z_layers = [0] * d["num_edges"]
        return d
    elif args.init == "cubic":
        n = args.size
        mid = n // 2
        d_full = build_grid_3d(n, n, n, with_colors=args.gpu)
        if args.hollow_face:
            hollow_planes = np.array([0], dtype=np.int32)  # XY plane
            hollow_coords = np.array([mid, mid, mid], dtype=np.int32)
            d = build_grid_3d(n, n, n, with_colors=args.gpu,
                              hollow_face_planes=hollow_planes,
                              hollow_face_coords=hollow_coords)
        else:
            d = d_full
        _attach_viz_boundary(d, d_full)

        # Compute z-layers from edge arrays
        edge_axes = d["edge_axes"]
        edge_iz = d["edge_iz"]
        sim_z_layers = []
        for i in range(d["num_edges"]):
            if edge_axes[i] == 2:  # z-edge spans two layers
                sim_z_layers.append(edge_iz[i] + 0.5)
            else:
                sim_z_layers.append(float(edge_iz[i]))

        return d
    else:
        d = build_lattice_3d(args.size, with_colors=args.gpu)
        d_full = d

        # For hollow modes, find octahedron and remove faces
        if args.init in ("hollow-face", "hollow-octa"):
            octahedra = find_octahedra(d)
            if not octahedra:
                print("WARNING: no octahedra found, need larger --size")
            else:
                octa = pick_central(octahedra, d)
                print(f"  Central octahedron at {octa["center"]}")

                if args.init == "hollow-octa":
                    # Compute circuit BEFORE removing faces (needs original bnd data)
                    circuit = find_eulerian_circuit(octa, d)
                    d, removed = remove_faces(d, octa["face_indices"])
                    d["_circuit"] = circuit
                    print("  Removed 8 octahedron faces")
                else:  # hollow-face
                    face_to_remove = [octa["face_indices"][0]]
                    d, removed = remove_faces(d, face_to_remove)
                    d["_removed_bnd"] = removed
                    print("  Removed 1 triangle face")

        _attach_viz_boundary(d, d_full)

        # Compute z-layers from vertex z-coordinates
        sim_z_layers = []
        for i in range(d["num_edges"]):
            z = round(d["edge_verts"][i, 0, 2], 2)
            sim_z_layers.append(z)

        return d


def make_initial_config(args, d):
    """Create initial flow configuration."""
    if args.init == "cubic" or args.init in ("Z2", "Z2tess"):
        num_edges = d["num_edges"]

        if args.hollow_face and d.get("removed_face_edges", np.array([])).shape[0] > 0:
            # Face cycle around the removed face
            edges = d["removed_face_edges"][0]
            signs = d["removed_face_signs"][0]
            config = np.zeros(num_edges, dtype=np.int32)
            for ei, s in zip(edges, signs):
                config[ei] = s * args.initial
            print(f"Face cycle on removed XY-face (flow={args.initial})")
            return config

        # Square circulation in center XY-plane
        edge_verts = d["edge_verts"]
        n = args.size

        edge_map = {}
        for ei in range(num_edges):
            k0 = vkey(edge_verts[ei, 0])
            k1 = vkey(edge_verts[ei, 1])
            edge_map[(k0, k1)] = ei
            edge_map[(k1, k0)] = ei

        if args.init == "cubic":
            gx, gy, gz = n // 2, n // 2, n // 2
        elif args.init in ("Z2", "Z2tess"):
            gx, gy, gz = n // 2, n // 2, 0 
        else:
            raise ValueError(f"Unknown {args.init=}")

        A = (float(gx), float(gy), float(gz))
        B = (float(gx + 1), float(gy), float(gz))
        C = (float(gx + 1), float(gy + 1), float(gz))
        D = (float(gx), float(gy + 1), float(gz))

        config = np.zeros(num_edges, dtype=np.int32)
        for v0, v1 in [(A, B), (B, C), (C, D), (D, A)]:
            k0 = vkey(np.array(v0))
            k1 = vkey(np.array(v1))
            ei = edge_map[(k0, k1)]
            if vkey(edge_verts[ei, 0]) == k0:
                config[ei] = args.initial
            else:
                config[ei] = -args.initial
        print(f"Square cycle at z={gz}, center=({gx},{gy})")
        return config

    elif args.init == "quad":
        edge_verts = d["edge_verts"]
        num_edges = d["num_edges"]

        edge_map = {}
        for ei in range(num_edges):
            k0 = vkey(edge_verts[ei, 0])
            k1 = vkey(edge_verts[ei, 1])
            edge_map[(k0, k1)] = ei
            edge_map[(k1, k0)] = ei

        z_raw = (args.size // 2) * 2
        z_raw -= z_raw % 4
        z_coord = round(z_raw / 2 * 0.7, 4)
        gx, gy = args.size // 2, args.size // 2

        A = (float(gx), float(gy), z_coord)
        B = (float(gx + 1), float(gy), z_coord)
        C = (float(gx + 1), float(gy + 1), z_coord)
        D = (float(gx), float(gy + 1), z_coord)

        config = np.zeros(num_edges, dtype=np.int32)
        for v0, v1 in [(A, B), (B, C), (C, D), (D, A)]:
            k0 = vkey(np.array(v0))
            k1 = vkey(np.array(v1))
            ei = edge_map[(k0, k1)]
            if vkey(edge_verts[ei, 0]) == k0:
                config[ei] = args.initial
            else:
                config[ei] = -args.initial
        return config

    elif args.init == "hollow-octa":
        circuit = d["_circuit"]
        config = circuit_to_config(circuit, d["num_edges"], args.initial)
        print(f"Octahedron Eulerian circuit (flow={args.initial}, 12 edges)")
        return config

    elif args.init == "hollow-face":
        removed = d["_removed_bnd"]
        config = np.zeros(d["num_edges"], dtype=np.int32)
        config[removed[0]["edges"]] = removed[0]["signs"] * args.initial
        print(f"Hollow face circulation (flow={args.initial}, 3 edges)")
        return config

    else:  # triangle
        center = np.array([args.size / 2, args.size / 2, args.size / 2 * 0.7])
        config = make_triangle_circulation(d, center, args.initial)
        print(f"Triangle circulation around center {center}")
        return config


def _face_vertex_cycle(edge_indices, edge_verts):
    n = len(edge_indices)
    e0, e_last = edge_indices[0], edge_indices[-1]
    k0_a = vkey(edge_verts[e0, 0])
    k0_b = vkey(edge_verts[e0, 1])
    kl_a = vkey(edge_verts[e_last, 0])
    kl_b = vkey(edge_verts[e_last, 1])
    if k0_a == kl_a or k0_a == kl_b:
        start = edge_verts[e0, 0]
        cur = edge_verts[e0, 1]
        cur_key = k0_b
    else:
        start = edge_verts[e0, 1]
        cur = edge_verts[e0, 0]
        cur_key = k0_a
    out = [start.copy(), cur.copy()]
    for i in range(1, n - 1):
        ei = edge_indices[i]
        ka = vkey(edge_verts[ei, 0])
        kb = vkey(edge_verts[ei, 1])
        if ka == cur_key:
            cur = edge_verts[ei, 1]
            cur_key = kb
        else:
            cur = edge_verts[ei, 0]
            cur_key = ka
        out.append(cur.copy())
    return np.stack(out)


def _euler_align_z_to(normal):
    v = mathutils.Vector((float(normal[0]), float(normal[1]), float(normal[2])))
    if v.length < 1e-12:
        return (0.0, 0.0, 0.0)
    v.normalize()
    q = mathutils.Vector((0.0, 0.0, 1.0)).rotation_difference(v)
    e = q.to_euler("XYZ")
    return (e.x, e.y, e.z)


def build_face_mesh_data(d):
    faces = d.get("faces_viz", d["faces"])
    edge_verts = d["edge_verts"]
    num_faces = int(faces.shape[0])
    face_size = int(faces.shape[1])

    flat = np.zeros((num_faces * face_size, 3), dtype=np.float64)
    polys = []
    centroids = np.zeros((num_faces, 3), dtype=np.float64)
    normals = np.zeros((num_faces, 3), dtype=np.float64)
    z_layers = np.zeros(num_faces, dtype=np.float64)
    base_euler = np.zeros((num_faces, 3), dtype=np.float64)

    for fi in range(num_faces):
        cycle = _face_vertex_cycle(faces[fi], edge_verts)
        base = fi * face_size
        flat[base:base + face_size] = cycle
        polys.append(tuple(range(base, base + face_size)))
        c = cycle.mean(axis=0)
        centroids[fi] = c
        z_layers[fi] = c[2]
        e1 = cycle[1] - cycle[0]
        e2 = cycle[2] - cycle[1]
        n = np.cross(e1, e2)
        nl = np.linalg.norm(n)
        if nl > 1e-12:
            nhat = n / nl
            normals[fi] = nhat
            base_euler[fi] = _euler_align_z_to(nhat)

    return flat, polys, centroids, normals, z_layers, base_euler


def build_boundary_matrix(d):
    bnd_row = d.get("bnd_row_viz", d["bnd_row"])
    bnd_col = d.get("bnd_col_viz", d["bnd_col"])
    bnd_val = d.get("bnd_val_viz", d["bnd_val"])
    num_faces = int(d.get("num_faces_viz", d["num_faces"]))
    return sp.csr_matrix(
        (bnd_val.astype(np.float64), (bnd_row, bnd_col)),
        shape=(d["num_edges"], num_faces),
    )


def compute_face_circulation(B, config):
    global _face_residual_warned
    f = config.astype(np.float64)
    n = B.shape[1]
    c = np.ones(2 * n)
    A_eq = sp.hstack([B, -B]).tocsr()
    res = linprog(c, A_eq=A_eq, b_eq=f, bounds=(0, None), method="highs")
    if not res.success:
        if not _face_residual_warned:
            print(f"\033[33m[face view] L1 solve failed: {res.message}\033[0m")
            _face_residual_warned = True
        return np.zeros(n, dtype=np.int32)
    F = res.x[:n] - res.x[n:]
    residual = float(np.linalg.norm(B @ F - f))
    if residual > 1e-3 and not _face_residual_warned:
        print(
            f"\033[33m[face view] non-conservative config, residual={residual:.3e}; "
            f"showing best-fit face picture.\033[0m"
        )
        _face_residual_warned = True
    return np.round(F).astype(np.int32)


def build_curl_template_object():
    R = 0.18
    thickness = 0.025
    arrow_len = 0.09
    n_arc = 24
    angles = np.linspace(np.pi / 2, -np.pi, n_arc)
    inner = np.stack([(R - thickness) * np.cos(angles),
                      (R - thickness) * np.sin(angles),
                      np.zeros(n_arc)], axis=1)
    outer = np.stack([(R + thickness) * np.cos(angles),
                      (R + thickness) * np.sin(angles),
                      np.zeros(n_arc)], axis=1)
    verts = [tuple(v) for v in inner] + [tuple(v) for v in outer]
    polys = []
    for i in range(n_arc - 1):
        polys.append((i, n_arc + i, n_arc + i + 1, i + 1))

    end_center = (inner[-1] + outer[-1]) * 0.5
    prev_center = (inner[-2] + outer[-2]) * 0.5
    d_ = end_center - prev_center
    d_ = d_ / (np.linalg.norm(d_) + 1e-12)
    perp = np.array([-d_[1], d_[0], 0.0])
    tip = end_center + d_ * arrow_len
    left = end_center + perp * (thickness * 2.5)
    right = end_center - perp * (thickness * 2.5)
    base_idx = len(verts)
    verts.extend([tuple(tip), tuple(left), tuple(right)])
    polys.append((base_idx + 1, base_idx, base_idx + 2))

    mesh = bpy.data.meshes.new("curl_template_mesh")
    mesh.from_pydata(verts, [], polys)
    mesh.update()
    obj = bpy.data.objects.new("_curl_template", mesh)
    bpy.context.collection.objects.link(obj)
    obj.hide_viewport = True
    obj.hide_render = True
    return obj


def _on_view_mode_change(self, context):
    _apply_view_mode()
    _request_mesh_update()


# Blender layer visibility UI
class FlowFireProperties(bpy.types.PropertyGroup):
    z_min: bpy.props.FloatProperty(
        name="Z Min", default=-1e6, soft_min=-100, soft_max=100,
    )
    z_max: bpy.props.FloatProperty(
        name="Z Max", default=1e6, soft_min=-100, soft_max=100,
    )
    hidden_alpha: bpy.props.FloatProperty(
        name="Hidden Opacity",
        default=0.0,
        min=0.0,
        max=1.0,
    )
    show_arrows: bpy.props.BoolProperty(
        name="Show Arrows",
        default=True,
    )
    show_flow_labels: bpy.props.BoolProperty(
        name="Show Flow Labels",
        default=True,
    )
    show_edges: bpy.props.BoolProperty(
        name="Edges",
        default=True,
        update=_on_view_mode_change,
    )
    show_faces: bpy.props.BoolProperty(
        name="Faces",
        default=False,
        update=_on_view_mode_change,
    )


class FLOWFIRE_OT_show_all(bpy.types.Operator):
    bl_idname = "flowfire.show_all"
    bl_label = "Show All"

    def execute(self, context):
        props = context.scene.flowfire_props
        props.z_min = -1e6
        props.z_max = 1e6
        return {"FINISHED"}


class FLOWFIRE_PT_LayerPanel(bpy.types.Panel):
    bl_label = "Layer Visibility"
    bl_idname = "FLOWFIRE_PT_layers"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "FlowFire"

    def draw(self, context):
        layout = self.layout
        props = context.scene.flowfire_props

        row = layout.row(align=True)
        row.prop(props, "show_edges", toggle=True)
        row.prop(props, "show_faces", toggle=True)
        layout.operator("flowfire.show_all")
        layout.prop(props, "z_min", slider=True)
        layout.prop(props, "z_max", slider=True)
        layout.prop(props, "hidden_alpha", slider=True)

        layout.separator()
        if props.show_edges:
            layout.prop(props, "show_arrows")
            layout.prop(props, "show_flow_labels")
        if props.show_faces:
            layout.label(text="Red = clockwise (F>0), Blue = counter-clockwise (F<0)")

        if sim_unique_layers:
            layout.label(text=f"Layers: {len(sim_unique_layers)} "
                         f"(z = {sim_unique_layers[0]:.1f} .. {sim_unique_layers[-1]:.1f})")


def setup_render_scene(args, d):
    """Set up camera, lighting, and render settings for paper-quality output."""
    scene = bpy.context.scene

    # Compute bounding box of the complex
    all_pts = d["edge_verts"].reshape(-1, 3)
    bbox_min = all_pts.min(axis=0)
    bbox_max = all_pts.max(axis=0)
    center = (bbox_min + bbox_max) / 2
    extent = np.linalg.norm(bbox_max - bbox_min)

    # Camera placement via spherical coordinates
    elev_rad = math.radians(args.camera_elev)
    azim_rad = math.radians(args.camera_azim)
    dist = extent * args.camera_distance

    cam_x = center[0] + dist * math.cos(elev_rad) * math.cos(azim_rad)
    cam_y = center[1] + dist * math.cos(elev_rad) * math.sin(azim_rad)
    cam_z = center[2] + dist * math.sin(elev_rad)

    cam_data = bpy.data.cameras.new("RenderCam")
    cam_data.lens = 50
    cam_obj = bpy.data.objects.new("RenderCam", cam_data)
    bpy.context.collection.objects.link(cam_obj)
    cam_obj.location = (cam_x, cam_y, cam_z)

    # Point camera at center using Track To constraint
    track = cam_obj.constraints.new(type="TRACK_TO")
    empty = bpy.data.objects.new("CamTarget", None)
    empty.location = tuple(center)
    bpy.context.collection.objects.link(empty)
    track.target = empty
    track.track_axis = "TRACK_NEGATIVE_Z"
    track.up_axis = "UP_Y"
    scene.camera = cam_obj

    # 3-point lighting
    def add_light(name, light_type, energy, location, size=2.0):
        data = bpy.data.lights.new(name, type=light_type)
        data.energy = energy
        if light_type == "AREA":
            data.size = size
        obj = bpy.data.objects.new(name, data)
        obj.location = location
        bpy.context.collection.objects.link(obj)
        return obj

    # Scale light power by extent² (inverse-square compensation) and
    # light size ∝ extent (soft shadows grow with scene). Baseline values
    # are calibrated for extent ≈ 10 (cubic --size 5-6).
    ref_extent = 10.0
    power_scale = max((extent / ref_extent) ** 2, 0.25)
    size_scale = max(extent / ref_extent, 0.5)

    key_offset = np.array([extent * 0.6, -extent * 0.4, extent * 0.8])
    add_light("KeyLight", "AREA", 1500 * power_scale,
              tuple(center + key_offset), size=3.0 * size_scale)

    fill_offset = np.array([-extent * 0.5, extent * 0.3, extent * 0.3])
    add_light("FillLight", "AREA", 600 * power_scale,
              tuple(center + fill_offset), size=4.0 * size_scale)

    rim_offset = np.array([-extent * 0.2, extent * 0.6, -extent * 0.3])
    add_light("RimLight", "AREA", 900 * power_scale,
              tuple(center + rim_offset), size=2.0 * size_scale)

    # Render engine: Cycles
    scene.render.engine = "CYCLES"
    scene.cycles.samples = args.render_samples
    scene.cycles.use_denoising = True
    # Rendered-viewport quality (separate from final render): without
    # this, preview samples default to ~1 and the viewport looks
    # extremely pixelated in Rendered shading mode.
    scene.cycles.preview_samples = 32
    if hasattr(scene.cycles, "use_preview_denoising"):
        scene.cycles.use_preview_denoising = True
    if hasattr(scene.cycles, "preview_denoiser"):
        try:
            scene.cycles.preview_denoiser = "OPENIMAGEDENOISE"
        except Exception:
            pass
    if hasattr(scene.cycles, "preview_adaptive_threshold"):
        scene.cycles.preview_adaptive_threshold = 0.1

    # Try to use GPU (Metal on macOS, CUDA/OptiX on Linux)
    prefs = bpy.context.preferences.addons.get("cycles")
    if prefs:
        cprefs = prefs.preferences
        for dev_type in ("METAL", "OPTIX", "CUDA"):
            try:
                cprefs.compute_device_type = dev_type
                cprefs.get_devices()
                for dev in cprefs.devices:
                    dev.use = True
                scene.cycles.device = "GPU"
                print(f"  Render device: {dev_type}")
                break
            except Exception:
                continue

    # Resolution
    try:
        w, h = args.render_res.split("x")
        scene.render.resolution_x = int(w)
        scene.render.resolution_y = int(h)
    except ValueError:
        scene.render.resolution_x = 1920
        scene.render.resolution_y = 1080
    scene.render.resolution_percentage = 100

    # Background
    if args.transparent_bg:
        scene.render.film_transparent = True
        scene.render.image_settings.color_mode = "RGBA"
    else:
        # Subtle vertical gradient backdrop (ray direction Z drives a
        # muted warm-grey → soft-white ramp). Reads as considered without
        # distracting from the lattice.
        world = bpy.data.worlds.get("World") or bpy.data.worlds.new("World")
        scene.world = world
        world.use_nodes = True
        wnodes = world.node_tree.nodes
        wlinks = world.node_tree.links
        wnodes.clear()

        tex_coord = wnodes.new("ShaderNodeTexCoord")
        tex_coord.location = (-900, 0)
        sep = wnodes.new("ShaderNodeSeparateXYZ")
        sep.location = (-700, 0)
        map_range = wnodes.new("ShaderNodeMapRange")
        map_range.location = (-500, 0)
        map_range.inputs["From Min"].default_value = -0.4
        map_range.inputs["From Max"].default_value = 0.6
        ramp = wnodes.new("ShaderNodeValToRGB")
        ramp.location = (-300, 0)
        ramp.color_ramp.elements[0].color = (0.48, 0.50, 0.54, 1.0)
        ramp.color_ramp.elements[1].color = (0.88, 0.90, 0.93, 1.0)
        bg_node = wnodes.new("ShaderNodeBackground")
        bg_node.location = (0, 0)
        bg_node.inputs["Strength"].default_value = 1.2
        world_output = wnodes.new("ShaderNodeOutputWorld")
        world_output.location = (200, 0)

        wlinks.new(tex_coord.outputs["Normal"], sep.inputs["Vector"])
        wlinks.new(sep.outputs["Z"], map_range.inputs["Value"])
        wlinks.new(map_range.outputs["Result"], ramp.inputs["Fac"])
        wlinks.new(ramp.outputs["Color"], bg_node.inputs["Color"])
        wlinks.new(bg_node.outputs["Background"], world_output.inputs["Surface"])

    # Remove default objects (cube, light, camera) if they exist
    for name in ("Cube", "Light", "Camera"):
        obj = bpy.data.objects.get(name)
        if obj:
            bpy.data.objects.remove(obj, do_unlink=True)


def render_single_frame(args):
    """Render one frame and exit."""
    scene = bpy.context.scene
    scene.frame_set(args.render_frame)
    output = args.output or "render.png"
    scene.render.filepath = str(Path(output).resolve())
    scene.render.image_settings.file_format = "PNG"
    print(f"  Rendering frame {args.render_frame} -> {output}")
    bpy.ops.render.render(write_still=True)
    print(f"  Done: {output}")


def render_animation(args):
    """Render all frames as MP4 and exit."""
    scene = bpy.context.scene
    output = args.output or "render.mp4"
    scene.render.filepath = str(Path(output).resolve())
    scene.render.image_settings.file_format = "FFMPEG"
    scene.render.ffmpeg.format = "MPEG4"
    scene.render.ffmpeg.codec = "H264"
    scene.render.ffmpeg.constant_rate_factor = "MEDIUM"
    scene.render.ffmpeg.audio_codec = "NONE"
    print(f"  Rendering animation -> {output}")
    bpy.ops.render.render(animation=True)
    print(f"  Done: {output}")


def main():
    global sim_d, sim_config, sim_unique_layers, sim_layer_indices
    global sim_B, sim_face_centroids, sim_face_z_layers, sim_face_euler
    global _edge_obj, _face_obj, _glyph_obj, _update_mesh_fn

    args = parse_args()
    rng = np.random.default_rng(args.seed) if args.shuffle else None
    print(f"\nArgs: size={args.size}, init={args.init}, initial={args.initial}, "
          f"shuffle={args.shuffle}, seed={args.seed}")

    # Register UI classes
    bpy.utils.register_class(FlowFireProperties)
    bpy.utils.register_class(FLOWFIRE_OT_show_all)
    bpy.utils.register_class(FLOWFIRE_PT_LayerPanel)
    bpy.types.Scene.flowfire_props = bpy.props.PointerProperty(
        type=FlowFireProperties)

    # Build complex
    print("Building complex...")
    d = build_complex(args)
    sim_d = d
    print(f"  {d["num_edges"]} edges, {d["num_faces"]} faces")
    print(f"  Degree range: [{d["degrees"].min()}, {d["degrees"].max()}]")

    # Compute layer info
    sim_unique_layers = sorted(set(sim_z_layers))
    print(f"  {len(sim_unique_layers)} z-layers (z = {sim_unique_layers[0]:.1f} .. {sim_unique_layers[-1]:.1f})")

    # Set slider values to match actual data
    props = bpy.context.scene.flowfire_props
    z_lo, z_hi = sim_unique_layers[0], sim_unique_layers[-1]
    props.z_min = z_lo - 0.5
    props.z_max = z_hi + 0.5

    # Initial config
    sim_config = make_initial_config(args, d)
    print(f"  |flow| = {np.sum(np.abs(sim_config))}")

    # Prefire
    if args.prefire != 0:
        max_steps = 10_000_000 if args.prefire == -1 else args.prefire
        print(f"  Prefiring {max_steps} steps{"(until stable)" if args.prefire == -1 else ""}...")
        if args.gpu:
            total = fire(sim_config, d, max_steps=max_steps, backend="cuda")
        else:
            total = fire_sequential(sim_config, d, max_steps=max_steps,
                                    shuffle=args.shuffle, seed=args.seed or 0)
        print(f"  Prefired {total} edges, |flow| = {np.sum(np.abs(sim_config))}")

    # Build Blender mesh
    edge_verts = d["edge_verts"]
    num_edges = d["num_edges"]

    verts = []
    einds = []
    cols = []
    for i in range(num_edges):
        v0, v1 = edge_verts[i]
        c = edge_color(int(sim_config[i]))
        verts.append(tuple(v0))
        cols.append(c)
        verts.append(tuple(v1))
        cols.append(c)
        einds.append((len(verts) - 2, len(verts) - 1))

    mesh = bpy.data.meshes.new("out_mesh")
    obj = bpy.data.objects.new("out", mesh)
    _edge_obj = obj
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    mesh.from_pydata(verts, einds, [])

    mesh.color_attributes.new(
        name="edge_color", type="FLOAT_COLOR", domain="POINT")
    rgba = np.ones((len(cols), 4), dtype=np.float32)
    rgba[:, :] = np.array(cols, dtype=np.float32)
    mesh.color_attributes["edge_color"].data.foreach_set("color", rgba.ravel())

    # Arrow attributes (edge domain). In --packet-only mode, arrows
    # are suppressed so only the traveling packet is visible.
    packet_only = args.packet_only
    use_arrows = args.arrows and not packet_only
    if use_arrows:
        mesh.attributes.new(name="arrow_pos", type="FLOAT_VECTOR", domain="EDGE")
        mesh.attributes.new(name="arrow_dir", type="FLOAT_VECTOR", domain="EDGE")
        mesh.attributes.new(name="arrow_scale", type="FLOAT", domain="EDGE")

    # Signed flow per edge (drives animated stripes in the tube shader)
    mesh.attributes.new(name="flow_signed", type="FLOAT", domain="EDGE")
    mesh.attributes["flow_signed"].data.foreach_set(
        "value", sim_config.astype(np.float32))

    mesh.update()

    # Material
    mat = bpy.data.materials.new(name="FlowFireMat")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    # Attribute inputs: palette color, arc-length param, signed flow
    attr_node = nodes.new("ShaderNodeAttribute")
    attr_node.attribute_name = "edge_color"
    attr_node.attribute_type = "GEOMETRY"
    attr_node.location = (-1400, 200)

    attr_t = nodes.new("ShaderNodeAttribute")
    attr_t.attribute_name = "tube_t"
    attr_t.attribute_type = "GEOMETRY"
    attr_t.location = (-1400, -200)

    attr_flow = nodes.new("ShaderNodeAttribute")
    attr_flow.attribute_name = "tube_flow"
    attr_flow.attribute_type = "GEOMETRY"
    attr_flow.location = (-1400, -400)

    # Frame-driven time (monotonic; per-frame step 0.05)
    time_val = nodes.new("ShaderNodeValue")
    time_val.location = (-1400, -600)
    time_val.outputs[0].default_value = 0.0
    try:
        drv = time_val.outputs[0].driver_add("default_value").driver
        drv.type = "SCRIPTED"
        drv.expression = "frame * 0.05"
    except Exception as e:
        print(f"  Could not attach #frame driver to shader time: {e}")

    # Packet: a narrow spike of emission traveling along the edge.
    # u  = fract(t - time * sign(flow) * min(|flow|, 4) * speed_factor)
    # d  = 0.5 - |u - 0.5|   (wrap-around distance from packet center at u=0)
    # pk = map_range(d, 0..sigma, 1..0, clamp)   (triangular falloff)
    # emit_str = base + amplitude * pk
    sign_flow = nodes.new("ShaderNodeMath")
    sign_flow.operation = "SIGN"
    sign_flow.location = (-1200, -300)
    links.new(attr_flow.outputs["Fac"], sign_flow.inputs[0])

    abs_flow = nodes.new("ShaderNodeMath")
    abs_flow.operation = "ABSOLUTE"
    abs_flow.location = (-1200, -450)
    links.new(attr_flow.outputs["Fac"], abs_flow.inputs[0])

    min_flow = nodes.new("ShaderNodeMath")
    min_flow.operation = "MINIMUM"
    min_flow.inputs[1].default_value = 4.0
    min_flow.location = (-1000, -450)
    links.new(abs_flow.outputs[0], min_flow.inputs[0])

    signed_mag = nodes.new("ShaderNodeMath")
    signed_mag.operation = "MULTIPLY"
    signed_mag.location = (-800, -400)
    links.new(sign_flow.outputs[0], signed_mag.inputs[0])
    links.new(min_flow.outputs[0], signed_mag.inputs[1])

    # Speed is direction-only (sign of flow); magnitude is not used so
    # all active edges travel at the same rate regardless of |flow|.
    # speed_factor 1.5 with time_val=frame*0.05 → crosses edge in ~13 frames.
    speed = nodes.new("ShaderNodeMath")
    speed.operation = "MULTIPLY"
    speed.inputs[1].default_value = 1.5
    speed.location = (-600, -400)
    links.new(sign_flow.outputs[0], speed.inputs[0])

    # scroll = time * speed
    scroll = nodes.new("ShaderNodeMath")
    scroll.operation = "MULTIPLY"
    scroll.location = (-400, -500)
    links.new(time_val.outputs[0], scroll.inputs[0])
    links.new(speed.outputs[0], scroll.inputs[1])

    # phase = t - scroll
    phase = nodes.new("ShaderNodeMath")
    phase.operation = "SUBTRACT"
    phase.location = (-200, -300)
    links.new(attr_t.outputs["Fac"], phase.inputs[0])
    links.new(scroll.outputs[0], phase.inputs[1])

    # u = fract(phase)
    u_node = nodes.new("ShaderNodeMath")
    u_node.operation = "WRAP"
    u_node.inputs[1].default_value = 1.0
    u_node.inputs[2].default_value = 0.0
    u_node.location = (0, -300)
    links.new(phase.outputs[0], u_node.inputs[0])

    # Directional comet trail: head at u=0, tail extends behind
    # (in direction opposite to motion) for trail_length of the edge.
    # Signed wrap distance: ranges [-0.5, 0.5] with 0 at head.
    #   signed = u - floor(u + 0.5)
    u_plus_half = nodes.new("ShaderNodeMath")
    u_plus_half.operation = "ADD"
    u_plus_half.inputs[1].default_value = 0.5
    u_plus_half.location = (200, -300)
    links.new(u_node.outputs[0], u_plus_half.inputs[0])

    floored = nodes.new("ShaderNodeMath")
    floored.operation = "FLOOR"
    floored.location = (400, -300)
    links.new(u_plus_half.outputs[0], floored.inputs[0])

    signed_dist = nodes.new("ShaderNodeMath")
    signed_dist.operation = "SUBTRACT"
    signed_dist.location = (600, -300)
    links.new(u_node.outputs[0], signed_dist.inputs[0])
    links.new(floored.outputs[0], signed_dist.inputs[1])

    # trail_coord = signed_dist * (-sign_flow)
    # → positive along the trail (behind head), negative ahead of head.
    neg_sign = nodes.new("ShaderNodeMath")
    neg_sign.operation = "MULTIPLY"
    neg_sign.inputs[1].default_value = -1.0
    neg_sign.location = (-1000, -300)
    links.new(sign_flow.outputs[0], neg_sign.inputs[0])

    trail_coord = nodes.new("ShaderNodeMath")
    trail_coord.operation = "MULTIPLY"
    trail_coord.location = (800, -300)
    links.new(signed_dist.outputs[0], trail_coord.inputs[0])
    links.new(neg_sign.outputs[0], trail_coord.inputs[1])

    # packet = map_range(trail_coord, 0..trail_length, 1..0, clamped)
    #   head (trail_coord = 0)            → 1
    #   tail end (trail_coord = trail_len)→ 0
    #   ahead of head (trail_coord < 0)   → clamped 0
    trail_length = 0.3
    packet = nodes.new("ShaderNodeMapRange")
    packet.location = (1000, -500)
    packet.inputs["From Min"].default_value = 0.0
    packet.inputs["From Max"].default_value = trail_length
    packet.inputs["To Min"].default_value = 1.0
    packet.inputs["To Max"].default_value = 0.0
    packet.clamp = True
    links.new(trail_coord.outputs[0], packet.inputs["Value"])

    # emission strength = 0.08 + 14.0 * packet  (dim tubes, bright packet)
    emit_str = nodes.new("ShaderNodeMath")
    emit_str.operation = "MULTIPLY_ADD"
    emit_str.inputs[1].default_value = 14.0
    emit_str.inputs[2].default_value = 0.08
    emit_str.location = (1200, -500)
    links.new(packet.outputs["Result"], emit_str.inputs[0])

    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (1000, 0)
    bsdf.inputs["Roughness"].default_value = 0.35
    bsdf.inputs["Metallic"].default_value = 0.0

    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (1300, 0)

    links.new(attr_node.outputs["Color"], bsdf.inputs["Base Color"])
    em_color = bsdf.inputs.get("Emission Color") or bsdf.inputs.get("Emission")
    em_strength = bsdf.inputs.get("Emission Strength")
    if em_color is not None:
        links.new(attr_node.outputs["Color"], em_color)

    if packet_only:
        # Packet-only: tube is invisible except where the packet glows.
        # BSDF alpha = packet * edge_color.alpha (z-hide still respected).
        alpha_mul = nodes.new("ShaderNodeMath")
        alpha_mul.operation = "MULTIPLY"
        alpha_mul.location = (1000, 100)
        links.new(packet.outputs["Result"], alpha_mul.inputs[0])
        links.new(attr_node.outputs["Alpha"], alpha_mul.inputs[1])
        links.new(alpha_mul.outputs[0], bsdf.inputs["Alpha"])
        if em_strength is not None:
            # Bump amplitude — the tube is gone, so the packet must read
            # on its own. Base dropped to 0 so tubes fully vanish.
            emit_str.inputs[1].default_value = 18.0
            emit_str.inputs[2].default_value = 0.0
            links.new(emit_str.outputs[0], em_strength)
    else:
        # Tube/arrow mode: static cylinders, no packet animation.
        links.new(attr_node.outputs["Alpha"], bsdf.inputs["Alpha"])
        if em_strength is not None:
            em_strength.default_value = 0.0
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])

    if hasattr(mat, "blend_method"):
        # Packet-only needs smooth alpha fade; tube mode keeps CLIP
        # for crisp silhouettes and z-hide.
        mat.blend_method = "BLEND" if packet_only else "CLIP"
        mat.alpha_threshold = 0.01
    if hasattr(mat, "shadow_method"):
        mat.shadow_method = "CLIP"
    obj.data.materials.append(mat)

    # Geometry nodes (edges as tubes + optional arrow cones)
    is_render = args.render_frame is not None or args.render_video
    tube_resolution = 12 if is_render else 4

    node_group = bpy.data.node_groups.new("FlowFireGeoNodes", "GeometryNodeTree")
    try:
        node_group.interface.new_socket(
            "Geometry", in_out="INPUT", socket_type="NodeSocketGeometry")
        node_group.interface.new_socket(
            "Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry")
    except AttributeError:
        node_group.inputs.new("NodeSocketGeometry", "Geometry")
        node_group.outputs.new("NodeSocketGeometry", "Geometry")

    gn = node_group.nodes
    gn_links = node_group.links

    input_node = gn.new("NodeGroupInput")
    input_node.location = (-800, 0)

    # Branch A: tubes (with captured arc-length + flow for animated shader)
    m2c = gn.new("GeometryNodeMeshToCurve")
    m2c.location = (-1400, 0)

    # Broadcast edge-domain "flow_signed" onto curve points as "tube_flow"
    store_flow = gn.new("GeometryNodeStoreNamedAttribute")
    store_flow.data_type = "FLOAT"
    store_flow.domain = "POINT"
    store_flow.location = (-1200, 0)
    store_flow.inputs["Name"].default_value = "tube_flow"

    named_flow_in = gn.new("GeometryNodeInputNamedAttribute")
    named_flow_in.data_type = "FLOAT"
    named_flow_in.location = (-1200, -200)
    named_flow_in.inputs["Name"].default_value = "flow_signed"

    # Subdivide so stripes have enough points to render smoothly.
    # (Each edge starts as a 2-point spline; Cuts=14 → 16 points.)
    subdiv = gn.new("GeometryNodeSubdivideCurve")
    subdiv.location = (-1000, 0)
    subdiv.inputs["Cuts"].default_value = 14

    # Store arc-length parameter (0..1) on the resampled curve as "tube_t"
    store_t = gn.new("GeometryNodeStoreNamedAttribute")
    store_t.data_type = "FLOAT"
    store_t.domain = "POINT"
    store_t.location = (-800, 0)
    store_t.inputs["Name"].default_value = "tube_t"

    spline_param = gn.new("GeometryNodeSplineParameter")
    spline_param.location = (-800, -200)

    circle = gn.new("GeometryNodeCurvePrimitiveCircle")
    circle.location = (-600, -200)
    circle.inputs["Resolution"].default_value = tube_resolution
    circle.inputs["Radius"].default_value = 0.03
    c2m = gn.new("GeometryNodeCurveToMesh")
    c2m.location = (-400, 0)
    set_mat = gn.new("GeometryNodeSetMaterial")
    set_mat.location = (-200, 0)
    set_mat.inputs["Material"].default_value = mat

    gn_links.new(input_node.outputs[0], m2c.inputs["Mesh"])
    gn_links.new(m2c.outputs["Curve"], store_flow.inputs["Geometry"])
    gn_links.new(named_flow_in.outputs["Attribute"], store_flow.inputs["Value"])
    gn_links.new(store_flow.outputs["Geometry"], subdiv.inputs["Curve"])
    gn_links.new(subdiv.outputs["Curve"], store_t.inputs["Geometry"])
    gn_links.new(spline_param.outputs["Factor"], store_t.inputs["Value"])
    gn_links.new(store_t.outputs["Geometry"], c2m.inputs["Curve"])
    gn_links.new(circle.outputs["Curve"], c2m.inputs["Profile Curve"])
    gn_links.new(c2m.outputs["Mesh"], set_mat.inputs["Geometry"])

    if use_arrows:
        # Branch B: arrow cones instanced on edge midpoints
        m2p = gn.new("GeometryNodeMeshToPoints")
        m2p.location = (-600, -400)
        m2p.mode = "EDGES"

        # Read arrow_pos attribute and set position
        attr_pos = gn.new("GeometryNodeInputNamedAttribute")
        attr_pos.location = (-600, -600)
        attr_pos.data_type = "FLOAT_VECTOR"
        attr_pos.inputs["Name"].default_value = "arrow_pos"

        set_pos = gn.new("GeometryNodeSetPosition")
        set_pos.location = (-400, -400)

        # Read arrow_dir and arrow_scale attributes
        attr_dir = gn.new("GeometryNodeInputNamedAttribute")
        attr_dir.location = (-400, -700)
        attr_dir.data_type = "FLOAT_VECTOR"
        attr_dir.inputs["Name"].default_value = "arrow_dir"

        attr_scale = gn.new("GeometryNodeInputNamedAttribute")
        attr_scale.location = (-400, -850)
        attr_scale.data_type = "FLOAT"
        attr_scale.inputs["Name"].default_value = "arrow_scale"

        # Align rotation to arrow_dir
        align_euler = gn.new("FunctionNodeAlignEulerToVector")
        align_euler.location = (-200, -600)
        align_euler.axis = "Z"

        # Cone mesh for arrow
        cone = gn.new("GeometryNodeMeshCone")
        cone.location = (-200, -800)
        cone.inputs["Vertices"].default_value = 8 if not is_render else 16
        cone.inputs["Radius Top"].default_value = 0.0
        cone.inputs["Radius Bottom"].default_value = 0.07
        cone.inputs["Depth"].default_value = 0.18

        # Combine scale components into vector
        combine_scale = gn.new("ShaderNodeCombineXYZ")
        combine_scale.location = (-200, -900)

        # Instance on points
        instance = gn.new("GeometryNodeInstanceOnPoints")
        instance.location = (0, -400)

        realize = gn.new("GeometryNodeRealizeInstances")
        realize.location = (200, -400)

        set_mat_arrow = gn.new("GeometryNodeSetMaterial")
        set_mat_arrow.location = (400, -400)
        set_mat_arrow.inputs["Material"].default_value = mat

        # Wire up Branch B
        gn_links.new(input_node.outputs[0], m2p.inputs["Mesh"])
        gn_links.new(m2p.outputs["Points"], set_pos.inputs["Geometry"])
        gn_links.new(attr_pos.outputs["Attribute"], set_pos.inputs["Position"])
        gn_links.new(set_pos.outputs["Geometry"], instance.inputs["Points"])
        gn_links.new(attr_dir.outputs["Attribute"], align_euler.inputs["Vector"])
        gn_links.new(align_euler.outputs["Rotation"], instance.inputs["Rotation"])
        gn_links.new(cone.outputs["Mesh"], instance.inputs["Instance"])
        gn_links.new(attr_scale.outputs["Attribute"], combine_scale.inputs["X"])
        gn_links.new(attr_scale.outputs["Attribute"], combine_scale.inputs["Y"])
        gn_links.new(attr_scale.outputs["Attribute"], combine_scale.inputs["Z"])
        gn_links.new(combine_scale.outputs["Vector"], instance.inputs["Scale"])
        gn_links.new(instance.outputs["Instances"], realize.inputs["Geometry"])
        gn_links.new(realize.outputs["Geometry"], set_mat_arrow.inputs["Geometry"])

        # Join both branches
        join = gn.new("GeometryNodeJoinGeometry")
        join.location = (600, 0)
        output_node = gn.new("NodeGroupOutput")
        output_node.location = (800, 0)

        gn_links.new(set_mat.outputs["Geometry"], join.inputs["Geometry"])
        gn_links.new(set_mat_arrow.outputs["Geometry"], join.inputs["Geometry"])
        gn_links.new(join.outputs["Geometry"], output_node.inputs[0])
    else:
        output_node = gn.new("NodeGroupOutput")
        output_node.location = (0, 0)
        gn_links.new(set_mat.outputs["Geometry"], output_node.inputs[0])

    mod = obj.modifiers.new(name="FlowFireGeoNodes", type="NODES")
    mod.node_group = node_group

    for _oname in ("out_faces", "out_faces_glyphs", "_curl_template"):
        _old = bpy.data.objects.get(_oname)
        if _old is not None:
            _olddata = _old.data
            bpy.data.objects.remove(_old, do_unlink=True)
            if _olddata and _olddata.users == 0:
                if isinstance(_olddata, bpy.types.Mesh):
                    bpy.data.meshes.remove(_olddata)
    for _mname in ("FlowFireFaceMat", "FlowFireGlyphMat"):
        _oldm = bpy.data.materials.get(_mname)
        if _oldm is not None:
            bpy.data.materials.remove(_oldm)
    _oldng = bpy.data.node_groups.get("FlowFireGlyphGeoNodes")
    if _oldng is not None:
        bpy.data.node_groups.remove(_oldng)

    sim_B = build_boundary_matrix(d)
    face_flat, face_polys, face_centroids_arr, face_normals, face_z, face_euler = build_face_mesh_data(d)
    glyph_points = face_centroids_arr + 0.012 * face_normals
    sim_face_centroids = face_centroids_arr
    sim_face_z_layers = face_z
    sim_face_euler = face_euler
    faces_viz = d.get("faces_viz", d["faces"])
    num_faces = int(faces_viz.shape[0])
    face_face_size = int(faces_viz.shape[1])

    face_mesh = bpy.data.meshes.new("out_faces_mesh")
    face_mesh.from_pydata([tuple(v) for v in face_flat], [], face_polys)
    face_mesh.color_attributes.new(
        name="face_color", type="FLOAT_COLOR", domain="POINT")
    face_rgba = np.zeros((face_flat.shape[0], 4), dtype=np.float32)
    face_mesh.color_attributes["face_color"].data.foreach_set(
        "color", face_rgba.ravel())
    face_mesh.update()
    face_obj = bpy.data.objects.new("out_faces", face_mesh)
    bpy.context.collection.objects.link(face_obj)
    _face_obj = face_obj

    mat_face = bpy.data.materials.new(name="FlowFireFaceMat")
    mat_face.use_nodes = True
    fnodes = mat_face.node_tree.nodes
    flinks = mat_face.node_tree.links
    fnodes.clear()
    fattr = fnodes.new("ShaderNodeAttribute")
    fattr.attribute_name = "face_color"
    fattr.attribute_type = "GEOMETRY"
    fattr.location = (-400, 0)
    fbsdf = fnodes.new("ShaderNodeBsdfPrincipled")
    fbsdf.location = (0, 0)
    fbsdf.inputs["Roughness"].default_value = 0.55
    fbsdf.inputs["Metallic"].default_value = 0.0
    foutput = fnodes.new("ShaderNodeOutputMaterial")
    foutput.location = (300, 0)
    flinks.new(fattr.outputs["Color"], fbsdf.inputs["Base Color"])
    flinks.new(fattr.outputs["Alpha"], fbsdf.inputs["Alpha"])
    flinks.new(fbsdf.outputs["BSDF"], foutput.inputs["Surface"])
    if hasattr(mat_face, "blend_method"):
        mat_face.blend_method = "BLEND"
    if hasattr(mat_face, "shadow_method"):
        mat_face.shadow_method = "HASHED"
    face_obj.data.materials.append(mat_face)

    curl_template = build_curl_template_object()

    glyph_mesh = bpy.data.meshes.new("out_faces_glyphs_mesh")
    glyph_mesh.from_pydata([tuple(c) for c in glyph_points], [], [])
    glyph_mesh.attributes.new(name="curl_scale", type="FLOAT_VECTOR", domain="POINT")
    glyph_mesh.attributes.new(name="curl_euler", type="FLOAT_VECTOR", domain="POINT")
    glyph_mesh.color_attributes.new(
        name="face_color", type="FLOAT_COLOR", domain="POINT")
    glyph_scale_buf = np.zeros((num_faces, 3), dtype=np.float32)
    glyph_euler_buf = face_euler.astype(np.float32)
    glyph_rgba = np.zeros((num_faces, 4), dtype=np.float32)
    glyph_mesh.attributes["curl_scale"].data.foreach_set(
        "vector", glyph_scale_buf.ravel())
    glyph_mesh.attributes["curl_euler"].data.foreach_set(
        "vector", glyph_euler_buf.ravel())
    glyph_mesh.color_attributes["face_color"].data.foreach_set(
        "color", glyph_rgba.ravel())
    glyph_mesh.update()
    glyph_obj = bpy.data.objects.new("out_faces_glyphs", glyph_mesh)
    bpy.context.collection.objects.link(glyph_obj)
    _glyph_obj = glyph_obj

    mat_glyph = bpy.data.materials.new(name="FlowFireGlyphMat")
    mat_glyph.use_nodes = True
    gmnodes = mat_glyph.node_tree.nodes
    gmlinks = mat_glyph.node_tree.links
    gmnodes.clear()
    gmattr = gmnodes.new("ShaderNodeAttribute")
    gmattr.attribute_name = "face_color"
    gmattr.attribute_type = "INSTANCER"
    gmattr.location = (-600, 0)
    gmemit = gmnodes.new("ShaderNodeEmission")
    gmemit.inputs["Strength"].default_value = 2.5
    gmemit.location = (-200, 0)
    gmout = gmnodes.new("ShaderNodeOutputMaterial")
    gmout.location = (100, 0)
    gmlinks.new(gmattr.outputs["Color"], gmemit.inputs["Color"])
    gmlinks.new(gmemit.outputs["Emission"], gmout.inputs["Surface"])
    if hasattr(mat_glyph, "blend_method"):
        mat_glyph.blend_method = "BLEND"
    if hasattr(mat_glyph, "shadow_method"):
        mat_glyph.shadow_method = "NONE"

    glyph_ng = bpy.data.node_groups.new("FlowFireGlyphGeoNodes", "GeometryNodeTree")
    try:
        glyph_ng.interface.new_socket(
            "Geometry", in_out="INPUT", socket_type="NodeSocketGeometry")
        glyph_ng.interface.new_socket(
            "Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry")
    except AttributeError:
        glyph_ng.inputs.new("NodeSocketGeometry", "Geometry")
        glyph_ng.outputs.new("NodeSocketGeometry", "Geometry")
    ggn = glyph_ng.nodes
    ggl = glyph_ng.links
    g_in = ggn.new("NodeGroupInput")
    g_in.location = (-800, 0)
    g_out = ggn.new("NodeGroupOutput")
    g_out.location = (800, 0)
    g_obj = ggn.new("GeometryNodeObjectInfo")
    g_obj.location = (-600, -200)
    g_obj.inputs["Object"].default_value = curl_template
    g_scale = ggn.new("GeometryNodeInputNamedAttribute")
    g_scale.data_type = "FLOAT_VECTOR"
    g_scale.location = (-600, -400)
    g_scale.inputs["Name"].default_value = "curl_scale"
    g_eulr = ggn.new("GeometryNodeInputNamedAttribute")
    g_eulr.data_type = "FLOAT_VECTOR"
    g_eulr.location = (-600, -600)
    g_eulr.inputs["Name"].default_value = "curl_euler"
    g_inst = ggn.new("GeometryNodeInstanceOnPoints")
    g_inst.location = (-200, 0)
    g_realize = ggn.new("GeometryNodeRealizeInstances")
    g_realize.location = (200, 0)
    g_setmat = ggn.new("GeometryNodeSetMaterial")
    g_setmat.location = (500, 0)
    g_setmat.inputs["Material"].default_value = mat_glyph
    ggl.new(g_in.outputs[0], g_inst.inputs["Points"])
    ggl.new(g_obj.outputs["Geometry"], g_inst.inputs["Instance"])
    ggl.new(g_scale.outputs["Attribute"], g_inst.inputs["Scale"])
    ggl.new(g_eulr.outputs["Attribute"], g_inst.inputs["Rotation"])
    ggl.new(g_inst.outputs["Instances"], g_realize.inputs["Geometry"])
    ggl.new(g_realize.outputs["Geometry"], g_setmat.inputs["Geometry"])
    ggl.new(g_setmat.outputs["Geometry"], g_out.inputs[0])

    g_mod = glyph_obj.modifiers.new(name="FlowFireGlyphs", type="NODES")
    g_mod.node_group = glyph_ng

    face_attr_ref = face_mesh.color_attributes["face_color"].data
    glyph_fc_ref = glyph_mesh.color_attributes["face_color"].data
    glyph_scale_ref = glyph_mesh.attributes["curl_scale"].data

    # Store refs for animation
    mesh_ref = mesh
    attr_ref = mesh.color_attributes["edge_color"].data

    order = np.arange(num_edges, dtype=np.int32)

    # Precompute per-edge z values as numpy array for fast range check
    z_per_edge = np.array(sim_z_layers, dtype=np.float64)

    # Per-edge endpoints and midpoints (shared by arrows and labels)
    v0s = edge_verts[:, 0]
    v1s = edge_verts[:, 1]
    edge_midpoints = ((v0s + v1s) * 0.5).astype(np.float32)

    # Precompute arrow direction vectors (unit vectors along each edge)
    if use_arrows:
        raw_dirs = (v1s - v0s).astype(np.float32)
        norms = np.linalg.norm(raw_dirs, axis=1, keepdims=True).clip(min=1e-8)
        unit_dirs = raw_dirs / norms
        arrow_pos_buf = np.zeros((num_edges, 3), dtype=np.float32)
        arrow_dir_buf = np.zeros((num_edges, 3), dtype=np.float32)
        arrow_scale_buf = np.zeros(num_edges, dtype=np.float32)

    # Flow-value labels: pool of FONT curve objects, one per edge
    use_flow_labels = args.flow_labels
    global sim_label_objs

    # Clean up labels from a previous run. Blender 5.1's outliner
    # segfaults when a sub-collection holds many hundreds of objects
    # (BKE_view_layer_base_find crash), so we link labels directly into
    # scene.collection and scan by name prefix for cleanup. We still
    # tear down any lingering "FlowLabels" collection from earlier runs.
    for old_obj in [o for o in bpy.data.objects
                    if o.name.startswith("flow_label_")]:
        old_data = old_obj.data
        bpy.data.objects.remove(old_obj, do_unlink=True)
        if old_data and old_data.users == 0:
            bpy.data.curves.remove(old_data)
    old_coll = bpy.data.collections.get("FlowLabels")
    if old_coll:
        bpy.data.collections.remove(old_coll)
    old_mat = bpy.data.materials.get("FlowLabelMat")
    if old_mat:
        bpy.data.materials.remove(old_mat)
    sim_label_objs = []

    if use_flow_labels:
        label_mat = bpy.data.materials.new(name="FlowLabelMat")
        label_mat.use_nodes = True
        lnodes = label_mat.node_tree.nodes
        llinks = label_mat.node_tree.links
        lnodes.clear()
        # Emission shader: self-lit, ignores scene lighting, reads clearly
        # in both Solid/Material-Preview viewports and Cycles renders.
        lemit = lnodes.new("ShaderNodeEmission")
        lemit.inputs["Color"].default_value = (1.0, 1.0, 1.0, 1.0)
        lemit.inputs["Strength"].default_value = 3.0
        lout = lnodes.new("ShaderNodeOutputMaterial")
        llinks.new(lemit.outputs["Emission"], lout.inputs["Surface"])

        scene_coll = bpy.context.scene.collection
        for i in range(num_edges):
            cdata = bpy.data.curves.new(f"flow_label_{i}", type="FONT")
            cdata.size = 0.18
            cdata.align_x = "CENTER"
            cdata.align_y = "CENTER"
            cdata.body = ""
            cdata.materials.append(label_mat)
            lobj = bpy.data.objects.new(f"flow_label_{i}", cdata)
            lobj.hide_viewport = True
            lobj.hide_render = True
            lobj.show_in_front = True  # draw over tubes in viewport
            scene_coll.objects.link(lobj)
            sim_label_objs.append(lobj)

        bpy.context.view_layer.update()

    # Viewport billboarding: reorient labels each redraw toward the
    # current 3D view (not scene.camera, which may not be the view).
    global _label_draw_handle
    if _label_draw_handle is not None:
        try:
            bpy.types.SpaceView3D.draw_handler_remove(
                _label_draw_handle, "WINDOW")
        except Exception:
            pass
        _label_draw_handle = None

    def _viewport_update_labels():
        try:
            rv3d = bpy.context.region_data
            if rv3d is None:
                return
            # In camera view, update_mesh already oriented toward scene.camera
            if rv3d.view_perspective == "CAMERA":
                return
            props = bpy.context.scene.flowfire_props
            if not props.show_flow_labels:
                return
            view_rot = rv3d.view_rotation
            view_dir = view_rot @ mathutils.Vector((0.0, 0.0, 1.0))  # toward viewer
            ox, oy, oz = view_dir.x * 0.08, view_dir.y * 0.08, view_dir.z * 0.08
            for i, lbl in enumerate(sim_label_objs):
                if lbl.hide_viewport:
                    continue
                lbl.rotation_mode = "QUATERNION"
                lbl.rotation_quaternion = view_rot
                mid = edge_midpoints[i]
                lbl.location = (
                    float(mid[0]) + ox,
                    float(mid[1]) + oy,
                    float(mid[2]) + oz,
                )
        except Exception as e:
            print(f"flow-label viewport update error: {e}")

    if use_flow_labels:
        _label_draw_handle = bpy.types.SpaceView3D.draw_handler_add(
            _viewport_update_labels, (), "WINDOW", "POST_PIXEL")

    # Render-time label orientation: face scene.camera. Fires before each
    # rendered frame so animation renders stay correctly billboarded.
    def _render_pre_orient_labels(scene, _depsgraph=None):
        if not use_flow_labels:
            return
        cam = scene.camera
        if cam is None:
            return
        cam_loc = np.array(cam.location, dtype=np.float32)
        for i, lbl in enumerate(sim_label_objs):
            if lbl.hide_render:
                continue
            mid = edge_midpoints[i]
            to_cam = cam_loc - mid
            n = float(np.linalg.norm(to_cam)) + 1e-8
            unit = to_cam / n
            lbl.location = (
                float(mid[0] + 0.08 * unit[0]),
                float(mid[1] + 0.08 * unit[1]),
                float(mid[2] + 0.08 * unit[2]),
            )
            lbl.rotation_mode = "QUATERNION"
            lbl.rotation_quaternion = mathutils.Vector(
                (float(to_cam[0]), float(to_cam[1]), float(to_cam[2]))
            ).to_track_quat("Z", "Y")

    if use_flow_labels:
        # Remove any stale handlers from prior runs
        for h in list(bpy.app.handlers.render_pre):
            if getattr(h, "__name__", "") == "_render_pre_orient_labels":
                bpy.app.handlers.render_pre.remove(h)
        bpy.app.handlers.render_pre.append(_render_pre_orient_labels)

    def update_mesh():
        props = bpy.context.scene.flowfire_props
        z_lo, z_hi = props.z_min, props.z_max
        h_alpha = props.hidden_alpha

        if props.show_faces:
            F = compute_face_circulation(sim_B, sim_config)
            face_rgba[:] = 0.0
            for fi in range(num_faces):
                c = face_color(int(F[fi]))
                z = float(sim_face_z_layers[fi])
                if z < z_lo or z > z_hi:
                    c = (c[0], c[1], c[2], c[3] * h_alpha)
                base = fi * face_face_size
                for k in range(face_face_size):
                    face_rgba[base + k, :] = c
            face_attr_ref.foreach_set("color", face_rgba.ravel())

            glyph_scale_buf[:] = 0.0
            glyph_rgba[:] = 0.0
            for fi in range(num_faces):
                v = int(F[fi])
                if v == 0:
                    continue
                z = float(sim_face_z_layers[fi])
                vis = h_alpha if (z < z_lo or z > z_hi) else 1.0
                mag = min(abs(v), FACE_COLOR_CAP) / FACE_COLOR_CAP
                size = (0.35 + 1.1 * mag) * vis
                sx = -size if v < 0 else size
                glyph_scale_buf[fi, 0] = sx
                glyph_scale_buf[fi, 1] = size
                glyph_scale_buf[fi, 2] = size
                gc = face_color(v)
                glyph_rgba[fi, 0] = gc[0]
                glyph_rgba[fi, 1] = gc[1]
                glyph_rgba[fi, 2] = gc[2]
                glyph_rgba[fi, 3] = gc[3] * vis
            glyph_scale_ref.foreach_set("vector", glyph_scale_buf.ravel())
            glyph_fc_ref.foreach_set("color", glyph_rgba.ravel())
            face_mesh.update()
            glyph_mesh.update()

        if props.show_edges:
            for i in range(num_edges):
                c = edge_color(int(sim_config[i]))
                if z_per_edge[i] < z_lo or z_per_edge[i] > z_hi:
                    c = (c[0], c[1], c[2], h_alpha)
                rgba[i * 2, :] = c
                rgba[i * 2 + 1, :] = c
            attr_ref.foreach_set("color", rgba.ravel())

            if use_arrows and props.show_arrows:
                flows = sim_config.astype(np.float32)
                pos_mask = flows > 0
                neg_mask = flows < 0

                arrow_pos_buf[:] = 0
                arrow_dir_buf[:] = 0
                arrow_scale_buf[:] = 0

                arrow_pos_buf[pos_mask] = v0s[pos_mask] * 0.3 + v1s[pos_mask] * 0.7
                arrow_dir_buf[pos_mask] = unit_dirs[pos_mask]
                arrow_pos_buf[neg_mask] = v1s[neg_mask] * 0.3 + v0s[neg_mask] * 0.7
                arrow_dir_buf[neg_mask] = -unit_dirs[neg_mask]
                arrow_scale_buf[pos_mask | neg_mask] = 1.0

                mesh_ref.attributes["arrow_pos"].data.foreach_set(
                    "vector", arrow_pos_buf.ravel())
                mesh_ref.attributes["arrow_dir"].data.foreach_set(
                    "vector", arrow_dir_buf.ravel())
                mesh_ref.attributes["arrow_scale"].data.foreach_set(
                    "value", arrow_scale_buf)
            elif use_arrows:
                arrow_scale_buf[:] = 0
                mesh_ref.attributes["arrow_scale"].data.foreach_set(
                    "value", arrow_scale_buf)

            if use_flow_labels:
                if props.show_flow_labels:
                    for i in range(num_edges):
                        v = int(sim_config[i])
                        lbl = sim_label_objs[i]
                        hidden_by_z = z_per_edge[i] < z_lo or z_per_edge[i] > z_hi
                        if v == 0 or hidden_by_z:
                            if not lbl.hide_viewport:
                                lbl.hide_viewport = True
                                lbl.hide_render = True
                            continue
                        if lbl.hide_viewport:
                            lbl.hide_viewport = False
                            lbl.hide_render = False
                        lbl.data.body = str(v)
                else:
                    for lbl in sim_label_objs:
                        if not lbl.hide_viewport:
                            lbl.hide_viewport = True
                            lbl.hide_render = True

            mesh_ref.attributes["flow_signed"].data.foreach_set(
                "value", sim_config.astype(np.float32))

            mesh_ref.update()

    # State for --single mode: position in current sweep
    sweep_pos = [0]

    def fire_single_edge():
        """Find and fire the next eligible edge in sweep order. Returns edge index or -1."""
        deg = d["degrees"]
        indptr = d["indptr"]
        indices = d["indices"]
        data = d["data"]
        num = len(order)
        for _ in range(num):
            i = int(order[sweep_pos[0]])
            sweep_pos[0] += 1
            if sweep_pos[0] >= num:
                sweep_pos[0] = 0
                if rng is not None:
                    rng.shuffle(order)
            if deg[i] > 0 and abs(int(sim_config[i])) >= deg[i]:
                sign = 1 if sim_config[i] > 0 else -1
                for p in range(indptr[i], indptr[i + 1]):
                    sim_config[indices[p]] -= sign * data[p]
                return i
        return -1

    prefired = args.prefire != 0

    def flow_fire(scene):
        nonlocal order, prefired
        if scene.frame_current <= 2 and not prefired:
            sim_config[:] = make_initial_config(args, d)
            sweep_pos[0] = 0
            if scene.frame_current == 1:
                update_mesh()
                return
        if scene.frame_current <= 2 and prefired:
            # After prefire, frame 1 shows the prefired state as-is
            if scene.frame_current == 1:
                update_mesh()
                return
            # Frame 2: clear the prefired flag so subsequent rewinds reset
            prefired = False

        if args.single:
            ei = fire_single_edge()
            if ei >= 0:
                total_flow = np.sum(np.abs(sim_config))
                print(f"Frame {scene.frame_current}: fired edge {ei}, |flow|={total_flow}")
            else:
                print(f"Frame {scene.frame_current}: quiescent")
        elif args.gpu:
            fired = fire_step(sim_config, d, use_cuda=True)
            if fired > 0:
                total_flow = np.sum(np.abs(sim_config))
                print(f"Frame {scene.frame_current}: fired {fired}, |flow|={total_flow}")
            else:
                print(f"Frame {scene.frame_current}: quiescent")
        else:
            if rng is not None:
                rng.shuffle(order)
            fired = fire_sequential_step(sim_config, d, order)
            if fired > 0:
                total_flow = np.sum(np.abs(sim_config))
                print(f"Frame {scene.frame_current}: fired {fired}, |flow|={total_flow}")
            else:
                print(f"Frame {scene.frame_current}: quiescent")

        update_mesh()

    bpy.app.handlers.frame_change_pre.clear()
    bpy.app.handlers.frame_change_pre.append(flow_fire)

    _update_mesh_fn = update_mesh

    # Render scene setup (camera, lights, engine) — always set up so
    # the user can manually render from Blender after adjusting z-sliders etc.
    setup_render_scene(args, d)

    update_mesh()
    _apply_view_mode()

    # Execute render and exit if requested
    if args.render_frame is not None:
        render_single_frame(args)
        return
    elif args.render_video:
        render_animation(args)
        return

    print("\033[32mReady.\033[0m Use Sidebar > FlowFire tab for layer visibility.")


if __name__ == "__main__":
    main()
