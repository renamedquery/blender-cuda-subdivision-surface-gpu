# blender-cuda-subdivison-surface-gpu

*An experiment to see if Catmull Clark subdividing can be accelerated on a GPU using CUDA. Written by katznboyz, and adapted from the instructions on [wikipedia](https://en.wikipedia.org/wiki/Catmull%E2%80%93Clark_subdivision_surface).*

# Current Progress:

![](https://i.imgur.com/9KbUzoB.png?raw=true)

*An input mesh generated in blender (left) next to the sudivided version of the mesh using my program (right). The current version still has a few errors when calculating midpoints, so that is what those holes are.*

Completed tasks:

- Reading from a `.obj` file to take in data (verts + faces).

- Writing to a `.obj` file to export the data (verts).

- Basic Catmull Clark subdivision algorithm.

- CPU Multithreading.

- Merge by distance algorithm.

TODO:

- Read normals and texture coordinates from `.obj` files.

- Write normals, texture coordinates, and faces to `.obj` files.

- Accelerate the algorithm by using CUDA.

- Fix broken vertices that cause holes.

# Compiling Instructions:

Compiling the CUDA file on Win64 (not completed):

`nvcc gpu-subsurf.cu -o gpu-subsurf.cuda -ccbin "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\bin\Hostx64\x64"`

Compiling the C++ file on Ubuntu 20.04:

`clang++ ./gpu-subsurf.cpp -o ./gpu-subsurf.o -fsanitize=address -fno-omit-frame-pointer -O3 -g`

# Limitations:

- Only works on quads.

- Open faced meshes will work, and will stay as quad topology; however, they will have strangely located quads, untrue to the original topology.