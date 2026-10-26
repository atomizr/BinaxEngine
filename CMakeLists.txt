# 3.20 — минимальная версия, которая полноценно поддерживает C++20
# стандарт и toolset'ы Visual Studio 2022 (раньше стояло 3.10).
cmake_minimum_required(VERSION 3.20)
project(BinaxEngine VERSION 1.0.0 LANGUAGES CXX C RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Статический CRT (/MT): .exe не зависит от vcruntime140.dll / msvcp140.dll
# и запускается на машине без установленного VC++ Redistributable.
# Должно задаваться до add_executable(), иначе CMake проигнорирует.
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

    # ---- Renderer ----
    src/Renderer/DeferredRenderer.cpp
    src/Renderer/Framebuffer.cpp
    src/Renderer/GBuffer.cpp
    src/Renderer/Mesh.cpp
    src/Renderer/MeshLibrary.cpp          # NEW
    src/Renderer/MaterialLibrary.cpp      # NEW
    src/Renderer/InstanceBuffer.cpp       # NEW
    src/Renderer/Shader.cpp
    src/Renderer/Texture.cpp
    src/Renderer/AssetRegistry.cpp
    src/Renderer/Cubemap.cpp

    # ---- Editor ----
    src/Editor/EditorUI.cpp
    src/Editor/EditorCamera.cpp
    src/Editor/Viewport.cpp
    src/Editor/PropertyWidgets.cpp

    # ---- Serialization ----
    src/Serialization/SceneSerializer.cpp
    src/Serialization/PropertySerializer.cpp
    src/Serialization/MaterialSerializer.cpp
    src/Serialization/PostProcessSerializer.cpp

    # ---- Reflection ----
    src/Reflect/Reflection.cpp
    src/Reflect/ComponentRegistry.cpp
    src/Reflect/EngineReflection.cpp

    # ---- Third party ----
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
    if (EXISTS "${ICON_FILE_ABSOLUTE}")
        file(TO_CMAKE_PATH "${ICON_FILE_ABSOLUTE}" BINAX_ICON_PATH)
        set(BINAX_ICON_RESOURCE "IDI_ICON1 ICON \"${BINAX_ICON_PATH}\"")
    else()
        # [FIX] Раньше путь к иконке подставлялся в .rc безусловно, и при
        #       отсутствующем файле сборка падала с RC2135. Теперь ресурс
        #       просто не объявляется — .exe получит иконку по умолчанию.
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
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# -------------------- ПОДКЛЮЧЕНИЕ БИБЛИОТЕК --------------------
# GLFW линкуется статически, вместе со статическим CRT (/MT) — на выходе
# получается автономный .exe без glfw3.dll и без DLL рантайма Visual C++.
#   glfw3_mt.lib — статическая, /MT  (используется здесь)
#   glfw3.lib    — статическая, /MD  (не подходит: конфликт с /MT)
#   glfw3dll.lib — импорт для glfw3.dll (не используется)
# Определение GLFW_DLL НЕ задаём: иначе glfw3.h объявит функции как dllimport.
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

# -------------------- RUNTIME ASSETS --------------------
# Шейдеры/шрифты/иконки грузятся относительно рабочего каталога или
# каталога exe. Чтобы свежая сборка не оставалась без ресурсов, копируем
# их рядом с .exe (если каталог resources есть в дереве исходников).
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
        COMMENT "Copying runtime resources to output directory"
    )
endif()

if(MSVC)
    # [FIX] Старые тулчейны не тянут этот код: MSVC 19.31 (VS 2022 17.4,
    #       "v143 default") падает с внутренней ошибкой C1001 на
    #       инстанцировании ComponentRegistry::Register и не поддерживает
    #       часть C++20-конструкций EnTT. Внятное сообщение лучше ICE.
    if (MSVC_VERSION LESS 1940)
        message(FATAL_ERROR
            "BinaxEngine requires MSVC 19.40+ (Visual Studio 2022 17.10+). "
            "Current toolset: ${MSVC_VERSION}. Select a newer toolset, e.g. "
            "MSBuild /p:VCToolsVersion=14.44.35207 or "
            "Project Properties -> General -> Platform Toolset.")
    endif()

    # [FIX] CMake проверяет компилятор при конфигурации, а MSBuild при сборке
    #       берёт версию тулсета из Microsoft.VCToolsVersion.v143.default.props.
    #       Эти версии могут расходиться (на машине разработчика v143 указывает
    #       на 14.31, хотя CMake нашёл 14.44), проверка MSVC_VERSION выше такой
    #       рассинхрон не видит, и сборка падает внутренней ошибкой компилятора
    #       C1001 на ComponentRegistry::Register. Закрепляем в проекте ровно тот
    #       тулсет, которым CMake проверял код: тогда конфигурация и сборка
    #       всегда используют один и тот же компилятор.
    # cl.exe лежит в <VS>/VC/Tools/MSVC/<version>/bin/Host<arch>/<arch>/cl.exe
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

    # RTTI отключён: движок не использует dynamic_cast/typeid
    # (рефлексия здесь статическая, на шаблонах).
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

    # LTCG / IPO намеренно отключён: при статическом CRT (/MT) связка
    # /GL + /MT на MSVC 14.44 падает на этапе линковки с LNK2016
    # («абсолютный символ @comp.id»). Оптимизация /O2 /Oi /Ot /Oy остаётся,
    # а сборка и линковка при этом заметно быстрее.
    # Если понадобится LTCG — вернуть INTERPROCEDURAL_OPTIMIZATION_RELEASE,
    # но тогда придётся собирать с динамическим CRT (/MD) и тянуть DLL рантайма.

    # ЖЕСТКОЕ ПОДАВЛЕНИЕ КОНФЛИКТА APIENTRY (C4005) НАПРЯМУЮ ДЛЯ ТАРГЕТА ДВИЖКА
    target_compile_options(${PROJECT_NAME} PRIVATE /wd4005)
endif()
