cmake_minimum_required(VERSION 3.5.0)
project(PBRRenderTest)

set(CMAKE_CXX_FLAGS "/std:c++17")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(OutputDir ${CMAKE_BINARY_DIR}/Release)
else()
    set(OutputDir ${CMAKE_BINARY_DIR}/Debug)
endif()

file(COPY glew32.dll DESTINATION ${OutputDir})
file(COPY SDL2.dll DESTINATION ${OutputDir})
file(COPY assimp-vc140-mt.dll DESTINATION ${OutputDir})
file(COPY shaders DESTINATION ${OutputDir})
file(COPY res DESTINATION ${OutputDir})

add_executable(runtime
    main.cpp
    gfx-boilerplate/stb_impl.cpp
)
target_link_libraries(runtime
    opengl32
    ../lib/x64/assimp
    ../lib/x64/SDL2
    ../lib/x64/SDL2main
    ../lib/x64/glew32
)
target_include_directories(runtime PRIVATE
    include
)