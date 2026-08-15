include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9
    GIT_SHALLOW TRUE)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

find_package(OpenGL REQUIRED)

add_library(avm_showcase_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")
target_include_directories(avm_showcase_imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends")
target_link_libraries(avm_showcase_imgui PUBLIC glfw OpenGL::GL)

add_executable(avm_showcase
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/calculator_view.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/native_json_view.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/showcase_demo.cpp")
target_link_libraries(avm_showcase PRIVATE avm_showcase_imgui)
avm_add_json_adapter_includes(avm_showcase)
target_include_directories(avm_showcase PRIVATE
    "${PROJECT_SOURCE_DIR}/3p"
    "${PROJECT_SOURCE_DIR}/adapters")
avm_apply_quality(avm_showcase)

add_executable(avm_native_json_examples_test
    "${CMAKE_CURRENT_SOURCE_DIR}/../../test/native_json_examples_test.cpp")
avm_add_json_adapter_includes(avm_native_json_examples_test)
target_include_directories(avm_native_json_examples_test PRIVATE
    "${PROJECT_SOURCE_DIR}/3p"
    "${PROJECT_SOURCE_DIR}/adapters")
avm_apply_quality(avm_native_json_examples_test)
avm_enable_test_assertions(avm_native_json_examples_test)
add_test(
    NAME native_json_examples_tests
    COMMAND avm_native_json_examples_test
        "${CMAKE_CURRENT_SOURCE_DIR}/../native-json/pair.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../native-json/relation.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../native-json/integer-add.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../native-json/text.json")

if(APPLE)
    target_compile_definitions(avm_showcase PRIVATE GL_SILENCE_DEPRECATION)
endif()
