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
#import "FSTUFF_AppleMetalStructs.h"
#import <OpenGLES/ES2/glext.h>
#import <GLKit/GLKit.h>

struct FSTUFF_GLESRenderer : public FSTUFF_Renderer {
    GLKView * nativeView = nil;
    FSTUFF_GPUData * appData = NULL;
    GLuint programObject = 0;
    int width = 0;
    int height = 0;

    FSTUFF_GLESRenderer() {
        appData = new FSTUFF_GPUData();
    }
    
    ~FSTUFF_GLESRenderer() override {
        delete appData;
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

    void    GetViewSizeMM(float *outWidthMM, float *outHeightMM) override {
        return FSTUFF_Apple_GetViewSizeMM(NULL, outWidthMM, outHeightMM);
    }

    void    RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha) override;

    void    SetProjectionMatrix(const gbMat4 & matrix) override {
        FSTUFF_Apple_CopyMatrix(this->appData->globals.projection_matrix, matrix);
    }

    void    SetShapeProperties(FSTUFF_ShapeType shape, size_t i, const gbMat4 & matrix, const gbVec4 & color) override
    {
        switch (shape) {
            case FSTUFF_ShapeCircle: {
                FSTUFF_Apple_CopyMatrix(this->appData->circles[i].model_matrix, matrix);
                FSTUFF_Apple_CopyVector(this->appData->circles[i].color, color);
            } break;
            case FSTUFF_ShapeBox: {
                FSTUFF_Apple_CopyMatrix(this->appData->boxes[i].model_matrix, matrix);
                FSTUFF_Apple_CopyVector(this->appData->boxes[i].color, color);
            } break;
        }
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
    
    self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (!self.context) {
        FSTUFF_Log(@"Failed to create ES context\n");
    }
    
    GLKView *view = (GLKView *)self.view;
    view.context = self.context;
    view.drawableDepthFormat = GLKViewDrawableDepthFormat24;

    renderer = new FSTUFF_GLESRenderer();
    renderer->nativeView = view;
    _sim.renderer = renderer;

    [EAGLContext setCurrentContext:self.context];

    renderer->programObject = 0;
    renderer->width = 1536;
    renderer->height = 2048;
    
    GLbyte vShaderStr[] = R"(
        uniform mat4 mMatrix;
        attribute vec4 vPosition;
        void main()
        {
            gl_Position = mMatrix * vPosition;
        }
    )";
    
    GLbyte fShaderStr[] = R"(
        precision mediump float;
        void main()
        {
            gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
        }
    )";
    GLuint vertexShader;
    GLuint fragmentShader;
    GLuint programObject;
    GLint linked;
    
    // Load the vertex/fragment shaders
    vertexShader = LoadShader(GL_VERTEX_SHADER, vShaderStr);
    fragmentShader = LoadShader(GL_FRAGMENT_SHADER, fShaderStr);
    // Create the program object
    programObject = glCreateProgram();
    if (programObject == 0) {
        FSTUFF_Log("glCreateProgram failed!\n");
        return;
    }

    glAttachShader(programObject, vertexShader);
    glAttachShader(programObject, fragmentShader);
    // Bind vPosition to attribute 0
    glBindAttribLocation(programObject, 0, "vPosition");
    // Link the program
    glLinkProgram(programObject);
    // Check the link status
    glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            char* infoLog = (char *) malloc(sizeof(char) * infoLen);
            glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
            FSTUFF_Log(@"Error linking program:\n%s\n\n", infoLog);
            free(infoLog);
        }
        glDeleteProgram(programObject);
        FSTUFF_Log("gl program linking failed!\n");
        return;
    }

    // Store the program object
    renderer->programObject = programObject;
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    _sim.Init();
}

- (void)_reshape
{
    // When reshape is called, update the view and projection matricies since this means the view orientation or size changed
    float widthMM, heightMM;
    _sim.renderer->GetViewSizeMM(&widthMM, &heightMM);
    _sim.ViewChanged(widthMM, heightMM);
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

GLuint LoadShader(GLenum type, GLbyte *shaderSrc)
{
    GLuint shader;
    GLint compiled;
    
    // Create the shader object
    shader = glCreateShader(type);
    if(shader == 0)
        return 0;
    // Load the shader source
    glShaderSource(shader, 1, (const GLchar* const *)&shaderSrc, NULL);
    
    // Compile the shader
    glCompileShader(shader);
    // Check the compile status
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        
        if (infoLen > 1) {
            char* infoLog = (char *) malloc(sizeof(char) * infoLen);
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            FSTUFF_Log(@"Error compiling shader:\n%s\n\n", infoLog);
            free(infoLog);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void FSTUFF_GLESRenderer::RenderShapes(FSTUFF_Shape * shape, size_t offset, size_t count, float alpha)
{
    //glBindBuffer(GL_ARRAY_BUFFER, (GLuint)(uintptr_t) this->vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)(uintptr_t) shape->gpuVertexBuffer);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    return;

    // Metal can raise a program-crashing assertion, if zero amount of shapes attempts to get
    // rendered.
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

    NSUInteger shapesOffsetInGpuData;
    switch (shape->type) {
        case FSTUFF_ShapeCircle:
            shapesOffsetInGpuData = offsetof(FSTUFF_GPUData, circles);
            break;
        case FSTUFF_ShapeBox:
            shapesOffsetInGpuData = offsetof(FSTUFF_GPUData, boxes);
            break;
        default:
            FSTUFF_Log(@"Unknown or unmapped FSTUFF_ShapeType in shape: %u\n", shape->type);
            return;
    }
    
    if (shape->type != FSTUFF_ShapeDebug) {
        return;
    }
    
//    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)(uintptr_t) esContext->vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)(uintptr_t) shape->gpuVertexBuffer);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

//        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)(uintptr_t) esContext->vertexBuffer);
//        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
//        glEnableVertexAttribArray(0);
//        glDrawArrays(GL_TRIANGLES, 0, 3);

//        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)(uintptr_t) shape->gpuVertexBuffer);
//        glEnableVertexAttribArray(0);
//        glDrawArrays(GL_TRIANGLES, 0, 3);

//        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);

//        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
//        glBindBuffer(GL_ARRAY_BUFFER, esContext->vertexBuffer);
//        glEnableVertexAttribArray(0);
//        glDrawArrays(GL_TRIANGLES, 0, 3);
}


#pragma mark - GLKView and GLKViewController delegate methods

- (void)update
{
//    _renderer.appData = &_gpuData;
//    FSTUFF_Update(&_sim, &_gpuData);
    _sim.Update();
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
    FSTUFF_GLESRenderer * renderer = (FSTUFF_GLESRenderer *)_sim.renderer;
    
    // Set the viewport
    int uniform_mMatrix = glGetUniformLocation(renderer->programObject, "mMatrix");
#if 0
    gbMat4 mat;
    gb_mat4_translate(&mat, {0.f, 0.f, 0.f});
    glUniformMatrix4fv(uniform_mMatrix, 1, 0, (const GLfloat *)&mat.e);
#else
    glUniformMatrix4fv(uniform_mMatrix, 1, 0, (const GLfloat *)&(renderer->appData->globals.projection_matrix));
#endif
    glViewport(0, 0, renderer->width, renderer->height);
    
    // Clear the color buffer
    glClear(GL_COLOR_BUFFER_BIT);
    // Use the program object
    glUseProgram(renderer->programObject);

    // Load the vertex data
//    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)(uintptr_t) renderer->vertexBuffer);
//    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
//    glEnableVertexAttribArray(0);
//    glDrawArrays(GL_TRIANGLES, 0, 3);
    
    _sim.Render();

}

@end
