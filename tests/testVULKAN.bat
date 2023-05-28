@echo off
cd x86
del /s *.txt  >nul 2>&1
copy "..\..\bin\*.asi" "*.asi"
start /W VulkanSample.exe
if exist onVkQueuePresentKHREvent.txt (
    echo VULKAN x86 Hook Test PASSED
    del onVkQueuePresentKHREvent.txt
) else (
    echo VULKAN x86 Hook Test FAILED
    exit 1
)

cd ../x64
del /s *.txt  >nul 2>&1
copy "..\..\bin\*.asi" "*.asi"

start /W VulkanSample64.exe
if exist onVkQueuePresentKHREvent.txt (
    echo VULKAN x64 Hook Test PASSED
    del onVkQueuePresentKHREvent.txt
) else (
    echo VULKAN x64 Hook Test FAILED
    exit 1
)