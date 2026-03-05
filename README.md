# Flow Firing

This repository contains work done on chip/flow firing to produce interesting visualizations.

## Available Prototypes

### Blender

The blender prototype is located in the `Prototype/Blender/viewer.blend` file. To test it out, open that file in blender. If you'd like to just view the python script used in the project without opening blender, it has been extracted and placed in the `Prototype/Blender/viewer.py` file.

To run the prototype, click on the "Scripting" tab of the editor and press the run button. This will set up the animation handler and initialize the mesh used for visualization. Then, click on the "Animation" tab of the editor and press the run button. Each time the frame changes, the handler will run a cycle of flow firing.

Note that the current prototype has a chance go exponentially grow infinitely, so you may have to kill the blender process if it begins to get out of control.

### WebGPU

There is a previous WebGPU implementation located in the `Prototype/WebGPU` folder. To run it, just open the `Prototype/WebGPU/index.html` file in a browser that supports WebGPU and has it properly enabled.

Note that this implementation is for chip firing, not flow firing, but has been included as a useful reference.
