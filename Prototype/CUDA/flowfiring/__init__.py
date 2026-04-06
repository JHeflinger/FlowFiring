"""flowfiring — GPU-parallel higher-dimensional chip-firing."""

from flowfiring._native._native import (
    build_lattice_3d,
    build_grid_2d,
    build_grid_2d_quad,
    build_grid_3d,
    color_conflict_graph,
    has_cuda,
)
from .firing import fire, fire_step, fire_sequential, fire_sequential_step
from .configs import (
    vkey,
    make_rect_cycle,
    make_triangle_circulation,
    hollow_for_grid,
    cycle_corners_for_hollow,
)

__all__ = [
    "build_lattice_3d",
    "build_grid_2d",
    "build_grid_2d_quad",
    "build_grid_3d",
    "color_conflict_graph",
    "has_cuda",
    "fire",
    "fire_step",
    "fire_sequential",
    "fire_sequential_step",
    "vkey",
    "make_rect_cycle",
    "make_triangle_circulation",
    "hollow_for_grid",
    "cycle_corners_for_hollow",
]
