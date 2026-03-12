cmake_minimum_required(VERSION 3.20)
project(homm1)

set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)

add_executable(homm1
    src/main.cpp
    src/agg.cpp
    src/bmp.cpp
    src/icn.cpp
    src/image.cpp
    src/palette.cpp
    src/til.cpp
)

target_include_directories(homm1 PRIVATE
    src/
    ${SDL2_INCLUDE_DIRS}
    
)

target_link_libraries(homm1 PRIVATE
    ${SDL2_LIBRARIES}
    z
)