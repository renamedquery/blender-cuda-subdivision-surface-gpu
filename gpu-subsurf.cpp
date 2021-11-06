// trying to get the catmull clark subdiv method working in c++ so I can translate it to cuda

#include <iostream>
#include <math.h>
#include <string>
#include <fstream>
#include <iterator>
#include <sstream>

using namespace std;

// can be gpu accelerated at least a little bit since it requires many lines to be read
// for a simple cube there will be no speedup, but for a mesh with millions of verts it will be faster
// currently only reads verts, faces and edges are todo
// __global__
void readObj(std::string path, std::string& output) {
    
    std::ifstream objFile(path);

    // tell the program to not count new lines
    objFile.unsetf(std::ios_base::skipws);

    std::string objFileLine;

    while (getline(objFile, objFileLine)) {

        std::stringstream ss{objFileLine};
        char objFileLineChar;
        ss >> objFileLineChar;

        switch (objFileLineChar) {

            case 'v': { // vert

                char objFileLineSecondChar = objFileLine[1];
                
                switch (objFileLineSecondChar) {

                    case ' ': { // empty second line, meaning it isnt vt/vn and is just a vert

                        output += objFileLine + "\n";
                        break;
                    }
                }
                break;
            }

            case 'f': { // face

                break;
            }
        }
    }
}

int main (void) {

    std::string objPath = "./testCube.obj";
    std::string objData = "";

    readObj(objPath, objData);

    cout << objData << endl;

    return 0;
}