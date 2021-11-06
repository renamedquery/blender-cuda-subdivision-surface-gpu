// using code from https://github.com/thinks/obj-io

#include <iostream>
#include <math.h>
#include <string>
#include <iomanip>
#include <chrono>
#include <ctime> 
#include <vector>
using namespace std;

struct PolygonFace {
    std::vector<std::uint16_t> indices;
};

struct Mesh {
    std::vector<float[3]> positions;
    std::vector<PolygonFace> faces;
};

__global__
void averageQuads(double vertices[], double averagedVertices[], int sizeOfArray) {

    int arrangedVertsPos = 0;

    for (int i = 0; i < sizeOfArray; i += 12) {
        int quadChunkPosition = (i / (4 * 3)) * (4 * 3);

        // this part is hardcoded but will later be turned into a loop. sorry!
        averagedVertices[arrangedVertsPos + 0] = (vertices[quadChunkPosition + 0] + vertices[quadChunkPosition + 3] + vertices[quadChunkPosition + 6] + vertices[quadChunkPosition + 9]) / 4;
        averagedVertices[arrangedVertsPos + 1] = (vertices[quadChunkPosition + 1] + vertices[quadChunkPosition + 4] + vertices[quadChunkPosition + 7] + vertices[quadChunkPosition + 10]) / 4;
        averagedVertices[arrangedVertsPos + 2] = (vertices[quadChunkPosition + 2] + vertices[quadChunkPosition + 5] + vertices[quadChunkPosition + 8] + vertices[quadChunkPosition + 11]) / 4;

        arrangedVertsPos += 3;
    }
}

__global__
void generateMesh(double vertices[], double averagedVertices[], int sizeOfArray) {

    int arrangedVertsPos = 0;

    for (int i = 0; i < sizeOfArray; i += 12) {

        // todo

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

    // 100000000 quads for stress testing
    double *verts = new double[5000*4*3]; // [total quads pairs][points in quad][3 dimensions]
    double *averagedVerts = new double[5000*3]; // [total interpolated points][3 dimensions]
    int sizeOfVertsArray_1 = 1000*4*3; // temporary hardcoded fix just to get this working

    // fill the vert array with ones (for testing)
    std::fill_n(verts, sizeOfVertsArray_1, 9);

    int blockSize = 256;
    int numberOfBlocks = ((sizeOfVertsArray_1 / 12) + blockSize - 1) / blockSize;
    
    // average the points
    auto start_1 = std::chrono::system_clock::now();
    averageQuads<<<numberOfBlocks, blockSize>>>(verts, averagedVerts, sizeOfVertsArray_1);
    auto end_1 = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsedSeconds_1 = end_1 - start_1;
    cout << "[CUDA] averageQuads finished in " << elapsedSeconds_1.count() << "s.\n";

    // turn the new points into a new mesh
    auto start_2 = std::chrono::system_clock::now();
    generateMesh<<<numberOfBlocks, blockSize>>>(verts, averagedVerts, sizeOfVertsArray_1);
    auto end_2 = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsedSeconds_2 = end_2 - start_2;
    cout << "[CUDA] generateMesh finished in " << elapsedSeconds_2.count() << "s.\n";

    delete[] verts;
    delete[] averagedVerts;

    return 0;
}