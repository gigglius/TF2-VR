cmake_minimum_required(VERSION 3.22.1)
project(tf2vr_native LANGUAGES C CXX)

if(NOT ANDROID)
    message(FATAL_ERROR "TF2-VR Android target must be built with the Android NDK")
endif()
if(NOT OPENXR_SDK_ROOT)
    message(FATAL_ERROR "Set OPENXR_SDK_ROOT to the extracted Khronos/Meta OpenXR SDK")
endif()

set(OPENXR_INCLUDE_CANDIDATES
    "${OPENXR_SDK_ROOT}/include"
    "${OPENXR_SDK_ROOT}/Include"
    "${OPENXR_SDK_ROOT}/OpenXR/Include")
set(OPENXR_INCLUDE_DIR "")
foreach(candidate IN LISTS OPENXR_INCLUDE_CANDIDATES)
    if(EXISTS "${candidate}/openxr/openxr.h")
        set(OPENXR_INCLUDE_DIR "${candidate}")
        break()
    endif()
endforeach()
if(NOT OPENXR_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find openxr/openxr.h under OPENXR_SDK_ROOT=${OPENXR_SDK_ROOT}")
endif()

set(OPENXR_LOADER_CANDIDATES
    "${OPENXR_SDK_ROOT}/Libs/Android/arm64-v8a/Release/libopenxr_loader.so"
    "${OPENXR_SDK_ROOT}/libs/android.arm64-v8a/Release/libopenxr_loader.so"
    "${OPENXR_SDK_ROOT}/OpenXR/Libs/Android/arm64-v8a/Release/libopenxr_loader.so")
set(OPENXR_LOADER "")
foreach(candidate IN LISTS OPENXR_LOADER_CANDIDATES)
    if(EXISTS "${candidate}")
        set(OPENXR_LOADER "${candidate}")
        break()
    endif()
endforeach()
if(NOT OPENXR_LOADER)
    message(FATAL_ERROR "Could not find Android ARM64 libopenxr_loader.so under OPENXR_SDK_ROOT=${OPENXR_SDK_ROOT}")
endif()

find_library(ANDROID_LIB android REQUIRED)
find_library(LOG_LIB log REQUIRED)
find_library(VULKAN_LIB vulkan REQUIRED)

set(NATIVE_APP_GLUE "${ANDROID_NDK}/sources/android/native_app_glue")
add_library(native_app_glue STATIC "${NATIVE_APP_GLUE}/android_native_app_glue.c")
target_include_directories(native_app_glue PUBLIC "${NATIVE_APP_GLUE}")

add_library(tf2vr SHARED main.cpp)
target_include_directories(tf2vr PRIVATE "${OPENXR_INCLUDE_DIR}" "${NATIVE_APP_GLUE}")
target_compile_definitions(tf2vr PRIVATE XR_USE_PLATFORM_ANDROID XR_USE_GRAPHICS_API_VULKAN)
target_link_libraries(tf2vr PRIVATE native_app_glue ${ANDROID_LIB} ${LOG_LIB} ${VULKAN_LIB} dl)

add_library(openxr_loader SHARED IMPORTED)
set_target_properties(openxr_loader PROPERTIES IMPORTED_LOCATION "${OPENXR_LOADER}")
target_link_libraries(tf2vr PRIVATE openxr_loader)
