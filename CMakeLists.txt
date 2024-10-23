PROJECT(VOC_VI)

CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

# ===== Setp 1 : Set Cross Compiler Path =====

SET(_SDK_PATH_ "/home/xjt/_Workspace_/VOC/System/")
SET(_BUILDROOT_PATH_ "${_SDK_PATH_}rk3588-linux/buildroot/output/rockchip_rk3588/host/")
SET(_COMPILER_PATH_ "${_BUILDROOT_PATH_}bin/")
SET(_INCLUDE_PATH_ "${_BUILDROOT_PATH_}/aarch64-buildroot-linux-gnu/sysroot/usr/include/")

SET(CMAKE_C_COMPILER ${_COMPILER_PATH_}aarch64-buildroot-linux-gnu-gcc)
SET(CMAKE_CXX_COMPILER ${_COMPILER_PATH_}aarch64-buildroot-linux-gnu-g++)

# ===== Setp 2 : Set Flags =====

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s -O3 -lrt -pthread -Wno-deprecated-declarations")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s -O3 -lrt -pthread -std=c++20 -Wno-deprecated-declarations")

# ===== Setp 3 : Set Application Name =====

SET(APP_NAME "vi")
SET(SHM_NAME "shm")

# ===== Setp 4 : Add Subdirectory =====

ADD_SUBDIRECTORY(src bin)
ADD_SUBDIRECTORY(shm)

INCLUDE_DIRECTORIES("${_INCLUDE_PATH_}rockchip")