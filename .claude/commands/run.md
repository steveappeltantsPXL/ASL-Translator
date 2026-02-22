Launch the VisearASLTranslator app.

Steps:
1. Check if already running:
   powershell -Command "Get-Process VisearASLTranslator -ErrorAction SilentlyContinue"

   If it is already running, tell the user and ask whether to restart it or leave it.

2. If not running (or user wants a restart, kill first):
   powershell -Command "Stop-Process -Name VisearASLTranslator -ErrorAction SilentlyContinue"

3. Launch:
   powershell -Command "Start-Process 'cmake-build-debug-msvc\Debug\VisearASLTranslator.exe'"

4. Confirm it started:
   powershell -Command "Get-Process VisearASLTranslator -ErrorAction SilentlyContinue"

   Report the PID if running, or report failure if the process isn't found.