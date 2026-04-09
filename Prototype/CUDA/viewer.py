"""
Blender visualization for animated flow-firing.

Usage:
    blender viewer.blend --python viewer.py
    blender viewer.blend --background --python viewer.py

    # Pass script args after '--':
    blender viewer.blend --python viewer.py -- --init quad --size 10
    blender viewer.blend --python viewer.py -- --init cubic --size 5
    blender viewer.blend --python viewer.py -- --shuffle --seed 42
"""

import sys
import argparse
from pathlib import Path

import numpy as np
import bpy  # type: ignore

# Add current working dir and env site-packages to sys path
# since Blender uses its own bundled python 
DIR = Path(__file__).parent
sys.path.insert(0, str(DIR))
sys.path.insert(0, str(DIR / ".venv/lib/python3.13/site-packages"))

from flowfiring import build_lattice_3d, build_grid_3d
from flowfiring.firing import fire_sequential_step, fire_step, fire_sequential, fire
from flowfiring.configs import make_triangle_circulation, vkey
from experiment_octa import (
    find_octahedra, pick_central, find_eulerian_circuit,
    circuit_to_config, remove_faces,
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


# Module state
sim_d = None
sim_config = None
sim_z_layers = []          # per-edge z-layer (int or float)
sim_unique_layers = []     # sorted unique layer values
sim_layer_indices = []     # per-edge index into sim_unique_layers


def parse_args():
    try:
        idx = sys.argv.index("--")
        script_args = sys.argv[idx + 1:]
    except ValueError:
        script_args = []
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", type=int, default=10)
    parser.add_argument("--init", choices=["triangle", "quad", "cubic",
                                          "hollow-face", "hollow-octa"],
                        default="quad")
    parser.add_argument("--initial", type=int, default=1000)
    parser.add_argument("--shuffle", action="store_true", default=False)
    parser.add_argument("--seed", type=int, default=None)
    parser.add_argument("--gpu", action="store_true", help="Use CUDA colored firing")
    parser.add_argument("--hollow-face", action="store_true",
                        help="Remove center XY-face (cubic only)")
    parser.add_argument("--prefire", type=int, default=0,
                        help="Fire N steps before showing (-1 = until stable)")
    return parser.parse_args(script_args)


def build_complex(args):
    """Build simplicial complex based on --init mode."""
    global sim_z_layers, sim_unique_layers, sim_layer_indices

    if args.init == "cubic":
        n = args.size
        mid = n // 2
        if args.hollow_face:
            hollow_planes = np.array([0], dtype=np.int32)  # XY plane
            hollow_coords = np.array([mid, mid, mid], dtype=np.int32)
            d = build_grid_3d(n, n, n, with_colors=args.gpu,
                              hollow_face_planes=hollow_planes,
                              hollow_face_coords=hollow_coords)
        else:
            d = build_grid_3d(n, n, n, with_colors=args.gpu)

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

        # For hollow modes, find octahedron and remove faces
        if args.init in ("hollow-face", "hollow-octa"):
            octahedra = find_octahedra(d)
            if not octahedra:
                print("WARNING: no octahedra found, need larger --size")
            else:
                octa = pick_central(octahedra, d)
                print(f"  Central octahedron at {octa['center']}")

                if args.init == "hollow-octa":
                    # Compute circuit BEFORE removing faces (needs original bnd data)
                    circuit = find_eulerian_circuit(octa, d)
                    d, removed = remove_faces(d, octa["face_indices"])
                    d["_circuit"] = circuit
                    print(f"  Removed 8 octahedron faces")
                else:  # hollow-face
                    face_to_remove = [octa["face_indices"][0]]
                    d, removed = remove_faces(d, face_to_remove)
                    d["_removed_bnd"] = removed
                    print(f"  Removed 1 triangle face")

        # Compute z-layers from vertex z-coordinates
        sim_z_layers = []
        for i in range(d["num_edges"]):
            z = round(d["edge_verts"][i, 0, 2], 2)
            sim_z_layers.append(z)

        return d


def make_initial_config(args, d):
    """Create initial flow configuration."""
    if args.init == "cubic":
        num_edges = d["num_edges"]

        if args.hollow_face and d["removed_face_edges"].shape[0] > 0:
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

        gx, gy, gz = n // 2, n // 2, n // 2
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


class FLOWFIRE_OT_show_all(bpy.types.Operator):
    bl_idname = "flowfire.show_all"
    bl_label = "Show All"

    def execute(self, context):
        props = context.scene.flowfire_props
        props.z_min = -1e6
        props.z_max = 1e6
        return {'FINISHED'}


class FLOWFIRE_PT_LayerPanel(bpy.types.Panel):
    bl_label = "Layer Visibility"
    bl_idname = "FLOWFIRE_PT_layers"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'FlowFire'

    def draw(self, context):
        layout = self.layout
        props = context.scene.flowfire_props

        layout.operator("flowfire.show_all")
        layout.prop(props, "z_min", slider=True)
        layout.prop(props, "z_max", slider=True)
        layout.prop(props, "hidden_alpha", slider=True)

        if sim_unique_layers:
            layout.label(text=f"Layers: {len(sim_unique_layers)} "
                         f"(z = {sim_unique_layers[0]:.1f} .. {sim_unique_layers[-1]:.1f})")


def main():
    global sim_d, sim_config, sim_unique_layers, sim_layer_indices

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
    print(f"  {d['num_edges']} edges, {d['num_faces']} faces")
    print(f"  Degree range: [{d['degrees'].min()}, {d['degrees'].max()}]")

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
        print(f"  Prefiring {max_steps} steps{'(until stable)' if args.prefire == -1 else ''}...")
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
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    mesh.from_pydata(verts, einds, [])

    mesh.color_attributes.new(
        name="edge_color", type='FLOAT_COLOR', domain='POINT')
    rgba = np.ones((len(cols), 4), dtype=np.float32)
    rgba[:, :] = np.array(cols, dtype=np.float32)
    mesh.color_attributes["edge_color"].data.foreach_set("color", rgba.ravel())
    mesh.update()

    # Material
    mat = bpy.data.materials.new(name="FlowFireMat")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    attr_node = nodes.new('ShaderNodeAttribute')
    attr_node.attribute_name = "edge_color"
    attr_node.attribute_type = 'GEOMETRY'
    attr_node.location = (-300, 0)

    bsdf = nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.location = (0, 0)

    output = nodes.new('ShaderNodeOutputMaterial')
    output.location = (300, 0)

    links.new(attr_node.outputs['Color'], bsdf.inputs['Base Color'])
    links.new(attr_node.outputs['Alpha'], bsdf.inputs['Alpha'])
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

    if hasattr(mat, 'blend_method'):
        mat.blend_method = 'HASHED'
    if hasattr(mat, 'shadow_method'):
        mat.shadow_method = 'HASHED'
    obj.data.materials.append(mat)

    # Geometry nodes (edges as tubes)
    node_group = bpy.data.node_groups.new("FlowFireGeoNodes", 'GeometryNodeTree')
    try:
        node_group.interface.new_socket(
            'Geometry', in_out='INPUT', socket_type='NodeSocketGeometry')
        node_group.interface.new_socket(
            'Geometry', in_out='OUTPUT', socket_type='NodeSocketGeometry')
    except AttributeError:
        node_group.inputs.new('NodeSocketGeometry', 'Geometry')
        node_group.outputs.new('NodeSocketGeometry', 'Geometry')

    input_node = node_group.nodes.new('NodeGroupInput')
    input_node.location = (-600, 0)
    m2c = node_group.nodes.new('GeometryNodeMeshToCurve')
    m2c.location = (-400, 0)
    circle = node_group.nodes.new('GeometryNodeCurvePrimitiveCircle')
    circle.location = (-400, -200)
    circle.inputs['Resolution'].default_value = 4
    circle.inputs['Radius'].default_value = 0.03
    c2m = node_group.nodes.new('GeometryNodeCurveToMesh')
    c2m.location = (-200, 0)
    set_mat = node_group.nodes.new('GeometryNodeSetMaterial')
    set_mat.location = (0, 0)
    set_mat.inputs['Material'].default_value = mat
    output_node = node_group.nodes.new('NodeGroupOutput')
    output_node.location = (200, 0)

    gn_links = node_group.links
    gn_links.new(input_node.outputs[0], m2c.inputs['Mesh'])
    gn_links.new(m2c.outputs['Curve'], c2m.inputs['Curve'])
    gn_links.new(circle.outputs['Curve'], c2m.inputs['Profile Curve'])
    gn_links.new(c2m.outputs['Mesh'], set_mat.inputs['Geometry'])
    gn_links.new(set_mat.outputs['Geometry'], output_node.inputs[0])

    mod = obj.modifiers.new(name="FlowFireGeoNodes", type='NODES')
    mod.node_group = node_group

    # Store refs for animation
    mesh_ref = mesh
    attr_ref = mesh.color_attributes["edge_color"].data

    order = np.arange(num_edges, dtype=np.int32)

    # Precompute per-edge z values as numpy array for fast range check
    z_per_edge = np.array(sim_z_layers, dtype=np.float64)

    def update_mesh():
        props = bpy.context.scene.flowfire_props
        z_lo, z_hi = props.z_min, props.z_max
        h_alpha = props.hidden_alpha
        for i in range(num_edges):
            c = edge_color(int(sim_config[i]))
            if z_per_edge[i] < z_lo or z_per_edge[i] > z_hi:
                c = (c[0], c[1], c[2], h_alpha)
            rgba[i * 2, :] = c
            rgba[i * 2 + 1, :] = c
        attr_ref.foreach_set("color", rgba.ravel())
        mesh_ref.update()

    def flow_fire(scene):
        nonlocal order
        if scene.frame_current <= 2:
            sim_config[:] = make_initial_config(args, d)
            if scene.frame_current == 1:
                update_mesh()
                return

        if args.gpu:
            fired = fire_step(sim_config, d, use_cuda=True)
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

    update_mesh()
    print("\033[32mReady.\033[0m Use Sidebar > FlowFire tab for layer visibility.")


if __name__ == "__main__":
    main()
