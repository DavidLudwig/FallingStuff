//
//  FSTUFF_AppleGLViewController.mm
//  FallingStuff_iOSGLES
//
//  Created by David Ludwig on 6/4/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#include "FSTUFF.h"
#import "FSTUFF_Apple.h"
#import "FSTUFF_AppleGL.h"
//#import <OpenGLES/ES2/glext.h>
#import <OpenGLES/ES3/glext.h>
#import <GLKit/GLKit.h>
#include <array>
#include <vector>

static const GLbyte FSTUFF_GL_VertexShaderSrc[] = R"(
    #version 300 es
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

static const GLbyte FSTUFF_GL_FragShaderSrc[] = R"(
    #version 300 es
    precision mediump float;
    in vec4 midColor;
    out vec4 finalColor;
    void main()
    {
        // color is listed in code as (R,G,B,A)
        finalColor = midColor;
    }
)";


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

    FSTUFF_GLESRenderer() {
        glGenBuffers(1, &circleMatricesBufID);
        glGenBuffers(1, &circleColorsBufID);
        glGenBuffers(1, &boxMatricesBufID);
        glGenBuffers(1, &boxColorsBufID);
        glGenBuffers(1, &debugShapeMatricesBufID);
        glGenBuffers(1, &debugShapeColorsBufID);
    }
    
    ~FSTUFF_GLESRenderer() override {
#ifdef __APPLE__
        if (nativeView) {
            CFRelease(nativeView);
        }
#endif
    }
    
    void    DestroyVertexBuffer(void * gpuVertexBuffer) override {
        GLuint bufferID = (GLuint)(uintptr_t)gpuVertexBuffer;
        if (bufferID != 0) {
            glDeleteBuffers(1, &bufferID);
        }
    }

    void *  NewVertexBuffer(void * src, size_t size) override {
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
    
    void    ViewChanged() override {
        FSTUFF_Log("IMPLEMENT ME: %s\n", __PRETTY_FUNCTION__);
    }

    FSTUFF_ViewSize GetViewSize() override {
        return FSTUFF_Apple_GetViewSize(nativeView);
    }

    void    RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha) override;

    void    SetProjectionMatrix(const gbMat4 & matrix) override {
        this->projectionMatrix = matrix;
    }

    void    SetShapeProperties(FSTUFF_ShapeType shape, size_t i, const gbMat4 & matrix, const gbVec4 & color) override
    {
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
    
    FSTUFF_CursorInfo GetCursorInfo() override {
        FSTUFF_Log("IMPLEMENT ME: %s\n", __PRETTY_FUNCTION__);
        return FSTUFF_CursorInfo();
    }
};

@interface FSTUFF_AppleGLViewController () {
    FSTUFF_Simulation _sim;
    FSTUFF_GLESRenderer * renderer;
}
@property (strong, nonatomic) EAGLContext *context;
@end

@implementation FSTUFF_AppleGLViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if ( ! self.context) {
        FSTUFF_Log(@"Failed to create ES context\n");
        return;
    }
    
    GLKView *view = (GLKView *)self.view;
    view.context = self.context;
    view.drawableDepthFormat = GLKViewDrawableDepthFormat24;

    [EAGLContext setCurrentContext:self.context];

    renderer = new FSTUFF_GLESRenderer();
    renderer->nativeView = (__bridge_retained void *) view;
    _sim.renderer = renderer;

    FSTUFF_ViewSize vs = renderer->GetViewSize();
    renderer->width = vs.widthPixels;
    renderer->height = vs.heightPixels;
    
    // Load the vertex/fragment shaders
    GLuint vertexShader = FSTUFF_GL_CompileShader(GL_VERTEX_SHADER, (const GLbyte *) FSTUFF_GL_VertexShaderSrc);
    GLuint fragmentShader = FSTUFF_GL_CompileShader(GL_FRAGMENT_SHADER, (const GLbyte *) FSTUFF_GL_FragShaderSrc);

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
    GLint didLink;
    glGetProgramiv(program, GL_LINK_STATUS, &didLink);
    if ( ! didLink) {
        std::string infoLog = FSTUFF_GL_GetInfoLog(program, glGetProgramiv, glGetProgramInfoLog);
        FSTUFF_Log(@"Error linking program:\n%s\n\n", infoLog.c_str());
        glDeleteProgram(program);
        return;
    }

    // Store the program object
    renderer->programObject = program;
    
    // Make note of shader-attribute positions
    renderer->vertexShaderAttribute_position = glGetAttribLocation(program, "position");
    renderer->vertexShaderAttribute_colorRGBX = glGetAttribLocation(program, "colorRGBX");
    renderer->vertexShaderAttribute_alpha = glGetAttribLocation(program, "alpha");
    renderer->vertexShaderAttribute_modelMatrix = glGetAttribLocation(program, "modelMatrix");

    // Init the game/simulation
    _sim.Init();
}

- (void)_reshape
{
    // When reshape is called, update the view and projection matricies since this means the view orientation or size changed
    const FSTUFF_ViewSize viewSize = _sim.renderer->GetViewSize();
    _sim.ViewChanged(viewSize);
}

- (void)dealloc
{
    [EAGLContext setCurrentContext:self.context];
    if ([EAGLContext currentContext] == self.context) {
        [EAGLContext setCurrentContext:nil];
    }
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];

    if ([self isViewLoaded] && ([[self view] window] == nil)) {
        self.view = nil;
        
        [EAGLContext setCurrentContext:self.context];
        if ([EAGLContext currentContext] == self.context) {
            [EAGLContext setCurrentContext:nil];
        }
        self.context = nil;
    }

    // Dispose of any resources that can be recreated.
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

static std::string FSTUFF_GL_GetInfoLog(
    GLuint src,
    void (*getLength)(GLuint,GLenum,GLint*),
    void (*getLog)(GLuint,GLsizei,GLsizei*,GLchar*)
)
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
        FSTUFF_Log(@"Error compiling shader:\n%s\n\n", infoLog.c_str());
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

void FSTUFF_GLESRenderer::RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha)
{
    // Some graphics APIs can raise a program-crashing assertion, if zero amount of shapes attempt
    // to get rendered.
    if (count == 0) {
        return;
    }
    
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
            FSTUFF_Log(@"Unknown or unmapped FSTUFF_PrimitiveType in shape: %u\n", shape->primitiveType);
            return;
    }
    
    //
    // Copy data to OpenGL
    //

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
    glVertexAttribDivisor(vertexShaderAttribute_colorRGBX, 1);
    
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
        glVertexAttribPointer     (location + i, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void *)(sizeof(float) * 4 * i));
        glEnableVertexAttribArray (location + i);
        glVertexAttribDivisor     (location + i, 1);
    }

    // Send to OpenGL: position
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)(uintptr_t) shape->gpuVertexBuffer);
    glVertexAttribPointer(vertexShaderAttribute_position, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(vertexShaderAttribute_position);
    
    //
    // Draw!
    //
    glDrawArraysInstanced(gpuPrimitiveType, 0, shape->numVertices, (int)count);
}


#pragma mark - GLKView and GLKViewController delegate methods

- (void)update
{
    _sim.Update();
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
    // Set the viewport
    const int uniform_viewMatrix = glGetUniformLocation(renderer->programObject, "viewMatrix");
    glUniformMatrix4fv(uniform_viewMatrix, 1, 0, (const GLfloat *)&(renderer->projectionMatrix));
    glViewport(0, 0, renderer->width, renderer->height);
    
    // Clear the color buffer
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use the program object
    glUseProgram(renderer->programObject);
    
    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render the scene
    _sim.Render();
}

@end
