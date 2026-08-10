cmake_minimum_required(VERSION 3.21)
project(BinaxEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(WARNING "This CMake setup expects 64-bit prebuilt GLFW libraries.")
endif()

if(MSVC)
    add_compile_options(/utf-8)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/bigobj>)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# ------------------------------------------------------------------------------
# GLFW (dynamic linking)
# ------------------------------------------------------------------------------
set(GLFW_INCLUDE_DIR "${EXTERNAL_DIR}/glfw/include")

if(MSVC)
    set(GLFW_IMPORT_LIBRARY "${EXTERNAL_DIR}/glfw/lib-vc2022/glfw3dll.lib")
    set(GLFW_RUNTIME_LIBRARY "${EXTERNAL_DIR}/glfw/lib-vc2022/glfw3.dll")
elseif(MINGW)
    set(GLFW_IMPORT_LIBRARY "${EXTERNAL_DIR}/glfw/lib-mingw-w64/libglfw3dll.a")
    set(GLFW_RUNTIME_LIBRARY "${EXTERNAL_DIR}/glfw/lib-mingw-w64/glfw3.dll")
else()
    set(GLFW_IMPORT_LIBRARY "${EXTERNAL_DIR}/glfw/lib-vc2022/glfw3dll.lib")
    set(GLFW_RUNTIME_LIBRARY "${EXTERNAL_DIR}/glfw/lib-vc2022/glfw3.dll")
endif()

foreach(_f "${GLFW_INCLUDE_DIR}/GLFW/glfw3.h" "${GLFW_IMPORT_LIBRARY}" "${GLFW_RUNTIME_LIBRARY}")
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR "GLFW file not found: ${_f}")
    endif()
endforeach()

add_library(binax_glfw SHARED IMPORTED GLOBAL)
set_target_properties(binax_glfw PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
    INTERFACE_COMPILE_DEFINITIONS "GLFW_DLL;GLFW_INCLUDE_NONE"
    IMPORTED_LOCATION "${GLFW_RUNTIME_LIBRARY}"
    IMPORTED_IMPLIB "${GLFW_IMPORT_LIBRARY}"
)
set_property(TARGET binax_glfw APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenGL::GL)

# ------------------------------------------------------------------------------
# GLAD
# ------------------------------------------------------------------------------
add_library(binax_glad STATIC "${EXTERNAL_DIR}/glad/src/glad.c")
target_include_directories(binax_glad PUBLIC "${EXTERNAL_DIR}/glad/include")
set_property(TARGET binax_glad APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenGL::GL)

# ------------------------------------------------------------------------------
# pugixml
# ------------------------------------------------------------------------------
add_library(binax_pugixml STATIC "${EXTERNAL_DIR}/xml/pugixml.cpp")
target_include_directories(binax_pugixml PUBLIC "${EXTERNAL_DIR}/xml")

# ------------------------------------------------------------------------------
# Header-only dependencies: EnTT, GLM, nlohmann/json, spdlog, stb, toml, MINIAUDIO
# ------------------------------------------------------------------------------
add_library(binax_headers INTERFACE)
target_include_directories(binax_headers INTERFACE
    "${EXTERNAL_DIR}"                # entt/, glm/, nlohmann/, spdlog/
    "${EXTERNAL_DIR}/toml"           # toml.hpp
    "${EXTERNAL_DIR}/stb"            # stb_image.h
    "${EXTERNAL_DIR}/miniaudio"      # miniaudio.h
)
target_link_libraries(binax_headers INTERFACE Threads::Threads)

# ------------------------------------------------------------------------------
# Dear ImGui + backend + ImGuizmo
# ------------------------------------------------------------------------------
add_library(binax_imgui STATIC
    "${EXTERNAL_DIR}/imgui/imgui.cpp"
    "${EXTERNAL_DIR}/imgui/imgui_demo.cpp"
    "${EXTERNAL_DIR}/imgui/imgui_draw.cpp"
    "${EXTERNAL_DIR}/imgui/imgui_tables.cpp"
    "${EXTERNAL_DIR}/imgui/imgui_widgets.cpp"
    "${EXTERNAL_DIR}/imgui/imgui_impl_glfw.cpp"
    "${EXTERNAL_DIR}/imgui/imgui_impl_opengl3.cpp"
    "${EXTERNAL_DIR}/imgui/ImGuizmo.cpp"
)
target_include_directories(binax_imgui PUBLIC "${EXTERNAL_DIR}/imgui")
target_compile_definitions(binax_imgui PUBLIC
    IMGUI_IMPL_OPENGL_LOADER_GLAD
    IMGUI_DEFINE_MATH_OPERATORS
)
target_link_libraries(binax_imgui PUBLIC
    binax_headers
    binax_glad
    binax_glfw
    OpenGL::GL
)

# ------------------------------------------------------------------------------
# Main engine interface target
# ------------------------------------------------------------------------------
add_library(BinaxEngine INTERFACE)
target_link_libraries(BinaxEngine INTERFACE
    binax_headers
    binax_glad
    binax_glfw
    binax_imgui
    binax_pugixml
    OpenGL::GL
    Threads::Threads
)

# ------------------------------------------------------------------------------
# Test executable
# ------------------------------------------------------------------------------
add_executable(BinaxTest main.cpp)
set_target_properties(BinaxTest PROPERTIES
    OUTPUT_NAME "BinaxEngine"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:BinaxTest>"
)
target_link_libraries(BinaxTest PRIVATE BinaxEngine)

if(MINGW)
    target_link_options(BinaxTest PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,--enable-auto-import
    )
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT BinaxTest)

# ------------------------------------------------------------------------------
# Copy GLFW DLL next to test.exe
# ------------------------------------------------------------------------------
add_custom_command(TARGET BinaxTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GLFW_RUNTIME_LIBRARY}"
            $<TARGET_FILE_DIR:BinaxTest>
    COMMENT "Copying GLFW runtime DLL"
    VERBATIM
)