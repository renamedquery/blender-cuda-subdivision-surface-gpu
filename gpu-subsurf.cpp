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
    double x = 0;
    double y = 0;
    double z = 0;
    bool modified = false;
};

struct vec2 {
    double x = 0;
    double y = 0;
};

struct vertex {
    vec3 position;
    vec2 textureCoordinate;
    vec3 normal;
    int id; // legacy attribute, do not use for actual ID
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
//__global__
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
                currentFace.vertexIndex[i - 1] = std::stod(lineDataSplitBySlashes[0]) - 1;
                currentFace.textureIndex[i - 1] = std::stod(lineDataSplitBySlashes[1]) - 1;
                currentFace.normalIndex[i - 1] = std::stod(lineDataSplitBySlashes[2]) - 1;

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

    objFile.close();
}

// can not be gpu accelerated - is sequental
void writeObj(std::string path, std::vector<vertex> vertices, std::vector<quadFace> faces) {

    std::ofstream objFile;
    objFile.open(path, ios::out | ios::trunc);

    objFile << "o TMP_NAME" << endl;

    for (int i = 0; i < vertices.size(); i++) {
        
        objFile << "v " << std::to_string(vertices[i].position.x) << " " << std::to_string(vertices[i].position.y) << " " << std::to_string(vertices[i].position.z) << endl;
    }

    objFile.close();
}

void getVertById(std::vector<vertex> vertices, int id, vertex& vert) {

    /*
    for (int i = 0; i < vertices.size(); i++) {
        
        if (vertices[i].id == id) {

            vert = vertices[i];
            break;
        }
    }
    */

   vert = vertices[id];
}

void getMaxVertID(std::vector<vertex> vertices, int& max) {

    /*
    for (int i = 0; i < vertices.size(); i++) {

        if (vertices[i].id > max) max = vertices[i].id;
    }
    */
   max = vertices.size();
}

// adapted from the instructions at https://en.wikipedia.org/wiki/Catmull%E2%80%93Clark_subdivision_surface
// should be gpu accelerated
//__global__
void catmullClarkSubdiv(std::vector<vertex>& vertices, std::vector<quadFace>& faces) {

    int maxVertID = 0;
    getMaxVertID(vertices, maxVertID);

    // face points and edge points

    for (int i = 0; i < faces.size(); i++) {

        vec3 faceAverageMiddlePoint;
        vec3 faceAverageMiddlePointNormal;
        vertex faceAverageMiddlePointVertex;

        // face midpoint

        faceAverageMiddlePoint.x = (
            (vertices[faces[i].vertexIndex[0]].position.x) + 
            (vertices[faces[i].vertexIndex[1]].position.x) + 
            (vertices[faces[i].vertexIndex[2]].position.x) + 
            (vertices[faces[i].vertexIndex[3]].position.x)
        ) / 4;

        faceAverageMiddlePoint.y = (
            (vertices[faces[i].vertexIndex[0]].position.y) + 
            (vertices[faces[i].vertexIndex[1]].position.y) + 
            (vertices[faces[i].vertexIndex[2]].position.y) + 
            (vertices[faces[i].vertexIndex[3]].position.y)
        ) / 4;

        faceAverageMiddlePoint.z = (
            (vertices[faces[i].vertexIndex[0]].position.z) + 
            (vertices[faces[i].vertexIndex[1]].position.z) + 
            (vertices[faces[i].vertexIndex[2]].position.z) + 
            (vertices[faces[i].vertexIndex[3]].position.z)
        ) / 4;

        getMaxVertID(vertices, maxVertID);
        
        faceAverageMiddlePointVertex.position = faceAverageMiddlePoint;
        faceAverageMiddlePointVertex.id = maxVertID;

        vertices.push_back(faceAverageMiddlePointVertex);

        // edge midpoints for this face
        // the mesh will have to be combined into one later on, since this will create duplicate verts

        for (int j = 0; j < 4; j++) {

            vec3 neighboringFacePointAverages;
            vec3 neighboringFacePointCenter;
            vec3 edgeAveragePoint;
            vertex edgePoint;

            int knownFaceID = i;
            int nextdoorFaceID = -1; // -1 so that the program will crash in the case of a mismatch

            // find neighboring face
            // search through all faces to find a face sharing points v1, v2 that exist in both the current face and the searching face
            // exclude the current face from the search, therefore the only other possible face containing both points is the desired face
            // this will be optimized later, ignore the 2323978423 nested loops

            for (int k = 0; k < faces.size(); k++) {

                int matchedPoints = 0;

                for (int l = 0; l < 4; l++) {

                    if (
                        (vertices[faces[k].vertexIndex[l]].position.x == vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.x &&
                        vertices[faces[k].vertexIndex[l]].position.y == vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.y &&
                        vertices[faces[k].vertexIndex[l]].position.z == vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.z)
                        ||
                        (vertices[faces[k].vertexIndex[l]].position.x == vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.x &&
                        vertices[faces[k].vertexIndex[l]].position.y == vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.y &&
                        vertices[faces[k].vertexIndex[l]].position.z == vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.z)

                    ) {

                        matchedPoints++;
                    }
                }

                if (matchedPoints > 1 && k != knownFaceID) {

                    nextdoorFaceID = k;
                    break;
                }
            }

            // find the averages for the edge points

            edgeAveragePoint.x = (vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.x + vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.x) / 2;
            edgeAveragePoint.y = (vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.y + vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.y) / 2;
            edgeAveragePoint.z = (vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.z + vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.z) / 2;

            // find the averages for the face points

            neighboringFacePointCenter.x = (
                (vertices[faces[nextdoorFaceID].vertexIndex[0]].position.x) + 
                (vertices[faces[nextdoorFaceID].vertexIndex[1]].position.x) + 
                (vertices[faces[nextdoorFaceID].vertexIndex[2]].position.x) + 
                (vertices[faces[nextdoorFaceID].vertexIndex[3]].position.x)
            ) / 4;

            neighboringFacePointCenter.y = (
                (vertices[faces[nextdoorFaceID].vertexIndex[0]].position.y) + 
                (vertices[faces[nextdoorFaceID].vertexIndex[1]].position.y) + 
                (vertices[faces[nextdoorFaceID].vertexIndex[2]].position.y) + 
                (vertices[faces[nextdoorFaceID].vertexIndex[3]].position.y)
            ) / 4;

            neighboringFacePointCenter.z = (
                (vertices[faces[nextdoorFaceID].vertexIndex[0]].position.z) + 
                (vertices[faces[nextdoorFaceID].vertexIndex[1]].position.z) + 
                (vertices[faces[nextdoorFaceID].vertexIndex[2]].position.z) + 
                (vertices[faces[nextdoorFaceID].vertexIndex[3]].position.z)
            ) / 4;

            neighboringFacePointAverages.x = (faceAverageMiddlePoint.x + neighboringFacePointCenter.x) / 2;
            neighboringFacePointAverages.y = (faceAverageMiddlePoint.y + neighboringFacePointCenter.y) / 2;
            neighboringFacePointAverages.z = (faceAverageMiddlePoint.z + neighboringFacePointCenter.z) / 2;


            // find the averages for the edges + face points

            edgePoint.position.x = (edgeAveragePoint.x + neighboringFacePointAverages.x) / 2;
            edgePoint.position.y = (edgeAveragePoint.y + neighboringFacePointAverages.y) / 2;
            edgePoint.position.z = (edgeAveragePoint.z + neighboringFacePointAverages.z) / 2;

            getMaxVertID(vertices, maxVertID);

            edgePoint.id = maxVertID;

            vertices.push_back(edgePoint);
        }

        // calculate the original points in their interpolated form

        //TODO
    }
}

void printVerts(std::vector<vertex> vertices){

    for (int i = 0; i < vertices.size(); i++) {

        std::cout << "[CPU] [" << std::to_string(vertices[i].id) << "] " << "V  : " << std::to_string(vertices[i].position.x) << ", " << std::to_string(vertices[i].position.y) << ", " << std::to_string(vertices[i].position.z) << endl;
        std::cout << "[CPU] [" << std::to_string(vertices[i].id) << "] " << "VN : " << std::to_string(vertices[i].normal.x) << ", " << std::to_string(vertices[i].normal.y) << ", " << std::to_string(vertices[i].normal.z) << endl;
    }
}

void printFaces(std::vector<quadFace> faces, std::vector<vertex> vertices) {

    for (int i = 0; i < faces.size(); i++) {

        // face IDs
        std::cout << "[CPU] Face Vec IDS = ";

        for (int j = 0; j < 4; j++) {

            std::cout << "[V = " << faces[i].vertexIndex[j] << "][VT = " << faces[i].textureIndex[j] << "][VN = " << faces[i].normalIndex[j] << "] ";
        }

        std::cout << endl;

        // face ID values
        std::cout << "[CPU] Vec COORDS   = ";

        for (int j = 0; j < 4; j++) {

            std::cout << "[" << vertices[faces[i].vertexIndex[j]].position.x << ", " << vertices[faces[i].vertexIndex[j]].position.y << ", " << vertices[faces[i].vertexIndex[j]].position.z << "] ";
        }

        std::cout << endl;
    }
}

int main (void) {

    std::string objPath = "./testCube.obj";
    std::string objOutputPath = "./testCubeOutput.obj";
    std::vector<vertex> objVertices;
    std::vector<quadFace> objFaces;

    readObj(objPath, objVertices, objFaces);

    std::string vertCount;
    std::string faceCount;

    vertCount = std::to_string(objVertices.size());
    faceCount = std::to_string(objFaces.size());

    // debugging stuff
    std::cout << "[CPU] FINISHED PARSING \"" << objPath << "\" WITH " << vertCount << " VERTS AND " << faceCount << " FACES" << endl;

    catmullClarkSubdiv(objVertices, objFaces);

    vertCount = std::to_string(objVertices.size());
    faceCount = std::to_string(objFaces.size());

    std::cout << "[CPU] FINISHED SUBDIVIDING \"" << objPath << "\" WITH " << vertCount << " VERTS AND " << faceCount << " FACES" << endl;

    printVerts(objVertices);
    printFaces(objFaces, objVertices);

    writeObj(objOutputPath, objVertices, objFaces);

    return 0;
}