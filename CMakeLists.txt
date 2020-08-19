cmake_minimum_required(VERSION 3.1.3)
project(
  thumbnailer
  LANGUAGES C CXX
  VERSION 0.1.0)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

enable_testing()

# Image IO
add_library(imageio_util STATIC ${CMAKE_CURRENT_SOURCE_DIR}/imageio/imageio_util.c)
target_link_libraries(imageio_util webp)
target_include_directories(imageio_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(imagedec STATIC ${CMAKE_CURRENT_SOURCE_DIR}/imageio/image_dec.c
                            ${CMAKE_CURRENT_SOURCE_DIR}/imageio/jpegdec.c
                            ${CMAKE_CURRENT_SOURCE_DIR}/imageio/metadata.c
                            ${CMAKE_CURRENT_SOURCE_DIR}/imageio/pngdec.c
                            ${CMAKE_CURRENT_SOURCE_DIR}/imageio/pnmdec.c
                            ${CMAKE_CURRENT_SOURCE_DIR}/imageio/tiffdec.c
                            ${CMAKE_CURRENT_SOURCE_DIR}/imageio/webpdec.c
                            ${CMAKE_CURRENT_SOURCE_DIR}/imageio/wicdec.c)
# Find the standard image libraries.
set(TB_DEP_IMG_LIBRARIES)
foreach(I_LIB PNG JPEG TIFF)
  find_package(${I_LIB})
  if(${I_LIB}_FOUND)
    target_compile_definitions(imagedec PUBLIC WEBP_HAVE_${I_LIB})
    list(APPEND TB_DEP_IMG_LIBRARIES ${I_LIB}::${I_LIB})
  endif()
endforeach()
message(STATUS "Adding ${TB_DEP_IMG_LIBRARIES} libraries.")

target_link_libraries(imagedec
                      imageio_util
                      webpmux
                      webpdemux
                      webp
                      ${TB_DEP_IMG_LIBRARIES})

add_library(imageenc STATIC ${CMAKE_CURRENT_SOURCE_DIR}/imageio/image_enc.c)
target_link_libraries(imageenc imageio_util webp)

# Thumbnailer library.
find_package(Protobuf)
if(NOT Protobuf_FOUND)
  message(FATAL_ERROR "Protobuf devel library missing.")
endif()
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/src/thumbnailer.proto)

add_library(thumbnailer_lib ${CMAKE_CURRENT_SOURCE_DIR}/src/thumbnailer.cc ${PROTO_SRCS})
target_include_directories(thumbnailer_lib PUBLIC ${PROTOBUF_INCLUDE_DIRS})
target_link_libraries(thumbnailer_lib protobuf::libprotobuf webp)

add_library(thumbnailer_compare_lib ${CMAKE_CURRENT_SOURCE_DIR}/src/thumbnailer_compare.cc ${PROTO_SRCS})
target_include_directories(thumbnailer_compare_lib PUBLIC ${PROTOBUF_INCLUDE_DIRS})
target_link_libraries(thumbnailer_compare_lib protobuf::libprotobuf webp)

add_executable(thumbnailer ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc)
target_link_libraries(thumbnailer thumbnailer_lib thumbnailer_compare_lib imagedec)

add_subdirectory(test)
