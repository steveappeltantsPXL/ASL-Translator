Kill any running VisearASLTranslator.exe process so the linker can overwrite the binary,
then do a full CMake Debug build, then offer to launch the app.

Steps:
1. Kill any running instance:
   powershell -Command "Stop-Process -Name VisearASLTranslator -ErrorAction SilentlyContinue"

2. Build (from project root):
   cmake --build cmake-build-debug-msvc --config Debug 2>&1

   Report compiler/linker errors clearly. If the build failed, show the first error and
   suggest a fix before asking to retry.

3. If the build succeeded, ask the user whether to launch the app:
   .\cmake-build-debug-msvc\Debug\VisearASLTranslator.exe
