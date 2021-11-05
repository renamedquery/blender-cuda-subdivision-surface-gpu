#include <iostream>
#include <math.h>

void interpolateQuad(double centerVert[3], double vert1[3], double vert2[3], double vert3[3], double vert4[3]) {
    for (int i = 0; i < 3; i++) {
        centerVert[i] = (vert1[i] + vert2[i] + vert3[i] + vert4[i]) / 4; // calculate the midpoint/average for each axis
    }
}

int main (void) {

    double verts[1000][4][3]; // [total quads pairs][points in quad][3 dimensions]
    double averagedVerts[1000][3]; // [total interpolated points][3 dimensions]
    int sizeOfVertsArray_1 = sizeof verts / sizeof verts[0];

    // initialize the array
    for (int i = 0; i < sizeOfVertsArray_1; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 3; k++) {
                verts[i][k][k] = 1; // assign 1 to each coordinate (just for testing)
            }
        }
    }

    // average the points
    for (int i = 0; i < sizeOfVertsArray_1; i++) {
        interpolateQuad(averagedVerts[i], verts[i][0], verts[i][1], verts[i][2], verts[i][3]);
    }

    delete verts;
    delete averagedVerts;

    return 0;
}