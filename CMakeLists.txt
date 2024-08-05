cmake_minimum_required(VERSION 3.0)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()


project(PSP2GCBL)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -Wall -O3 -fno-lto -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions")

include_directories()

add_definitions(
  #-DDEBUG
)

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(${PROJECT_NAME}
    main.c
)

target_link_libraries(${PROJECT_NAME}
	SceLibKernel_stub
	SceAppMgr_stub
  SceKernelModulemgr_stub
  SceKernelThreadMgr_stub
	taihen_stub
)

vita_create_self(${PROJECT_NAME}.suprx ${PROJECT_NAME}
    CONFIG ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}.yml
)