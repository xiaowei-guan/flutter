// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/playground/backend/gles/playground_impl_gles.h"
#include "flutter/fml/logging.h"

#define IMPELLER_PLAYGROUND_SUPPORTS_ANGLE FML_OS_MACOSX

#if IMPELLER_PLAYGROUND_SUPPORTS_ANGLE
#include <dlfcn.h>
#endif

//#define EFL_BETA_API_SUPPORT
#include <Ecore_Wl2.h>
#include <GLES2/gl2.h>
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2ext.h>
#include <tbm_dummy_display.h>
#include <tbm_surface.h>
#include <tbm_surface_queue.h>

// #define GLFW_INCLUDE_NONE
// #include "third_party/glfw/include/GLFW/glfw3.h"

#include "flutter/fml/build_config.h"
#include "impeller/entity/gles/entity_shaders_gles.h"
#include "impeller/entity/gles/framebuffer_blend_shaders_gles.h"
#include "impeller/entity/gles/modern_shaders_gles.h"
#include "impeller/fixtures/gles/fixtures_shaders_gles.h"
#include "impeller/fixtures/gles/modern_fixtures_shaders_gles.h"
//#include "impeller/playground/imgui/gles/imgui_shaders_gles.h"
#include "impeller/renderer/backend/gles/context_gles.h"
#include "impeller/renderer/backend/gles/surface_gles.h"

namespace impeller {

class PlaygroundImplGLES::ReactorWorker final : public ReactorGLES::Worker {
 public:
  ReactorWorker() = default;

  // |ReactorGLES::Worker|
  bool CanReactorReactOnCurrentThreadNow(
      const ReactorGLES& reactor) const override {
    ReaderLock lock(mutex_);
    auto found = reactions_allowed_.find(std::this_thread::get_id());
    if (found == reactions_allowed_.end()) {
      return false;
    }
    return found->second;
  }

  void SetReactionsAllowedOnCurrentThread(bool allowed) {
    WriterLock lock(mutex_);
    reactions_allowed_[std::this_thread::get_id()] = allowed;
  }

 private:
  mutable RWMutex mutex_;
  std::map<std::thread::id, bool> reactions_allowed_ IPLR_GUARDED_BY(mutex_);

  ReactorWorker(const ReactorWorker&) = delete;

  ReactorWorker& operator=(const ReactorWorker&) = delete;
};

PlaygroundImplGLES::PlaygroundImplGLES(PlaygroundSwitches switches, SharedHandle window_ecore_handle)
    : PlaygroundImpl(switches),
      worker_(std::shared_ptr<ReactorWorker>(new ReactorWorker())) {

  window_ecore_handle_ = window_ecore_handle;
  TizenGeometry geometry = window_ecore_handle_->GetGeometry();
  CreateSurface(window_ecore_handle_->GetRenderTarget(),
                         window_ecore_handle_->GetRenderTargetDisplay(), geometry.width,
                         geometry.height);

  eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);
  worker_->SetReactionsAllowedOnCurrentThread(true);
}

PlaygroundImplGLES::~PlaygroundImplGLES() {
  DestroySurface();
}

static std::vector<std::shared_ptr<fml::Mapping>>
ShaderLibraryMappingsForPlayground() {
  return {
      std::make_shared<fml::NonOwnedMapping>(
          impeller_entity_shaders_gles_data,
          impeller_entity_shaders_gles_length),
      std::make_shared<fml::NonOwnedMapping>(
          impeller_modern_shaders_gles_data,
          impeller_modern_shaders_gles_length),
      std::make_shared<fml::NonOwnedMapping>(
          impeller_framebuffer_blend_shaders_gles_data,
          impeller_framebuffer_blend_shaders_gles_length),
      std::make_shared<fml::NonOwnedMapping>(
          impeller_fixtures_shaders_gles_data,
          impeller_fixtures_shaders_gles_length),
      std::make_shared<fml::NonOwnedMapping>(
          impeller_modern_fixtures_shaders_gles_data,
          impeller_modern_fixtures_shaders_gles_length),
      // std::make_shared<fml::NonOwnedMapping>(
      //     impeller_imgui_shaders_gles_data, impeller_imgui_shaders_gles_length),
  };
}

// |PlaygroundImpl|
std::shared_ptr<Context> PlaygroundImplGLES::GetContext() const {
  auto gl = std::make_unique<ProcTableGLES>(CreateGLProcAddressResolver());
  if (!gl->IsValid()) {
    FML_LOG(ERROR) << "Proc table when creating a playground was invalid.";
    return nullptr;
  }

  auto context =
      ContextGLES::Create(switches_.flags, std::move(gl),
                          ShaderLibraryMappingsForPlayground(), true);
  if (!context) {
    FML_LOG(ERROR) << "Could not create context.";
    return nullptr;
  }

  auto worker_id = context->AddReactorWorker(worker_);
  if (!worker_id.has_value()) {
    FML_LOG(ERROR) << "Could not add reactor worker.";
    return nullptr;
  }
  return context;
}

// |PlaygroundImpl|
Playground::GLProcAddressResolver
PlaygroundImplGLES::CreateGLProcAddressResolver() const {
  return [this](const char* name) -> void* {
      //return reinterpret_cast<void*>(::glfwGetProcAddress(name));
      return OnProcResolver(name);
    };
}

// |PlaygroundImpl|
PlaygroundImpl::WindowHandle PlaygroundImplGLES::GetWindowHandle() const {
  return window_ecore_handle_.get();
}

// |PlaygroundImpl|
std::unique_ptr<Surface> PlaygroundImplGLES::AcquireSurfaceFrame(
    std::shared_ptr<Context> context) {

  EGLint egl_width, egl_height;
  eglQuerySurface(egl_display_, egl_surface_, EGL_WIDTH, &egl_width);
  eglQuerySurface(egl_display_, egl_surface_, EGL_HEIGHT, &egl_height);

  int width = (int)egl_width;
  int height = (int)egl_height;

  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  SurfaceGLES::SwapCallback swap_callback = [this]() -> bool {
    eglSwapBuffers(egl_display_, egl_surface_);
    return true;
  };
  return SurfaceGLES::WrapFBO(context,                         //
                              swap_callback,                   //
                              0u,                              //
                              PixelFormat::kR8G8B8A8UNormInt,  //
                              ISize::MakeWH(width, height)     //
  );
}

fml::Status PlaygroundImplGLES::SetCapabilities(
    const std::shared_ptr<Capabilities>& capabilities) {
  return fml::Status(
      fml::StatusCode::kUnimplemented,
      "PlaygroundImplGLES doesn't support setting the capabilities.");
}


bool PlaygroundImplGLES::CreateSurface(void* render_target,
                                     void* render_target_display,
                                     int32_t width,
                                     int32_t height) {
  if (render_target_display) {
    egl_display_ =
        eglGetDisplay(static_cast<wl_display*>(render_target_display));
  } else {
    egl_display_ = eglGetDisplay(tbm_dummy_display_create());
  }

  if (egl_display_ == EGL_NO_DISPLAY) {
    FML_LOG(ERROR) << "Could not get EGL display.";
    return false;
  }

  if (!ChooseEGLConfiguration()) {
    FML_LOG(ERROR) << "Could not choose an EGL configuration.";
    return false;
  }

  eglQueryString(egl_display_, EGL_EXTENSIONS);

  {
    const EGLint attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

    egl_context_ =
        eglCreateContext(egl_display_, egl_config_, EGL_NO_CONTEXT, attribs);
    if (egl_context_ == EGL_NO_CONTEXT) {
      FML_LOG(ERROR) << "Could not create an onscreen context.";
      return false;
    }

    egl_resource_context_ =
        eglCreateContext(egl_display_, egl_config_, egl_context_, attribs);
    if (egl_resource_context_ == EGL_NO_CONTEXT) {
      FML_LOG(ERROR) << "Could not create an offscreen context.";
      return false;
    }
  }

  {
    const EGLint attribs[] = {EGL_NONE};

    if (render_target_display) {
      auto* egl_window =
          static_cast<EGLNativeWindowType*>(ecore_wl2_egl_window_native_get(
              static_cast<Ecore_Wl2_Egl_Window*>(render_target)));
      egl_surface_ = eglCreateWindowSurface(egl_display_, egl_config_,
                                            *egl_window, attribs);
    }

    if (egl_surface_ == EGL_NO_SURFACE) {
      FML_LOG(ERROR) << "Could not create an onscreen window surface.";
      return false;
    }
  }

  {
    const EGLint attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};

    egl_resource_surface_ =
        eglCreatePbufferSurface(egl_display_, egl_config_, attribs);
    if (egl_resource_surface_ == EGL_NO_SURFACE) {
      FML_LOG(ERROR) << "Could not create an offscreen window surface.";
      return false;
    }
  }

  return true;
}

void PlaygroundImplGLES::DestroySurface() {
  if (egl_display_) {
    eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);

    if (EGL_NO_SURFACE != egl_surface_) {
      eglDestroySurface(egl_display_, egl_surface_);
      egl_surface_ = EGL_NO_SURFACE;
    }

    if (EGL_NO_CONTEXT != egl_context_) {
      eglDestroyContext(egl_display_, egl_context_);
      egl_context_ = EGL_NO_CONTEXT;
    }

    if (EGL_NO_SURFACE != egl_resource_surface_) {
      eglDestroySurface(egl_display_, egl_resource_surface_);
      egl_resource_surface_ = EGL_NO_SURFACE;
    }

    if (EGL_NO_CONTEXT != egl_resource_context_) {
      eglDestroyContext(egl_display_, egl_resource_context_);
      egl_resource_context_ = EGL_NO_CONTEXT;
    }

    eglTerminate(egl_display_);
    egl_display_ = EGL_NO_DISPLAY;
  }
}

bool PlaygroundImplGLES::ChooseEGLConfiguration() {
  if (!eglInitialize(egl_display_, nullptr, nullptr)) {
    FML_LOG(ERROR) << "Could not initialize the EGL display.";
    return false;
  }

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    FML_LOG(ERROR) << "Could not bind the ES API.";
    return false;
  }

  EGLint config_size = 0;
  if (!eglGetConfigs(egl_display_, nullptr, 0, &config_size)) {
    FML_LOG(ERROR) << "Could not query framebuffer configurations.";
    return false;
  }

  EGLConfig* configs = (EGLConfig*)calloc(config_size, sizeof(EGLConfig));
  if (!configs) {
    FML_LOG(ERROR) << "Failed to allocate memory for EGL configurations.";
    return false;
  }
  EGLint num_config;

  EGLint impeller_config_attribs[] = {
      // clang-format off
      EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
      EGL_RED_SIZE,        8,
      EGL_GREEN_SIZE,      8,
      EGL_BLUE_SIZE,       8,
      EGL_ALPHA_SIZE,      8,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SAMPLE_BUFFERS,  1,
      EGL_SAMPLES,         4,
      EGL_STENCIL_SIZE,    8,
      EGL_DEPTH_SIZE,      0,
      EGL_NONE
      // clang-format on
  };
  if (!eglChooseConfig(egl_display_, impeller_config_attribs, configs,
                       config_size, &num_config)) {
    free(configs);
    FML_LOG(ERROR) << "No matching configurations found.";
    return false;
  }

  int buffer_size = 32;
  EGLint size;
  for (int i = 0; i < num_config; i++) {
    eglGetConfigAttrib(egl_display_, configs[i], EGL_BUFFER_SIZE, &size);
    if (buffer_size == size) {
      egl_config_ = configs[i];
      break;
    }
  }
  free(configs);
  if (!egl_config_) {
    FML_LOG(ERROR) << "No matching configuration found.";
    return false;
  }

  return true;
}

void* PlaygroundImplGLES::OnProcResolver(const char* name) const {
  auto address = eglGetProcAddress(name);
  if (address != nullptr) {
    return reinterpret_cast<void*>(address);
  }
#define GL_FUNC(FunctionName)                     \
  else if (strcmp(name, #FunctionName) == 0) {    \
    return reinterpret_cast<void*>(FunctionName); \
  }
  GL_FUNC(eglGetCurrentDisplay)
  GL_FUNC(eglQueryString)
  GL_FUNC(glActiveTexture)
  GL_FUNC(glAttachShader)
  GL_FUNC(glBindAttribLocation)
  GL_FUNC(glBindBuffer)
  GL_FUNC(glBindFramebuffer)
  GL_FUNC(glBindRenderbuffer)
  GL_FUNC(glBindTexture)
  GL_FUNC(glBlendColor)
  GL_FUNC(glBlendEquation)
  GL_FUNC(glBlendFunc)
  GL_FUNC(glBufferData)
  GL_FUNC(glBufferSubData)
  GL_FUNC(glCheckFramebufferStatus)
  GL_FUNC(glClear)
  GL_FUNC(glClearColor)
  GL_FUNC(glClearStencil)
  GL_FUNC(glColorMask)
  GL_FUNC(glCompileShader)
  GL_FUNC(glCompressedTexImage2D)
  GL_FUNC(glCompressedTexSubImage2D)
  GL_FUNC(glCopyTexSubImage2D)
  GL_FUNC(glCreateProgram)
  GL_FUNC(glCreateShader)
  GL_FUNC(glCullFace)
  GL_FUNC(glDeleteBuffers)
  GL_FUNC(glDeleteFramebuffers)
  GL_FUNC(glDeleteProgram)
  GL_FUNC(glDeleteRenderbuffers)
  GL_FUNC(glDeleteShader)
  GL_FUNC(glDeleteTextures)
  GL_FUNC(glDepthMask)
  GL_FUNC(glDisable)
  GL_FUNC(glDisableVertexAttribArray)
  GL_FUNC(glDrawArrays)
  GL_FUNC(glDrawElements)
  GL_FUNC(glEnable)
  GL_FUNC(glEnableVertexAttribArray)
  GL_FUNC(glFinish)
  GL_FUNC(glFlush)
  GL_FUNC(glFramebufferRenderbuffer)
  GL_FUNC(glFramebufferTexture2D)
  GL_FUNC(glFrontFace)
  GL_FUNC(glGenBuffers)
  GL_FUNC(glGenerateMipmap)
  GL_FUNC(glGenFramebuffers)
  GL_FUNC(glGenRenderbuffers)
  GL_FUNC(glGenTextures)
  GL_FUNC(glGetBufferParameteriv)
  GL_FUNC(glGetError)
  GL_FUNC(glGetFloatv)
  GL_FUNC(glGetFramebufferAttachmentParameteriv)
  GL_FUNC(glGetIntegerv)
  GL_FUNC(glGetProgramInfoLog)
  GL_FUNC(glGetProgramiv)
  GL_FUNC(glGetRenderbufferParameteriv)
  GL_FUNC(glGetShaderInfoLog)
  GL_FUNC(glGetShaderiv)
  GL_FUNC(glGetShaderPrecisionFormat)
  GL_FUNC(glGetString)
  GL_FUNC(glGetUniformLocation)
  GL_FUNC(glIsTexture)
  GL_FUNC(glLineWidth)
  GL_FUNC(glLinkProgram)
  GL_FUNC(glPixelStorei)
  GL_FUNC(glReadPixels)
  GL_FUNC(glRenderbufferStorage)
  GL_FUNC(glScissor)
  GL_FUNC(glShaderSource)
  GL_FUNC(glStencilFunc)
  GL_FUNC(glStencilFuncSeparate)
  GL_FUNC(glStencilMask)
  GL_FUNC(glStencilMaskSeparate)
  GL_FUNC(glStencilOp)
  GL_FUNC(glStencilOpSeparate)
  GL_FUNC(glTexImage2D)
  GL_FUNC(glTexParameterf)
  GL_FUNC(glTexParameterfv)
  GL_FUNC(glTexParameteri)
  GL_FUNC(glTexParameteriv)
  GL_FUNC(glTexSubImage2D)
  GL_FUNC(glUniform1f)
  GL_FUNC(glUniform1fv)
  GL_FUNC(glUniform1i)
  GL_FUNC(glUniform1iv)
  GL_FUNC(glUniform2f)
  GL_FUNC(glUniform2fv)
  GL_FUNC(glUniform2i)
  GL_FUNC(glUniform2iv)
  GL_FUNC(glUniform3f)
  GL_FUNC(glUniform3fv)
  GL_FUNC(glUniform3i)
  GL_FUNC(glUniform3iv)
  GL_FUNC(glUniform4f)
  GL_FUNC(glUniform4fv)
  GL_FUNC(glUniform4i)
  GL_FUNC(glUniform4iv)
  GL_FUNC(glUniformMatrix2fv)
  GL_FUNC(glUniformMatrix3fv)
  GL_FUNC(glUniformMatrix4fv)
  GL_FUNC(glUseProgram)
  GL_FUNC(glVertexAttrib1f)
  GL_FUNC(glVertexAttrib2fv)
  GL_FUNC(glVertexAttrib3fv)
  GL_FUNC(glVertexAttrib4fv)
  GL_FUNC(glVertexAttribPointer)
  GL_FUNC(glViewport)
#define GL_FUNC_EXT(ExtFunctionName, FunctionName)                    \
  else if (strcmp(name, #ExtFunctionName) == 0) {                     \
    return reinterpret_cast<void*>(eglGetProcAddress(#FunctionName)); \
  }
  GL_FUNC_EXT(glMultiDrawArraysIndirectEXT, glMultiDrawArraysIndirect)
  GL_FUNC_EXT(glMultiDrawElementsIndirectEXT, glMultiDrawElementsIndirect)
#undef GL_FUNC_EXT
#undef GL_FUNC

  FML_LOG(ERROR) << "Could not resolve: " << name;
  return nullptr;
}

}  // namespace impeller
