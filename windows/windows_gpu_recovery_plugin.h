#ifndef FLUTTER_PLUGIN_WINDOWS_GPU_RECOVERY_PLUGIN_H_
#define FLUTTER_PLUGIN_WINDOWS_GPU_RECOVERY_PLUGIN_H_

#include <flutter/plugin_registrar_windows.h>
#include <flutter_windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <memory>
#include <optional>

#include "include/windows_gpu_recovery/gpu_recovery_message.h"

namespace windows_gpu_recovery {

/// How often to poll the sentinel D3D device for removal (ms).
constexpr UINT_PTR kGpuWatchdogTimerId = 0x475056;
constexpr UINT kWatchdogIntervalMs = 2000;

/// Detects GPU device loss and triggers engine recreation.
///
/// Detection: a sentinel D3D11 device is created on the same adapter as
/// ANGLE at startup. When the GPU resets (sleep, TDR, VM restore), the
/// sentinel's GetDeviceRemovedReason() returns DEVICE_REMOVED permanently.
/// The Windows Application event log is also watched for new Windows Error
/// Reporting LiveKernelEvent records with problem signature P1=141.
///
/// Recovery: a Vectored Exception Handler is installed to catch crashes
/// during ANGLE cleanup. The plugin posts WM_GPU_RECOVERY to the host
/// window, which should destroy and recreate the FlutterViewController.
class WindowsGpuRecoveryPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

  WindowsGpuRecoveryPlugin(flutter::PluginRegistrarWindows* registrar,
                            flutter::FlutterView* view);
  virtual ~WindowsGpuRecoveryPlugin();

  WindowsGpuRecoveryPlugin(const WindowsGpuRecoveryPlugin&) = delete;
  WindowsGpuRecoveryPlugin& operator=(const WindowsGpuRecoveryPlugin&) = delete;

 private:
  /// Handles WM_TIMER for the watchdog.
  std::optional<LRESULT> HandleWindowProc(HWND hwnd, UINT message,
                                          WPARAM wparam, LPARAM lparam);

  /// Returns true if the sentinel D3D device reports DEVICE_REMOVED.
  bool IsDeviceLost();

  /// Returns true if a new WER LiveKernelEvent with P1=141 was logged.
  bool IsLiveKernelEvent141Detected();

  /// Opens the Application event log and starts its cursor at the current end,
  /// so reports from before this plugin instance started are ignored.
  bool InitializeEventLogWatch();

  flutter::PluginRegistrarWindows* registrar_;
  flutter::FlutterView* view_;
  HWND host_hwnd_ = nullptr;
  int proc_delegate_id_ = 0;
  bool recovery_requested_ = false;

  /// Persistent D3D11 device on ANGLE's adapter. Reports DEVICE_REMOVED
  /// permanently after any GPU reset — unlike a freshly created test device
  /// which would succeed because the adapter recovers in ~100ms.
  Microsoft::WRL::ComPtr<ID3D11Device> sentinel_device_;

  /// Legacy Event Log API handle and next record cursor. Application records
  /// are readable by normal desktop applications without elevation.
  HANDLE application_event_log_ = nullptr;
  DWORD next_event_record_ = 0;
};

}  // namespace windows_gpu_recovery

#endif  // FLUTTER_PLUGIN_WINDOWS_GPU_RECOVERY_PLUGIN_H_
