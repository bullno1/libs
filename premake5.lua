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

-- Samples double as documentation snippets and smoke tests.
-- They stay separate from the test suite: bcrash_handler crashes on purpose.
make_sample "bstacktrace"
make_sample "bcrash_handler"

-- Every directory under tests/ is a btest suite linked into a single binary
local test_dirs = {
  "autolist",
  "barena",
  "barray",
  "bco",
  "bent",
  "bhamt",
  "bhash",
  "bresmon",
  "bscn",
  "bseg",
  "bserial",
  "bspscq",
  "bstacktrace",
  "bsv",
  "mem_layout",
  "tlsf",
  "xincbin",
}

project "tests"
    kind "ConsoleApp"
    language "C"
    targetdir "bin/%{cfg.buildcfg}"

    -- Resources embedded by the xincbin test are resolved against this
    includedirs {
      "tests/xincbin",
    }

    files {
      "tests/main.c",
    }
    for _, dir in ipairs(test_dirs) do
      files {
        "tests/"..dir.."/*.h",
        "tests/"..dir.."/*.c",
      }
    end

    -- Only include rc as a compilable file in Windows
    filter { "system:windows" }
      files {
        "tests/xincbin/*.rc",
      }

    filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

    filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

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

  project "tests"
    filter {}
    files {
      "tests/bsfn/*.h",
      "tests/bsfn/*.c",
    }
    links { "dl" }
    dependson { "bsfn_module1", "bsfn_module2" }
end
