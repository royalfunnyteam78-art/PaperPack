cmake_minimum_required(VERSION 3.16)
project(PaperPack CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static runtime so the plugins do not need extra Visual C++ DLLs
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

set(OFX_INCLUDES
    "${CMAKE_SOURCE_DIR}/src"
    "${OFX_ROOT}/include"
    "${OFX_ROOT}/include/openfx"
    "${OFX_ROOT}/Support/include")

# The OpenFX support library is compiled once and shared by every plugin below
add_library(ofxsupport OBJECT ${SUPPORT_SRC})
target_include_directories(ofxsupport PRIVATE ${OFX_INCLUDES})
target_compile_definitions(ofxsupport PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)

# Puts <name>.ofx into out/<name>.ofx.bundle/Contents/Win64/ after building
function(ofx_bundle target name)
    set_target_properties(${target} PROPERTIES PREFIX "" SUFFIX ".ofx" OUTPUT_NAME "${name}")
    target_include_directories(${target} PRIVATE ${OFX_INCLUDES})
    target_compile_definitions(${target} PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        target_compile_options(${target} PRIVATE /EHsc /O2)
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/out/${name}.ofx.bundle/Contents/Win64"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${target}>" "${CMAKE_SOURCE_DIR}/out/${name}.ofx.bundle/Contents/Win64/${name}.ofx")
endfunction()

# The real plugin pack (10 effects)
add_library(PaperPack SHARED src/ofx_glue.cpp $<TARGET_OBJECTS:ofxsupport>)
ofx_bundle(PaperPack PaperPack)

# Diagnostic plugins: pass-through effects used to find out what Filmora accepts
foreach(L 0 1 2)
    add_library(PPDiag${L} SHARED src/ofx_glue.cpp $<TARGET_OBJECTS:ofxsupport>)
    target_compile_definitions(PPDiag${L} PRIVATE PP_DIAG=${L})
    ofx_bundle(PPDiag${L} PP_Diag${L})
endforeach()

# Raw OpenFX C API plugin (no support library)
add_library(PPRawMin SHARED src/raw_min.cpp)
ofx_bundle(PPRawMin PP_RawMin)
