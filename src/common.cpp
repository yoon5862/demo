#include<iostream>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/time.h>
#include<time.h>
#include<unistd.h>
#include<png.h>

#include"common.h"

using namespace std;

//-----------------------------------------------------
//Common util
signed long long getCurrentMsec(void){
    struct timespec ts;
    signed long long curTime = 0;

    memset(&ts, 0, sizeof(struct timespec));
    clock_gettime(CLOCK_MONOTONIC, &ts);
    curTime = (signed long long )(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;

    return curTime;
}

//-----------------------------------------------------
//PNGHandler_C class
PNGHandler_C::PNGHandler_C(void)
    : m_width(0), m_height(0), m_pixelData(NULL), m_pixelType(PIXEL_TYPE_LAST)
{}

PNGHandler_C::PNGHandler_C(const char* fileName){
    int width, height;
    png_byte color_type;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep pixel_Data;

    char header[8];    // 8 is the maximum size that can be checked
    FILE *file = fopen(fileName, "rb");
    PRINT_ERROR_AND_RETURN(!file, "[SYSTEM] PNG file open error : " << fileName, exit(1));
    fread(header, 1, 8, file);

    //Read PNG file
    png_sig_cmp((png_bytep)header, 0, 8);

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);
    setjmp(png_jmpbuf(png_ptr));

    png_init_io(png_ptr, file);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    PRINT_ERROR_AND_RETURN(!(color_type == PNG_COLOR_TYPE_RGB_ALPHA), "[SYSTEM] PNG file format error" << endl << "[SYSTEM] PNG_COLOR_TYPE_RGB_ALPHA is supported only", exit(1));

    png_get_bit_depth(png_ptr, info_ptr);

    png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    setjmp(png_jmpbuf(png_ptr));

    pixel_Data = (png_bytep)malloc(sizeof(png_byte) * height * png_get_rowbytes(png_ptr, info_ptr));
    PRINT_ERROR_AND_RETURN(pixel_Data == NULL, "[SYSTEM] malloc is failed", exit(1));

    for(int y = 0;y < height;y++){
        png_read_row(png_ptr, (pixel_Data + y * png_get_rowbytes(png_ptr, info_ptr)), (png_bytep)NULL);
    }

    //Set member variable.
    m_width = width;
    m_height = height;
    m_pixelData = (unsigned char*)pixel_Data;

    if(color_type == PNG_COLOR_TYPE_RGB_ALPHA) m_pixelType = PIXEL_TYPE_RGB_ALPHA;
    else m_pixelType = PIXEL_TYPE_LAST;

    fclose(file);
}

PNGHandler_C::~PNGHandler_C(void){
    if(m_pixelData != NULL){
        free(m_pixelData);
    }
}

RESULT_T PNGHandler_C::setData(unsigned int width, unsigned int height, PixelType_T pixelType, unsigned char* pixelData){
    if(!(pixelType == PIXEL_TYPE_RGB || pixelType == PIXEL_TYPE_RGB_ALPHA)) return FALSE;

    int bytePerPixel = 0;

    if(m_pixelData != NULL) free(m_pixelData);

    m_width = width;
    m_height = height;
    m_pixelType = pixelType;

    if(pixelType == PIXEL_TYPE_RGB) bytePerPixel = 3;
    else if(pixelType == PIXEL_TYPE_RGB_ALPHA) bytePerPixel = 4;

    m_pixelData = (unsigned char*)malloc(sizeof(unsigned char) * m_width * m_height * bytePerPixel);
    PRINT_ERROR_AND_RETURN(m_pixelData == NULL, "[SYSTEM] malloc is failed", return FALSE);
    memcpy(m_pixelData, pixelData, sizeof(unsigned char) * m_width * m_height * bytePerPixel);

    return TRUE;
}

RESULT_T PNGHandler_C::writeToFile(const char* fileName){
    FILE *file = NULL;
    int bytePerPixel = 0;

    if(m_pixelType == PIXEL_TYPE_RGB) bytePerPixel = 3;
    else if(m_pixelType == PIXEL_TYPE_RGB_ALPHA) bytePerPixel = 4;

    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_byte color_type;

    file = fopen(fileName, "wb");
    PRINT_ERROR_AND_RETURN(file == NULL, "[SYSTEM] PNG file open error : " << fileName, return FALSE);

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);
    setjmp(png_jmpbuf(png_ptr));

    png_init_io(png_ptr, file);

    if(m_pixelType == PIXEL_TYPE_RGB) color_type = PNG_COLOR_TYPE_RGB;
    else if(m_pixelType == PIXEL_TYPE_RGB_ALPHA) color_type = PNG_COLOR_TYPE_RGB_ALPHA;

    png_set_IHDR(png_ptr, info_ptr, m_width, m_height, 8, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    for(int y = 0;y < (int)m_height;y++){
        png_write_row(png_ptr, m_pixelData + y * m_width * bytePerPixel);
    }

    png_write_end(png_ptr, NULL);
    fclose(file);
    png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

    return TRUE;
}

unsigned int PNGHandler_C::getWidth(void){
    return m_width;
}

unsigned int PNGHandler_C::getHeight(void){
    return m_height;
}

unsigned char* PNGHandler_C::getPixelData(void){
    return m_pixelData;
}

PixelType_T PNGHandler_C::getPixelType(void){
    return m_pixelType;
}

//-----------------------------------------------------
//Size_C class
Size_C::Size_C(void)
    :w(0), h(0)
{}

Size_C::~Size_C(void){

}

Size_C::Size_C(unsigned int width, unsigned int height)
    :w(width), h(height)
{}

Size_C::Size_C(const Size_C& size){
    w = size.w;
    h = size.h;
}

Size_C& Size_C::operator=(const Size_C& size){
    w = size.w;
    h = size.h;

    return *this;
}

RESULT_T Size_C::findMinSize(Size_C size1, Size_C size2){
    if(!(size1.w && size1.h && size2.w && size2.h)) return FALSE;

    w = (size1.w < size2.w) ? size1.w : size2.w;
    h = (size1.h < size2.h) ? size1.h : size2.h;

    return TRUE;
}










