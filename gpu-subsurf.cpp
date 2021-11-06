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
    int id; // will be 1 less than the actual face id since this starts at 0
};

struct quadFace {
    int vertexIndex[4];
    int textureIndex[4];
    int normalIndex[4];
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
void readObj(std::string path, std::vector<vertex>& vertices, std::vector<quadFace>& faces) {
    
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

            quadFace currentFace;

            for (int i = 1; i < lineDataSplitBySpaces.size(); i++) {
                
                std::vector<std::string> lineDataSplitBySlashes = stringSplit(lineDataSplitBySpaces[i], '/');

                // vertex_index, texture_index, normal_index
                currentFace.vertexIndex[i - 1] = std::stod(lineDataSplitBySlashes[0]);
                currentFace.textureIndex[i - 1] = std::stod(lineDataSplitBySlashes[1]);
                currentFace.normalIndex[i - 1] = std::stod(lineDataSplitBySlashes[2]);

            }

            faces.push_back(currentFace);
        }

        if (wasVert) {
            if (currentVert.id < dataCount_v || currentVert.id < dataCount_vn) {

                vertices.push_back(currentVert);
            }

            // check for which part of the vert has already been written to since the verts are written before the normals verts
            // if the vert type is 1 (v) and the vert hasnt been modified on the verts array
            if (vertType == 1 && !vertices[(dataCount_v - 1)].position.modified) {

                vertices[(dataCount_v - 1)].position.x = currentVert.position.x;
                vertices[(dataCount_v - 1)].position.y = currentVert.position.y;
                vertices[(dataCount_v - 1)].position.z = currentVert.position.z;
                vertices[(dataCount_v - 1)].position.modified = true;

            // if the vert type is 3 (vn) and the vert hasnt been modified on the verts array
            } else if (vertType == 3 && !vertices[(dataCount_vn - 1)].normal.modified) {

                vertices[(dataCount_vn - 1)].normal.x = currentVert.normal.x;
                vertices[(dataCount_vn - 1)].normal.y = currentVert.normal.y;
                vertices[(dataCount_vn - 1)].normal.z = currentVert.normal.z;
                vertices[(dataCount_vn - 1)].normal.modified = true;

            }

            id++;
        }
    }
}

void getVertById(std::vector<vertex> vertices, int id, vertex& vert) {

    for (int i = 0; i < vertices.size(); i++) {
        
        if (vertices[i].id == id) vert = vertices[i];
    }
}

void getMaxVertID(std::vector<vertex> vertices, int& max) {

    for (int i = 0; i < vertices.size(); i++) {

        if (vertices[i].id > max) max = vertices[i].id;
    }
}

void getEdgeAverage(vertex cornerVerts[4], vec3 averages[4], int cornerVertIndex, int cornerVertID_1, int cornerVertID_2, std::vector<vertex> vertices) {

    averages[cornerVertIndex].x = (cornerVerts[cornerVertID_1].position.x + cornerVerts[cornerVertID_2].position.x) / 2;
    averages[cornerVertIndex].y = (cornerVerts[cornerVertID_1].position.y + cornerVerts[cornerVertID_2].position.y) / 2;
    averages[cornerVertIndex].z = (cornerVerts[cornerVertID_1].position.z + cornerVerts[cornerVertID_2].position.z) / 2;

    cornerVerts[cornerVertIndex].position = averages[cornerVertIndex];

    getMaxVertID(vertices, cornerVerts[cornerVertIndex].id);
}

// adapted from https://en.wikipedia.org/wiki/Catmull%E2%80%93Clark_subdivision_surface
void catmullClarkSubdiv(std::vector<vertex>& vertices, std::vector<quadFace>& faces) {

    // calculate the middle face point averages
    std::vector<vertex> middlePointAverages;

    for (int i = 0; i < faces.size(); i++) {

        vec3 faceAverageMiddlePoint;
        vec3 faceAverageMiddlePointNormal;
        vertex faceAverageMiddlePointVertex;
        
        for (int i = 0; i < 4; i++) {

            vertex currentVert;
            getVertById(vertices, faces[i].vertexIndex[i], currentVert);

            faceAverageMiddlePoint.x += currentVert.position.x;
            faceAverageMiddlePoint.y += currentVert.position.y;
            faceAverageMiddlePoint.z += currentVert.position.z;

            faceAverageMiddlePointNormal.x += currentVert.normal.x;
            faceAverageMiddlePointNormal.y += currentVert.normal.y;
            faceAverageMiddlePointNormal.z += currentVert.normal.z;
        }

        faceAverageMiddlePointVertex.position.x = faceAverageMiddlePoint.x / 4;
        faceAverageMiddlePointVertex.position.y = faceAverageMiddlePoint.y / 4;
        faceAverageMiddlePointVertex.position.z = faceAverageMiddlePoint.z / 4;

        faceAverageMiddlePointVertex.normal.x = faceAverageMiddlePointNormal.x / 4;
        faceAverageMiddlePointVertex.normal.y = faceAverageMiddlePointNormal.y / 4;
        faceAverageMiddlePointVertex.normal.z = faceAverageMiddlePointNormal.z / 4;

        int maxVertID = 0;
        getMaxVertID(vertices, maxVertID);
        faceAverageMiddlePointVertex.id = maxVertID + 1;

        middlePointAverages.push_back(faceAverageMiddlePointVertex);
    }

    // combine middlePointAverages and vertices into one array
    vertices.insert(vertices.end(), middlePointAverages.begin(), middlePointAverages.end());

    // calculate the middle edge point averages
    std::vector<vertex> edgePointAverages;

    for (int i = 0; i < faces.size(); i++) {

        vec3 edgeAverageMiddlePoint;
        vec3 edgeAverageMiddlePointNormal;
        vertex edgeAverageMiddlePointVertex;

        // quad edge 1
        /*
            1  |  2
            -------
            3  |  4

               1
            4     2
               3
        */

        vertex cornerVerts[4];

        for (int j = 0; j < 4; j++) getVertById(vertices, faces[i].vertexIndex[j], cornerVerts[j]);

        vec3 cornerVertsAverages[4];

        getEdgeAverage(cornerVerts, cornerVertsAverages, 0, 0, 1, vertices);
        getEdgeAverage(cornerVerts, cornerVertsAverages, 1, 1, 2, vertices);
        getEdgeAverage(cornerVerts, cornerVertsAverages, 2, 2, 3, vertices);
        getEdgeAverage(cornerVerts, cornerVertsAverages, 3, 3, 0, vertices);

        // combine cornerVerts and vertices into one array
        for (int j = 0; j < 4; j++) (vertices.push_back(cornerVerts[j]));
    }
}

void printVerts(std::vector<vertex> vertices){

    for (int i = 0; i < vertices.size(); i++) {

        cout << "[CPU] [" << std::to_string(vertices[i].id) << "]" << "V:  " << std::to_string(vertices[i].position.x) << ", " << std::to_string(vertices[i].position.y) << ", " << std::to_string(vertices[i].position.z) << endl;
        cout << "[CPU] [" << std::to_string(vertices[i].id) << "]" << "VN: " << std::to_string(vertices[i].normal.x) << ", " << std::to_string(vertices[i].normal.y) << ", " << std::to_string(vertices[i].normal.z) << endl;
    }
}

void printFaces(std::vector<quadFace> faces) {

    for (int i = 0; i < faces.size(); i++) {

        cout << "[CPU] ";

        for (int j = 0; j < 4; j++) {

            cout << "[V = " << faces[i].vertexIndex[j] << "][VT = " << faces[i].textureIndex[j] << "][VN = " << faces[i].normalIndex[j] << "] ";
        }

        cout << endl;
    }
}

int main (void) {

    std::string objPath = "./testCube.obj";
    std::vector<vertex> objVertices;
    std::vector<quadFace> objFaces;

    readObj(objPath, objVertices, objFaces);

    std::string vertCount;
    std::string faceCount;

    vertCount = std::to_string(objVertices.size());
    faceCount = std::to_string(objFaces.size());

    // debugging stuff
    cout << "[CPU] FINISHED PARSING \"" << objPath << "\"WITH " << vertCount << " VERTS AND " << faceCount << " FACES" << endl;

    printFaces(objFaces);
    printVerts(objVertices);

    catmullClarkSubdiv(objVertices, objFaces);

    vertCount = std::to_string(objVertices.size());
    faceCount = std::to_string(objFaces.size());

    cout << "[CPU] FINISHED SUBDIVIDING \"" << objPath << "\"WITH " << vertCount << " VERTS AND " << faceCount << " FACES" << endl;

    return 0;
}