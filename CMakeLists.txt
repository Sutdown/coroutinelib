cmake_minimum_required(VERSION 3.16)
project(CoRoutineLib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SOURCES
    src/thread/thread.cc
    src/thread/thread.h
    src/fiber/fiber.cc
    src/fiber/fiber.h
    src/scheduler/scheduler.cc
    src/scheduler/scheduler.h
    src/timer/timer.cc
    src/timer/timer.h
    tests/test_scheduler.cc
)

include_directories(
      src/thread
      src/fiber
      src/scheduler
      src/timer
)

add_executable(test ${SOURCES})