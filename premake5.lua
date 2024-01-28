workspace "FusionDxHook"
   configurations { "Release", "Debug" }
   platforms { "Win32", "Win64" }
   location "build"
   objdir ("build/obj")
   buildlog ("build/log/%{prj.name}.log")
   buildoptions {"-std:c++latest"}
   
   kind "SharedLib"
   language "C++"
   targetextension ".asi"
   characterset ("Unicode")
   staticruntime "On"
   
   defines { "rsc_CompanyName=\"ThirteenAG\"" }
   defines { "rsc_LegalCopyright=\"MIT License\""} 
   defines { "rsc_FileVersion=\"1.0.0.0\"", "rsc_ProductVersion=\"1.0.0.0\"" }
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"FusionDxHook.asi\"" }
   defines { "rsc_FileDescription=\"https://thirteenag.github.io/wfp\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/%{prj.name}\"" }
   
   files { "includes/*.h" }
   files { "source/FusionDxHook.cpp" }
   files { "source/resources/Versioninfo.rc" }
   includedirs { "source" }
   includedirs { "includes" }
   includedirs { "includes/safetyhook" }
   files { "includes/safetyhook/*.*" }
   
   pbcommands = { 
      "setlocal EnableDelayedExpansion",
      --"set \"path=" .. (gamepath) .. "\"",
      "set file=$(TargetPath)",
      "FOR %%i IN (\"%file%\") DO (",
      "set filename=%%~ni",
      "set fileextension=%%~xi",
      "set target=!path!!filename!!fileextension!",
      "if exist \"!target!\" copy /y \"!file!\" \"!target!\"",
      ")" }

   function setpaths (gamepath, exepath, scriptspath)
      scriptspath = scriptspath or "scripts/"
      if (gamepath) then
         cmdcopy = { "set \"path=" .. gamepath .. scriptspath .. "\"" }
         table.insert(cmdcopy, pbcommands)
         postbuildcommands (cmdcopy)
         debugdir (gamepath)
         if (exepath) then
            debugcommand (gamepath .. exepath)
            dir, file = exepath:match'(.*/)(.*)'
            debugdir (gamepath .. (dir or ""))
         end
      end
      targetdir ("bin")
   end
   
   filter "configurations:Debug"
      defines "DEBUG"
      symbols "On"

   filter "configurations:Release"
      defines "NDEBUG"
      optimize "On"

project "FusionDxHook"
   architecture "x32"
   setpaths("Z:/WFP/Games/PPSSPP/", "PPSSPPWindows.exe")
project "FusionDxHook64"
   architecture "x64"
   setpaths("Z:/WFP/Games/PPSSPP/", "PPSSPPWindows.exe")