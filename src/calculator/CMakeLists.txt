# Dungeon Master Calculator: C++17, Qt 6 or Qt 5 (Widgets).
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
#   cmake --build build -j
#   ./build/dm_calculator [SAVEGAME]     the window
#   ctest --test-dir build               the checks (no Qt needed)
#
# Without Qt only the checks and the command-line tools are built.
cmake_minimum_required(VERSION 3.16)
project(dm_calculator CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(MSVC)
  add_compile_options(/W3 /utf-8)
else()
  add_compile_options(-Wall -Wextra)
endif()

# The formulas and the file readers: plain C++, no Qt
add_library(calc_core STATIC
  core/Builtin.cpp core/Defense.cpp core/GameData.cpp core/HeroAttack.cpp
  core/MonsterAttack.cpp core/Recovery.cpp core/Spells.cpp
  io/Adf.cpp io/Config.cpp io/GraphicsDat.cpp io/SaveGame.cpp)

add_executable(calc_tests tests/core_tests.cpp)
target_link_libraries(calc_tests calc_core)
add_executable(calc_dump cli/dump.cpp)
target_link_libraries(calc_dump calc_core)
add_executable(calc_save cli/save.cpp)
target_link_libraries(calc_save calc_core)

enable_testing()
add_test(NAME formulas COMMAND calc_tests)

find_package(Threads REQUIRED)
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets QUIET)
if(QT_FOUND)
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
  set(CMAKE_AUTOMOC ON)
  add_executable(dm_calculator WIN32 MACOSX_BUNDLE
    ui/main.cpp ui/MainWindow.cpp ui/MainWindow.h ui/Session.cpp ui/Session.h
    ui/Sidebar.cpp ui/Sidebar.h ui/Theme.cpp ui/Theme.h ui/Widgets.cpp ui/Widgets.h
    ui/Pages.h ui/Page.cpp ui/HeroAttackPage.cpp ui/MonsterAttackPage.cpp ui/SpellPage.cpp
    ui/RecoveryPage.cpp ui/WhatIfPage.cpp)
  target_link_libraries(dm_calculator calc_core Qt${QT_VERSION_MAJOR}::Widgets Threads::Threads)
else()
  message(STATUS "Qt not found: building only the checks and command-line tools")
endif()
