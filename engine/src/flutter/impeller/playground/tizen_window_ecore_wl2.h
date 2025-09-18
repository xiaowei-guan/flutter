// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_PLAYGROUND_TIZEN_WINDOW_ECORE_WL2_H_
#define FLUTTER_IMPELLER_PLAYGROUND_TIZEN_WINDOW_ECORE_WL2_H_

#define EFL_BETA_API_SUPPORT
#include <Ecore_Wl2.h>
#include <tizen-extension-client-protocol.h>

#include <cstdint>
#include <string>
#include <vector>

namespace impeller {

struct TizenGeometry {
  int32_t left = 0, top = 0, width = 0, height = 0;
};

class TizenViewEventHandlerDelegate {
 public:
  virtual void OnResize(int32_t left,
                        int32_t top,
                        int32_t width,
                        int32_t height) = 0;

  virtual void OnRotate(int32_t degree) = 0;

  virtual void OnPointerMove(double x,
                             double y,
                             size_t timestamp,

                             int32_t device_id) = 0;

  virtual void OnPointerDown(double x,
                             double y,

                             size_t timestamp,
                             int32_t device_id) = 0;

  virtual void OnPointerUp(double x,
                           double y,
                           size_t timestamp,
                           int32_t device_id) = 0;

  virtual void OnScroll(double x,
                        double y,
                        double delta_x,
                        double delta_y,
                        size_t timestamp,
                        int32_t device_id) = 0;

  virtual void OnKey(const char* key,
                     const char* string,
                     const char* compose,
                     uint32_t modifiers,
                     uint32_t scan_code,
                     const char* device_name,
                     bool is_down) = 0;

  virtual void OnComposeBegin() = 0;

  virtual void OnComposeChange(const std::string& str, int cursor_pos) = 0;

  virtual void OnComposeEnd() = 0;

  virtual void OnCommit(const std::string& str) = 0;
};

class TizenWindowEcoreWl2 {
 public:
  TizenWindowEcoreWl2(TizenGeometry geometry,
                      bool transparent,
                      bool focusable,
                      bool top_level,
                      bool pointing_device_support,
                      bool floating_menu_support,
                      void* window_handle,
                      bool is_vulkan);

  ~TizenWindowEcoreWl2();

  TizenGeometry GetGeometry();

  bool SetGeometry(TizenGeometry geometry);

  TizenGeometry GetScreenGeometry();

  void* GetRenderTarget();

  void* GetRenderTargetDisplay() { return wl2_display_; }

  void* GetNativeHandle() { return ecore_wl2_window_; }

  int32_t GetRotation();

  int32_t GetDpi();

  uintptr_t GetWindowId();

  uint32_t GetResourceId();

  void SetPreferredOrientations(const std::vector<int>& rotations);

  void BindKeys(const std::vector<std::string>& keys);

  void Show();

  void UpdateFlutterCursor(const std::string& kind);

 private:
  bool CreateWindow(void* window_handle);

  void DestroyWindow();

  void SetWindowOptions();

  void EnableCursor();

  void SetPointingDeviceSupport();

  void SetFloatingMenuSupport();

  void ShowUnsupportedToast();

  void RegisterEventHandlers();

  void UnregisterEventHandlers();

  void SetTizenPolicyNotificationLevel(int level);

  Ecore_Wl2_Display* ecore_wl2_display_ = nullptr;
  Ecore_Wl2_Window* ecore_wl2_window_ = nullptr;
  Ecore_Wl2_Egl_Window* ecore_wl2_egl_window_ = nullptr;
  wl_display* wl2_display_ = nullptr;
  wl_surface* wl2_surface_ = nullptr;
  std::vector<Ecore_Event_Handler*> ecore_event_handlers_;
  tizen_policy* tizen_policy_ = nullptr;
  uint32_t resource_id_ = 0;

  TizenGeometry initial_geometry_ = {0, 0, 0, 0};
  bool transparent_ = false;
  bool focusable_ = false;
  bool top_level_ = false;

  bool pointing_device_support_ = true;
  bool floating_menu_support_ = true;
  bool show_unsupported_toast_ = false;
  bool is_vulkan_ = false;
   TizenViewEventHandlerDelegate* view_delegate_ = nullptr;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_PLAYGROUND_TIZEN_WINDOW_ECORE_WL2_H_
