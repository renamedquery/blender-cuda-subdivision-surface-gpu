temporary readme, will be improved later.

compiling the cuda file on win64: `nvcc gpu-subsurf.cu -o gpu-subsurf.cuda -ccbin "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\bin\Hostx64\x64"`

compiling the c++ file on ubuntu 20.04: `clang++ ./gpu-subsurf.cpp -o ./gpu-subsurf.o -fsanitize=address -fno-omit-frame-pointer -O1 -g`

currently only works on quads