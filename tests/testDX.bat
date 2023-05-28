@echo off
cd x86
del /s *.txt  >nul 2>&1
copy "..\..\bin\*.asi" "*.asi"

start /W D3D8Sample.exe
if exist onPresentEvent.txt (
    echo D3D8 x86 Hook Test PASSED
    del onPresentEvent.txt
) else (
    echo D3D8 x86 Hook Test FAILED
    exit 1
)
start /W D3D9Sample.exe
if exist onPresentEvent.txt (
    echo D3D9 x86 Hook Test PASSED
    del onPresentEvent.txt
) else (
    echo D3D9 x86 Hook Test FAILED
    exit 1
)
start /W D3D10Sample.exe
if exist onPresentEvent.txt (
    echo D3D10 x86 Hook Test PASSED
    del onPresentEvent.txt
) else (
    echo D3D10 x86 Hook Test FAILED
    exit 1
)
start /W D3D10_1Sample.exe
if exist onPresentEvent.txt (
    echo D3D10_1 x86 Hook Test PASSED
    del onPresentEvent.txt
) else (
    echo D3D10_1 x86 Hook Test FAILED
    exit 1
)
start /W D3D11Sample.exe
if exist onPresentEvent.txt (
    echo D3D11 x86 Hook Test PASSED
    del onPresentEvent.txt
) else (
    echo D3D11 x86 Hook Test FAILED
    exit 1
)
start /W D3D12Sample.exe
if exist onPresentEvent.txt (
    echo D3D12 x86 Hook Test PASSED
    del onPresentEvent.txt
) else (
    echo D3D12 x86 Hook Test FAILED
    exit 1
)
start /W OpenGLSample.exe
if exist onSwapBuffersEvent.txt (
    echo OPENGL x86 Hook Test PASSED
    del onSwapBuffersEvent.txt
) else (
    echo OPENGL x86 Hook Test FAILED
    exit 1
)

cd ../x64
del /s *.txt  >nul 2>&1
copy "..\..\bin\*.asi" "*.asi"

start /W D3D9Sample64.exe
if exist onPresentEvent.txt (
    echo D3D9 x64 Hook Test PASSED
    del onPresentEvent.txt
) else (
    echo D3D9 x64 Hook Test FAILED
    exit 1
)
start /W D3D10Sample64.exe
if exist onPresentEvent.txt (
    echo D3D10 x64 Hook Test PASSED
    del onPresentEvent.txt
) else (
    echo D3D10 x64 Hook Test FAILED
    exit 1
)
start /W D3D10_1Sample64.exe
if exist onPresentEvent.txt (
    echo D3D10_1 x64 Hook Test PASSED
    del onPresentEvent.txt
) else (
    echo D3D10_1 x64 Hook Test FAILED
    exit 1
)
start /W D3D11Sample64.exe
if exist onPresentEvent.txt (
    echo D3D11 x64 Hook Test PASSED
    del onPresentEvent.txt
) else (
    echo D3D11 x64 Hook Test FAILED
    exit 1
)
start /W D3D12Sample64.exe
if exist onPresentEvent.txt (
    echo D3D12 x64 Hook Test PASSED
    del onPresentEvent.txt
) else (
    echo D3D12 x64 Hook Test FAILED
    exit 1
)
start /W OpenGLSample64.exe
if exist onSwapBuffersEvent.txt (
    echo OPENGL x64 Hook Test PASSED
    del onSwapBuffersEvent.txt
) else (
    echo OPENGL x64 Hook Test FAILED
    exit 1
)
