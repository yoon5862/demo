#include<iostream>
#include<stdlib.h>
#include"NoiseReduction.h"

#define TIME_CHECK (0)

using namespace std;

static bool _isFirstFrame = true;
const int _cLclSizeX = 6;
const int _cLclSizeY = 2;

NoiseReduction_C::NoiseReduction_C(void)
    : m_clDrv("./noiseReduction.cl"), m_input(NULL), m_output(NULL), m_isSizeChanged(true), m_isAllocated(false), m_isDataFlowChanged(true), m_isPixelTypeChanged(true), m_YBufIdx(0), m_inputMem(0), m_outputMem(0), m_UMem(0), m_VMem(0), m_alphaMem(0), m_ovlMem(0), m_nmapMem(0)
{
    RESULT_T res;

    m_YMem[0] = 0;
    m_YMem[1] = 0;

    m_programGPU = m_clDrv.getCLProgram();
    m_contextGPU = m_clDrv.getCLContext();
    m_commandQueueGPU = m_clDrv.getCLCommandQueue();

    res = makeCLKernel();
    PRINT_ERROR_AND_RETURN(res != TRUE, "[SYSTEM] CL make kernel error", exit(1));
}

NoiseReduction_C::~NoiseReduction_C(void){
    RESULT_T res;
    cl_int clResult = 0, errNum;

    res = freeDataFlowBuffer();
    PRINT_ERROR_AND_RETURN(res != TRUE, "[SYSTEM] CL DataFlow memory free error", exit(1));

    res = freeCaculateBuffer();
    PRINT_ERROR_AND_RETURN(res != TRUE, "[SYSTEM] CL Caculate memory free error", exit(1));

    errNum = clReleaseKernel(m_kernelInitvalue);
    clprint_errNum("clReleaseKernel", errNum, clResult);
    errNum = clReleaseKernel(m_kernelProcessing);
    clprint_errNum("clReleaseKernel", errNum, clResult);
    errNum = clReleaseKernel(m_kernelEliminateNoise);
    clprint_errNum("clReleaseKernel", errNum, clResult);
    errNum = clReleaseKernel(m_kernelConvertYUV);
    clprint_errNum("clReleaseKernel", errNum, clResult);
    errNum = clReleaseKernel(m_kernelConvertRGB);
    clprint_errNum("clReleaseKernel", errNum, clResult);

    PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL finalize is error", exit(1));
}

RESULT_T NoiseReduction_C::setDataFlow(unsigned char* input, unsigned char* output){
    m_input = input;
    m_output = output;

    m_isDataFlowChanged = true;

    return TRUE;
}

RESULT_T NoiseReduction_C::process(void){
    RESULT_T res;

    res = checkInputOutputSize();
    PRINT_ERROR_AND_RETURN(res != TRUE, "[SYSTEM] Size setting error", return FALSE);

    res = allocDynamicBuffer();
    PRINT_ERROR_AND_RETURN(res != TRUE, "[SYSTEM] CL alloc buffer error", return FALSE);

    res = setKernelArg();
    PRINT_ERROR_AND_RETURN(res != TRUE, "[SYSTEM] CL set Kernel Arg error", return FALSE);

    res = launchKernel();
    PRINT_ERROR_AND_RETURN(res != TRUE, "[SYSTEM] CL launch Kernel error", return FALSE);

    res = getResult();
    PRINT_ERROR_AND_RETURN(res != TRUE, "[SYSTEM] CL get result error", return FALSE);

    if(_isFirstFrame == true) _isFirstFrame = false;

    m_YBufIdx = 1 - m_YBufIdx;

    return TRUE;
}

void NoiseReduction_C::setInputSize(Size_C size){
    if(size.w != m_inputSize.w && size.h != m_inputSize.h){
        m_inputSize = size;
        m_isSizeChanged = true;
    }
}

void NoiseReduction_C::setOutputSize(Size_C size){
    if(size.w != m_outputSize.w && size.h != m_outputSize.h){
        m_outputSize = size;
        m_isSizeChanged = true;
    }
}

void NoiseReduction_C::setPixelType(PixelType_T type){
    if(m_pixelType != type){
        m_pixelType = type;
        m_isPixelTypeChanged = true;
    }
}

RESULT_T NoiseReduction_C::setKernelArg(void){
    cl_int clResult = 0, errNum;

    errNum = clSetKernelArg(m_kernelConvertYUV, 0, sizeof(cl_mem), (void *)&m_inputMem);
    clprint_errNum("clSetKernelArg-kernelConvertYUV", errNum, clResult);
    errNum = clSetKernelArg(m_kernelConvertYUV, 1, sizeof(cl_mem), (void *)&m_YMem[m_YBufIdx]);
    clprint_errNum("clSetKernelArg-kernelConvertYUV", errNum, clResult);
    errNum = clSetKernelArg(m_kernelConvertYUV, 2, sizeof(cl_mem), (void *)&m_UMem);
    clprint_errNum("clSetKernelArg-kernelConvertYUV", errNum, clResult);
    errNum = clSetKernelArg(m_kernelConvertYUV, 3, sizeof(cl_mem), (void *)&m_VMem);
    clprint_errNum("clSetKernelArg-kernelConvertYUV", errNum, clResult);
    errNum = clSetKernelArg(m_kernelConvertYUV, 4, sizeof(cl_mem), (void *)&m_alphaMem);
    clprint_errNum("clSetKernelArg-kernelConvertYUV", errNum, clResult);

    PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL ConvertYUV kernel set Arg is error", return FALSE);

    if(_isFirstFrame == false){
        errNum = clSetKernelArg(m_kernelInitvalue, 0, sizeof(cl_mem), (void *)&m_ovlMem);
        clprint_errNum("clSetKernelArg-kernelInitvalue", errNum, clResult);
        errNum = clSetKernelArg(m_kernelInitvalue, 1, sizeof(cl_mem), (void *)&m_nmapMem);
        clprint_errNum("clSetKernelArg-kernelInitvalue", errNum, clResult);

        PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL Initvalue kernel set Arg is error", return FALSE);

        errNum = clSetKernelArg(m_kernelProcessing, 0, sizeof(cl_mem), (void *)&m_YMem[m_YBufIdx]);
        clprint_errNum("clSetKernelArg-kernelProcessing", errNum, clResult);
        errNum = clSetKernelArg(m_kernelProcessing, 1, sizeof(cl_mem), (void *)&m_YMem[1 - m_YBufIdx]);
        clprint_errNum("clSetKernelArg-kernelProcessing", errNum, clResult);
        errNum = clSetKernelArg(m_kernelProcessing, 2, sizeof(cl_mem), (void *)&m_ovlMem);
        clprint_errNum("clSetKernelArg-kernelProcessing", errNum, clResult);
        errNum = clSetKernelArg(m_kernelProcessing, 3, sizeof(cl_mem), (void *)&m_nmapMem);
        clprint_errNum("clSetKernelArg-kernelProcessing", errNum, clResult);
        //set local memory
        //--------------
        errNum = clSetKernelArg(m_kernelProcessing, 4, sizeof(unsigned char) * (_cLclSizeX + 6) * (_cLclSizeY + 2), NULL);
        clprint_errNum("clSetKernelArg-kernelProcessing", errNum, clResult);
        errNum = clSetKernelArg(m_kernelProcessing, 5, sizeof(int) * (_cLclSizeX + 6) * (_cLclSizeY + 2), NULL);
        clprint_errNum("clSetKernelArg-kernelProcessing", errNum, clResult);
        errNum = clSetKernelArg(m_kernelProcessing, 6, sizeof(int) * (_cLclSizeX + 6) * (_cLclSizeY + 2), NULL);
        clprint_errNum("clSetKernelArg-kernelProcessing", errNum, clResult);
        errNum = clSetKernelArg(m_kernelProcessing, 7, sizeof(int) * (_cLclSizeX + 6) * (_cLclSizeY + 2), NULL);
        clprint_errNum("clSetKernelArg-kernelProcessing", errNum, clResult);
        //--------------
        errNum = clSetKernelArg(m_kernelProcessing, 8, sizeof(int), (void *)&m_inputSize.w);
        clprint_errNum("clSetKernelArg-kernelProcessing", errNum, clResult);

        PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL Processing kernel set Arg is error", return FALSE);

        errNum = clSetKernelArg(m_kernelEliminateNoise, 0, sizeof(cl_mem), (void *)&m_YMem[m_YBufIdx]);
        clprint_errNum("clSetKernelArg-kernelEliminateNoise", errNum, clResult);
        errNum = clSetKernelArg(m_kernelEliminateNoise, 1, sizeof(cl_mem), (void *)&m_YMem[1 - m_YBufIdx]);
        clprint_errNum("clSetKernelArg-kernelEliminateNoise", errNum, clResult);
        errNum = clSetKernelArg(m_kernelEliminateNoise, 2, sizeof(cl_mem), (void *)&m_ovlMem);
        clprint_errNum("clSetKernelArg-kernelEliminateNoise", errNum, clResult);
        errNum = clSetKernelArg(m_kernelEliminateNoise, 3, sizeof(cl_mem), (void *)&m_nmapMem);
        clprint_errNum("clSetKernelArg-kernelEliminateNoise", errNum, clResult);

        PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL EliminateNoise kernel set Arg is error", return FALSE);
    }

    errNum = clSetKernelArg(m_kernelConvertRGB, 0, sizeof(cl_mem), (void *)&m_alphaMem);
    clprint_errNum("clSetKernelArg-kernelConvertRGB", errNum, clResult);
    if(_isFirstFrame == false){
        errNum = clSetKernelArg(m_kernelConvertRGB, 1, sizeof(cl_mem), (void *)&m_YMem[1 - m_YBufIdx]);
        clprint_errNum("clSetKernelArg-kernelConvertRGB", errNum, clResult);
    }
    else{
        errNum = clSetKernelArg(m_kernelConvertRGB, 1, sizeof(cl_mem), (void *)&m_YMem[m_YBufIdx]);
        clprint_errNum("clSetKernelArg-kernelConvertRGB", errNum, clResult);
    }
    errNum = clSetKernelArg(m_kernelConvertRGB, 2, sizeof(cl_mem), (void *)&m_UMem);
    clprint_errNum("clSetKernelArg-kernelConvertRGB", errNum, clResult);
    errNum = clSetKernelArg(m_kernelConvertRGB, 3, sizeof(cl_mem), (void *)&m_VMem);
    clprint_errNum("clSetKernelArg-kernelConvertRGB", errNum, clResult);
    errNum = clSetKernelArg(m_kernelConvertRGB, 4, sizeof(cl_mem), (void *)&m_outputMem);
    clprint_errNum("clSetKernelArg-kernelConvertRGB", errNum, clResult);

    PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL ConvertRGB kernel set Arg is error", return FALSE);

    return TRUE;
}

RESULT_T NoiseReduction_C::launchKernel(void){
    cl_int clResult = 0, errNum;
    size_t global2D[2];
    size_t local2D[2];

#if TIME_CHECK
    signed long long startTime = 0, workTime = 0;

    startTime = getCurrentMsec();
#endif
    global2D[0] = m_inputSize.w; global2D[1] = m_inputSize.h;
    local2D[0] = _cLclSizeX; local2D[1] = _cLclSizeY;
    errNum = clEnqueueNDRangeKernel(m_commandQueueGPU, m_kernelConvertYUV, 2, NULL, global2D, local2D, 0, NULL, NULL);
    clprint_errNum("clEnqueueNDRangeKernel-kernelConvertYUV", errNum, clResult);
    PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL ConvertYUV kernel launch is error", return FALSE);
#if TIME_CHECK
    clFlush(m_commandQueueGPU);
    clFinish(m_commandQueueGPU);

    workTime = getCurrentMsec() - startTime;
    cout << "ConvertYUV is " << workTime << "ms" << endl;
#endif

    if(_isFirstFrame == false){
#if TIME_CHECK
        startTime = getCurrentMsec();
#endif
        global2D[0] = m_inputSize.w / 16; global2D[1] = m_inputSize.h;
        local2D[0] = _cLclSizeX; local2D[1] = _cLclSizeY;
        errNum = clEnqueueNDRangeKernel(m_commandQueueGPU, m_kernelInitvalue, 2, NULL, global2D, local2D, 0, NULL, NULL);
        clprint_errNum("clEnqueueNDRangeKernel-kernelInitvalue", errNum, clResult);
        PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL Initvalue kernel launch is error", return FALSE);
#if TIME_CHECK
        clFlush(m_commandQueueGPU);
        clFinish(m_commandQueueGPU);

        workTime = getCurrentMsec() - startTime;
        cout << "Initvalue is " << workTime << "ms" << endl;
#endif

#if TIME_CHECK
        startTime = getCurrentMsec();
#endif
        global2D[0] = m_inputSize.w - 6; global2D[1] = m_inputSize.h - 2;
        local2D[0] = _cLclSizeX; local2D[1] = _cLclSizeY;
        errNum = clEnqueueNDRangeKernel(m_commandQueueGPU, m_kernelProcessing, 2, NULL, global2D, local2D, 0, NULL, NULL);
        clprint_errNum("clEnqueueNDRangeKernel-kernelProcessing", errNum, clResult);
        PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL Processing kernel launch is error", return FALSE);
#if TIME_CHECK
        clFlush(m_commandQueueGPU);
        clFinish(m_commandQueueGPU);

        workTime = getCurrentMsec() - startTime;
        cout << "Processing is " << workTime << "ms" << endl;
#endif

#if TIME_CHECK
        startTime = getCurrentMsec();
#endif
        global2D[0] = m_inputSize.w / 16; global2D[1] = m_inputSize.h;
        local2D[0] = _cLclSizeX; local2D[1] = _cLclSizeY;
        errNum = clEnqueueNDRangeKernel(m_commandQueueGPU, m_kernelEliminateNoise, 2, NULL, global2D, local2D, 0, NULL, NULL);
        clprint_errNum("clEnqueueNDRangeKernel-kernelEliminateNoise", errNum, clResult);
        PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL EliminateNoise kernel launch is error", return FALSE);
#if TIME_CHECK
        clFlush(m_commandQueueGPU);
        clFinish(m_commandQueueGPU);

        workTime = getCurrentMsec() - startTime;
        cout << "EliminateNoise is " << workTime << "ms" << endl;
#endif
    }

#if TIME_CHECK
    startTime = getCurrentMsec();
#endif
    global2D[0] = m_outputSize.w; global2D[1] = m_outputSize.h;
    local2D[0] = _cLclSizeX; local2D[1] = _cLclSizeY;
    errNum = clEnqueueNDRangeKernel(m_commandQueueGPU, m_kernelConvertRGB, 2, NULL, global2D, local2D, 0, NULL, NULL);
    clprint_errNum("clEnqueueNDRangeKernel-kernelConvertRGB", errNum, clResult);
    PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL ConvertRGB kernel launch is error", return FALSE);
#if TIME_CHECK
    clFlush(m_commandQueueGPU);
    clFinish(m_commandQueueGPU);

    workTime = getCurrentMsec() - startTime;
    cout << "ConvertRGB is " << workTime << "ms" << endl;
#endif

    return TRUE;
}

RESULT_T NoiseReduction_C::getResult(void){
    cl_int clResult = 0, errNum;

    size_t origin[3] = {0, 0, 0}; //offset. 0, 0, 0 이며 원점에서 시작.
    size_t size[3];
    size[0] = m_outputSize.w; size[1] = m_outputSize.h; size[2] = 1;
    errNum = clEnqueueReadImage(m_commandQueueGPU, m_outputMem, CL_TRUE, origin, size, 0, 0, (void *)m_output, 0, NULL, NULL);
    clprint_errNum("clEnqueueReadImage-m_outputMem", errNum, clResult);
    PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL get result is error", return FALSE);

    return TRUE;
}

RESULT_T NoiseReduction_C::checkInputOutputSize(void){
    if((m_inputSize.w != m_outputSize.w)||(m_inputSize.h != m_outputSize.h)) return FALSE;
    else return TRUE;
}

RESULT_T NoiseReduction_C::allocDynamicBuffer(void){
    RESULT_T res;
    cl_int clResult = 0, errNum;
    cl_image_format imageFormat;

    if(m_isAllocated == false || m_isSizeChanged == true || m_isDataFlowChanged == true){
        switch(m_pixelType){
        case PIXEL_TYPE_RGB:
            imageFormat.image_channel_order = CL_RGB;
            break;
        case PIXEL_TYPE_RGB_ALPHA:
            imageFormat.image_channel_order = CL_RGBA;
            break;
        case PIXEL_TYPE_LAST:
        default:
            cout << "[SYSTEM] Format is invalid" << endl;
            return FALSE;
            break;
        }
        imageFormat.image_channel_data_type = CL_UNORM_INT8;

        res = freeDataFlowBuffer();
        PRINT_ERROR_AND_RETURN(res != TRUE, "[SYSTEM] CL DataFlow memory free error", return FALSE);

        m_inputMem = clCreateImage2D(m_contextGPU, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, &imageFormat, m_inputSize.w, m_inputSize.h, 0, m_input, &errNum);
        clprint_errNum("clCreateImage2D-m_inputMem", errNum, clResult);

        m_outputMem = clCreateImage2D(m_contextGPU, CL_MEM_WRITE_ONLY, &imageFormat, m_outputSize.w, m_outputSize.h, 0, NULL, &errNum);
        clprint_errNum("clCreateImage2D-m_outputMem", errNum, clResult);
    }

    if(m_isAllocated == false || m_isSizeChanged == true){

        res = freeCaculateBuffer();
        PRINT_ERROR_AND_RETURN(res != TRUE, "[SYSTEM] CL Caculate memory free error", return FALSE);

        m_YMem[0] = clCreateBuffer(m_contextGPU, CL_MEM_READ_WRITE, sizeof(unsigned char) * m_inputSize.w * m_inputSize.h, NULL, &errNum);
        clprint_errNum("clCreateBuffer-m_YMem[0]", errNum, clResult);

        m_YMem[1] = clCreateBuffer(m_contextGPU, CL_MEM_READ_WRITE, sizeof(unsigned char) * m_inputSize.w * m_inputSize.h, NULL, &errNum);
        clprint_errNum("clCreateBuffer-m_YMem[1]", errNum, clResult);

        m_UMem = clCreateBuffer(m_contextGPU, CL_MEM_READ_WRITE, sizeof(unsigned char) * m_inputSize.w * m_inputSize.h, NULL, &errNum);
        clprint_errNum("clCreateBuffer-m_UMem", errNum, clResult);

        m_VMem = clCreateBuffer(m_contextGPU, CL_MEM_READ_WRITE, sizeof(unsigned char) * m_inputSize.w * m_inputSize.h, NULL, &errNum);
        clprint_errNum("clCreateBuffer-m_VMem", errNum, clResult);

        m_alphaMem = clCreateBuffer(m_contextGPU, CL_MEM_READ_WRITE, sizeof(unsigned char) * m_inputSize.w * m_inputSize.h, NULL, &errNum);
        clprint_errNum("clCreateBuffer-m_alphaMem", errNum, clResult);

        m_ovlMem = clCreateBuffer(m_contextGPU, CL_MEM_READ_WRITE, sizeof(int) * m_inputSize.w * m_inputSize.h, NULL, &errNum);
        clprint_errNum("clCreateBuffer-m_ovlMem", errNum, clResult);

        m_nmapMem = clCreateBuffer(m_contextGPU, CL_MEM_READ_WRITE, sizeof(int) * m_inputSize.w * m_inputSize.h, NULL, &errNum);
        clprint_errNum("clCreateBuffer-m_nmapMem", errNum, clResult);

        PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL image2D and buffer create are error", return FALSE);
    }

    m_isAllocated = true;
    m_isSizeChanged = false;
    m_isDataFlowChanged = false;
    m_isPixelTypeChanged = false;

    return TRUE;
}

RESULT_T NoiseReduction_C::freeDataFlowBuffer(void){
    cl_int clResult = 0, errNum;

    if(m_inputMem != 0){
        errNum = clReleaseMemObject(m_inputMem);
        clprint_errNum("clReleaseMemObject-m_inputMem", errNum, clResult);
    }

    if(m_outputMem != 0){
        errNum = clReleaseMemObject(m_outputMem);
        clprint_errNum("clReleaseMemObject-m_outputMem", errNum, clResult);
    }

    PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL DataFlow memory delete is error", return FALSE);

    return TRUE;
}

RESULT_T NoiseReduction_C::freeCaculateBuffer(void){
    cl_int clResult = 0, errNum;

    if(m_YMem[0] != 0){
        errNum = clReleaseMemObject(m_YMem[0]);
        clprint_errNum("clReleaseMemObject-m_YMem[0]", errNum, clResult);
    }

    if(m_YMem[1] != 0){
        errNum = clReleaseMemObject(m_YMem[1]);
        clprint_errNum("clReleaseMemObject-m_YMem[1]", errNum, clResult);
    }

    if(m_UMem != 0){
        errNum = clReleaseMemObject(m_UMem);
        clprint_errNum("clReleaseMemObject-m_UMem", errNum, clResult);
    }

    if(m_VMem != 0){
        errNum = clReleaseMemObject(m_VMem);
        clprint_errNum("clReleaseMemObject-m_VMem", errNum, clResult);
    }

    if(m_alphaMem != 0){
        errNum = clReleaseMemObject(m_alphaMem);
        clprint_errNum("clReleaseMemObject-m_alphaMem", errNum, clResult);
    }

    if(m_ovlMem != 0){
        errNum = clReleaseMemObject(m_ovlMem);
        clprint_errNum("clReleaseMemObject-m_ovlMem", errNum, clResult);
    }

    if(m_nmapMem != 0){
        errNum = clReleaseMemObject(m_nmapMem);
        clprint_errNum("clReleaseMemObject-m_nmapMem", errNum, clResult);
    }

    PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] CL Caculate memory delete is error", return FALSE);

    return TRUE;
}

RESULT_T NoiseReduction_C::makeCLKernel(void){
    cl_int clResult = 0, errNum;

    //Make OpenCL kernel
    m_kernelInitvalue = clCreateKernel(m_programGPU, "init_value", &errNum);
    clprint_errNum("clCreateKernel-init_value", errNum, clResult);

    m_kernelProcessing = clCreateKernel(m_programGPU, "processing", &errNum);
    clprint_errNum("clCreateKernel-processing", errNum, clResult);

    m_kernelEliminateNoise = clCreateKernel(m_programGPU, "eliminateNoise", &errNum);
    clprint_errNum("clCreateKernel-eliminateNoise", errNum, clResult);

    m_kernelConvertYUV = clCreateKernel(m_programGPU, "convert_yuv", &errNum);
    clprint_errNum("clCreateKernel-convert_yuv", errNum, clResult);

    m_kernelConvertRGB = clCreateKernel(m_programGPU, "convert_rgb", &errNum);
    clprint_errNum("clCreateKernel-convert_rgb", errNum, clResult);

    PRINT_ERROR_AND_RETURN(clResult, "[SYSTEM] clCreateKernel is error", return FALSE);

    return TRUE;
}

