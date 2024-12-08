#include<iostream>
#include<stdio.h>
#include<string.h>

#include"common.h"
#include"MultiplePNGIO.h"
#include"PQEngine.h"
#include"Renderer.h"

using namespace std;

int main(int argc, char* argv[]){
    RESULT_T res;

    IPictureIO_C<unsigned char*> *stream = new MultiplePNGIO_C(); //png읽음
    IPQAlg_C<unsigned char*> *algorithm = new Renderer_C(); //렌더링

    PQEngine_C<unsigned char*> engine(stream, algorithm); 
    res = engine.execute();
    if(res != TRUE){
        cout << "[SYSTEM] PQEngine_C execute error" << endl;
    }

    delete algorithm;
    delete stream;

    return 0;
}

