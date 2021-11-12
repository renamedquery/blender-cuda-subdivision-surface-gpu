# blender-cuda-subdivison-surface-gpu

*An experiment to see if Catmull Clark subdividing can be accelerated on a GPU using CUDA. Written by katznboyz, and adapted from the instructions on [wikipedia](https://en.wikipedia.org/wiki/Catmull%E2%80%93Clark_subdivision_surface).*

# Current Progress:

![](https://i.imgur.com/xBXRA6w.png?raw=true)

*An input mesh generated in blender (left) next to the sudivided version of the mesh using my program (right). The current version still has a few errors when calculating midpoints, so that is what the hole on the bottom left is.*

![](https://i.imgur.com/2Hgt3gv.png?raw=true)

*While open faced meshes with quad topology will not crash the program, they will not be subdivided properly. This input mesh (left) and the output mesh (right) are an example of this misbehavior.*

![](https://i.imgur.com/qn0Zjil.png?raw=true)

![](https://i.imgur.com/EVQtgq1.png?raw=true)

*The CPU program that I made took 11 seconds to subdivide a mesh that was subdivided in 2 seconds by the GPU*

Completed tasks:

- Reading from a `.obj` file to take in data (verts + faces).

- Writing to a `.obj` file to export the data (verts).

- Basic Catmull Clark subdivision algorithm.

- CPU Multithreading.

- GPU Acceleration.

- Merge by distance algorithm.

TODO:

- Read normals and texture coordinates from `.obj` files.

- Write normals, texture coordinates, and faces to `.obj` files.

- Fix broken vertices that cause holes.

- Add support for open faced meshes with quad topology.

- Make `averageCornerVertices()` work on low-poly meshes.

- Speed up `mergeByDistance()`.

# Compiling Instructions:

Compiling the CUDA file on Win64 (not completed):

`nvcc gpu-subsurf.cu -o gpu-subsurf.cuda -ccbin "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\bin\Hostx64\x64" -G -g -arch=sm_86`

Compiling the C++ file on Ubuntu 20.04:

`clang++ ./cpu-subsurf.cpp -o ./cpu-subsurf.o -fsanitize=address -fno-omit-frame-pointer -O1 -g`

# Limitations:

- Only works on quads.

- Open faced meshes will work, and will stay as quad topology; however, they will have strangely located quads, untrue to the original topology.