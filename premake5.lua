-- This repository only exists to produce the .asi that the applications in tests/
-- are started with, so nothing else but that is configured here. Visual Studio 2026
-- has no mixed platform solutions, therefore the 32 bit and the 64 bit build are a
-- workspace each.
function FusionDxHookSetup(name, platform, arch)
   workspace (name)
      configurations { "Release", "Debug" }
      platforms { platform }
      architecture (arch)
      location "build"
      objdir ("build/obj")
      targetdir ("bin")

      kind "SharedLib"
      language "C++"
      targetextension ".asi"
      characterset ("Unicode")
      staticruntime "On"
      cppdialect "C++latest"

      -- read by source/resources/Versioninfo.rc
      defines { "rsc_CompanyName=\"ThirteenAG\"" }
      defines { "rsc_LegalCopyright=\"MIT License\"" }
      defines { "rsc_FileVersion=\"1.0.0.0\"", "rsc_ProductVersion=\"1.0.0.0\"" }
      defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"FusionDxHook.asi\"" }
      defines { "rsc_FileDescription=\"https://thirteenag.github.io/wfp\"" }
      defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/FusionDxHook\"" }

      files { "includes/*.h" }
      files { "includes/safetyhook/*.*" }
      files { "source/FusionDxHook.cpp" }
      files { "source/resources/Versioninfo.rc" }
      includedirs { "includes" }
      includedirs { "includes/safetyhook" }
      includedirs { "source" }
      includedirs { "source/resources" }

      filter "configurations:Debug"
         defines "DEBUG"
         symbols "On"

      filter "configurations:Release"
         defines "NDEBUG"
         optimize "On"

      filter {}
end

FusionDxHookSetup("FusionDxHook", "Win32", "x86")

project "FusionDxHook"

FusionDxHookSetup("FusionDxHook64", "x64", "x64")

project "FusionDxHook64"