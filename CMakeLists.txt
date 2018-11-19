cmake_minimum_required(VERSION 3.12)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
project(FallingStuff)

# add_subdirectory(external/Chipmunk2D)


find_package(SDL2 REQUIRED)
find_package(OpenGL 2 REQUIRED COMPONENTS OpenGL)

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
    src/FSTUFF_OpenGLES.cpp
    src/FSTUFF_Apple.mm
    src/FSTUFF_AppleMetal.mm
    src/FSTUFF_SDLMain.cpp
    src/imgui_impl_mtl.mm
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
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
    external/Chipmunk2D/src/cpHastySpace.c
    external/Chipmunk2D/src/cpMarch.c
    external/Chipmunk2D/src/cpPinJoint.c
    external/Chipmunk2D/src/cpPivotJoint.c
    external/Chipmunk2D/src/cpPolyShape.c
    external/Chipmunk2D/src/cpPolyline.c
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
target_include_directories(FallingStuff
    PRIVATE
        ./external/Chipmunk2D/include 
        ./external/imgui
        ${SDL2_INCLUDE_DIR}
)
target_link_libraries(FallingStuff
    ${SDL2_LIBRARY}
    OpenGL::GL
)
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
