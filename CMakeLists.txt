cmake_minimum_required(VERSION 3.15)
project(terminal_roguelike CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# Target 1: executable 'roguelike'
add_executable(roguelike
    main.cpp
    src/map.cpp
    src/ai.cpp
    src/engine.cpp
)

# Target 2: executable 'headless_test'
add_executable(headless_test
    tests/test_headless.cpp
    src/map.cpp
    src/ai.cpp
    src/engine.cpp
)

if(WIN32)
    target_link_libraries(roguelike PRIVATE user32 gdi32)
    target_link_libraries(headless_test PRIVATE user32 gdi32)
endif()
