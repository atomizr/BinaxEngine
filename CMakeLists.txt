cmake_minimum_required(VERSION 3.10)
project(BinaxEngine VERSION 1.0.0 LANGUAGES CXX C RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    src/Renderer/Shader.cpp
    src/Renderer/Texture.cpp
    src/Renderer/AssetRegistry.cpp
    src/Renderer/Cubemap.cpp
    src/Editor/EditorUI.cpp
    src/Editor/EditorCamera.cpp
    src/Editor/Viewport.cpp
    src/Serialization/SceneSerializer.cpp
    src/Reflect/Reflection.cpp
    src/Reflect/ComponentRegistry.cpp
    src/Reflect/EngineReflection.cpp
    src/Editor/PropertyWidgets.cpp
    src/Serialization/PropertySerializer.cpp
    ${THIRDPARTY_DIR}/glad/src/glad.c
    ${THIRDPARTY_DIR}/imgui/imgui.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_draw.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_tables.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_widgets.cpp
    ${THIRDPARTY_DIR}/imgui/ImGuizmo.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_impl_glfw.cpp
    ${THIRDPARTY_DIR}/imgui/imgui_impl_opengl3.cpp
)

# -------------------- WINDOWS RESOURCE (.rc) --------------------
if (WIN32)
    set(ICON_FILE_ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/binax.ico")
    if (NOT EXISTS "${ICON_FILE_ABSOLUTE}")
        message(WARNING "Icon not found at ${ICON_FILE_ABSOLUTE} — .exe will use default icon")
    endif()

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/version.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
        @ONLY
    )
    list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

# -------------------- INCLUDE DIRECTORIES --------------------
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
    GLFW_DLL
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# -------------------- ПОДКЛЮЧЕНИЕ БИБЛИОТЕК --------------------
set(GLFW_LIB_DIR ${THIRDPARTY_DIR}/glfw/lib-vc2022)
set(GLFW_LIBRARY ${GLFW_LIB_DIR}/glfw3dll.lib)

if (NOT EXISTS ${GLFW_LIBRARY})
    message(WARNING "GLFW import library not found at ${GLFW_LIBRARY}. Make sure the path is correct.")
endif()

find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME}
    ${GLFW_LIBRARY}
    ${OPENGL_LIBRARIES}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GLFW_LIB_DIR}/glfw3.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying glfw3.dll to output directory"
)

if(MSVC)
    # Жесткое отключение RTTI (Обязательно для твоей кастомной рефлексии)
    string(REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

    # Убираем дебажную инфу из релизных конфигураций
    foreach(flag_var
            CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE
            CMAKE_C_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_MINSIZEREL)
        string(REPLACE "/Zi" "" ${flag_var} "${${flag_var}}")
        string(REPLACE "/ZI" "" ${flag_var} "${${flag_var}}")
    endforeach()

    # Агрессивная оптимизация (Без /GL, используем стандартный подход CMake для IPO)
    set(RE_COMPILER_FLAGS "/O2 /Oi /Ot /Oy /Gw /Gy /GS-")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${RE_COMPILER_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${RE_COMPILER_FLAGS}")

    # Флаги линкера (Без /LTCG, его добавит свойство ниже)
    set(RE_LINKER_FLAGS "/DEBUG:NONE /OPT:REF /OPT:ICF /MANIFEST:NO")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${RE_LINKER_FLAGS}")

    # Включаем Link Time Code Generation (LTCG / IPO) правильным способом CMake
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT error)
    if(ipo_supported)
        set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()

    # ЖЕСТКОЕ ПОДАВЛЕНИЕ КОНФЛИКТА APIENTRY (C4005) НАПРЯМУЮ ДЛЯ ТАРГЕТА ДВИЖКА
    target_compile_options(${PROJECT_NAME} PRIVATE /wd4005)
endif()