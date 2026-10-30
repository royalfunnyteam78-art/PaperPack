cmake_minimum_required(VERSION 3.16)
project(PaperPack CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static runtime so the plugin does not need extra Visual C++ DLLs
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

set(OFX_ROOT "${CMAKE_SOURCE_DIR}/openfx" CACHE PATH "Path to the OpenFX SDK checkout")

set(SUPPORT_NAMES
    ofxsCore ofxsImageEffect ofxsInteract ofxsLog ofxsMultiThread
    ofxsParams ofxsProperty ofxsPropertyValidation)

set(SUPPORT_SRC "")
foreach(n ${SUPPORT_NAMES})
    set(f "${OFX_ROOT}/Support/Library/${n}.cpp")
    if(EXISTS "${f}")
        list(APPEND SUPPORT_SRC "${f}")
    else()
        message(WARNING "OpenFX support file not found: ${f}")
    endif()
endforeach()

add_library(PaperPack SHARED src/ofx_glue.cpp ${SUPPORT_SRC})

target_include_directories(PaperPack PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${OFX_ROOT}/include"
    "${OFX_ROOT}/include/openfx"
    "${OFX_ROOT}/Support/include")

target_compile_definitions(PaperPack PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    target_compile_options(PaperPack PRIVATE /EHsc /O2)
endif()

set_target_properties(PaperPack PROPERTIES PREFIX "" SUFFIX ".ofx" OUTPUT_NAME "PaperPack")
