// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_PLAYGROUND_BACKEND_GLES_PLAYGROUND_IMPL_GLES_H_
#define FLUTTER_IMPELLER_PLAYGROUND_BACKEND_GLES_PLAYGROUND_IMPL_GLES_H_

#include "impeller/playground/playground_impl.h"

#include <egl.h>

#include <string>

#include "impeller/playground/tizen_window_ecore_wl2.h"

namespace impeller {

using SharedHandle = std::shared_ptr<TizenWindowEcoreWl2>;

class PlaygroundImplGLES final : public PlaygroundImpl {
 public:
  explicit PlaygroundImplGLES(PlaygroundSwitches switches, SharedHandle window_ecore_handle);

  ~PlaygroundImplGLES();

  fml::Status SetCapabilities(
      const std::shared_ptr<Capabilities>& capabilities) override;

private:
  bool CreateSurface(void* render_target,
                     void* render_target_display,
                     int32_t width,
                     int32_t height);

  void DestroySurface();
  bool ChooseEGLConfiguration();
  void* OnProcResolver(const char* name) const;

 private:
  class ReactorWorker;

  SharedHandle window_ecore_handle_;

  std::shared_ptr<ReactorWorker> worker_;

  // |PlaygroundImpl|
  std::shared_ptr<Context> GetContext() const override;

  // |PlaygroundImpl|
  WindowHandle GetWindowHandle() const override;

  // |PlaygroundImpl|
  std::unique_ptr<Surface> AcquireSurfaceFrame(
      std::shared_ptr<Context> context) override;

  // |PlaygroundImpl|
  Playground::GLProcAddressResolver CreateGLProcAddressResolver()
      const override;

  PlaygroundImplGLES(const PlaygroundImplGLES&) = delete;

  PlaygroundImplGLES& operator=(const PlaygroundImplGLES&) = delete;

  private:
    EGLConfig egl_config_ = nullptr;
    EGLDisplay egl_display_ = EGL_NO_DISPLAY;
    EGLContext egl_context_ = EGL_NO_CONTEXT;
    EGLSurface egl_surface_ = EGL_NO_SURFACE;
    EGLContext egl_resource_context_ = EGL_NO_CONTEXT;
    EGLSurface egl_resource_surface_ = EGL_NO_SURFACE;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_PLAYGROUND_BACKEND_GLES_PLAYGROUND_IMPL_GLES_H_
