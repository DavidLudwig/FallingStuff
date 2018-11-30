//
//  FSTUFF_OpenGL.cpp
//  FallingStuff
//
//  Created by David Ludwig on 11/16/18.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#include "FSTUFF_OpenGL.h"
#include "FSTUFF.h"
#include "FSTUFF_Apple.h"

#if __has_include(<OpenGL/glu.h>)
    #include <OpenGL/glu.h>
    #define FSTUFF_HAS_GLU 1
#endif

static const GLbyte FSTUFF_GL_VertexShaderSrcGLCorev3[] = \
R"(#version 330 core
    uniform mat4 viewMatrix;
    layout (location = 0) in vec4 position;
    layout (location = 1) in vec4 colorRGBX;
    layout (location = 2) in float alpha;
    layout (location = 3) in mat4 modelMatrix;
    out vec4 midColor;
    void main()
    {
        gl_Position = (viewMatrix * modelMatrix) * position;
        midColor = vec4(colorRGBX.rgb, alpha);
    }
)";

static const GLbyte FSTUFF_GL_FragmentShaderSrcGLCorev3[] = \
R"(#version 330 core
    precision mediump float;
    in vec4 midColor;
    out vec4 finalColor;
    void main()
    {
        // color is listed in code as (R,G,B,A)
        finalColor = midColor;
    }
)";


static const GLbyte FSTUFF_GL_VertexShaderSrcGLESv2[] = \
R"(
    uniform mat4 viewMatrix;
    attribute vec4 position;
    attribute vec4 colorRGBX;
    attribute float alpha;
    attribute mat4 modelMatrix;
    varying vec4 midColor;
    void main()
    {
        gl_Position = (viewMatrix * modelMatrix) * position;
        midColor = vec4(colorRGBX.rgb, alpha);
    }
)";

static const GLbyte FSTUFF_GL_FragmentShaderSrcGLESv2[] = \
R"(
    precision mediump float;
    varying vec4 midColor;
    void main()
    {
        // color is listed in code as (R,G,B,A)
        gl_FragColor = midColor;
    }
)";

static const GLbyte FSTUFF_GL_VertexShaderSrcGLESv3[] = \
R"(#version 300 es
    uniform mat4 viewMatrix;
    layout (location = 0) in vec4 position;
    layout (location = 1) in vec4 colorRGBX;
    layout (location = 2) in float alpha;
    layout (location = 3) in mat4 modelMatrix;
    out vec4 midColor;
    void main()
    {
        gl_Position = (viewMatrix * modelMatrix) * position;
        midColor = vec4(colorRGBX.rgb, alpha);
    }
)";

static const GLbyte FSTUFF_GL_FragmentShaderSrcGLESv3[] = \
R"(#version 300 es
    precision mediump float;
    in vec4 midColor;
    out vec4 finalColor;
    void main()
    {
        // color is listed in code as (R,G,B,A)
        finalColor = midColor;
    }
)";

void FSTUFF_GLCheck_Inner(FSTUFF_CodeLocation location)
{
    const GLenum rawError = glGetError();
    if (rawError == GL_NO_ERROR) {
        return;
    }

#if FSTUFF_HAS_GLU
    const char * strError = (const char *) gluErrorString(rawError);
    FSTUFF_FatalError_Inner(
        location,
        "glGetError() failed with result, 0x%x (%s)",
        (unsigned int) rawError,
        (strError ? strError : "(null)")
    );
#else
    FSTUFF_FatalError_Inner(
        location,
        "glGetError() failed with result, 0x%x",
        (unsigned int) rawError
    );
#endif
}

static std::string
FSTUFF_GL_GetInfoLog(
    GLuint src,
    void (FSTUFF_stdcall *getLength)(GLuint, GLenum, GLint *),
    void (FSTUFF_stdcall *getLog)(GLuint, GLsizei, GLsizei *, GLchar *))
{
    std::string result;
    GLint numBytes = 0;
    glGetShaderiv(src, GL_INFO_LOG_LENGTH, &numBytes);
    if (numBytes > 0) {
        result.resize(numBytes);
        glGetShaderInfoLog(src, numBytes, NULL, &result[0]);
    }
    return result;
}

static GLuint FSTUFF_GL_CompileShader(GLenum type, const GLbyte *shaderSrc)
{
    // Create the shader object
    GLuint shader = glCreateShader(type);
    if ( ! shader) {
        return 0;
    }

    // Load the shader source
    glShaderSource(shader, 1, (const GLchar* const *)&shaderSrc, NULL);

    // Compile the shader
    glCompileShader(shader);
    GLint didCompile;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &didCompile);
    if ( ! didCompile) {
        std::string infoLog = FSTUFF_GL_GetInfoLog(shader, glGetShaderiv, glGetShaderInfoLog);
        FSTUFF_Log("Error compiling shader:\n%s\n\n", infoLog.c_str());
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

FSTUFF_GLESRenderer::FSTUFF_GLESRenderer() {
}

void FSTUFF_GLESRenderer::Init() {
    const char * info = nullptr;

    info = (const char *) glGetString(GL_VERSION);
    FSTUFF_Log("GL version: \"%s\"\n", (info ? info : ""));

    info = (const char *) glGetString(GL_SHADING_LANGUAGE_VERSION);
    FSTUFF_Log("GLSL version: \"%s\"\n", (info ? info : ""));
    FSTUFF_GLCheck();
    
    FSTUFF_Assert((bool)this->getProcAddress);
    this->glGetStringi = (decltype(this->glGetStringi)) this->getProcAddress("glGetStringi");
    if ((this->glVersion != FSTUFF_GLVersion::GLESv2) &&
        (this->glGetStringi != nullptr))
    {
        int numExtensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        FSTUFF_Log("GL num extensions: %d\n", numExtensions);
        FSTUFF_Log("GL extensions: ");
        for (int i = 0; i < numExtensions; ++i) {
            info = (const char *) this->glGetStringi(GL_EXTENSIONS, i);
            FSTUFF_Log("%s ", (info ? info : ""));
            if (info) {
                this->glExtensionsCache.emplace(info);
            }
        }
        FSTUFF_Log("\n");
    } else {
        info = (const char *) glGetString(GL_EXTENSIONS);
        FSTUFF_Log("GL extensions: \"%s\"\n", (info ? info : ""));
        if (info) {
            // Copy the list of extensions into an indexed collection
            const char * start = info;
            const char * current = info;
            while (*current != '\0') {
                if (*current == ' ') {
                    if (current != start) {
                        this->glExtensionsCache.emplace(start, current - start);
                        start = current + 1;
                    }
                }
                ++current;
            }
        }
    }

    FSTUFF_Assert(sim);
    FSTUFF_Assert(sim->viewSize.widthPixels > 0);
    FSTUFF_Assert(sim->viewSize.heightPixels > 0);
    
    FSTUFF_GLCheck();
    
    switch (glVersion) {
        case FSTUFF_GLVersion::GLESv2:
            if (this->glExtensionsCache.find("GL_ANGLE_instanced_arrays") != this->glExtensionsCache.end()) {
                this->glDrawArraysInstanced = (decltype(this->glDrawArraysInstanced)) this->getProcAddress("glDrawArraysInstancedANGLE");
                this->glVertexAttribDivisor = (decltype(this->glVertexAttribDivisor)) this->getProcAddress("glVertexAttribDivisorANGLE");
            }
            else if ((this->glExtensionsCache.find("GL_EXT_instanced_arrays") != this->glExtensionsCache.end()) &&
                     (this->glExtensionsCache.find("GL_EXT_draw_instanced")   != this->glExtensionsCache.end()))
            {
                this->glDrawArraysInstanced = (decltype(this->glDrawArraysInstanced)) this->getProcAddress("glDrawArraysInstancedEXT");
                this->glVertexAttribDivisor = (decltype(this->glVertexAttribDivisor)) this->getProcAddress("glVertexAttribDivisorEXT");
            }
            break;
        case FSTUFF_GLVersion::GLESv3:
        case FSTUFF_GLVersion::GLCorev3:
            this->glDrawArraysInstanced = (decltype(this->glDrawArraysInstanced)) this->getProcAddress("glDrawArraysInstanced");
            this->glVertexAttribDivisor = (decltype(this->glVertexAttribDivisor)) this->getProcAddress("glVertexAttribDivisor");
            break;
    }
    FSTUFF_Assert(this->glDrawArraysInstanced != nullptr);
    FSTUFF_Assert(this->glVertexAttribDivisor != nullptr);

    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLESv2:
            break;
        case FSTUFF_GLVersion::GLESv3:
        case FSTUFF_GLVersion::GLCorev3:
            glGenVertexArrays(1, &mainVAO);
            FSTUFF_GLCheck();
            break;
    }
    
    glGenBuffers(1, &circleMatricesBufID);
    glGenBuffers(1, &circleColorsBufID);
    glGenBuffers(1, &boxMatricesBufID);
    glGenBuffers(1, &boxColorsBufID);
    glGenBuffers(1, &debugShapeMatricesBufID);
    glGenBuffers(1, &debugShapeColorsBufID);
    FSTUFF_GLCheck();
    
    // Load the vertex/fragment shaders
    const GLbyte * vertexShaderSrc = nullptr;
    const GLbyte * fragmentShaderSrc = nullptr;
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLCorev3:
            vertexShaderSrc = FSTUFF_GL_VertexShaderSrcGLCorev3;
            fragmentShaderSrc = FSTUFF_GL_FragmentShaderSrcGLCorev3;
            break;
        case FSTUFF_GLVersion::GLESv2:
            vertexShaderSrc = FSTUFF_GL_VertexShaderSrcGLESv2;
            fragmentShaderSrc = FSTUFF_GL_FragmentShaderSrcGLESv2;
            break;
        case FSTUFF_GLVersion::GLESv3:
            vertexShaderSrc = FSTUFF_GL_VertexShaderSrcGLESv3;
            fragmentShaderSrc = FSTUFF_GL_FragmentShaderSrcGLESv3;
            break;
    }
    
    FSTUFF_Assert(vertexShaderSrc != nullptr);
    FSTUFF_Assert(fragmentShaderSrc != nullptr);

    GLuint vertexShader = FSTUFF_GL_CompileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint fragmentShader = FSTUFF_GL_CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    // Create the program object
    GLuint program = glCreateProgram();
    if ( ! program) {
        FSTUFF_Log("glCreateProgram failed!\n");
        return;
    }
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    // Link the program
    glLinkProgram(program);
    GLint didLink = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &didLink);
    if ( ! didLink) {
        std::string infoLog = FSTUFF_GL_GetInfoLog(program, glGetProgramiv, glGetProgramInfoLog);
        FSTUFF_Log("Error linking program:\n%s\n\n", infoLog.c_str());
        glDeleteProgram(program);
        return;
    }

    // Store the program object
    this->programObject = program;
    
    // Make note of shader-attribute positions
    this->vertexShaderAttribute_position = glGetAttribLocation(program, "position");
    this->vertexShaderAttribute_colorRGBX = glGetAttribLocation(program, "colorRGBX");
    this->vertexShaderAttribute_alpha = glGetAttribLocation(program, "alpha");
    this->vertexShaderAttribute_modelMatrix = glGetAttribLocation(program, "modelMatrix");
    
    FSTUFF_GLCheck();
}

void FSTUFF_GLESRenderer::BeginFrame() {
    FSTUFF_Assert(sim);
    FSTUFF_Assert(sim->viewSize.widthPixels > 0);
    FSTUFF_Assert(sim->viewSize.heightPixels > 0);
    FSTUFF_GLCheck();
    
    // Use the vertex array object
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLESv2:
            break;
        case FSTUFF_GLVersion::GLESv3:
        case FSTUFF_GLVersion::GLCorev3:
            glBindVertexArray(this->mainVAO);
            break;
    }

    // Use the program object
    glUseProgram(this->programObject);

    // Set the viewport
    const int uniform_viewMatrix = glGetUniformLocation(this->programObject, "viewMatrix");
    glUniformMatrix4fv(uniform_viewMatrix, 1, 0, (const GLfloat *)&(this->projectionMatrix));
    glViewport(0, 0, sim->viewSize.widthPixels, sim->viewSize.heightPixels);

    // Clear the color buffer
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    FSTUFF_GLCheck();
}

FSTUFF_GLESRenderer::~FSTUFF_GLESRenderer() {
#ifdef __APPLE__
    if ((nativeViewType == FSTUFF_NativeViewType::Apple) && nativeView) {
        CFRelease(nativeView);
    }
#endif
}

void FSTUFF_GLESRenderer::DestroyVertexBuffer(void * gpuVertexBuffer) {
    GLuint bufferID = (GLuint)(uintptr_t)gpuVertexBuffer;
    if (bufferID != 0) {
        glDeleteBuffers(1, &bufferID);
    }
}

void * FSTUFF_GLESRenderer::NewVertexBuffer(void * src, size_t size) {
    // Make note of current buffer
    GLuint prevBuffer = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint*)&prevBuffer);

    // Create new buffer, then populate it with data.
    GLuint newBuffer = 0;
    glGenBuffers(1, &newBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, newBuffer);
    glBufferData(GL_ARRAY_BUFFER, size, src, GL_STATIC_DRAW);

    // Restore previously-current buffer
    glBindBuffer(GL_ARRAY_BUFFER, prevBuffer);
    
    // All done!
    return (void *)(uintptr_t)newBuffer;
}

void FSTUFF_GLESRenderer::ViewChanged() {
}

void FSTUFF_GLESRenderer::SetProjectionMatrix(const gbMat4 & matrix) {
    this->projectionMatrix = matrix;
}

void FSTUFF_GLESRenderer::SetShapeProperties(FSTUFF_ShapeType shape, size_t i, const gbMat4 &matrix, const gbVec4 &color) {
    switch (shape) {
        case FSTUFF_ShapeCircle: {
            this->circleMatrices[i] = matrix;
            this->circleColors[i] = color;
        } break;
        case FSTUFF_ShapeBox: {
            this->boxMatrices[i] = matrix;
            this->boxColors[i] = color;
        } break;
        case FSTUFF_ShapeDebug: {
            this->debugShapeMatrices[i] = matrix;
            this->debugShapeColors[i] = color;
        } break;
    }
}

FSTUFF_CursorInfo FSTUFF_GLESRenderer::GetCursorInfo() {
    FSTUFF_Log("IMPLEMENT ME: %s\n", FSTUFF_CurrentFunction);
    return FSTUFF_CursorInfo();
}

void FSTUFF_GLESRenderer::RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha)
{
    // Some graphics APIs can raise a program-crashing assertion, if zero amount of shapes attempt
    // to get rendered.
    if (count == 0) {
        return;
    }
    FSTUFF_GLCheck();
    
    GLenum gpuPrimitiveType;
    switch (shape->primitiveType) {
        case FSTUFF_PrimitiveLineStrip:
            gpuPrimitiveType = GL_LINE_STRIP;
            break;
        case FSTUFF_PrimitiveTriangles:
            gpuPrimitiveType = GL_TRIANGLES;
            break;
        case FSTUFF_PrimitiveTriangleFan:
            gpuPrimitiveType = GL_TRIANGLE_STRIP;
            break;
        default:
            FSTUFF_Log("Unknown or unmapped FSTUFF_PrimitiveType in shape: %u\n", shape->primitiveType);
            return;
    }
    
    //
    // Copy data to OpenGL
    //
    FSTUFF_GLCheck();

    // Send to OpenGL: color
    switch (shape->type) {
        case FSTUFF_ShapeCircle:
            glBindBuffer(GL_ARRAY_BUFFER, this->circleColorsBufID);
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(float) * 4, this->circleColors + offset, GL_DYNAMIC_DRAW);
            break;
        case FSTUFF_ShapeBox:
            glBindBuffer(GL_ARRAY_BUFFER, this->boxColorsBufID);
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(float) * 4, this->boxColors + offset, GL_DYNAMIC_DRAW);
            break;
        case FSTUFF_ShapeDebug:
            glBindBuffer(GL_ARRAY_BUFFER, this->debugShapeColorsBufID);
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(float) * 4, this->debugShapeColors + offset, GL_DYNAMIC_DRAW);
            break;
    }
    glVertexAttribPointer(vertexShaderAttribute_colorRGBX, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(vertexShaderAttribute_colorRGBX);
    this->glVertexAttribDivisor(vertexShaderAttribute_colorRGBX, 1);

    // Send to OpenGL: alpha
    glVertexAttrib1f(vertexShaderAttribute_alpha, alpha);

    // Send to OpenGL: modelMatrix
    switch (shape->type) {
        case FSTUFF_ShapeCircle:
            glBindBuffer(GL_ARRAY_BUFFER, this->circleMatricesBufID);
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(gbMat4), this->circleMatrices + offset, GL_DYNAMIC_DRAW);
            break;
        case FSTUFF_ShapeBox:
            glBindBuffer(GL_ARRAY_BUFFER, this->boxMatricesBufID);
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(gbMat4), this->boxMatrices + offset, GL_DYNAMIC_DRAW);
            break;
        case FSTUFF_ShapeDebug:
            glBindBuffer(GL_ARRAY_BUFFER, this->debugShapeMatricesBufID);
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(gbMat4), this->debugShapeMatrices + offset, GL_DYNAMIC_DRAW);
            break;
    }
    for (int i = 0; i < 4; ++i) {
        const int location = vertexShaderAttribute_modelMatrix;
        glVertexAttribPointer       (location + i, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void *)(sizeof(float) * 4 * i));
        glEnableVertexAttribArray   (location + i);
        this->glVertexAttribDivisor (location + i, 1);
    }

    // Send to OpenGL: position
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)(uintptr_t) shape->gpuVertexBuffer);
    glVertexAttribPointer(vertexShaderAttribute_position, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(vertexShaderAttribute_position);

    //
    // Draw!
    //
    FSTUFF_GLCheck();
    this->glDrawArraysInstanced(gpuPrimitiveType, 0, shape->numVertices, (int)count);
    FSTUFF_GLCheck();
}