//
//  FSTUFF_OpenGL.h
//  FallingStuff
//
//  Created by David Ludwig on 11/16/18.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#ifndef FSTUFF_OpenGL_h
#define FSTUFF_OpenGL_h

#include "FSTUFF.h"
#include "FSTUFF_Constants.h"
#include "gb_math.h"
#include <unordered_set>

#define GL_SILENCE_DEPRECATION
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#if __APPLE__
    #if TARGET_OS_IOS
        #include <OpenGLES/ES3/glext.h>
    #else
        #include <OpenGL/gl3.h>
        #include <OpenGL/gl3ext.h>
    #endif
#else
    // The below are from https://www.khronos.org/registry/OpenGL/index_es.php#headers3
    #include <GLES3/gl3.h>
    #include <GLES2/gl2ext.h>
    #include <GLES3/gl3platform.h>
#endif

#if _MSC_VER
	#define FSTUFF_stdcall __stdcall
#else
	#define FSTUFF_stdcall
#endif

void FSTUFF_GLCheck_Inner(FSTUFF_CodeLocation location);
#define FSTUFF_GLCheck() FSTUFF_GLCheck_Inner(FSTUFF_CODELOC)

enum class FSTUFF_GLVersion {
    GLCorev3,
    GLESv2,
    GLESv3,
};

template <typename T>
struct FSTUFF_GL_Shaders {
    T simulationVertex;
    T simulationFragment;
    T imGuiVertex;
    T imGuiFragment;
};
typedef FSTUFF_GL_Shaders<const char *> FSTUFF_GL_ShaderCode;


struct FSTUFF_GLESRenderer : public FSTUFF_Renderer {
    FSTUFF_GLVersion glVersion = FSTUFF_GLVersion::GLESv3;
    
    void * nativeView = nullptr;
    FSTUFF_NativeViewType nativeViewType = FSTUFF_NativeViewType::Unknown;
    
    std::function<void *(const char *)> getProcAddress;
    
    void (FSTUFF_stdcall * glBindSampler)(GLuint, GLuint);
    void (FSTUFF_stdcall * glDrawArraysInstanced)(GLenum, GLint, GLsizei, GLsizei) = nullptr;
    const GLubyte * (FSTUFF_stdcall * glGetStringi)(GLenum, GLuint);
    void (FSTUFF_stdcall * glVertexAttribDivisor)(GLuint, GLuint) = nullptr;
    
    std::unordered_set<std::string> glExtensionsCache;

    gbMat4 projectionMatrix;

    GLuint mainVAO = 0;     // 'VAO' == 'Vertex Array Object'

    gbMat4 circleMatrices[FSTUFF_MaxCircles];
    GLuint circleMatricesBufID = -1;
    gbVec4 circleColors[FSTUFF_MaxCircles];
    GLuint circleColorsBufID = -1;
    gbMat4 boxMatrices[FSTUFF_MaxBoxes];
    GLuint boxMatricesBufID = -1;
    gbVec4 boxColors[FSTUFF_MaxBoxes];
    GLuint boxColorsBufID = -1;
    gbMat4 segmentMatrices[FSTUFF_MaxSegments];
    GLuint segmentMatricesBufID = -1;
    gbVec4 segmentColors[FSTUFF_MaxSegments];
    GLuint segmentColorsBufID = -1;

    gbMat4 debugShapeMatrices[1];
    GLuint debugShapeMatricesBufID = -1;
    gbVec4 debugShapeColors[1];
    GLuint debugShapeColorsBufID = -1;

    GLuint simProgram = 0;
    GLint simVS_position = -1;
    GLint simVS_colorRGBX = -1;
    GLint simVS_alpha = -1;
    GLint simVS_modelMatrix = -1;

    GLuint imGuiProgram = 0;
    GLuint imGuiVBO = 0;
    GLuint imGuiElements = 0;
    GLint imGui_tex = -1;
    GLint imGui_projMtx = -1;
    GLint imGui_position = -1;
    GLint imGui_uv = -1;
    GLint imGui_color = -1;

    FSTUFF_GLESRenderer();
    ~FSTUFF_GLESRenderer() override;
    void    Init();
    void    BeginFrame() override;

    void *  NewVertexBuffer(void * src, size_t size) override;
    void    DestroyVertexBuffer(void * gpuVertexBuffer) override;

    FSTUFF_Texture NewTexture(const uint8_t * srcRGBA32, int width, int height) override;
    void    DestroyTexture(FSTUFF_Texture tex) override;

    void    ViewChanged() override;
    void    RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha) override;
    void    RenderImGuiDrawData(ImDrawData * drawData);
    void    SetProjectionMatrix(const gbMat4 & matrix) override;
    void    SetShapeProperties(FSTUFF_ShapeType shape, size_t i, const gbMat4 & matrix, const gbVec4 & color) override;
    FSTUFF_CursorInfo GetCursorInfo() override;
};


#endif // FSTUFF_OpenGL_h
