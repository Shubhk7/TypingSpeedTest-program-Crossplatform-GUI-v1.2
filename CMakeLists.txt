cmake_minimum_required(VERSION 3.10)
project(TypingTest C)

set(CMAKE_C_STANDARD 99)

add_executable(TypingTest Program.c)

# Point these to your MSVC-built raylib
target_include_directories(TypingTest PRIVATE "C:/path/to/raylib/include")
target_link_directories(TypingTest PRIVATE "C:/path/to/raylib/lib")

target_link_libraries(TypingTest raylib)

if (MSVC)
    target_link_libraries(TypingTest winmm.lib gdi32.lib opengl32.lib)
endif()
