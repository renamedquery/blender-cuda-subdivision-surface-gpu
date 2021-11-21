# blender-cuda-subdivison-surface-gpu

*A fork of [Blender 3.0.0](https://github.com/blender/blender/tree/blender-v3.0-release) with support for a CUDA accelerated Subdivision Surface modifier. Subdivison Surface algorithm is adapted from the instructions on [Wikipedia](https://en.wikipedia.org/wiki/Catmull%E2%80%93Clark_subdivision_surface). Full credit goes to the devs of Blender; I am just modifying their source code.*

# Current Progress:

![](https://i.imgur.com/gZAbyqv.png?raw=true)

*Wireframe of a mesh generated my program (left) and the input mesh (left).*

![](https://i.imgur.com/MfCeLJB.png?raw=true)

*Open faced meshes are now supported by this program. (Output on the right)*

![](https://i.imgur.com/wSqWEDn.png?raw=true)

![](https://i.imgur.com/sgEMxG7.png?raw=true)

*Currently, the modifier is a work in progress, however it has been somewhat implemented into my custom build of Blender for now. Unfortunately, Blender doesn't expose the modifiers to `bpy` so the only way that I could add one was through modifying Blender's source code.*

You can view the old CUDA file [here](https://github.com/katznboyz1/blender-cuda-subdivision-surface-gpu/blob/master/custom_source/gpu-subsurf.old.cu), and the work in progress rewrite [here](https://github.com/katznboyz1/blender-cuda-subdivision-surface-gpu/blob/master/custom_source/gpu-subsurf.cu). The modifier's source code is available [here](https://github.com/katznboyz1/blender-cuda-subdivision-surface-gpu/blob/master/source/blender/modifiers/intern/MOD_gpusubsurf.c).

Completed tasks:

- Make a UI for the modifier in Blender.

- Complete the basic front end and back end functionality of the modifier (CPU, not CUDA accelerated yet).

TODO:

- Fix the `nvcc fatal   : A single input file is required for a non-link phase when an outputfile is specified` error.

- Finish accelerating `BKE_subdiv_to_mesh_cuda()` using CUDA. 

# Compiling Instructions:

Compiling the Blender fork in this repo (currently broken):

`./make.cmd`

# Limitations:

- Only works on quads (should be fixed by using Blender's built in subdiv method with CUDA acceleration).

# Tested CUDA Capable GPUs

- RTX 3080

# Tested OSes

- Windows 10 (1909, 64bit)