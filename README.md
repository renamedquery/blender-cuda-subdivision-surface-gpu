# blender-cuda-subdivison-surface-gpu

*An experiment to see if Catmull Clark subdividing can be accelerated on a GPU using CUDA. Written by katznboyz, and adapted from the instructions on [wikipedia](https://en.wikipedia.org/wiki/Catmull%E2%80%93Clark_subdivision_surface).*

# Current Progress:

![](https://i.imgur.com/xBXRA6w.png?raw=true)

*An input mesh generated in blender (left) next to the sudivided version of the mesh using my program (right). The current version still has a few errors when calculating midpoints, so that is what the hole on the bottom left is.*

![](https://i.imgur.com/2Hgt3gv.png?raw=true)

*While open faced meshes with quad topology will not crash the program, they will not be subdivided properly. This input mesh (left) and the output mesh (right) are an example of this misbehavior.*

![](https://i.imgur.com/PrWCvy5.png?raw=true)

![](https://i.imgur.com/pbs97J1.png?raw=true)

*The multithreaded CPU program took 2 minutes and 46 seconds to subdivide a cube with 24578 vertices and 24576 faces, while the GPU accelerated version only took ~8.5 seconds.*

Completed tasks:

- Reading from a `.obj` file to take in data (verts + faces).

- Writing to a `.obj` file to export the data (verts).

- Basic Catmull Clark subdivision algorithm.

- CPU Multithreading.

- GPU Acceleration.

- Merge by distance algorithm.

TODO:

- Investigate the feasibility of using the method described in the paper ["Efficient GPU Rendering of Subdivision Surfaces using Adaptive Quadtrees"](http://www.graphics.stanford.edu/~niessner/papers/2016/4subdiv/brainerd2016efficient.pdf).

- Read normals and texture coordinates from `.obj` files.

- Write normals, texture coordinates, and faces to `.obj` files.

- Add support for open faced meshes with quad topology.

- Make `averageCornerVertices()` work on low-poly meshes.

# Compiling Instructions:

Compiling the CUDA file on Win64 (not completed):

`nvcc gpu-subsurf.cu -o gpu-subsurf.cuda -ccbin "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\bin\Hostx64\x64" -G -g -arch=sm_86`

Compiling the C++ file on Ubuntu 20.04:

`clang++ ./cpu-subsurf.cpp -o ./cpu-subsurf.o -fsanitize=address -fno-omit-frame-pointer -O1 -g`

# Limitations:

- Only works on quads.

- Open faced meshes will work, and will stay as quad topology; however, they will have strangely located quads, untrue to the original topology.