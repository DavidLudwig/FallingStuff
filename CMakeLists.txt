cmake_minimum_required(VERSION 3.12)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
project(FallingStuff)

# Use -Oz on Emscripten-made MinSizeRel builds
if (EMSCRIPTEN)
    set(CMAKE_C_FLAGS_MINSIZEREL "-DNDEBUG -Oz")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "-DNDEBUG -Oz")
    set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "-Oz --llvm-lto 3")
endif()

if (NOT EMSCRIPTEN)
    # SDL2 -is- used on Emscripten, however, the compiler + linker
    # flags for such are often unusual, due to use of '-s USE_SDL=2',
    # rather than specific, library files.  FindSDL2.cmake does not
    # (yet) support this ('-s USE_SDL=2').
    find_package(SDL2 REQUIRED)
endif()

if (WIN32)
    # "unofficial"-prefixed packages are from Microsoft's vcpkg
    find_package(unofficial-angle CONFIG REQUIRED)
else()
    find_package(OpenGL REQUIRED)
endif()

if (APPLE)
    find_library(METAL_FRAMEWORK Metal)
    if (NOT METAL_FRAMEWORK)
        message(FATAL_ERROR "Metal.framework not found!")
    endif()

    find_library(METALKIT_FRAMEWORK MetalKit)
    if (NOT METALKIT_FRAMEWORK)
        message(FATAL_ERROR "MetalKit.framework not found!")
    endif()

    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
    if (NOT COREGRAPHICS_FRAMEWORK)
        message(FATAL_ERROR "CoreGraphics.framework not found!")
    endif()
endif()

add_executable(FallingStuff
    src/FSTUFF.cpp
    src/FSTUFF_OpenGL.cpp
    src/FSTUFF_Apple.mm
    src/FSTUFF_AppleMetal.mm
    src/FSTUFF_Log.cpp
    src/FSTUFF_SDLMain.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_widgets.cpp
    external/Chipmunk2D/src/chipmunk.c
    external/Chipmunk2D/src/cpArbiter.c
    external/Chipmunk2D/src/cpArray.c
    external/Chipmunk2D/src/cpBBTree.c
    external/Chipmunk2D/src/cpBody.c
    external/Chipmunk2D/src/cpCollision.c
    external/Chipmunk2D/src/cpConstraint.c
    external/Chipmunk2D/src/cpDampedRotarySpring.c
    external/Chipmunk2D/src/cpDampedSpring.c
    external/Chipmunk2D/src/cpGearJoint.c
    external/Chipmunk2D/src/cpGrooveJoint.c
    external/Chipmunk2D/src/cpHashSet.c
    external/Chipmunk2D/src/cpMarch.c
    external/Chipmunk2D/src/cpPinJoint.c
    external/Chipmunk2D/src/cpPivotJoint.c
    external/Chipmunk2D/src/cpPolyShape.c
    external/Chipmunk2D/src/cpRatchetJoint.c
    external/Chipmunk2D/src/cpRobust.c
    external/Chipmunk2D/src/cpRotaryLimitJoint.c
    external/Chipmunk2D/src/cpShape.c
    external/Chipmunk2D/src/cpSimpleMotor.c
    external/Chipmunk2D/src/cpSlideJoint.c
    external/Chipmunk2D/src/cpSpace.c
    external/Chipmunk2D/src/cpSpaceComponent.c
    external/Chipmunk2D/src/cpSpaceDebug.c
    external/Chipmunk2D/src/cpSpaceHash.c
    external/Chipmunk2D/src/cpSpaceQuery.c
    external/Chipmunk2D/src/cpSpaceStep.c
    external/Chipmunk2D/src/cpSpatialIndex.c
    external/Chipmunk2D/src/cpSweep1D.c
)

# dludwig@pobox.com: the following don't build on MSVC, and appear to be
# exclude-able.  Are they needed elsewhere?
#
# external/Chipmunk2D/src/cpHastySpace.c
# external/Chipmunk2D/src/cpPolyline.c

target_include_directories(FallingStuff
    PRIVATE
        ./external/Chipmunk2D/include 
        ./external/imgui
        ${SDL2_INCLUDE_DIR}
)
target_link_libraries(FallingStuff
    ${SDL2_LIBRARY}
)

if (EMSCRIPTEN)
    # EMSCRIPTEN_OPTIONS is for '-s KEY=VALUE' options, some of which are
    # needed when compiling, and others when linking.
    set(EMSCRIPTEN_OPTIONS "-s USE_SDL=2")

    set_target_properties(FallingStuff
        PROPERTIES
            COMPILE_FLAGS "${EMSCRIPTEN_OPTIONS} -fno-rtti -fno-exceptions"
            LINK_FLAGS "${EMSCRIPTEN_OPTIONS} --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/emscripten_template_page.html"
            SUFFIX ".html"
    )
endif()

if (WIN32)
    # "unofficial"-prefixed targets are from Microsoft's vcpkg
    target_link_libraries(FallingStuff
        unofficial::angle::libEGL
        unofficial::angle::libGLESv2
    )
elseif (NOT EMSCRIPTEN)
    target_link_libraries(FallingStuff
        OpenGL::GL
    )
endif()

if (APPLE)
    target_link_libraries(FallingStuff
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${COREGRAPHICS_FRAMEWORK}
    )
    set_property (TARGET FallingStuff APPEND_STRING PROPERTY 
                COMPILE_FLAGS "-fobjc-arc")
endif()
set_property(TARGET FallingStuff PROPERTY CXX_STANDARD 17)

install (TARGETS FallingStuff RUNTIME DESTINATION bin)
if (EMSCRIPTEN)
    install(
        FILES
            ${CMAKE_BINARY_DIR}/FallingStuff.js
            ${CMAKE_BINARY_DIR}/FallingStuff.wasm
        DESTINATION
            bin
    ) 
endif()
