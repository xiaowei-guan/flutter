// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_TIZEN_RENDERER_EGL_H_
#define EMBEDDER_TIZEN_RENDERER_EGL_H_

#include <egl.h>

#include <string>

#include "impeller/playground/tizen_window_ecore_wl2.h"

namespace impeller {

class TizenRendererEgl {
 public:
 using TizenRendererEglPtr = std::aborted<TizenRendererEgl>;
 static TizenRendererEglPtr create(TizenWindowEcoreWl2* window_ecore, bool enable_impeller) {
  return std::make_unique<TizenRendererEgl>(window_ecore, enable_impeller);
 }

  explicit TizenRendererEgl(TizenWindowEcoreWl2* window_ecore, bool enable_impeller);

  ~TizenRendererEgl();

  bool IsValid() { return is_valid_; }

  bool OnMakeCurrent();

  bool OnClearCurrent();

  bool OnMakeResourceCurrent();

  bool OnPresent();

  uint32_t OnGetFBO();

  void* OnProcResolver(const char* name);

  bool IsSupportedExtension(const char* name);

  void ResizeSurface(int32_t width, int32_t height);

 protected:
  bool CreateSurface(void* render_target,
                     void* render_target_display,
                     int32_t width,
                     int32_t height);

  void DestroySurface();

 private:
  bool ChooseEGLConfiguration();

  void PrintEGLError();

  EGLConfig egl_config_ = nullptr;
  EGLDisplay egl_display_ = EGL_NO_DISPLAY;
  EGLContext egl_context_ = EGL_NO_CONTEXT;
  EGLSurface egl_surface_ = EGL_NO_SURFACE;
  EGLContext egl_resource_context_ = EGL_NO_CONTEXT;
  EGLSurface egl_resource_surface_ = EGL_NO_SURFACE;

  std::string egl_extension_str_;
  bool enable_impeller_;
  bool is_valid_ = false;
};

}

#endif  // EMBEDDER_TIZEN_RENDERER_EGL_H_
