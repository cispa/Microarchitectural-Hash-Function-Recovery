include(FetchContent)

function(message)
    if (NOT MESSAGE_QUIET)
        _message(${ARGN})
    endif()
endfunction()

# === PCG Random ====================================================================

message(STATUS "Fetching external dependency PCG-Random")
set(MESSAGE_QUIET OFF)

FetchContent_Declare(
        pcgrand
        GIT_REPOSITORY https://github.com/imneme/pcg-cpp.git
        GIT_TAG v0.98.1
)

FetchContent_MakeAvailable(pcgrand)

set(MESSAGE_QUIET OFF)
message(STATUS "Fetching external dependency PCG-Random -- done!")

# === CLI11 =========================================================================

message(STATUS "Fetching external dependency CLI11")
set(MESSAGE_QUIET OFF)

FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
  GIT_TAG        v2.2.0
)

FetchContent_MakeAvailable(cli11)

set(MESSAGE_QUIET OFF)
message(STATUS "Fetching external dependency CLI11 -- done!")

# === Plog ===========================================================================

message(STATUS "Fetching external dependency Plog")
set(MESSAGE_QUIET OFF)

FetchContent_Declare(
  plog
  GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
  GIT_TAG        1.1.9
)

FetchContent_MakeAvailable(plog)

set(MESSAGE_QUIET OFF)
message(STATUS "Fetching external dependency Plog -- done!")

# === Pteditor =======================================================================

message(STATUS "Fetching external dependency Pteditor")
set(MESSAGE_QUIET OFF)

FetchContent_Declare(
  pteditor
  GIT_REPOSITORY https://github.com/s8lvg/PTEditor.git
)

FetchContent_MakeAvailable(pteditor)

set(MESSAGE_QUIET OFF)
message(STATUS "Fetching external dependency Pteditor -- done!")

