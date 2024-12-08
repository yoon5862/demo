#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include"MultiplePNGIO.h"

#define RES_PATH "./resource" //이미지 경로

using namespace std;

MultiplePNGIO_C::MultiplePNGIO_C(void)
    : m_inputPNG(NULL), m_pixelType(PIXEL_TYPE_LAST), m_frameNum(0), m_curFrameId(0)
{
    RESULT_T res;

    m_frameNum = getFrameNum();
    res = readPNGfiles();
    PRINT_ERROR_AND_RETURN(res != TRUE, "[SYSTEM] readPNGfiles is error", exit(1));
}

MultiplePNGIO_C::~MultiplePNGIO_C(void){
    if(m_inputPNG != NULL){
        for(unsigned int i = 0;i < m_frameNum;i++){
            delete m_inputPNG[i];
        }
        delete[] m_inputPNG;
    }
}

unsigned char* MultiplePNGIO_C::getInputBuffer(void){
    return m_inputPNG[m_curFrameId]->getPixelData();
}

Size_C MultiplePNGIO_C::getInputSize(void){
    return m_inputSize;
}

PixelType_T MultiplePNGIO_C::getPixelType(void){
    return m_pixelType;
}

RESULT_T MultiplePNGIO_C::requireBuffersLock(void){
    return TRUE;
}

RESULT_T MultiplePNGIO_C::releaseBuffersLock(void){
    m_curFrameId++;
    if(m_curFrameId >= m_frameNum) m_curFrameId = 0;

    return TRUE;
}

unsigned int MultiplePNGIO_C::getFrameNum(void){
    FILE *file;
    char imgFileName[100];
    unsigned int fileNum = 0;

    while(1){
        sprintf(imgFileName, "%s/input%d.png", RES_PATH, fileNum);

        file = fopen(imgFileName, "rb");
        if(file == NULL) break;
        fclose(file);

        fileNum++;
    }

    PRINT_ERROR_AND_RETURN(fileNum == 0, "[SYSTEM] Resource is not exist", exit(1));

    return fileNum;
}

RESULT_T MultiplePNGIO_C::readPNGfiles(void){
    char imgFileName[100];
    int bytePerPixel = 0;

    m_inputPNG = new PNGHandler_C*[m_frameNum];

    for(unsigned int i = 0;i < m_frameNum;i++){
        sprintf(imgFileName, "%s/input%d.png", RES_PATH, i);

        m_inputPNG[i] = new PNGHandler_C(imgFileName);

        //Set property
        if(i == 0){
            m_pixelType = m_inputPNG[i]->getPixelType();
            m_inputSize.w = m_inputPNG[i]->getWidth();
            m_inputSize.h = m_inputPNG[i]->getHeight();
        }
    }

    return TRUE;
}










