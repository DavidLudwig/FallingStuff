//
//  FSTUFF_OpenGLES.h
//  FallingStuff
//
//  Created by David Ludwig on 11/16/18.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#ifndef FSTUFF_OpenGLES_h
#define FSTUFF_OpenGLES_h

#include "FSTUFF.h"
#include "FSTUFF_Constants.h"
#include "gb_math.h"
#include <OpenGLES/ES3/glext.h>

struct FSTUFF_GLESRenderer : public FSTUFF_Renderer {
    void * nativeView = nullptr;

    gbMat4 projectionMatrix;

    gbMat4 circleMatrices[FSTUFF_MaxCircles];
    GLuint circleMatricesBufID = -1;
    gbVec4 circleColors[FSTUFF_MaxCircles];
    GLuint circleColorsBufID = -1;
    gbMat4 boxMatrices[FSTUFF_MaxBoxes];
    GLuint boxMatricesBufID = -1;
    gbVec4 boxColors[FSTUFF_MaxBoxes];
    GLuint boxColorsBufID = -1;
    gbMat4 debugShapeMatrices[1];
    GLuint debugShapeMatricesBufID = -1;
    gbVec4 debugShapeColors[1];
    GLuint debugShapeColorsBufID = -1;

    GLuint programObject = 0;
    int width = 0;
    int height = 0;
    GLint vertexShaderAttribute_position = -1;
    GLint vertexShaderAttribute_colorRGBX = -1;
    GLint vertexShaderAttribute_alpha = -1;
    GLint vertexShaderAttribute_modelMatrix = -1;

    FSTUFF_GLESRenderer();
    ~FSTUFF_GLESRenderer() override;
    void    Init();
    void    BeginFrame();
    void    DestroyVertexBuffer(void * gpuVertexBuffer) override;
    void *  NewVertexBuffer(void * src, size_t size) override;
    void    ViewChanged() override;
    FSTUFF_ViewSize GetViewSize() override;
    void    RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha) override;
    void    SetProjectionMatrix(const gbMat4 & matrix) override;
    void    SetShapeProperties(FSTUFF_ShapeType shape, size_t i, const gbMat4 & matrix, const gbVec4 & color) override;
    FSTUFF_CursorInfo GetCursorInfo() override;
};


#endif // FSTUFF_OpenGLES_h
