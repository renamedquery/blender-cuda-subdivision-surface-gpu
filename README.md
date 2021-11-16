# blender-cuda-subdivison-surface-gpu

*A branch of [Blender 3.0.0](https://github.com/blender/blender/tree/blender-v3.0-release) with support for a CUDA accelerated Subdivision Surface modifier. Subdivison Surface algorithm is adapted from the instructions on [Wikipedia](https://en.wikipedia.org/wiki/Catmull%E2%80%93Clark_subdivision_surface).*

# Current Progress:

![](https://i.imgur.com/gZAbyqv.png?raw=true)

*Wireframe of a mesh generated my program (left) and the input mesh (left).*

![](https://i.imgur.com/MfCeLJB.png?raw=true)

*Open faced meshes are now supported by this program. (Output on the right)*

![](https://i.imgur.com/kQ1NVKq.png?raw=true)

![](https://i.imgur.com/m19QGxP.png?raw=true)

*Currently, the modifier is a work in progress, however it has been somewhat implemented into my custom build of Blender for now. Unfortunately, Blender doesn't expose the modifiers to `bpy` so the only way that I could add one was through modifying Blender's source code.*

![](https://i.imgur.com/Z9CokPC.png?raw=true)

![](https://i.imgur.com/pbs97J1.png?raw=true)

*The multithreaded CPU program (now removed from the repository) took 2 minutes and 46 seconds to subdivide a cube with 24578 vertices and 24576 faces, while the GPU accelerated version only took ~5.5 seconds.*

You can view the CUDA file [here](https://github.com/katznboyz1/blender-cuda-subdivision-surface-gpu/blob/master/custom_source/gpu-subsurf.cu).

Completed tasks:

- Reading from a `.obj` file to take in data (verts + faces).

- Writing to a `.obj` file to export the data (verts).

- Basic Catmull Clark subdivision algorithm.

- CPU Multithreading.

- GPU Acceleration.

- Merge by distance algorithm.

TODO:

- Add a modifier to Blender's source code that enables users to use the CUDA Subdivision Surface modifier easily.

- In the Blender modifier, make the CUDA `mergeByDistance()` function a tick box to enable or disable it for faster subdivision times. This will allow the user to use Blender's built in *"merge by distance"* modifier. The CUDA merge option will be un-ticked by default.

- Create a "hash map" (not exactly a hash, but a unique representation) of all the vertex coordinates that allows the program to efficiently locate multiple vertices that share the same coordinate in *n(1)* function calls instead of *n^2* function calls.

- Read normals and texture coordinates from `.obj` files.

- Write normals, texture coordinates, and faces to `.obj` files.

# Compiling Instructions:

Compiling the CUDA file on Win64 (not completed):

`nvcc gpu-subsurf.cu -o gpu-subsurf.cuda -ccbin "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\bin\Hostx64\x64" -G -g -arch=sm_86`

Compiling the C++ file on Ubuntu 20.04:

`clang++ ./cpu-subsurf.cpp -o ./cpu-subsurf.o -fsanitize=address -fno-omit-frame-pointer -O1 -g`

# Limitations:

- Only works on quads.

# Tested Hardware

- RTX 3080