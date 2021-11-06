# originally written by katznboyz, in november 2021

# DEPRECATED FILE, WILL BE DELETED WHEN THE REPO IS CLEANED UP!!!

from cuda import cuda, nvrtc
import numpy as np

program = '''
#include <iostream>
#include <math.h>
#include <string>
#include <iomanip>
#include <chrono>
#include <ctime> 
using namespace std;

__global__
void averageQuads(double vertices[], double arrangedVertices[], int sizeOfArray) {

    int arrangedVertsPos = 0;

    for (int i = 0; i < sizeOfArray; i++) {
        int quadChunkPosition = (i / (4 * 3)) * (4 * 3);

        // this part is hardcoded but will later be turned into a loop. sorry!
        arrangedVertices[arrangedVertsPos + 0] = (vertices[quadChunkPosition + 0] + vertices[quadChunkPosition + 3] + vertices[quadChunkPosition + 6] + vertices[quadChunkPosition + 9]) / 4;
        arrangedVertices[arrangedVertsPos + 1] = (vertices[quadChunkPosition + 1] + vertices[quadChunkPosition + 4] + vertices[quadChunkPosition + 7] + vertices[quadChunkPosition + 10]) / 4;
        arrangedVertices[arrangedVertsPos + 2] = (vertices[quadChunkPosition + 2] + vertices[quadChunkPosition + 5] + vertices[quadChunkPosition + 8] + vertices[quadChunkPosition + 11]) / 4;

        i += 12;
        arrangedVertsPos += 3;
    }
}

std::string vertToString(double vert[3]) {
    std::string vertStringified = "[";
    for (int i = 0; i < 3; i++) {
        vertStringified = vertStringified + std::to_string(vert[i]) + ",";
    }
    vertStringified = vertStringified + "]";
    return vertStringified;
}

int main (void) {

    double *verts = new double[100000000*4*3]; // [total quads pairs][points in quad][3 dimensions]
    double *averagedVerts = new double[100000000*3]; // [total interpolated points][3 dimensions]
    int sizeOfVertsArray_1 = 1000*4*3; // temporary hardcoded fix just to get this working

    // fill the vert array with ones (for testing)
    std::fill_n(verts, sizeOfVertsArray_1, 9);

    int blockSize = 256;
    int numberOfBlocks = ((sizeOfVertsArray_1 / 12) + blockSize - 1) / blockSize;

    // average the points
    auto start = std::chrono::system_clock::now();
    averageQuads<<<numberOfBlocks, blockSize>>>(verts, averagedVerts, sizeOfVertsArray_1);
    auto end = std::chrono::system_clock::now();

    std::chrono::duration<double> elapsedSeconds = end - start;

    cout << "averageQuads finished in " << elapsedSeconds.count() << "s.\n";

    delete[] verts;
    delete[] averagedVerts;

    return 0;
}
'''

err, prog = nvrtc.nvrtcCreateProgram(str.encode(program), b'gpu-subsurf.tmp.cu', 0, [], [])

opts = [b'--fmad=false', b'--gpu-architecture=compute_75']

err, = nvrtc.nvrtcCompileProgram(prog, 2, opts)

err, = cuda.cuInit(0)

err, cuDevice = cuda.cuDeviceGet(0)

err, context = cuda.cuCtxCreate(0, cuDevice)

ptx = np.char.array(ptx)

err, module = cuda.cuModuleLoadData(ptx.ctypes.data)

err, kernel = cuda.cuModuleGetFunction(module, b'averageQuads')