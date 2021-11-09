// trying to get the catmull clark subdiv method working in c++ so I can translate it to cuda

#include <iostream>
#include <math.h>
#include <string>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

using namespace std;

std::mutex threadingMutex;

struct vec3 {
    double x = 0;
    double y = 0;
    double z = 0;
    bool modified = false;
    int status = 0;
};

struct vec2 {
    double x = 0;
    double y = 0;
};

struct vertex {
    vec3 position;
    vec2 textureCoordinate;
    vec3 normal;
    int id;
    int neighboringFaceIDs[4];
    bool alreadyAveraged = false;
};

struct quadFace {
    int vertexIndex[4];
    int textureIndex[4];
    int normalIndex[4];
    vec3 midpoint;
    int midpointVertID;
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

        } else if (lineType.compare("f") == 0) {

            quadFace currentFace;

            for (int i = 1; i < lineDataSplitBySpaces.size(); i++) {
                
                std::vector<std::string> lineDataSplitBySlashes = stringSplit(lineDataSplitBySpaces[i], '/');

                // vertex_index, texture_index, normal_index
                currentFace.vertexIndex[i - 1] = std::stod(lineDataSplitBySlashes[0]) - 1;
                currentFace.textureIndex[i - 1] = 0;
                currentFace.normalIndex[i - 1] = 0;

            }

            faces.push_back(currentFace);
        }

        if (wasVert) {

            if (currentVert.id < dataCount_v) vertices.push_back(currentVert);

            // check for which part of the vert has already been written to since the verts are written before the normals verts
            // if the vert type is 1 (v) and the vert hasnt been modified on the verts array
            if (vertType == 1 && !vertices[(dataCount_v - 1)].position.modified) {

                vertices[(dataCount_v - 1)].position.x = currentVert.position.x;
                vertices[(dataCount_v - 1)].position.y = currentVert.position.y;
                vertices[(dataCount_v - 1)].position.z = currentVert.position.z;
                vertices[(dataCount_v - 1)].position.modified = true;
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

    objFile << "o EXPERIMENTAL_MESH" << endl;

    for (int i = 0; i < vertices.size(); i++) {
        
        objFile << "v " << std::to_string(vertices[i].position.x) << " " << std::to_string(vertices[i].position.y) << " " << std::to_string(vertices[i].position.z) << endl;
    }

    for (int i = 0; i < faces.size(); i++) {

        objFile << "f ";

        for (int j = 0; j < 4; j++) {

            objFile << std::to_string(faces[i].vertexIndex[j] + 1) << " ";
        }

        objFile << endl;
    }

    objFile.close();
}

void getVertById(std::vector<vertex> vertices, int id, vertex& vert) {

   vert = vertices[id];
}

void getMaxVertID(std::vector<vertex> vertices, int& max) {

    max = vertices.size();
}

// from the "original point" section of https://en.wikipedia.org/wiki/Catmull%E2%80%93Clark_subdivision_surface
void barycenter(vec3 edgeMidpoint1, vec3 edgeMidpoint2, vec3 edgeMidpoint3, vec3 edgeMidpoint4, vec3 faceMidpoint1, vec3 faceMidpoint2, vec3 faceMidpoint3, vec3 faceMidpoint4, vec3& point) {

    vec3 edgeMidpointAverage;
    vec3 faceMidpointAverage;

    edgeMidpointAverage.x = (edgeMidpoint1.x + edgeMidpoint2.x + edgeMidpoint3.x + edgeMidpoint4.x) * 4;
    edgeMidpointAverage.y = (edgeMidpoint1.y + edgeMidpoint2.y + edgeMidpoint3.y + edgeMidpoint4.y) * 4;
    edgeMidpointAverage.z = (edgeMidpoint1.z + edgeMidpoint2.z + edgeMidpoint3.z + edgeMidpoint4.z) * 4;

    faceMidpointAverage.x = (faceMidpoint1.x + faceMidpoint2.x + faceMidpoint3.x + faceMidpoint3.x) * 4;
    faceMidpointAverage.y = (faceMidpoint1.y + faceMidpoint2.y + faceMidpoint3.y + faceMidpoint3.y) * 4;
    faceMidpointAverage.z = (faceMidpoint1.z + faceMidpoint2.z + faceMidpoint3.z + faceMidpoint3.z) * 4;

    point.x = (edgeMidpointAverage.x + faceMidpointAverage.x) / 2;
    point.y = (edgeMidpointAverage.y + faceMidpointAverage.y) / 2;
    point.z = (edgeMidpointAverage.z + faceMidpointAverage.z) / 2;
}

void catmullClarkFacePointsAndEdges(std::vector<vertex>& vertices, std::vector<quadFace>& faces, std::vector<quadFace>& newFaces, int maxVertID, int i, int& completeThreads, int maxVertsAtStart) {
    
    vec3 faceAverageMiddlePoint;
    vec3 faceAverageMiddlePointNormal;
    vertex faceAverageMiddlePointVertex;
    quadFace currentSubdividedFaces[4];

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

    faces[i].midpoint = faceAverageMiddlePoint;
    
    faceAverageMiddlePointVertex.position = faceAverageMiddlePoint;
    faceAverageMiddlePointVertex.id = maxVertsAtStart + (i * 5) + 0;

    faces[i].midpointVertID = faceAverageMiddlePointVertex.id;

    for (int j = 0; j < 4; j++) currentSubdividedFaces[j].vertexIndex[0] = faceAverageMiddlePointVertex.id; // face point [0] will be the center of the subdivided face

    threadingMutex.lock();
    vertices[faceAverageMiddlePointVertex.id] = faceAverageMiddlePointVertex;
    threadingMutex.unlock();

    // edge midpoints for this face
    // the mesh will have to be combined into one later on, since this will create duplicate verts

    vec3 edgeAveragePoints[4];

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

                bool firstFacePairMatch = (
                    vertices[faces[k].vertexIndex[l]].position.x == vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.x &&
                    vertices[faces[k].vertexIndex[l]].position.y == vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.y &&
                    vertices[faces[k].vertexIndex[l]].position.z == vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.z
                );

                bool secondFacePairMatch = (
                    vertices[faces[k].vertexIndex[l]].position.x == vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.x &&
                    vertices[faces[k].vertexIndex[l]].position.y == vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.y &&
                    vertices[faces[k].vertexIndex[l]].position.z == vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.z
                );

                if (firstFacePairMatch || secondFacePairMatch) {
                    
                    matchedPoints++;
                }
            }

            if (matchedPoints > 1 && k != knownFaceID) {

                nextdoorFaceID = k;
                vertices[faces[i].vertexIndex[j]].neighboringFaceIDs[j] = k;
                break;
            }
        }

        // find the averages for the edge points

        edgeAveragePoint.x = (vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.x + vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.x) / 2;
        edgeAveragePoint.y = (vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.y + vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.y) / 2;
        edgeAveragePoint.z = (vertices[faces[knownFaceID].vertexIndex[(j + 1) % 4]].position.z + vertices[faces[knownFaceID].vertexIndex[(j + 0) % 4]].position.z) / 2;

        currentSubdividedFaces[j].vertexIndex[2] = faces[knownFaceID].vertexIndex[(j + 0) % 4];

        // find the averages for the face points

        if (nextdoorFaceID < faces.size() && nextdoorFaceID > 0) {

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

            edgePoint.id = maxVertsAtStart + (i * 5) + (j + 1);

            currentSubdividedFaces[j].vertexIndex[3] = edgePoint.id;
            currentSubdividedFaces[(j + 1) % 4].vertexIndex[1] = edgePoint.id;
        }

        neighboringFacePointAverages.x = (neighboringFacePointAverages.x + edgeAveragePoint.x) / 2;
        neighboringFacePointAverages.y = (neighboringFacePointAverages.y + edgeAveragePoint.y) / 2;
        neighboringFacePointAverages.z = (neighboringFacePointAverages.z + edgeAveragePoint.z) / 2;

        edgeAveragePoints[j] = neighboringFacePointAverages;
    }

    for (int j = 0; j < 4; j++) {

        threadingMutex.lock();
        vertices[maxVertsAtStart + (i * 5) + (j + 1)].position = edgeAveragePoints[j];
        threadingMutex.unlock();
    }

    /*if (faces[i].midpointVertID == faceAverageMiddlePointVertex.id) {

        for (int j = 0; j < 4; j++) {
            //std::cout << std::to_string(faceAverageMiddlePointVertex.id) << endl;

            vertices[faces[i].midpointVertID].position.x = (
                vertices[faces[i].midpointVertID].position.x +
                edgeAveragePoints[0].x +
                edgeAveragePoints[1].x +
                edgeAveragePoints[2].x +
                edgeAveragePoints[3].x
            ) / 5;

            vertices[faces[i].midpointVertID].position.y = (
                vertices[faces[i].midpointVertID].position.y +
                edgeAveragePoints[0].y +
                edgeAveragePoints[1].y +
                edgeAveragePoints[2].y +
                edgeAveragePoints[3].y
            ) / 5;

            vertices[faces[i].midpointVertID].position.z = (
                vertices[faces[i].midpointVertID].position.z +
                edgeAveragePoints[0].z +
                edgeAveragePoints[1].z +
                edgeAveragePoints[2].z +
                edgeAveragePoints[3].z
            ) / 5;
        }
    }*/

    for (int j = 0; j < 4; j++) {

        threadingMutex.lock();
        newFaces.push_back(currentSubdividedFaces[j]);
        threadingMutex.unlock();
    }

    threadingMutex.lock();
    completeThreads++;
    threadingMutex.unlock();
}

void catmullClarkFacePointsAndEdgesAverage(std::vector<vertex>& vertices, std::vector<quadFace>& faces, int maxVertsAtStart, int i, int& completeThreads) {

    int facesArraySize = faces.size();

    vec3 edgeMidpoints[4];

    int currentEdgeMidpointsMatches = 0;

    for (int j = 0; j < facesArraySize; j++) {

        for (int k = 0; k < 4; k++) {

            if (faces[j / 4].vertexIndex[(k + 1) % 4] == i && currentEdgeMidpointsMatches < 4) {

                vec3 currentEdgeMidpoint;
                currentEdgeMidpoint.x = ((vertices[faces[j].vertexIndex[(k + 1) % 4]].position.x + vertices[i].position.x) / 2);
                currentEdgeMidpoint.y = ((vertices[faces[j].vertexIndex[(k + 1) % 4]].position.y + vertices[i].position.y) / 2);
                currentEdgeMidpoint.z = ((vertices[faces[j].vertexIndex[(k + 1) % 4]].position.z + vertices[i].position.z) / 2);

                edgeMidpoints[currentEdgeMidpointsMatches] = currentEdgeMidpoint;

                currentEdgeMidpointsMatches++;
            }
        }
    }

    vec3 edgeMidpointsAverage;
    edgeMidpointsAverage.x = (edgeMidpoints[0].x + edgeMidpoints[1].x + edgeMidpoints[2].x + edgeMidpoints[3].x) / 4;
    edgeMidpointsAverage.y = (edgeMidpoints[0].y + edgeMidpoints[1].y + edgeMidpoints[2].y + edgeMidpoints[3].y) / 4;
    edgeMidpointsAverage.z = (edgeMidpoints[0].z + edgeMidpoints[1].z + edgeMidpoints[2].z + edgeMidpoints[3].z) / 4;

    /*if (!(
        edgeMidpointsAverage.x == 0 &&
        edgeMidpointsAverage.y == 0 &&
        edgeMidpointsAverage.z == 0
        ) &&
        !(currentEdgeMidpointsMatches < 4)
    ) {

        threadingMutex.lock();
        vertices[i].position = edgeMidpointsAverage;
        threadingMutex.unlock();
        
        std::cout << std::to_string(edgeMidpointsAverage.x) << endl;
    }*/

    threadingMutex.lock();
    completeThreads++;
    threadingMutex.unlock();
}

// adapted from the instructions at https://en.wikipedia.org/wiki/Catmull%E2%80%93Clark_subdivision_surface
// should be gpu accelerated
//__global__
void catmullClarkSubdiv(std::vector<vertex>& vertices, std::vector<quadFace>& faces, const int MAX_CORES, int maxVertsAtStart) {

    const int originalMaxVertID = maxVertsAtStart; // for finding the original non-interpolated verts

    // face points and edge points

    int completeThreads = 0;
    std::atomic<int> workInProgressThreads(0);
    int threadCountOverrunHalts = 0; // the amount of times the program has to stop spawning new threads to wait for the old ones to fall below the MAX_CORES limit

    // each thread adds 5 new face points
    // calculate the total new points

    int totalNewVertsToAllocate = faces.size() * 5;

    std::vector<quadFace> newFaces;

    // make new placeholder vertices

    for (int i = 0; i < totalNewVertsToAllocate; i++) {
        
        vertex vert;
        vertices.push_back(vert);
    }

    std::cout << "[CPU] [catmullClarkFacePointsAndEdges()] SPAWNING " << faces.size() << " THREADS" << endl;

    for (int i = 0; i < faces.size(); i++) {

        workInProgressThreads++;
        std::thread(catmullClarkFacePointsAndEdges, std::ref(vertices), std::ref(faces), std::ref(newFaces), originalMaxVertID, i, std::ref(completeThreads), maxVertsAtStart).detach();

        while (workInProgressThreads - completeThreads > MAX_CORES) {
            
            threadCountOverrunHalts++;

            if (workInProgressThreads - completeThreads <= MAX_CORES) break;
        }
    };

    std::cout << "[CPU] [catmullClarkFacePointsAndEdges()] THREAD SPAWNING IS DONE" << endl;
    std::cout << "[CPU] [catmullClarkFacePointsAndEdges()] threadCountOverrunHalts=" << std::to_string(threadCountOverrunHalts) << endl;
    std::cout << "[CPU] [catmullClarkFacePointsAndEdges()] WAITING FOR THREADS TO FINISH" << endl;

    while (true) {

        if (workInProgressThreads <= completeThreads) break;
    }

    std::cout << "[CPU] [catmullClarkFacePointsAndEdges()] ALL THREADS ARE DONE" << endl;

    std::cout << "[CPU] [catmullClarkFacePointsAndEdgesAverage()] SPAWNING " << originalMaxVertID << " THREADS" << endl;

    completeThreads = 0;
    workInProgressThreads = 0;
    threadCountOverrunHalts = 0;

    // neighboring face midpoint gathering
    for (int i = 0; i < originalMaxVertID; i++) {

        workInProgressThreads++;
        std::thread(catmullClarkFacePointsAndEdgesAverage, std::ref(vertices), std::ref(faces), originalMaxVertID, i, std::ref(completeThreads)).detach();

        if (i % 100 == 0) {

            std::cout << "[CPU] " << std::to_string(((float)i / (float)originalMaxVertID) * 100) << "% DONE" << endl;
        }

        while (workInProgressThreads - completeThreads > MAX_CORES) {
            
            threadCountOverrunHalts++;

            if (workInProgressThreads - completeThreads <= MAX_CORES) break;
        }
    }

    std::cout << "[CPU] [catmullClarkFacePointsAndEdgesAverage()] THREAD SPAWNING IS DONE" << endl;
    std::cout << "[CPU] [catmullClarkFacePointsAndEdgesAverage()] threadCountOverrunHalts=" << std::to_string(threadCountOverrunHalts) << endl;
    std::cout << "[CPU] [catmullClarkFacePointsAndEdgesAverage()] WAITING FOR THREADS TO FINISH" << endl;

    while (true) {

        if (workInProgressThreads <= completeThreads) break;
    }

    faces.clear();
    faces = newFaces;

    std::cout << "[CPU] [catmullClarkFacePointsAndEdgesAverage()] ALL THREADS ARE DONE" << endl;
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

    const int MAX_CORES = std::thread::hardware_concurrency() == 0 ? 4 : std::thread::hardware_concurrency();

    std::cout << "[CPU] USING MAX_CORES=" << std::to_string(MAX_CORES) << endl;
    
    std::string objPath = "./testMesh.obj";
    std::string objOutputPath = "./testMeshOutput.obj";
    std::vector<vertex> objVertices;
    std::vector<quadFace> objFaces;

    readObj(objPath, objVertices, objFaces);

    std::string vertCount;
    std::string faceCount;

    vertCount = std::to_string(objVertices.size());
    faceCount = std::to_string(objFaces.size());

    // debugging stuff
    std::cout << "[CPU] FINISHED PARSING \"" << objPath << "\" WITH " << vertCount << " VERTS AND " << faceCount << " FACES" << endl;

    catmullClarkSubdiv(objVertices, objFaces, MAX_CORES, objFaces.size());

    vertCount = std::to_string(objVertices.size());
    faceCount = std::to_string(objFaces.size());

    std::cout << "[CPU] FINISHED SUBDIVIDING \"" << objPath << "\" WITH " << vertCount << " VERTS AND " << faceCount << " FACES" << endl;

    writeObj(objOutputPath, objVertices, objFaces);

    //printVerts(objVertices);
    //printFaces(objFaces, objVertices);

    return 0;
}