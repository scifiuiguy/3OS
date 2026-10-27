# Native binaries (Windows + Quest)

| Platform | Path in `3OS_Unity` | Build |
|----------|---------------------|--------|
| Windows Editor | `Plugins/Windows/3os_kernel.dll` | Desktop CMake target `3os_kernel` |
| Android / Quest | `Plugins/Android/lib3os_kernel.so` | NDK Android build |

**Sample override:** `3OS_Unity_Sample_Project/Assets/Plugins/Android/lib3os_kernel.so` is copied into Quest APKs and shadows the UPM package. After every Android kernel rebuild, sync both locations:

**One command (recommended):** `.\tools\build_final_demo.ps1` — if kernel sources are newer than the embedded SO, it runs `build_android_kernel.ps1` (cmake + sync) then builds the APK.

Manual only if you want native without Unity:

```powershell
.\tools\build_android_kernel.ps1
```

Use `-SkipKernelRebuild` on `build_final_demo.ps1` to fail instead of auto-rebuild; `-AllowStaleKernel` to skip native entirely (not for gate tests).

Log verification on device: logcat tag `3OS_Kernel` line `build_stamp=…`, and Unity `[3OS] native ready … kernel_build=…`.
