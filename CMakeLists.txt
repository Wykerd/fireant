cmake_minimum_required(VERSION 3.4)

project(fireant LANGUAGES C)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Debug")
    set(CMAKE_BUILD_TYPE "Debug")
endif()
message(STATUS "Building in ${CMAKE_BUILD_TYPE} mode")
message(STATUS "Building with ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION} on ${CMAKE_SYSTEM} for platform ${CMAKE_SYSTEM_NAME}")

add_library(fireant STATIC
    src/runtime.c
    src/version.c
    src/utils.c
    src/modules.c
    src/std.c
)

add_executable(fa-cli
    src/cli.c
)

string(TOLOWER ${CMAKE_SYSTEM_NAME} FA_PLATFORM)

target_compile_definitions(fireant PRIVATE 
    FA_PLATFORM="${FA_PLATFORM}" 
    FA_VERSION_MAJOR=0
    FA_VERSION_MINOR=1
    FA_VERSION_PATCH=0
    FA_VERSION_SUFFIX="-indev.1"
)

add_subdirectory(deps/quickjs)

include_directories(deps/quickjs/src)

set_target_properties(fireant PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_link_libraries(fireant quickjs m uv)
target_link_libraries(fa-cli fireant)