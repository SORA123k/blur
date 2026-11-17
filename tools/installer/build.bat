@echo off
rem Builds the tiny per-user installer stub. Requires MSVC + Windows SDK.
rem The release exe is: stub.exe + payload.zip + a 16 byte footer
rem (8 byte zip offset, then the ascii magic "BLURZIP1").
rem
rem   build.bat                -> stub.exe
rem   package.ps1 <zip> <out>  -> setup exe
setlocal
where cl >nul 2>&1
if errorlevel 1 (
    echo cl.exe not found. Run this from a "x64 Native Tools Command Prompt for VS".
    exit /b 1
)
cl /nologo /O2 /MT /EHsc /std:c++20 /utf-8 /DUNICODE /D_UNICODE installer.cpp ^
   /link /SUBSYSTEM:WINDOWS /OUT:stub.exe ^
   user32.lib shell32.lib ole32.lib advapi32.lib
@endlocal
