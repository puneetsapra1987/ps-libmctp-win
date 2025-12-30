# MMBI Windows Driver & Libmctp Integration Guide

This document details the compilation, driver installation, and verification steps for the simulated Memory-Mapped BMC Interface (MMBI) on Windows.

## 1. Prerequisites
- **Visual Studio 2017/2019/2022** (Standard C/C++ tools)
- **Windows Driver Kit (WDK)** (for `devcon` and driver headers)
- **CMake** 3.5+
- **Administrator Privileges** (for driver installation)

## 2. Compilation

### Step 2.1: Build libmctp and Test Apps
Run the provided batch file to clean, build, and test automatically:
```powershell
./build.bat clean
```

Or manually:
```powershell
mkdir build
cd build
cmake ..
cmake --build .
# Check output for: mctp.lib, test_stress_mmbi.exe, MmbiDriver.sys (if WDK configured) or use provided dummy
```

### Step 2.2: Compile the Windows Driver
*Note: The `windows_driver/` folder contains standard KMDF source. You may need to create a Visual Studio Driver Project (.vcxproj) pointing to these files if not using a WDK-integrated CMake.*
Assuming you have built `MmbiDriver.sys` and verified `MmbiDriver.inf`:
1. Ensure `MmbiDriver.sys` is in the same folder as `MmbiDriver.inf` (or updated INF path).

## 3. Driver Installation
The simulation requires the custom KMDF driver to route traffic between `\\.\MMBI0` and `\\.\MMBI1`.

### Step 3.1: Enable Test Signing
Since the driver is self-signed/unsigned, enable test mode:
```powershell
bcdedit /set testsigning on
# RESTART YOUR COMPUTER NOW
```

### Step 3.2: Install via DevCon
Using the WDK `devcon.exe`:
```powershell
cd windows_driver
devcon install MmbiDriver.inf Root\MmbiDriver
```
*Verify success in Device Manager. You should see "MMBI Simulated Device" under System Devices.*

## 4. Verification & Test Execution

### Step 4.1: Run Unit Tests
These verify the core logic of `libmctp` and do not require the driver.
```powershell
./build.bat
# Expected: All Pass
```

### Step 4.2: Run Stress Tests (Driver Required)
This automated suite verifies 25MB file transfers.
```powershell
.\Debug\test_stress_mmbi.exe
```
**Expected Output:**
```text
=== Starting Stress Test: Unidirectional ===
Spawning BMC Child...
Running Host (Sender)...
[host] Initializing on \\.\MMBI0
[bmc] Initializing on \\.\MMBI1
[host] Sending...
[bmc] RX: Received message... Length: 26214400
[bmc] RX: Verification PASSED.
=== Unidirectional Test PASSED ===

=== Starting Stress Test: Bidirectional ===
...
=== Bidirectional Test PASSED ===
```

### Step 4.3: Manual Verification
Open two PowerShell Terminals:

**Terminal 1 (Receiver):**
```powershell
.\Debug\test_file_transfer.exe bmc recv
```

**Terminal 2 (Sender):**
```powershell
.\Debug\test_file_transfer.exe host send
```

## 5. Troubleshooting
- **"Failed to open device"**: Ensure driver is installed and visible in Device Manager. Check `CreateFile` error code (2 = FileNotFound, 5 = AccessDenied).
- **"Size mismatch"**: Check if `max_message_size` in `meson.options` matches the app transfer size.
