// trying to get the catmull clark subdiv method working in c++ so I can translate it to cuda

#include <iostream>
#include <math.h>
#include <string>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

using namespace std;

struct vec3 {
    double x;
    double y;
    double z;
    bool modified = false;
};

struct vec2 {
    double x;
    double y;
};

struct vertex {
    vec3 position;
    vec2 textureCoordinate;
    vec3 normal;
    int id;
};

struct face {
    int vertexIndex;
    int textureIndex;
    int normalIndex;
};

std::vector<std::string> stringSplit(std::string string, char delimiter) {

    std::vector<std::string> splitString;
    std::string currentString = "";

    for (int i = 0; i < string.length(); i++) {
        if (string[i] == delimiter) {

            splitString.push_back(currentString);
            currentString = "";
        } else {

            currentString += string[i];

            if (i + 1 == string.length()) {
                splitString.push_back(currentString);
            }
        }
    }

    return splitString;
}

// can be gpu accelerated at least a little bit since it requires many lines to be read
// for a simple cube there will be no speedup, but for a mesh with millions of verts it will be faster
// currently only reads verts, faces and edges are todo
// __global__
void readObj(std::string path, std::vector<vertex>& vertices, std::vector<face>& faces) {
    
    std::ifstream objFile(path);

    // tell the program to not count new lines
    objFile.unsetf(std::ios_base::skipws);

    std::string objFileLine;

    int dataCount_v = 0;
    int dataCount_vn = 0;
    int id = 0;

    while (getline(objFile, objFileLine)) {

        std::stringstream ss{objFileLine};
        char objFileLineChar;
        ss >> objFileLineChar;

        std::vector<std::string> lineDataSplitBySpaces = stringSplit(objFileLine, ' ');
        std::string lineType = lineDataSplitBySpaces[0];

        vertex currentVert;

        bool wasVert = false;
        int vertType = 0; // 0 = none, 1 = vert, 2 = texture coordinate, 3 = normal vert

        if (lineType.compare("v") == 0) {
            currentVert.position.x = std::stod(lineDataSplitBySpaces[1]);
            currentVert.position.y = std::stod(lineDataSplitBySpaces[2]);
            currentVert.position.z = std::stod(lineDataSplitBySpaces[3]);
            currentVert.position.modified = true;
            currentVert.id = id;

            wasVert = true;
            vertType = 1;
            dataCount_v++;

        } else if (lineType.compare("vn") == 0) {
            currentVert.normal.x = std::stod(lineDataSplitBySpaces[1]);
            currentVert.normal.y = std::stod(lineDataSplitBySpaces[2]);
            currentVert.normal.z = std::stod(lineDataSplitBySpaces[3]);
            currentVert.normal.modified = true;
            currentVert.id = id;

            wasVert = true;
            vertType = 3;
            dataCount_vn++;

        } else if (lineType.compare("f") == 0) {

        }

        if (wasVert) {
            if (currentVert.id < dataCount_v || currentVert.id < dataCount_vn) {

                vertices.push_back(currentVert);
            }

            // check for which part of the vert has already been written to since the verts are written before the normals verts
            if (vertType == 1 && !vertices[(dataCount_v - 1)].position.modified) {

                vertices[(dataCount_v - 1)].position.x = currentVert.position.x;
                vertices[(dataCount_v - 1)].position.y = currentVert.position.y;
                vertices[(dataCount_v - 1)].position.z = currentVert.position.z;
                vertices[(dataCount_v - 1)].position.modified = true;

            } else if (vertType == 3 && !vertices[(dataCount_vn - 1)].normal.modified) {

                vertices[(dataCount_vn - 1)].normal.x = currentVert.position.x;
                vertices[(dataCount_vn - 1)].normal.y = currentVert.position.y;
                vertices[(dataCount_vn - 1)].normal.z = currentVert.position.z;
                vertices[(dataCount_vn - 1)].normal.modified = true;

            }

            id++;
        }
    }
}

void printVerts(std::vector<vertex> vertices){

    for (int i = 0; i < vertices.size(); i++) {
        cout << std::to_string(vertices[i].position.x) << ", " << std::to_string(vertices[i].position.y) << ", " << std::to_string(vertices[i].position.z) << endl;
    }
}

int main (void) {

    std::string objPath = "./testCube.obj";
    std::vector<vertex> objVertices;
    std::vector<face> objFaces;

    readObj(objPath, objVertices, objFaces);

    std::string vertCount = std::to_string(objVertices.size());
    std::string faceCount = std::to_string(objFaces.size());

    // debugging stuff
    cout << "FINISHED WITH " << vertCount << " VERTS AND " << faceCount << " FACES" << endl;
    printVerts(objVertices);

    return 0;
}