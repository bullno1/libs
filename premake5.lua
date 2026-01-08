local function make_project(name)
  project(name)
    kind "ConsoleApp"
    language "C"
    targetdir "bin/%{cfg.buildcfg}"

    includedirs {
      "tests/"..name,
    }

    files {
      "tests/"..name.."/*.h",
      "tests/"..name.."/*.c",
    }

    -- Only include rc as a compilable file in Windows
    filter { "system:windows" }
       files {
         "tests/"..name.."/*.rc",
       }

    filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

    filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
end

workspace "libs"
  location(_ACTION)
  configurations { "Debug", "Release" }
  architecture "x86_64"

  warnings "Extra"
  fatalwarnings { "All" }

  filter { "action:vs*" }
    buildoptions {
      "/std:c11",
      "/experimental:c11atomics",
    }
    disablewarnings {
      "4100",
      "4200",
      "4152",
    }

  debugdir "bin/%{cfg.buildcfg}"

make_project "autolist"
make_project "xincbin"
make_project "mem_layout"
make_project "barena"
make_project "tlsf"
make_project "bresmon"
make_project "bhash"
make_project "bcoro"
make_project "bserial"
make_project "bspscq"

project "tests"
    kind "ConsoleApp"
    language "C"
    targetdir "bin/%{cfg.buildcfg}"

    files {
      "tests/main.c",
      "tests/barray/*.h",
      "tests/barray/*.c",
      "tests/bent/*.h",
      "tests/bent/*.c",
      "tests/bsv/*.h",
      "tests/bsv/*.c",
    }

    filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

    filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
