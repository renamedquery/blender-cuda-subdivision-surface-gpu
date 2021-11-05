#include <iostream>
#include <math.h>
#include <string>
#include <iomanip>
using namespace std;

void interpolateQuad(double centerVert[3], double vert1[3], double vert2[3], double vert3[3], double vert4[3]) {
    for (int i = 0; i < 3; i++) {
        centerVert[i] = (vert1[i] + vert2[i] + vert3[i] + vert4[i]) / 4; // calculate the midpoint/average for each axis
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

    double verts[1000][4][3]; // [total quads pairs][points in quad][3 dimensions]
    double averagedVerts[1000][3]; // [total interpolated points][3 dimensions]
    int sizeOfVertsArray_1 = sizeof verts / sizeof verts[0];

    // fill the vert array with ones (for testing)
    for (int i = 0; i < sizeOfVertsArray_1; i++) {
        for (int j = 0; j < 4; j++) {
            std::fill_n(verts[i][j], 4, 1);
        }
    }

    // average the points
    for (int i = 0; i < sizeOfVertsArray_1; i++) {
        interpolateQuad(averagedVerts[i], verts[i][0], verts[i][1], verts[i][2], verts[i][3]);
    }

    // write the points to the console
    for (int i = 0; i < sizeOfVertsArray_1; i++) {
        cout << std::setprecision(15) << "QUAD #" << i << " POINTS ARE [" << vertToString(verts[i][0]) << ", " << vertToString(verts[i][1]) << ", " << vertToString(verts[i][2]) << ", " << vertToString(verts[i][3]) << "] AVERAGE IS [" << vertToString(averagedVerts[i]) << "]\n";
    }

    return 0;
}