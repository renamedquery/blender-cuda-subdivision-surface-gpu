// trying to get the catmull clark subdiv method working in c++ so I can translate it to cuda

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

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
    int edgeVertexIndex[4];
    int textureIndex[4];
    int normalIndex[4];
    vec3 midpoint;
    int midpointVertID;
    int edgeSimplificationMatches = 0;
};

__device__ long long int facesSize;
__device__ long long int verticesSize;
__device__ long long int threadID;

__device__ vertex objVertices[];
__device__ quadFace objFaces[];
__device__ vec3 faceMidpoints[];
__device__ long long int localFaceMidpointVertIDs[];
__device__ quadFace newFaces[];
__device__ vertex newVertices[]; 

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

// currently only reads verts, faces and edges are todo
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

__global__ void catmullClarkFacePointsAndEdges(int facesSize, int maxVertsAtStart) {
    
    int i = threadID;
    threadID++;

    quadFace currentSubdividedFaces[4];
    
    for (int j = 0; j < 4; j++) currentSubdividedFaces[j].vertexIndex[3] = localFaceMidpointVertIDs[i]; // face point [0] will be the center of the subdivided face

    // edge midpoints for this face
    // the mesh will have to be combined into one later on, since this will create duplicate verts

    // vertex ids for the edges

    int vertexIDs[4];

    for (int j = 0; j < 4; j++) {

        vec3 edgeAveragePoint;

        vertex edgePoint;

        int neighboringFaceIDs[4];

        int knownFaceID;

        int matchedPoints; // the amount of points per face that have been matched

        // find neighboring face
        // search through all faces to find a face sharing points v1, v2 that exist in both the current face and the searching face
        // exclude the current face from the search, therefore the only other possible face containing both points is the desired face
        // this will be optimized later, ignore the 2323978423 nested loops
        
        for (int k = 0; k < facesSize; k++) {

            for (int l = 0; l < 4; l++) {

                if (objFaces[i].vertexIndex[j] == objFaces[k].vertexIndex[l]) {
                    
                    neighboringFaceIDs[matchedPoints] = k;
                    matchedPoints++;
                }
            }
        }


        edgeAveragePoint.x = (objVertices[objFaces[knownFaceID].vertexIndex[(j + 1) % 4]].position.x + objVertices[objFaces[knownFaceID].vertexIndex[(j + 0) % 4]].position.x) / 2;
        edgeAveragePoint.y = (objVertices[objFaces[knownFaceID].vertexIndex[(j + 1) % 4]].position.y + objVertices[objFaces[knownFaceID].vertexIndex[(j + 0) % 4]].position.y) / 2;
        edgeAveragePoint.z = (objVertices[objFaces[knownFaceID].vertexIndex[(j + 1) % 4]].position.z + objVertices[objFaces[knownFaceID].vertexIndex[(j + 0) % 4]].position.z) / 2;

        currentSubdividedFaces[j].vertexIndex[1] = objFaces[knownFaceID].vertexIndex[(j + 0) % 4];

        // find the averages for the face points

        edgePoint.id = maxVertsAtStart + (i * 5) + (j + 1);

        vertexIDs[j] = edgePoint.id;

        currentSubdividedFaces[j].vertexIndex[0] = edgePoint.id;
        currentSubdividedFaces[(j + 1) % 4].vertexIndex[2] = edgePoint.id;

        objVertices[vertexIDs[j]].position = edgeAveragePoint;
        objFaces[i].edgeVertexIndex[j] = vertexIDs[j];
    }

    for (int j = 0; j < 4; j++) {

        newFaces[(threadID * 4) + j] = currentSubdividedFaces[j];
    }

    objVertices[localFaceMidpointVertIDs[i]].position = faceMidpoints[i];
}
/*
__global__
void averageCornerVertices(int facesSize, const int * const & i) {

    for (int j = 0; j < 4; j++) {

        int matchedPoints = 0;
        int neighboringFaceIDs[4];

        vec3 neighboringFaceMidpointsAverage;
        vec3 edgeMidpointsAverage;
        vec3 finalMidpointAverage;

        for (int k = 0; k < facesSize; k++) {

            for (int l = 0; l < 4; l++) {

                if (objFaces[i].vertexIndex[j] == objFaces[k].vertexIndex[l]) {

                    neighboringFaceIDs[matchedPoints] = k;

                    edgeMidpointsAverage.x += (objVertices[objFaces[i].vertexIndex[j]].position.x + objVertices[objFaces[k].vertexIndex[(l + 1) % 4]].position.x) / 2;
                    edgeMidpointsAverage.y += (objVertices[objFaces[i].vertexIndex[j]].position.y + objVertices[objFaces[k].vertexIndex[(l + 1) % 4]].position.y) / 2;
                    edgeMidpointsAverage.z += (objVertices[objFaces[i].vertexIndex[j]].position.z + objVertices[objFaces[k].vertexIndex[(l + 1) % 4]].position.z) / 2;

                    matchedPoints++;
                }
            }
        }

        for (int k = 0; k < 4; k++) {

            neighboringFaceMidpointsAverage.x += faceMidpoints[neighboringFaceIDs[k]].x;
            neighboringFaceMidpointsAverage.y += faceMidpoints[neighboringFaceIDs[k]].y;
            neighboringFaceMidpointsAverage.z += faceMidpoints[neighboringFaceIDs[k]].z;
        }

        neighboringFaceMidpointsAverage.x /= matchedPoints;
        neighboringFaceMidpointsAverage.y /= matchedPoints;
        neighboringFaceMidpointsAverage.z /= matchedPoints;

        edgeMidpointsAverage.x /= matchedPoints;
        edgeMidpointsAverage.y /= matchedPoints;
        edgeMidpointsAverage.z /= matchedPoints;

        finalMidpointAverage.x = (neighboringFaceMidpointsAverage.x + edgeMidpointsAverage.x) / 2;
        finalMidpointAverage.y = (neighboringFaceMidpointsAverage.y + edgeMidpointsAverage.y) / 2;
        finalMidpointAverage.z = (neighboringFaceMidpointsAverage.z + edgeMidpointsAverage.z) / 2;

        newVertices[objFaces[i].vertexIndex[j]].position = edgeMidpointsAverage; // find a way to get the finalMidpointAverage to work properly
    }
}
*/
/*
__global__
void mergeByDistance(std::vector<vertex>& vertices, int i, int& completeThreads, std::vector<quadFace>& faces) {

    if (faces[i].edgeSimplificationMatches < 4) {

        for (int j = 0; j < faces.size(); j++) {

            if (!(faces[j].edgeSimplificationMatches < 4)) continue;

            int matches = 0;

            for (int k = 0; k < 4; k ++) {

                if (!(faces[j].edgeSimplificationMatches < 4)) continue;

                for (int l = 0; l < 4; l++) {

                    if (!(faces[j].edgeSimplificationMatches < 4)) continue;

                    if (
                        vertices[faces[i].vertexIndex[k]].position.x == vertices[faces[j].vertexIndex[l]].position.x &&
                        vertices[faces[i].vertexIndex[k]].position.y == vertices[faces[j].vertexIndex[l]].position.y &&
                        vertices[faces[i].vertexIndex[k]].position.z == vertices[faces[j].vertexIndex[l]].position.z &&
                        faces[i].vertexIndex[k] != faces[j].vertexIndex[l]
                    ) {
                        
                        matches++;

                        threadingMutex.lock();
                        faces[j].edgeSimplificationMatches++;
                        faces[i].edgeSimplificationMatches++;
                        threadingMutex.unlock();

                        if (!(matches < 1) && vertices[faces[j].vertexIndex[l]].position.status == 0) {

                            threadingMutex.lock();
                            vertices[faces[i].vertexIndex[k]].position.status = 1;
                            faces[j].vertexIndex[l] = faces[i].vertexIndex[k];
                            threadingMutex.unlock();
                        }
                    }
                }
            }
        }
    }

    threadingMutex.lock();
    completeThreads++;
    threadingMutex.unlock();
}
*/

int main (void) {
    
    std::string objPath = "./testMesh.obj";
    std::string objOutputPath = "./testMeshOutput.obj";

    std::vector<vertex> vertices;
    std::vector<quadFace> faces;

    const int blockSize = 256;

    readObj(objPath, vertices, faces);

    int facesSize = faces.size();
    int verticesSize = vertices.size();

    long long int threadID_host = 1LL;

    vertex *objVertices_host = new vertex[verticesSize]; 
    quadFace *objFaces_host = new quadFace[facesSize]; 
    vec3 *faceMidpoints_host = new vec3[facesSize]; 
    long long int *localFaceMidpointVertIDs_host = new long long int[facesSize]; 
    quadFace *newFaces_host = new quadFace[facesSize * 4]; 
    vertex *newVertices_host = new vertex[verticesSize + (facesSize * 5)]; 

    for (int j = 0; j < verticesSize; j++) {

        objVertices_host[j] = vertices[j];
    } 

    for (int j = 0; j < facesSize; j++) {

        objFaces_host[j] = faces[j];
    }

    for (long long int j = 0; j < facesSize; j++) {

        vec3 faceAverageMiddlePoint;

        faceAverageMiddlePoint.x = (
            (objVertices[objFaces[j].vertexIndex[0]].position.x) + 
            (objVertices[objFaces[j].vertexIndex[1]].position.x) + 
            (objVertices[objFaces[j].vertexIndex[2]].position.x) + 
            (objVertices[objFaces[j].vertexIndex[3]].position.x)
        ) / 4;

        faceAverageMiddlePoint.y = (
            (objVertices[objFaces[j].vertexIndex[0]].position.y) + 
            (objVertices[objFaces[j].vertexIndex[1]].position.y) + 
            (objVertices[objFaces[j].vertexIndex[2]].position.y) + 
            (objVertices[objFaces[j].vertexIndex[3]].position.y)
        ) / 4;

        faceAverageMiddlePoint.z = (
            (objVertices[objFaces[j].vertexIndex[0]].position.z) + 
            (objVertices[objFaces[j].vertexIndex[1]].position.z) + 
            (objVertices[objFaces[j].vertexIndex[2]].position.z) + 
            (objVertices[objFaces[j].vertexIndex[3]].position.z)
        ) / 4;

        faceMidpoints_host[j] = faceAverageMiddlePoint;
        localFaceMidpointVertIDs_host[j] = (verticesSize + (j * 5) + 0);
    }

    cudaMemset(&threadID, 0, sizeof(long long int));
    cudaMemcpy((void *)threadID, &threadID_host, sizeof(long long int), cudaMemcpyHostToDevice);

    cudaMemset(&objVertices_host, 0, verticesSize * sizeof(vertex));
    cudaMemcpy(objVertices, &objVertices_host, verticesSize * sizeof(vertex), cudaMemcpyHostToDevice);

    cudaMemset(&objFaces_host, 0, facesSize * sizeof(quadFace));
    cudaMemcpy(objFaces, &objFaces_host, facesSize * sizeof(quadFace), cudaMemcpyHostToDevice);
    
    cudaMemset(&faceMidpoints_host, 0, facesSize * sizeof(vec3));
    cudaMemcpy(faceMidpoints, &faceMidpoints_host, facesSize * sizeof(vec3), cudaMemcpyHostToDevice);
    
    cudaMemset(&localFaceMidpointVertIDs_host, 0, facesSize * sizeof(long long int));
    cudaMemcpy(localFaceMidpointVertIDs, &localFaceMidpointVertIDs_host, facesSize * sizeof(long long int), cudaMemcpyHostToDevice);
    
    cudaMemset(&newFaces_host, 0, (facesSize * 4) * sizeof(vertex));
    cudaMemcpy(newFaces, &newFaces_host, (facesSize * 4) * sizeof(vertex), cudaMemcpyHostToDevice);
    
    cudaMemset(&newVertices_host, 0, (verticesSize + (facesSize * 5)) * sizeof(vertex));
    cudaMemcpy(newVertices, &newVertices_host, (verticesSize + (facesSize * 5)) * sizeof(vertex), cudaMemcpyHostToDevice);

    catmullClarkFacePointsAndEdges<<<(facesSize + blockSize - 1) / blockSize, blockSize>>>(facesSize, verticesSize);

    cudaDeviceSynchronize();

    int tmpCopyThreadID;

    cudaMemcpyFromSymbol(&threadID, tmpCopyThreadID, sizeof(int), 0, cudaMemcpyDeviceToHost);

    std::cout << tmpCopyThreadID << endl;

    facesSize *= 4;

    //averageCornerVertices<<<(facesSize + blockSize - 1) / blockSize, blockSize>>>(facesSize, *&threadID);

    cudaDeviceSynchronize();

    //std::thread(averageCornerVertices, std::ref(vertices), std::ref(newVertices), std::ref(faces), i, std::ref(completeThreads), maxVertsAtStart, std::ref(faceMidpoints), std::ref(localFaceMidpointVertIDs)).detach();

    /*
    //catmullClarkSubdiv(objVertices, objFaces, MAX_CORES, objFaces.size());
    const int originalMaxVertID = maxVertsAtStart; // for finding the original non-interpolated verts

    // face points and edge points

    int completeThreads = 0;
    int threadCountOverrunHalts = 0; // the amount of times the program has to stop spawning new threads to wait for the old ones to fall below the MAX_CORES limit

    
    std::atomic<int> workInProgressThreads(0);

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

    std::vector<vec3> faceMidpoints;
    std::vector<int> localFaceMidpointVertIDs;

    for (int j = 0; j < faces.size(); j++) {

        vec3 faceAverageMiddlePoint;

        faceAverageMiddlePoint.x = (
            (vertices[faces[j].vertexIndex[0]].position.x) + 
            (vertices[faces[j].vertexIndex[1]].position.x) + 
            (vertices[faces[j].vertexIndex[2]].position.x) + 
            (vertices[faces[j].vertexIndex[3]].position.x)
        ) / 4;

        faceAverageMiddlePoint.y = (
            (vertices[faces[j].vertexIndex[0]].position.y) + 
            (vertices[faces[j].vertexIndex[1]].position.y) + 
            (vertices[faces[j].vertexIndex[2]].position.y) + 
            (vertices[faces[j].vertexIndex[3]].position.y)
        ) / 4;

        faceAverageMiddlePoint.z = (
            (vertices[faces[j].vertexIndex[0]].position.z) + 
            (vertices[faces[j].vertexIndex[1]].position.z) + 
            (vertices[faces[j].vertexIndex[2]].position.z) + 
            (vertices[faces[j].vertexIndex[3]].position.z)
        ) / 4;

        faceMidpoints.push_back(faceAverageMiddlePoint);
        localFaceMidpointVertIDs.push_back(maxVertsAtStart + (j * 5) + 0);
    }

    for (int i = 0; i < faces.size(); i++) {

        workInProgressThreads++;
        std::thread(catmullClarkFacePointsAndEdges, std::ref(vertices), std::ref(faces), std::ref(newFaces), originalMaxVertID, i, std::ref(completeThreads), maxVertsAtStart, std::ref(faceMidpoints), std::ref(localFaceMidpointVertIDs)).detach();

        if (i % 100 == 0) {

            std::cout << "[CPU] [catmullClarkFacePointsAndEdges()] " << std::to_string(((float)i / (float)faces.size()) * 100) << "% DONE" << endl;
        }

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

    std::cout << "[CPU] [averageCornerVertices()] SPAWNING " << originalMaxVertID << " THREADS" << endl;

    completeThreads = 0;
    workInProgressThreads = 0;
    threadCountOverrunHalts = 0;

    auto newVertices = vertices;

    // neighboring face midpoint gathering
    for (int i = 0; i < faces.size(); i++) {

        workInProgressThreads++;
        std::thread(averageCornerVertices, std::ref(vertices), std::ref(newVertices), std::ref(faces), i, std::ref(completeThreads), maxVertsAtStart, std::ref(faceMidpoints), std::ref(localFaceMidpointVertIDs)).detach();

        //if (i % 100 == 0) {

            //std::cout << "[CPU] [averageCornerVertices()] " << std::to_string(((float)i / (float)faces.size()) * 100) << "% DONE" << endl;
        //}

        while (workInProgressThreads - completeThreads > MAX_CORES) {
            
            threadCountOverrunHalts++;

            if (workInProgressThreads - completeThreads <= MAX_CORES) break;
        }
    }

    //std::cout << "[CPU] [averageCornerVertices()] THREAD SPAWNING IS DONE" << endl;
    //std::cout << "[CPU] [averageCornerVertices()] threadCountOverrunHalts=" << std::to_string(threadCountOverrunHalts) << endl;
    //std::cout << "[CPU] [averageCornerVertices()] WAITING FOR THREADS TO FINISH" << endl;

    while (true) {

        if (workInProgressThreads <= completeThreads) break;
    }

    vertices = newVertices;

    //std::cout << "[CPU] [averageCornerVertices()] ALL THREADS ARE DONE" << endl;

    //std::cout << "[CPU] [mergeByDistance()] SPAWNING " << originalMaxVertID << " THREADS" << endl;

    completeThreads = 0;
    workInProgressThreads = 0;
    threadCountOverrunHalts = 0;

    faces.clear();
    faces = newFaces;

    // neighboring face midpoint gathering
    for (int i = 0; i < faces.size(); i++) {

        workInProgressThreads++;
        //std::thread(mergeByDistance, std::ref(vertices), i, std::ref(completeThreads), std::ref(faces)).detach();

        //if (i % (100 * 4) == 0) {

            //std::cout << "[CPU] [mergeByDistance()] " << std::to_string(((float)i / (float)faces.size()) * 100) << "% DONE" << endl;
        //}

        while (workInProgressThreads - completeThreads > MAX_CORES) {
            
            threadCountOverrunHalts++;

            if (workInProgressThreads - completeThreads <= MAX_CORES) break;
        }
    }

    //std::cout << "[CPU] [mergeByDistance()] THREAD SPAWNING IS DONE" << endl;
    //std::cout << "[CPU] [mergeByDistance()] threadCountOverrunHalts=" << std::to_string(threadCountOverrunHalts) << endl;
    //std::cout << "[CPU] [mergeByDistance()] WAITING FOR THREADS TO FINISH" << endl;

    while (true) {

        if (workInProgressThreads <= completeThreads) break;
    }

    //std::cout << "[CPU] [mergeByDistance()] ALL THREADS ARE DONE" << endl;

    vertCount = std::to_string(objVertices.size());
    faceCount = std::to_string(objFaces.size());

    writeObj(objOutputPath, objVertices, objFaces);

    //printVerts(objVertices);
    //printFaces(objFaces, objVertices);*/

    return 0;
}