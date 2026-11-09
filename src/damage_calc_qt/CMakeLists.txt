# Damage calculator (C++ / Qt): exact distribution + multi-threaded Monte Carlo
# of the game's physical-attack formulas.  Port of damage_calc_gui.py.
#
#   cmake -S . -B build && cmake --build build -j
#   ./build/damage_calc_qt           the GUI (Qt5 or Qt6, Widgets + Concurrent)
#   ./build/damage_cli [scenarios] [trials] [threads]   self-test, no Qt
cmake_minimum_required(VERSION 3.16)
project(damage_calc_qt CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

find_package(Threads REQUIRED)

add_library(damagecore STATIC
  DamageData.cpp DamageEngine.cpp DamageExact.cpp DamageMonteCarlo.cpp)
target_link_libraries(damagecore Threads::Threads)

add_executable(damage_cli damage_cli.cpp)
target_link_libraries(damage_cli damagecore)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets Concurrent QUIET)
if(QT_FOUND)
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent)
  set(CMAKE_AUTOMOC ON)
  add_executable(damage_calc_qt main.cpp MainWindow.cpp MainWindow.h)
  target_compile_definitions(damage_calc_qt PRIVATE
    DAMAGE_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(damage_calc_qt damagecore
    Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Concurrent)
else()
  message(STATUS "Qt not found: building only damage_cli")
endif()
