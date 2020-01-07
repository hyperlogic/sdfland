cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
set(PROJECT_NAME sdfland)

project(${PROJECT_NAME} LANGUAGES CXX)

if(WIN32)
    set(VCPKG_INCLUDE_DIR "$ENV{VCPKG_ROOT}/installed/x64-windows/include")
    set(VCPKG_LIB_DIR "$ENV{VCPKG_ROOT}/installed/x64-windows/lib")

    set(SDL2_LIBRARIES ${VCPKG_LIB_DIR}/SDL2.lib ${VCPKG_LIB_DIR}/manual-link/SDL2main.lib)
    set(GLEW_LIBRARIES ${VCPKG_LIB_DIR}/glew32.lib)
    set(PNG_LIBRARIES ${VCPKG_LIB_DIR}/libpng16.lib)

    include_directories($VCPKG_INCLUDE_DIR)

else()
    find_package(SDL2 REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(libpng REQUIRED)
endif()

find_package(OPENGL REQUIRED)

add_executable(${PROJECT_NAME} src/main.cpp src/sdfscene.cpp
    src/render/image.cpp
    src/render/program.cpp
    src/render/render.cpp
    src/render/texture.cpp)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS /SUBSYSTEM:WINDOWS)
endif()

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} ${GLEW_LIBRARIES} ${PNG_LIBRARIES} opengl32)
