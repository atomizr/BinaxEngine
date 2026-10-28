cmake_minimum_required(VERSION 3.20)
project(BinaxEngine VERSION 1.0.0 LANGUAGES CXX C RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)

set(SOURCES
    src/main.cpp
    src/Core/Input.cpp
    src/Core/Timer.cpp
    src/Core/Window.cpp
    src/Utils/Log.cpp

    src/Renderer/DeferredRenderer.cpp
    src/Renderer/Framebuffer.cpp
    src/Renderer/GBuffer.cpp
    src/Renderer/Mesh.cpp
    src/Renderer/MeshLibrary.cpp
    src/Renderer/MaterialLibrary.cpp
    src/Renderer/InstanceBuffer.cpp
    src/Renderer/Shader.cpp
    src/Renderer/Texture.cpp
    src/Renderer/AssetRegistry.cpp
    src/Renderer/Cubemap.cpp

    src/Editor/EditorUI.cpp
    src/Editor/EditorCamera.cpp
    src/Editor/Viewport.cpp
    src/Editor/PropertyWidgets.cpp

    src/Serialization/SceneSerializer.cpp
    src/Serialization/PropertySerializer.cpp
    src/Serialization/MaterialSerializer.cpp
    src/Serialization/PostProcessSerializer.cpp

    src/Reflect/Reflection.cpp
    src/Reflect/ComponentRegistry.cpp
    src/Reflect/EngineReflection.cpp

    ${THIRDPARTY_DIR}/glad/src/glad.c
    ${THIRDPARTY_DIR}/imgui/imgui.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_draw.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_tables.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_widgets.cpp
    ${THIRDPARTY_DIR}/imgui/ImGuizmo.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_impl_glfw.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_impl_opengl3.cpp
)

if (WIN32)
    set(ICON_FILE_ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/binax.ico")
    if (EXISTS "${ICON_FILE_ABSOLUTE}")
        file(TO_CMAKE_PATH "${ICON_FILE_ABSOLUTE}" BINAX_ICON_PATH)
        set(BINAX_ICON_RESOURCE "IDI_ICON1 ICON \"${BINAX_ICON_PATH}\"")
    else()
        message(WARNING "Icon not found at ${ICON_FILE_ABSOLUTE} — .exe will use default icon")
        set(BINAX_ICON_RESOURCE "// No icon: resources/icons/binax.ico is missing")
    endif()

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/version.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
        @ONLY
    )
    list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${THIRDPARTY_DIR}
    ${THIRDPARTY_DIR}/entt
    ${THIRDPARTY_DIR}/glad/include
    ${THIRDPARTY_DIR}/glfw/include
    ${THIRDPARTY_DIR}/glm
    ${THIRDPARTY_DIR}/imgui
    ${THIRDPARTY_DIR}/nlohmann
    ${THIRDPARTY_DIR}/spdlog
    ${THIRDPARTY_DIR}/stb
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

set(GLFW_LIB_DIR ${THIRDPARTY_DIR}/glfw/lib-vc2022)
set(GLFW_LIBRARY ${GLFW_LIB_DIR}/glfw3_mt.lib)

if (NOT EXISTS ${GLFW_LIBRARY})
    message(WARNING "GLFW static library not found at ${GLFW_LIBRARY}. Make sure the path is correct.")
endif()

find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME}
    ${GLFW_LIBRARY}
    ${OPENGL_LIBRARIES}
)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
        COMMENT "Copying runtime resources to output directory"
    )
endif()

if(MSVC)
    if (MSVC_VERSION LESS 1940)
        message(FATAL_ERROR
            "BinaxEngine requires MSVC 19.40+ (Visual Studio 2022 17.10+). "
            "Current toolset: ${MSVC_VERSION}. Select a newer toolset, e.g. "
            "MSBuild /p:VCToolsVersion=14.44.35207 or "
            "Project Properties -> General -> Platform Toolset.")
    endif()

    get_filename_component(_binax_toolset "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_binax_toolset "${_binax_toolset}" DIRECTORY)
    get_filename_component(_binax_toolset "${_binax_toolset}" DIRECTORY)
    get_filename_component(_binax_toolset "${_binax_toolset}" DIRECTORY)
    get_filename_component(_binax_toolset_version "${_binax_toolset}" NAME)
    if (_binax_toolset_version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        set_property(TARGET ${PROJECT_NAME} PROPERTY
                     VS_GLOBAL_VCToolsVersion "${_binax_toolset_version}")
        message(STATUS "MSVC toolset pinned to ${_binax_toolset_version}")
    else()
        message(WARNING "Could not derive MSVC toolset version from "
                        "${CMAKE_CXX_COMPILER}; build may pick a different toolset")
    endif()

    string(REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

    foreach(flag_var
            CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE
            CMAKE_C_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_MINSIZEREL)
        string(REPLACE "/Zi" "" ${flag_var} "${${flag_var}}")
        string(REPLACE "/ZI" "" ${flag_var} "${${flag_var}}")
    endforeach()

    set(RE_COMPILER_FLAGS "/O2 /Oi /Ot /Oy /Gw /Gy /GS-")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${RE_COMPILER_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${RE_COMPILER_FLAGS}")

    set(RE_LINKER_FLAGS "/DEBUG:NONE /OPT:REF /OPT:ICF /MANIFEST:NO")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${RE_LINKER_FLAGS}")

    target_compile_options(${PROJECT_NAME} PRIVATE /wd4005)
endif()
