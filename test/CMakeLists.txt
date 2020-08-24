cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)

set(TEST test_thumbnailer)
add_executable(${TEST} thumbnailer_test.cc)
target_link_libraries(${TEST} GTest::GTest GTest::Main thumbnailer_lib thumbnailer_slope_optim_lib imagedec webpmux)

if(CMAKE_COMPILER_IS_GNUC OR CMAKE_COMPILER_IS_GNUCXX)
	target_link_libraries(${TEST}  pthread)
endif()

gtest_discover_tests(${TEST})
