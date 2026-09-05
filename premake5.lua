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

local function make_sample(name)
  project(name)
    kind "ConsoleApp"
    language "C"
    targetdir "bin/%{cfg.buildcfg}"

    files {
      "samples/"..name..".c",
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
  filter {"system:windows", "action:vs*"}
    systemversion "10.0.26100.0"

  warnings "Extra"
  fatalwarnings { "All" }

  filter { "action:vs*" }
    buildoptions {
      "/std:clatest",
      "/experimental:c11atomics",
    }
    defines { "_CRT_SECURE_NO_WARNINGS" }
    disablewarnings {
      "4100",
      "4200",
      "4152",
      "4459",
      "4324",
      "4116",
      "4189", -- Too many misdiagnostics
    }

  debugdir "bin/%{cfg.buildcfg}"

make_project "autolist"
make_project "xincbin"
make_project "mem_layout"
make_project "barena"
make_project "tlsf"
make_project "bresmon"
make_project "bhash"
make_project "bserial"
make_project "bspscq"
make_sample "bstacktrace"
make_sample "bcrash_handler"

-- bsfn only supports Linux: the test dlopens two builds of the same module
if os.target() == "linux" then
  local function make_bsfn_module(variant)
    project("bsfn_module"..variant)
      kind "SharedLib"
      language "C"
      targetdir "bin/%{cfg.buildcfg}"
      targetprefix ""
      pic "On"

      files {
        "tests/bsfn/module/module.c",
      }

      defines { "BSFN_MODULE_VARIANT="..variant }

      filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

      filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
  end

  make_bsfn_module(1)
  make_bsfn_module(2)

  make_project "bsfn"
    links { "dl" }
    dependson { "bsfn_module1", "bsfn_module2" }
end

project "tests"
    kind "ConsoleApp"
    language "C"
    targetdir "bin/%{cfg.buildcfg}"

    files {
      "tests/main.c",
      "tests/barray/*.h",
      "tests/barray/*.c",
      "tests/bseg/*.h",
      "tests/bseg/*.c",
      "tests/bhamt/*.h",
      "tests/bhamt/*.c",
      "tests/bent/*.h",
      "tests/bent/*.c",
      "tests/bsv/*.h",
      "tests/bsv/*.c",
      "tests/bscn/*.h",
      "tests/bscn/*.c",
      "tests/bco/*.h",
      "tests/bco/*.c",
      "tests/bstacktrace/*.c",
    }

    filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

    filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
