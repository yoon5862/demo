#include<iostream>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
#include"Renderer.h"

#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>
#include <vector>

#include "monitoring.hpp"
#include "util.hpp"
#include "quality_aware_scheduler.hpp"

#define SURFACE_WIDTH (1920)
#define SURFACE_HEIGHT (1080)

#define VERTEX_ARRAY (0)
#define TEXCOORD_ARRAY (1)

using namespace std;

static void DisplayHandleGlobal(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
  auto egl_object = (Renderer_C *)data;

  if(egl_object != nullptr) {
    if(strcmp(interface, "wl_compositor") == 0) {
      egl_object->set_wl_compositor((struct wl_compositor *)wl_registry_bind(registry, id, &wl_compositor_interface, 1));
    } else if(strcmp(interface, "wl_shell") == 0) {
      egl_object->set_wl_shell((struct wl_shell *)wl_registry_bind(registry, id, &wl_shell_interface, 1));
    } else if(strcmp(interface, "wl_webos_shell") == 0) {
      egl_object->set_wl_webos_shell((struct wl_webos_shell *)wl_registry_bind(registry, id, &wl_webos_shell_interface, 1));
    }
  }
}

static const struct wl_registry_listener registry_listener = { DisplayHandleGlobal };

static void HandlePing(void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
  wl_shell_surface_pong(shell_surface, serial);
}

static void HandleConfigure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height) {

}

static void HandlePopupDone(void *data, struct wl_shell_surface *shell_surface) {

}

static const struct wl_shell_surface_listener shell_surface_listener = {
  HandlePing,
  HandleConfigure,
  HandlePopupDone
};

Renderer_C::Renderer_C(void) {
  initializeWayland();
  initializeEGL();
  initializeGL();
}

Renderer_C::~Renderer_C(void) {
  finalizeEGL();
  finalizeWayland();
}

RESULT_T Renderer_C::setDataFlow(unsigned char* input) {
  unsigned char* input2 = new unsigned char[m_inputSize.w * m_inputSize.h * 4];
  memcpy(input2, input, m_inputSize.w * m_inputSize.h * 4);

  resizenInference(input2);
  glBindTexture(GL_TEXTURE_2D, m_infoTextureId);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_inputSize.w, m_inputSize.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, input2); //png data를 gpu에서 볼 수 있게 txt로 copy
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  delete[] input2;
  return TRUE;
}
void Renderer_C::resizenInference(unsigned char* input) {
  std::cout<<"*****************************************"<<std::endl;
  cv::Mat img(m_inputSize.h, m_inputSize.w, CV_8UC4, input); //8UCA: 8bit unsigned char 4channel(RGBA)
  std::cout << "m_inputSize.h: " << m_inputSize.h << std::endl;
  std::cout << "m_inputSize.w: " << m_inputSize.w << std::endl;
  float threshold = 0.5; //TODO: fixme if needed

  // Load the TFLite model
  /*
  TODO: fix model_path
  TODO: get model_name, system_info from other source
  ex)
  model_name = getInfoFromPorfolio(~~);
  system_info = getInfoFromSystem(~~);
  */

  pthread_t thread_1;
  float delay = 1.0f;
  float * delay_ = &delay;

  pthread_create(&thread_1, NULL, background_monitoring, (void*)delay_);
  std::string scheduled_model = exec_demo();

  const char* model_path = "/home/root/tmp/model";
  const char* model_name = scheduled_model.c_str();
  const char* system_info = "busy";
  std::cout<<"model_name: "<<model_name<<std::endl;
  std::cout<<"system_info: "<<system_info<<std::endl;

  std::unique_ptr<tflite::FlatBufferModel> model = tflite::FlatBufferModel::BuildFromFile((std::string(model_path) + "/" + model_name).c_str());
  tflite::ops::builtin::BuiltinOpResolver resolver;
  std::unique_ptr<tflite::Interpreter> interpreter;
  tflite::InterpreterBuilder(*model, resolver)(&interpreter);
  interpreter->AllocateTensors();

  // Get input tensor
  TfLiteTensor* input_tensor = interpreter->input_tensor(0);
  int input_height = input_tensor->dims->data[2];
  int input_width = input_tensor->dims->data[3];
  // int input_channels = input_tensor->dims->data[1]; //not used
  // std::cout << "Input Size(hwc): " << input_height << "x" << input_width << "x" << input_channels << std::endl;

  // Preprocess the image
  cv::Mat resized_img;
  cv::resize(img, resized_img, cv::Size(input_width, input_height));
  resized_img.convertTo(resized_img, CV_32F, 1.0 / 255);
  if (resized_img.channels() == 4) {
    cv::cvtColor(resized_img, resized_img, cv::COLOR_BGRA2BGR);
  }

  // Create a blob from the image
  cv::Mat blob = cv::dnn::blobFromImage(resized_img, 1.0, cv::Size(input_width, input_height));
  if (blob.empty()) {
      std::cerr << "can't get input_blob image" << std::endl;
      return;
  }

  // Reshape the blob to match the input tensor shape & copy blob to input tensor
  blob = blob.reshape(1, 1);
  //std::cout << "Reshaped blob size: " << blob.size << std::endl;
  float* inputTensor = interpreter->typed_input_tensor<float>(0);
  if (inputTensor != nullptr) {
      std::memcpy(inputTensor, blob.data, blob.total() * sizeof(float));
  }
  auto start = std::chrono::high_resolution_clock::now();

  interpreter->Invoke();

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> inference_time = end - start;
  std::cout << "Inference time: " << inference_time.count() << " seconds" << std::endl;
  // Get output tensors
  //std::cout << "Get Output Tensors" << std::endl;
  const std::vector<int>& outputs = interpreter->outputs();

  TfLiteTensor* output_labels_tensor = interpreter->tensor(outputs[0]);
  int* output_labels = reinterpret_cast<int*>(output_labels_tensor->data.data);

  TfLiteTensor* output_boxes_tensor = interpreter->tensor(outputs[1]);
  float* output_boxes = reinterpret_cast<float*>(output_boxes_tensor->data.data);

  TfLiteTensor* output_scores_tensor = interpreter->tensor(outputs[2]);
  float* output_scores = reinterpret_cast<float*>(output_scores_tensor->data.data);

  int num_boxes = output_boxes_tensor->dims->data[1];

  std::vector<std::array<float, 4>> selected_boxes;
  std::vector<float> selected_scores;

  // Select First boxes and scores
  for (int i = 0; i < num_boxes; ++i) {
    if (output_labels[i] == 0 && output_scores[i] > threshold) {
      selected_boxes.push_back({output_boxes[4 * i], output_boxes[4 * i + 1], output_boxes[4 * i + 2], output_boxes[4 * i + 3]});
      selected_scores.push_back(output_scores[i]);
    }
    }
  std::cout << "Number of selected boxes: " << selected_boxes.size() << std::endl;

  /* 1. DRAW: Bounding BOX*/
  if(selected_boxes.size() == 0) {
    std::cout << "No object detected" << std::endl;
  }else{
    std::cout << "Object detected" << std::endl;
    std::array<float, 4> box = selected_boxes[0];
    int x_min = static_cast<int>(box[0] * m_inputSize.w);
    int y_min = static_cast<int>(box[1] * m_inputSize.h);
    int x_max = static_cast<int>(box[2] * m_inputSize.w);
    int y_max = static_cast<int>(box[3] * m_inputSize.h);
    int thickness = 10;

    cv::rectangle(img, cv::Point(x_min, y_min), cv::Point(x_max, y_max), cv::Scalar(0, 255, 0, 255), thickness);

    std::string label = std::to_string(selected_scores[0]);
    double font_scale = 2;
    cv::putText(img, label, cv::Point(x_max, y_max - 10), cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(0, 255, 0, 255), 5);
  }

  /* 2. DRAW: BOX AND MODEL, System Info */
  drawModelSystemInfo(img, model_name, system_info);

  memcpy(input, img.data, img.total() * img.elemSize());
}

void Renderer_C::drawModelSystemInfo(cv::Mat& img, const std::string& model_name, const std::string& system_info) {
    int base_line;
    std::string model_info = "-model: " + model_name;
    std::string system_info_text = "-system: " + system_info;
    double font_scale = 1.0; // Adjust font scale to a smaller value
    int thickness = 2; // Adjust thickness to a smaller value

    cv::Size model_text_size = cv::getTextSize(model_info, cv::FONT_HERSHEY_SIMPLEX, font_scale, thickness, &base_line);
    cv::Size system_text_size = cv::getTextSize(system_info_text, cv::FONT_HERSHEY_SIMPLEX, font_scale, thickness, &base_line);

    // Adjust box size based on text sizes
    int box_width = std::max(model_text_size.width, system_text_size.width) + 40;
    int box_height = model_text_size.height + system_text_size.height + 60;
    cv::Rect white_box(10, 10, box_width, box_height); // Adjusted position to top-left corner

    // Draw white box & put model info, system info text
    cv::rectangle(img, white_box, cv::Scalar(255, 255, 255, 255), cv::FILLED);
    cv::putText(img, model_info, cv::Point(20, 20 + model_text_size.height), cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(0, 0, 0, 255), thickness);
    cv::putText(img, system_info_text, cv::Point(20, 40 + model_text_size.height + system_text_size.height), cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(0, 0, 0, 255), thickness);
}

RESULT_T Renderer_C::process(void) {
  glViewport(0, 0, 1920, 1080);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  float pfIdentity[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
  };

  int i32Location = glGetUniformLocation(m_uiProgramObject, "myPMVMatrix");
  glUniformMatrix4fv(i32Location, 1, GL_FALSE, pfIdentity);

  glEnableVertexAttribArray(VERTEX_ARRAY);
  glVertexAttribPointer(VERTEX_ARRAY, 3, GL_FLOAT, GL_FALSE, m_ui32VertexStride, 0);

  glEnableVertexAttribArray(TEXCOORD_ARRAY);
  glVertexAttribPointer(TEXCOORD_ARRAY, 2, GL_FLOAT, GL_FALSE, m_ui32VertexStride, (void *)(3 * sizeof(GLfloat)));

  glDrawArrays(GL_TRIANGLES, 0, 6);

  eglSwapBuffers(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_READ));

  wl_display_flush(wl_display_);

  return TRUE;
}

void Renderer_C::setInputSize(Size_C size) {
  m_inputSize = size;
}

void Renderer_C::setPixelType(PixelType_T type) {}

void Renderer_C::initializeEGL(void){
  int configs;

  int want_red = 8;
  int want_green = 8;
  int want_blue = 8;
  int want_alpha = 8;

  EGLint major_version;
  EGLint minor_version;

  m_eglDisplay = eglGetDisplay((EGLNativeDisplayType)wl_display_);
  eglInitialize(m_eglDisplay, &major_version, &minor_version);

  eglBindAPI(EGL_OPENGL_ES_API);

  eglGetConfigs(m_eglDisplay, NULL, 0, &configs);

  m_pEgl_config = (EGLConfig *)alloca(configs * sizeof(EGLConfig));
  {
    const int NUM_ATTRIBS = 21;
    EGLint *attr = (EGLint *)malloc(NUM_ATTRIBS * sizeof(EGLint));
    int i = 0;

    attr[i++] = EGL_RED_SIZE; attr[i++] = want_red;
    attr[i++] = EGL_GREEN_SIZE; attr[i++] = want_green;
    attr[i++] = EGL_BLUE_SIZE; attr[i++] = want_blue;
    attr[i++] = EGL_ALPHA_SIZE; attr[i++] = want_alpha;
    attr[i++] = EGL_DEPTH_SIZE; attr[i++] = 24;
    attr[i++] = EGL_STENCIL_SIZE; attr[i++] = 0;
    attr[i++] = EGL_SURFACE_TYPE; attr[i++] = EGL_WINDOW_BIT;
    attr[i++] = EGL_RENDERABLE_TYPE; attr[i++] = EGL_OPENGL_ES2_BIT;

    //multi sampel
    attr[i++] = EGL_SAMPLE_BUFFERS; attr[i++] = 1;
    attr[i++] = EGL_SAMPLES; attr[i++] = 4;

    attr[i++] = EGL_NONE;

    assert(i <= NUM_ATTRIBS);

    eglChooseConfig(m_eglDisplay, attr, m_pEgl_config, configs, &configs);

    free(attr);
  }

  for(m_config_select = 0;m_config_select < configs;m_config_select++){
    EGLint red_size, green_size, blue_size, alpha_size, depth_size;

    eglGetConfigAttrib(m_eglDisplay, m_pEgl_config[m_config_select] , EGL_RED_SIZE, &red_size);
    eglGetConfigAttrib(m_eglDisplay, m_pEgl_config[m_config_select] , EGL_GREEN_SIZE, &green_size);
    eglGetConfigAttrib(m_eglDisplay, m_pEgl_config[m_config_select] , EGL_BLUE_SIZE, &blue_size);
    eglGetConfigAttrib(m_eglDisplay, m_pEgl_config[m_config_select] , EGL_ALPHA_SIZE, &alpha_size);
    eglGetConfigAttrib(m_eglDisplay, m_pEgl_config[m_config_select] , EGL_DEPTH_SIZE, &depth_size);

    if((red_size == want_red) && (green_size == want_green) && (blue_size == want_blue) && (alpha_size == want_alpha)){
      break;
    }
  }

  native_egl_window_ = wl_egl_window_create(wl_surface_, 1920, 1080);

  m_currentEglConfig = m_pEgl_config[m_config_select];
  m_eglSurface = eglCreateWindowSurface(m_eglDisplay, m_currentEglConfig, (NativeWindowType)(native_egl_window_), NULL);

  {
    EGLint ctx_attrib_list[3] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
    };
    ctx_attrib_list[1] = 2; //Client Version.

    m_eglContext = eglCreateContext(m_eglDisplay, m_currentEglConfig, EGL_NO_CONTEXT, ctx_attrib_list);
  }

  eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);

  eglSwapInterval(m_eglDisplay, 1);
}

void Renderer_C::finalizeEGL(void){
  glDeleteTextures(1, &m_infoTextureId);

  eglMakeCurrent(m_eglDisplay, NULL, NULL, EGL_NO_CONTEXT);
  eglDestroySurface(m_eglDisplay, m_eglSurface);
  eglDestroyContext(m_eglDisplay, m_eglContext);

  wl_egl_window_destroy(native_egl_window_);

  eglTerminate(m_eglDisplay);
}

void Renderer_C::set_wl_compositor(struct wl_compositor *compositor) {
  wl_compositor_ = compositor;
}

void Renderer_C::set_wl_shell(struct wl_shell *shell) {
  wl_shell_ = shell;
}

void Renderer_C::set_wl_webos_shell(struct wl_webos_shell *webos_shell) {
  wl_webos_shell_ = webos_shell;
}

void Renderer_C::initializeGL(void){
  const char* pszFragShader = "\
                               uniform sampler2D sampler2d;\
                               varying mediump vec2 myTexCoord;\
                               void main(void)\
                               {\
                                 gl_FragColor = texture2D(sampler2d, myTexCoord);\
                               }";

  const char* pszVertShader = "\
                               attribute mediump vec4 myVertex;\
                               attribute mediump vec4 myUV;\
                               uniform mediump mat4 myPMVMatrix;\
                               varying mediump vec2 myTexCoord;\
                               void main(void)\
                               {\
                                 myTexCoord = myUV.st;\
                                 gl_Position = myPMVMatrix * myVertex;\
                               }";

  m_uiFragShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(m_uiFragShader, 1, (const char**)&pszFragShader, NULL);
  glCompileShader(m_uiFragShader);

  GLint bShaderCompiled;
  glGetShaderiv(m_uiFragShader, GL_COMPILE_STATUS, &bShaderCompiled);
  if(!bShaderCompiled){
    cout << "[BROWNIE]Frag Shader error" << endl;
    exit(1);
  }

  m_uiVertShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(m_uiVertShader, 1, (const char**)&pszVertShader, NULL);
  glCompileShader(m_uiVertShader);
  glGetShaderiv(m_uiVertShader, GL_COMPILE_STATUS, &bShaderCompiled);

  if(!bShaderCompiled){
    cout << "[BROWNIE]Vertex Shader error" << endl;
    exit(1);
  }

  m_uiProgramObject = glCreateProgram();

  glAttachShader(m_uiProgramObject, m_uiFragShader);
  glAttachShader(m_uiProgramObject, m_uiVertShader);

  glBindAttribLocation(m_uiProgramObject, VERTEX_ARRAY, "myVertex");
  glBindAttribLocation(m_uiProgramObject, TEXCOORD_ARRAY, "myUV");

  glLinkProgram(m_uiProgramObject);

  GLint bLinked;
  glGetProgramiv(m_uiProgramObject, GL_LINK_STATUS, &bLinked);

  if(!bLinked){
    cout << "[BROWNIE]Program Link error" << endl;
    exit(1);
  }

  glUseProgram(m_uiProgramObject);
  glGenTextures(1, &m_infoTextureId);

  glUniform1i(glGetUniformLocation(m_uiProgramObject, "sampler2d"), 0);

  GLfloat afVertices[] = {
    -1.00f, -1.00f, 0.0f,
    0.00f,  1.00f,
    1.00f, -1.00f, 0.0f,
    1.00f,  1.00f,
    1.00f,  1.00f, 0.0f,
    1.00f,  0.00f,
    -1.00f, -1.00f, 0.0f,
    0.00f,  1.00f,
    1.00f,  1.00f, 0.0f,
    1.00f,  0.00f,
    -1.00f,  1.00f, 0.0f,
    0.00f,  0.00f,
  };

  glGenBuffers(1, &m_ui32Vbo);

  glBindBuffer(GL_ARRAY_BUFFER, m_ui32Vbo);

  m_ui32VertexStride = 5 * sizeof(GLfloat);

  glBufferData(GL_ARRAY_BUFFER, 6 * m_ui32VertexStride, afVertices, GL_STATIC_DRAW);
}

bool Renderer_C::initializeWayland(void) {
  int result, i;

  wl_display_ = wl_display_connect(nullptr);
  if(wl_display_ == nullptr) return false;

  wl_registry_ = wl_display_get_registry(wl_display_);
  if(wl_registry_ == nullptr)
    goto wayland_init_fail;

  result = wl_registry_add_listener(wl_registry_, &registry_listener, this);
  if(result == -1)
    goto wayland_init_fail;

  for(i = 0; i < 2; ++i) {
    if(wl_display_roundtrip(wl_display_) == -1)
      goto wayland_init_fail;
  }

  if(wl_compositor_ == nullptr || wl_shell_ == nullptr || wl_webos_shell_ == nullptr)
    goto wayland_init_fail;

  wl_surface_ = wl_compositor_create_surface(wl_compositor_);
  if(wl_surface_ == nullptr)
    goto wayland_init_fail;

  wl_shell_surface_ = wl_shell_get_shell_surface(wl_shell_, wl_surface_);
  if(wl_shell_surface_ == nullptr)
    goto wayland_init_fail;
  wl_shell_surface_add_listener(wl_shell_surface_, &shell_surface_listener, NULL);

  wl_webos_shell_surface_ = wl_webos_shell_get_shell_surface(wl_webos_shell_, wl_surface_);
  if(wl_webos_shell_surface_ == nullptr)
    goto wayland_init_fail;
  wl_webos_shell_surface_set_property(wl_webos_shell_surface_, "_WEBOS_WINDOW_TYPE", "_WEBOS_WINDOW_TYPE_FLOATING");
  wl_webos_shell_surface_set_property(wl_webos_shell_surface_, "needFocus", "false");

  return true;

wayland_init_fail:
  finalizeWayland();

  return false;
}

void Renderer_C::finalizeWayland(void) {
  if(wl_webos_shell_surface_) {
    wl_webos_shell_surface_destroy(wl_webos_shell_surface_);
    wl_webos_shell_surface_ = nullptr;
  }

  if(wl_surface_) {
    wl_surface_destroy(wl_surface_);
    wl_surface_ = nullptr;
  }

  if(wl_webos_shell_) {
    wl_webos_shell_destroy(wl_webos_shell_);
    wl_webos_shell_ = nullptr;
  }

  if(wl_shell_) {
    wl_shell_destroy(wl_shell_);
    wl_shell_ = nullptr;
  }

  if(wl_compositor_) {
    wl_compositor_destroy(wl_compositor_);
    wl_compositor_ = nullptr;
  }

  if(wl_registry_) {
    wl_registry_destroy(wl_registry_);
    wl_registry_ = nullptr;
  }

  if(wl_display_) {
    wl_display_flush(wl_display_);
    wl_display_disconnect(wl_display_);
    wl_display_ = nullptr;
  }
}

