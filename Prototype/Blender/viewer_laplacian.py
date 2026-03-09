
# Higher-dimensional chip-firing per TMCF Ch.7:
# - Builds triangular faces
# - Computes combinatorial Laplacian L2 = \del_2 * \del_2^T
# - Fires eligible edges sequentially: c' = c - sign * L[:,i]

import numpy as np
import bpy

# --- Configuration ---
sim_size = 10
sim_colors = [
    (0, 0, 0, 0.0),
    (1.0, 0, 0, 1.0),
    (0, 1.0, 0, 1.0),
    (0, 0, 1.0, 1.0),
    (1.0, 1.0, 0, 1.0),
    (1.0, 0, 1.0, 1.0),
    (0, 1.0, 1.0, 1.0),
]


# --- Data structures ---
class SimEdge:
    def __init__(self, type, direction, value=0):
        self.type = type
        self.value = value
        self.direction = direction

    def color(self):
        if abs(self.value) >= len(sim_colors):
            return sim_colors[-1]
        return sim_colors[abs(self.value)]


class Metadata:
    def instantiate(self, rgba, mesh):
        self.attr = mesh.color_attributes["edge_color"].data
        self.rgba = rgba
        self.mesh = mesh


# --- Module state ---
sim_edges = []
sim_edge_verts = []   # ((x0,y0,z0), (x1,y1,z1)) per edge
sim_config = None     # np.ndarray flow values (int32)
sim_laplacian = None  # L2 matrix (int32)
sim_deg = None        # diagonal of L2 (coboundary degrees)
sim_metadata = None   # Blender mesh handles


# --- Mesh generation (from viewer.py) ---
def generate_default_edges():
    """
    Generate edges for the 3D simplicial complex.

    Even z-layers: axis-aligned grid edges with alternating offset.
    Odd z-layers: diagonal edges from grid corners to cell centers.
    Also populates sim_edges and sim_edge_verts.
    """
    edges = []
    for z in range(sim_size * 2):
        if z % 2 == 0:
            offset = 0
            if z % 4 != 0:
                offset = 0.5
            for x in range(sim_size):
                for y in range(sim_size * 2):
                    if y % 2 == 0:
                        sim_edges.append(
                            SimEdge(int(0 + (z % 4) / 2),
                                    1 if y % 4 == 0 else -1)
                        )
                        v0 = (x + offset, y / 2 + offset, z / 2 * 0.7)
                        v1 = (x + 1 + offset, y / 2 + offset, z / 2 * 0.7)
                        edges.append((v0, v1, 0))
                        sim_edge_verts.append((v0, v1))
                    else:
                        sim_edges.append(
                            SimEdge(int(2 + (z % 4) / 2),
                                    1 if x % 2 == 0 else -1)
                        )
                        v0 = (x + offset, (y - 1) / 2 + offset, z / 2 * 0.7)
                        v1 = (x + offset, (y - 1) / 2 + 1 + offset, z / 2 * 0.7)
                        edges.append((v0, v1, 0))
                        sim_edge_verts.append((v0, v1))
        else:
            for x in range(sim_size):
                for y in range(sim_size):
                    if (z - 1) % 4 == 0:
                        v_center = (x + 0.5, y + 0.5, (z + 1) / 2 * 0.7)

                        sim_edges.append(SimEdge(4, 1))
                        v0 = (x, y, (z - 1) / 2 * 0.7)
                        edges.append((v0, v_center, 0))
                        sim_edge_verts.append((v0, v_center))

                        sim_edges.append(SimEdge(5, -1))
                        v0 = (x + 1, y, (z - 1) / 2 * 0.7)
                        edges.append((v0, v_center, 0))
                        sim_edge_verts.append((v0, v_center))

                        sim_edges.append(SimEdge(6, -1))
                        v0 = (x, y + 1, (z - 1) / 2 * 0.7)
                        edges.append((v0, v_center, 0))
                        sim_edge_verts.append((v0, v_center))

                        sim_edges.append(SimEdge(7, 1))
                        v0 = (x + 1, y + 1, (z - 1) / 2 * 0.7)
                        edges.append((v0, v_center, 0))
                        sim_edge_verts.append((v0, v_center))
                    else:
                        v_center = (x + 0.5, y + 0.5, (z - 1) / 2 * 0.7)

                        sim_edges.append(SimEdge(8, -1))
                        v0 = (x, y, (z + 1) / 2 * 0.7)
                        edges.append((v0, v_center, 0))
                        sim_edge_verts.append((v0, v_center))

                        sim_edges.append(SimEdge(9, 1))
                        v0 = (x + 1, y, (z + 1) / 2 * 0.7)
                        edges.append((v0, v_center, 0))
                        sim_edge_verts.append((v0, v_center))

                        sim_edges.append(SimEdge(10, 1))
                        v0 = (x, y + 1, (z + 1) / 2 * 0.7)
                        edges.append((v0, v_center, 0))
                        sim_edge_verts.append((v0, v_center))

                        sim_edges.append(SimEdge(11, -1))
                        v0 = (x + 1, y + 1, (z + 1) / 2 * 0.7)
                        edges.append((v0, v_center, 0))
                        sim_edge_verts.append((v0, v_center))
    return edges


def convert_edges_to_vec(edges):
    """
    Convert edge list to Blender-compatible vertices with per-vertex colors.

    Each edge gets 2 unique vertices (Blender requires unique verts for
    per-edge coloring via vertex color attributes).
    """
    verts = []
    einds = []
    cols = []
    for v0, v1, rgb in edges:
        c = sim_colors[rgb]
        verts.append(v0)
        cols.append(c)
        verts.append(v1)
        cols.append(c)
        einds.append((len(verts) - 1, len(verts) - 2))
    return verts, einds, cols


# --- Simplicial Complex ---
def _round_vert(v):
    """Round vertex coordinates for reliable hashing/comparison."""
    return (round(v[0], 4), round(v[1], 4), round(v[2], 4))


def build_faces_from_geometry():
    """
    Find all triangular faces by detecting 3-cycles in the edge graph.

    Algorithm:
    1. Build vertex->edges adjacency and edge lookup dicts
    2. For each vertex, check all pairs of incident edges
    3. If the two other endpoints are connected, we have a triangle
    4. Deduplicate via frozenset of edge indices
    """
    vert_to_edges = {}  # rounded vertex -> list of (edge_index, other_vertex)
    edge_lookup = {}    # frozenset(rv0, rv1) -> edge_index

    for i, (v0, v1) in enumerate(sim_edge_verts):
        rv0 = _round_vert(v0)
        rv1 = _round_vert(v1)
        vert_to_edges.setdefault(rv0, []).append((i, rv1))
        vert_to_edges.setdefault(rv1, []).append((i, rv0))
        edge_lookup[frozenset([rv0, rv1])] = i

    faces = set()
    for v, neighbors in vert_to_edges.items():
        for a in range(len(neighbors)):
            for b in range(a + 1, len(neighbors)):
                ei_a, va = neighbors[a]
                ei_b, vb = neighbors[b]
                key = frozenset([va, vb])
                if key in edge_lookup:
                    ei_c = edge_lookup[key]
                    faces.add(frozenset([ei_a, ei_b, ei_c]))

    faces = [tuple(sorted(f)) for f in faces]
    print(f"Found {len(faces)} triangular faces")
    return faces


def build_laplacian(faces):
    """
    Build boundary operator d2 and compute L2 = d2 * d2^T.

    For each face (triangle):
    1. Find 3 unique vertices, sort lexicographically
    2. Define oriented boundary cycle v0->v1->v2->v0 (Klivans Eq. 7.1)
    3. Assign sign +/-1 to each edge based on cycle direction
    4. Compute L2 = d2 @ d2^T
    """
    global sim_laplacian, sim_deg

    num_edges = len(sim_edges)
    num_faces = len(faces)

    # Precompute rounded edge vertices
    edge_verts_rounded = [
        (_round_vert(v0), _round_vert(v1)) for v0, v1 in sim_edge_verts
    ]

    # d2 matrix: num_edges x num_faces
    boundary = np.zeros((num_edges, num_faces), dtype=np.int32)

    for f_idx, face_edges in enumerate(faces):
        # Collect 3 unique vertices of this triangle
        verts_set = set()
        for ei in face_edges:
            rv0, rv1 = edge_verts_rounded[ei]
            verts_set.add(rv0)
            verts_set.add(rv1)

        if len(verts_set) != 3:
            print(f"Warning: face {f_idx} has {len(verts_set)} vertices, skipping")
            continue

        # Sort lexicographically for canonical orientation
        tri_verts = sorted(verts_set)
        v0, v1, v2 = tri_verts

        # Oriented boundary cycle: v0->v1, v1->v2, v2->v0
        cycle_edges = [(v0, v1), (v1, v2), (v2, v0)]

        for src, dst in cycle_edges:
            key = frozenset([src, dst])
            for ei in face_edges:
                rv_a, rv_b = edge_verts_rounded[ei]
                if frozenset([rv_a, rv_b]) == key:
                    if (rv_a, rv_b) == (src, dst):
                        boundary[ei, f_idx] = 1
                    else:
                        boundary[ei, f_idx] = -1
                    break

    # L2 = d2 * d2^T
    sim_laplacian = boundary @ boundary.T
    sim_deg = np.diag(sim_laplacian).copy()

    print(f"Laplacian shape: {sim_laplacian.shape}")
    print(f"Degree range: [{sim_deg.min()}, {sim_deg.max()}]")
    print(f"L symmetric: {np.allclose(sim_laplacian, sim_laplacian.T)}")

    # Degree distribution
    unique, counts = np.unique(sim_deg, return_counts=True)
    for d, c in zip(unique, counts):
        print(f"  deg={d}: {c} edges")

    return sim_laplacian


# Simulation
def restart_simulation():
    """Reset all edge values and place initial flow at center."""
    global sim_config
    sim_config = np.zeros(len(sim_edges), dtype=np.int32)

    # Place initial flow at center edge (same index formula as viewer.py)
    center_idx = int(
        sim_size * sim_size * 2 * (sim_size / 2)
        + sim_size * sim_size * 4 * (sim_size / 2)
        + sim_size * sim_size * 4
        + sim_size * 2
    )
    if center_idx < len(sim_config):
        sim_config[center_idx] = 1000
        print(f"Initial flow: edge {center_idx} = {sim_config[center_idx]}, "
              f"deg = {sim_deg[center_idx]}")
    # sim_config[np.random.permutation(len(sim_edges))[:500]] = 1000

    for i in range(len(sim_edges)):
        sim_edges[i].value = int(sim_config[i])


def flow_fire(scene):
    """
    Each edge i fires if |c_i| >= deg(i). Firing subtracts sign * L[:,i]
    from the configuration. No confluence in higher-dimensional chip-firing.
    """
    global sim_config

    if scene.frame_current <= 2:
        restart_simulation()
        if scene.frame_current == 1:
            update_mesh()
            return

    # Sequential single-edge firing
    fired = 0
    for i in range(len(sim_edges)):
        if sim_deg[i] > 0 and abs(sim_config[i]) >= sim_deg[i]:
            sign = 1 if sim_config[i] > 0 else -1
            sim_config -= sign * sim_laplacian[:, i]
            fired += 1

    for i in range(len(sim_edges)):
        sim_edges[i].value = int(sim_config[i])

    if fired > 0:
        total_flow = np.sum(np.abs(sim_config))
        print(f"Frame {scene.frame_current}: fired {fired} edges, "
              f"|flow| = {total_flow}")
    else:
        print(f"Frame {scene.frame_current}: quiescent (no edges fired)")

    update_mesh()


def update_mesh():
    """Update Blender mesh vertex colors from simulation state."""
    for i, e in enumerate(sim_edges):
        sim_metadata.rgba[i * 2, :] = e.color()
        sim_metadata.rgba[i * 2 + 1, :] = e.color()
    sim_metadata.attr.foreach_set("color", sim_metadata.rgba.ravel())
    sim_metadata.mesh.update()


# --- Blender setup ---
def setup_blender_scene():
    """
    Create mesh object, material, geometry nodes, and initialize simulation.
    """

    global sim_metadata
    sim_metadata = Metadata()

    # Generate edges and build Blender mesh data
    print("\nGenerating mesh...")
    edges = generate_default_edges()
    verts, einds, cols = convert_edges_to_vec(edges)
    print(f"  {len(sim_edges)} edges, {len(verts)} vertices")

    # Create new mesh + object
    mesh = bpy.data.meshes.new("out_mesh")
    obj = bpy.data.objects.new("out", mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)

    mesh.from_pydata(verts, einds, [])

    # Add color attribute
    if "edge_color" not in mesh.color_attributes:
        mesh.color_attributes.new(
            name="edge_color", type='FLOAT_COLOR', domain='POINT'
        )

    rgba = np.ones((len(cols), 4), dtype=np.float32)
    rgba[:, :] = np.array(cols, dtype=np.float32)
    mesh.color_attributes["edge_color"].data.foreach_set("color", rgba.ravel())
    mesh.update()

    # Material
    mat_name = "FlowFireMat"
    mat = bpy.data.materials.new(name=mat_name)

    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    # Attribute node reads the edge_color vertex attribute
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

    # Enable transparency for EEVEE (alpha hashed handles overlapping geometry)
    mat.blend_method = 'HASHED'
    mat.shadow_method = 'HASHED'

    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)

    # Geometry nodes setup (render edges as tubes)
    gn_name = "FlowFireGeoNodes"
    node_group = bpy.data.node_groups.new(gn_name, 'GeometryNodeTree')

    # Create interface sockets (API differs between Blender versions)
    try:
        # Blender 4.0+
        node_group.interface.new_socket(
            'Geometry', in_out='INPUT', socket_type='NodeSocketGeometry'
        )
        node_group.interface.new_socket(
            'Geometry', in_out='OUTPUT', socket_type='NodeSocketGeometry'
        )
    except AttributeError:
        # Blender 3.x
        node_group.inputs.new('NodeSocketGeometry', 'Geometry')
        node_group.outputs.new('NodeSocketGeometry', 'Geometry')

    # Input -> Mesh to Curve -> Curve to Mesh (circle profile) -> Set Material -> Output
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

    # Attach modifier to object
    mod = obj.modifiers.new(name="FlowFireGeoNodes", type='NODES')
    mod.node_group = node_group

    # Initialize metadata
    rgba_state = np.ones((len(cols), 4), dtype=np.float32)
    sim_metadata.instantiate(rgba_state, mesh)

    # Build simplicial complex
    print("Building faces from geometry...")
    faces = build_faces_from_geometry()

    print("Building Laplacian L2 = d2 * d2^T...")
    build_laplacian(faces)

    # Animation handler
    bpy.app.handlers.frame_change_pre.clear()
    bpy.app.handlers.frame_change_pre.append(flow_fire)

    # Fire
    restart_simulation()
    update_mesh()

    print("\033[32mSuccessfully\033[0m finished configuring Laplacian flow firing!")


if __name__ == "__main__":
    setup_blender_scene()
