"""Thin dict-unpacking wrappers around native fire functions."""

import numpy as np

from flowfiring._native import _native


def fire_sequential_step(config, d, order=None):
    """One sequential firing sweep. Returns fired count."""
    if order is None:
        order = np.arange(d["num_edges"], dtype=np.int32)
    return _native.fire_sequential_step(
        config, d["degrees"], d["indptr"], d["indices"], d["data"], order)


def fire_sequential(config, d, max_steps=1000, shuffle=False, seed=0):
    """Multi-step sequential firing until convergence. Returns total fired."""
    return _native.fire_sequential(
        config, d["degrees"], d["indptr"], d["indices"], d["data"],
        max_steps, shuffle, seed)


def fire_step(config, d, use_cuda=False):
    """One colored firing step. Returns fired count."""
    return _native.fire_step(
        config, d["degrees"], d["indptr"], d["indices"], d["data"],
        d["color_offsets"], d["color_edges"], use_cuda)


def fire(config, d, max_steps=1000, backend="cpu"):
    """Multi-step colored firing until convergence. Returns total fired."""
    use_cuda = (backend == "cuda")
    return _native.fire_colored(
        config, d["degrees"], d["indptr"], d["indices"], d["data"],
        d["color_offsets"], d["color_edges"], max_steps, use_cuda)
