//
//  FSTUFF_AppleMetalViewController.m
//  FallingStuff_iOSGLES
//
//  Created by David Ludwig on 6/4/16.
//  Copyright © 2018 David Ludwig. All rights reserved.
//

#import "FSTUFF_AppleMetalViewController.h"
#import <OpenGLES/ES2/glext.h>
#include "FSTUFF.h"
#import <GLKit/GLKit.h>

typedef struct
{
    // Handle to a program object
    GLuint programObject;
} UserData;

typedef struct
{
    UserData * userData = NULL;
    int width = 0;
    int height = 0;
    void * vertexBuffer = 0;
} ESContext;


@interface FSTUFF_AppleMetalViewController () {
    ESContext _esContext;
    UserData _userData;

    FSTUFF_Simulation _sim;
    FSTUFF_GPUData _gpuData;
}
@property (strong, nonatomic) EAGLContext *context;
//@property (strong, nonatomic) GLKBaseEffect *effect;

- (void)setupGL;
- (void)tearDownGL;

//- (BOOL)loadShaders;
//- (BOOL)compileShader:(GLuint *)shader type:(GLenum)type file:(NSString *)file;
//- (BOOL)linkProgram:(GLuint)prog;
//- (BOOL)validateProgram:(GLuint)prog;
@end

@implementation FSTUFF_AppleMetalViewController

- (FSTUFF_Simulation *) sim
{
    return &_sim;
}

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
    
    [self setupGL];
    _sim.gpuDevice = NULL;
    _sim.nativeView = (__bridge void *)self.view;
    _sim.Init();
}

- (void)_reshape
{
    // When reshape is called, update the view and projection matricies since this means the view orientation or size changed
    float widthMM, heightMM;
    FSTUFF_GetViewSizeMM((__bridge void *)self.view, &widthMM, &heightMM);
    FSTUFF_ViewChanged(&_sim, widthMM, heightMM);
}

- (void)dealloc
{    
    [self tearDownGL];
    
    if ([EAGLContext currentContext] == self.context) {
        [EAGLContext setCurrentContext:nil];
    }
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];

    if ([self isViewLoaded] && ([[self view] window] == nil)) {
        self.view = nil;
        
        [self tearDownGL];
        
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
    
    if(!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        
        if(infoLen > 1)
        {
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

int Init(ESContext *esContext)
{
    UserData *userData = esContext->userData;
    
//    struct FSTUFF_Vertex
//    {
//        float4 position [[position]];
//        float4 color;
//    };
//    
//    vertex FSTUFF_Vertex FSTUFF_VertexShader(constant float4 * position [[buffer(0)]],
//                                             constant FSTUFF_GPUGlobals * gpuGlobals [[buffer(1)]],
//                                             constant FSTUFF_ShapeGPUInfo * gpuShapes [[buffer(2)]],
//                                             constant float * alpha [[buffer(3)]],
//                                             uint vertexId [[vertex_id]],
//                                             uint shapeId [[instance_id]])
//    {
//        FSTUFF_Vertex vert;
//        vert.position = (gpuGlobals->projection_matrix * gpuShapes[shapeId].model_matrix) * position[vertexId];
//        vert.color = {
//            gpuShapes[shapeId].color[0],
//            gpuShapes[shapeId].color[1],
//            gpuShapes[shapeId].color[2],
//            gpuShapes[shapeId].color[3] * (*alpha),
//        };
//        return vert;
//    }
//

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
    if(programObject == 0)
        return 0;
    glAttachShader(programObject, vertexShader);
    glAttachShader(programObject, fragmentShader);
    // Bind vPosition to attribute 0
    glBindAttribLocation(programObject, 0, "vPosition");
    // Link the program
    glLinkProgram(programObject);
    // Check the link status
    glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
    if(!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
        
        if(infoLen > 1)
        {
            char* infoLog = (char *) malloc(sizeof(char) * infoLen);
            glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
            FSTUFF_Log(@"Error linking program:\n%s\n\n", infoLog);
            
            free(infoLog);
        }
        glDeleteProgram(programObject);
        return FALSE;
    }

    // Store the program object
    userData->programObject = programObject;
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    return TRUE;
}

- (void)setupGL
{
    [EAGLContext setCurrentContext:self.context];

    _esContext.userData = &_userData;
    _esContext.userData->programObject = 0;
    _esContext.width = 1536;
    _esContext.height = 2048;
    Init(&_esContext);
    
    GLfloat vVertices[] = {
        0.0f, 0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f
    };
    
    _esContext.vertexBuffer = FSTUFF_NewVertexBuffer(NULL, vVertices, sizeof(vVertices));
}

void * FSTUFF_NewVertexBuffer(void * gpuDevice, void * src, size_t size)
{
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

void FSTUFF_DestroyVertexBuffer(FSTUFF_Renderer * renderer, void * _gpuVertexBuffer)
{
    GLuint bufferID = (GLuint)(uintptr_t)_gpuVertexBuffer;
    if (bufferID != 0) {
        glDeleteBuffers(1, &bufferID);
    }
}

void FSTUFF_Renderer::RenderShapes(FSTUFF_Shape * shape,
                                   size_t baseInstance,
                                   size_t count,
                                   float alpha)
{
    FSTUFF_GPUData * gpuData = (FSTUFF_GPUData *) _gpuData;
    
//    id <MTLDevice> gpuDevice = (__bridge id <MTLDevice>)_gpuDevice;
//    id <MTLRenderCommandEncoder> gpuRenderer = (__bridge id <MTLRenderCommandEncoder>)_gpuRenderer;
//    id <MTLBuffer> gpuData = (__bridge id <MTLBuffer>)_gpuData;
//    

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

//    [gpuRenderer pushDebugGroup:[NSString stringWithUTF8String:shape->debugName]];
//    [gpuRenderer setVertexBuffer:(__bridge id <MTLBuffer>)shape->gpuVertexBuffer offset:0 atIndex:0];                    // 'position[<vertex id>]'
//    [gpuRenderer setVertexBuffer:gpuData offset:offsetof(FSTUFF_GPUData, globals) atIndex:1];   // 'gpuGlobals'
//    [gpuRenderer setVertexBuffer:gpuData offset:shapesOffsetInGpuData atIndex:2];               // 'gpuShapes[<instance id>]'
//    [gpuRenderer setVertexBytes:&alpha length:sizeof(alpha) atIndex:3];                         // 'alpha'
//    
//#if TARGET_OS_IOS
//    const MTLFeatureSet featureSetForBaseInstance = MTLFeatureSet_iOS_GPUFamily3_v1;
//#else
//    const MTLFeatureSet featureSetForBaseInstance = MTLFeatureSet_OSX_GPUFamily1_v1;
//#endif
//    if (baseInstance == 0) {
//        [gpuRenderer drawPrimitives:gpuPrimitiveType
//                        vertexStart:0
//                        vertexCount:shape->numVertices
//                      instanceCount:count];
//    } else if ([gpuDevice supportsFeatureSet:featureSetForBaseInstance]) {
//        [gpuRenderer drawPrimitives:gpuPrimitiveType
//                        vertexStart:0
//                        vertexCount:shape->numVertices
//                      instanceCount:count
//                       baseInstance:baseInstance];
//    }
//    [gpuRenderer popDebugGroup];


//    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
//    glBindBuffer(GL_ARRAY_BUFFER, esContext->vertexBuffer);
//    glEnableVertexAttribArray(0);
//    glDrawArrays(GL_TRIANGLES, 0, 3);


}

- (void)tearDownGL
{
    [EAGLContext setCurrentContext:self.context];
    
    
}

#pragma mark - GLKView and GLKViewController delegate methods

- (void)update
{
    _renderer.appData = &_gpuData;
    FSTUFF_Update(&_sim, &_gpuData);
}

void Draw(ESContext *esContext, FSTUFF_Simulation * sim, FSTUFF_GPUData * gpuData)
{
    UserData *userData = esContext->userData;
    
    // Set the viewport
    int uniform_mMatrix = glGetUniformLocation(userData->programObject, "mMatrix");
//    matrix_float4x4 mat = matrix_identity_float4x4;
    matrix_float4x4 mat = matrix_from_translation(0.0f, 0.f, 0.f);
    glUniformMatrix4fv(uniform_mMatrix, 1, 0, (const GLfloat *)&mat);
//    glUniformMatrix4fv(uniform_mMatrix, 1, 0, (const GLfloat *)&gpuData->globals.projection_matrix);
    
    glViewport(0, 0, esContext->width, esContext->height);
    
    // Clear the color buffer
    glClear(GL_COLOR_BUFFER_BIT);
    // Use the program object
    glUseProgram(userData->programObject);

    // Load the vertex data
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)(uintptr_t) esContext->vertexBuffer);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    //eglSwapBuffers(esContext->eglDisplay, esContext->eglSurface);
    
    FSTUFF_Renderer renderer = {NULL, NULL, gpuData};
    //FSTUFF_Render(sim, &renderer);
    sim->Render();
    
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
    Draw(&_esContext, &_sim, &_gpuData);
}

@end
