# libmctp-mmbi (Windows Port)

This is a specialized port of `libmctp` for Windows, focused on the Memory-Mapped BMC Interface (MMBI).

## Project Structure
- `src/`: Core MCTP implementation and MMBI binding.
- `include/`: Public and internal headers.
- `tests/`: Unit tests and stress tests for connectivity.
- `windows_driver/`: Windows KMDF driver for the MMBI hardware.

## Building
This project primarily uses CMake for Windows builds.

### Automated Build (Recommended)
Run the provided batch file to clean, build, and test automatically:
```powershell
./build.bat clean
```

### Manual CMake (Visual Studio)
```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Debug
```

## Testing
Run the test executables in the `build/Debug` or `build` directory:
- `test_core.exe`: Core library logic.
- `test_mmbi.exe`: MMBI binding loopback.
- `test_stress_mmbi.exe`: 25MB throughput test.

## License
This project is licensed under the GPL-2.0 License.
