// ImGui GLFW binding with Metal
// In this binding, ImTextureID is used to store a pointer to a Metal texture. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include "FSTUFF.h"
#if __APPLE__

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "FSTUFF_Apple.h"

#include <imgui.h>
#include "imgui_impl_mtl.h"
#include "imgui_internal.h"     // Needed for access to ImGuiContext member-variables

// GLFW
//#include <GLFW/glfw3.h>
//#ifdef TARGET_OS_MAC
//#define GLFW_EXPOSE_NATIVE_COCOA
//#define GLFW_EXPOSE_NATIVE_NSGL // Don't really need this, but GLFW won't let us not specify it
//#include <GLFW/glfw3native.h>
//#endif

//typedef void GLFWwindow;

id<MTLBuffer> FSTUFF_ImGuiMetal::DequeueReusableBuffer(NSUInteger size) {
    for (int i = 0; i < [this->mtlBufferPool count]; ++i) {
        id<MTLBuffer> candidate = this->mtlBufferPool[i];
        if ([candidate length] >= size) {
            [this->mtlBufferPool removeObjectAtIndex:i];
            return candidate;
        }
    }

    return [this->renderer->device newBufferWithLength:size options:MTLResourceCPUCacheModeDefaultCache];
}

void FSTUFF_ImGuiMetal::EnqueueReusableBuffer(id<MTLBuffer> buffer) {
    [this->mtlBufferPool insertObject:buffer atIndex:0];
}

ImGuiIO & FSTUFF_ImGuiMetal::GetIO() {
    //return ImGui::GetIO();
    return this->imGuiContext->IO;
}

// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// If text or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
void FSTUFF_ImGuiMetal::RenderDrawLists(ImDrawData* draw_data)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO& io = ImGui::GetIO();
    FSTUFF_ImGuiMetal * gui = (FSTUFF_ImGuiMetal *)io.UserData;
    if (gui) {
        gui->_RenderDrawLists(draw_data);
    }
}

void FSTUFF_ImGuiMetal::_RenderDrawLists(ImDrawData* draw_data)
{
    ImGuiIO& io = GetIO();
    int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fb_width == 0 || fb_height == 0)
        return;
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    id<MTLCommandBuffer> commandBuffer = this->mtlCurrentCommandBuffer;
    id<MTLRenderCommandEncoder> commandEncoder = this->mtlCurrentCommandEncoder;

    [commandEncoder setRenderPipelineState:this->mtlRenderPipelineState];

    MTLViewport viewport = {
        .originX = 0, .originY = 0, .width = (double)fb_width, .height = (double)fb_height, .znear = 0, .zfar = 1
    };
    [commandEncoder setViewport:viewport];

    float left = 0, right = io.DisplaySize.x, top = 0, bottom = io.DisplaySize.y;
    float near = 0;
    float far = 1;
    float sx = 2 / (right - left);
    float sy = 2 / (top - bottom);
    float sz = 1 / (far - near);
    float tx = (right + left) / (left - right);
    float ty = (top + bottom) / (bottom - top);
    float tz = near / (far - near);
    float orthoMatrix[] = {
        sx,  0,  0, 0,
         0, sy,  0, 0,
         0,  0, sz, 0,
        tx, ty, tz, 1
    };

    [commandEncoder setVertexBytes:orthoMatrix length:sizeof(float) * 16 atIndex:1];

    // Render command lists
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const unsigned char* vtx_buffer = (const unsigned char*)&cmd_list->VtxBuffer.front();
        const ImDrawIdx* idx_buffer = &cmd_list->IdxBuffer.front();

        NSUInteger vertexBufferSize = sizeof(ImDrawVert) * cmd_list->VtxBuffer.size();
        id<MTLBuffer> vertexBuffer = this->DequeueReusableBuffer(vertexBufferSize);
        memcpy([vertexBuffer contents], vtx_buffer, vertexBufferSize);

        NSUInteger indexBufferSize = sizeof(ImDrawIdx) * cmd_list->IdxBuffer.size();
        id<MTLBuffer> indexBuffer = this->DequeueReusableBuffer(indexBufferSize);
        memcpy([indexBuffer contents], idx_buffer, indexBufferSize);

        [commandEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];

        int idx_buffer_offset = 0;
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                MTLScissorRect scissorRect = {
                    .x = (NSUInteger)pcmd->ClipRect.x,
                    .y = (NSUInteger)(pcmd->ClipRect.y),
                    .width = (NSUInteger)(pcmd->ClipRect.z - pcmd->ClipRect.x),
                    .height = (NSUInteger)(pcmd->ClipRect.w - pcmd->ClipRect.y)
                };

                if (scissorRect.x + scissorRect.width <= fb_width && scissorRect.y + scissorRect.height <= fb_height)
                {
                    [commandEncoder setScissorRect:scissorRect];
                }

                [commandEncoder setFragmentTexture:(__bridge id<MTLTexture>)pcmd->TextureId atIndex:0];

                [commandEncoder setFragmentSamplerState:this->mtlSamplerState atIndex:0];

                [commandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                           indexCount:(GLsizei)pcmd->ElemCount
                                            indexType:sizeof(ImDrawIdx) == 2 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
                                          indexBuffer:indexBuffer
                                    indexBufferOffset:sizeof(ImDrawIdx) * idx_buffer_offset];
            }

            idx_buffer_offset += pcmd->ElemCount;
        }

        dispatch_queue_t queue = dispatch_get_current_queue();
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer) {
            dispatch_async(queue, ^{
                this->EnqueueReusableBuffer(vertexBuffer);
                this->EnqueueReusableBuffer(indexBuffer);
            });
        }];
    }
}

const char* FSTUFF_ImGuiMetal::GetClipboardText(void * userdata)
{
    FSTUFF_LOG_IMPLEMENT_ME("");
//    @autoreleasepool {
//        static std::string text;
//        NSPasteboard * pasteboard = [NSPasteboard generalPasteboard];
//        NSString * available = [pasteboard availableTypeFromArray:@[NSPasteboardTypeString]];
//        if ([available isEqualToString:NSPasteboardTypeString]) {
//            NSString * str = [pasteboard stringForType:NSPasteboardTypeString];
//            if (str) {
//                text = [str UTF8String];
//            } else {
//                text.clear();
//            }
//        } else {
//            text.clear();
//        }
//        return text.c_str();
//    }
    return "";
}

void FSTUFF_ImGuiMetal::SetClipboardText(void * userdata, const char* text)
{
    FSTUFF_LOG_IMPLEMENT_ME("");
//    glfwSetClipboardString(this->g_Window, text);

//    @autoreleasepool {
//        NSPasteboard * pasteboard = [NSPasteboard generalPasteboard];
//        [pasteboard setString:[[NSString alloc] initWithUTF8String:text]
//                      forType:NSPasteboardTypeString];
//    }
}

void FSTUFF_ImGuiMetal::MouseButtonCallback(GLFWwindow*, int button, int action, int /*mods*/)
{
    FSTUFF_LOG_IMPLEMENT_ME("");
//    if (action == GLFW_PRESS && button >= 0 && button < 3)
//        this->g_MousePressed[button] = true;
}

void FSTUFF_ImGuiMetal::ScrollCallback(GLFWwindow*, double /*xoffset*/, double yoffset)
{
    FSTUFF_LOG_IMPLEMENT_ME("");
//    this->g_MouseWheel += (float)yoffset; // Use fractional mouse wheel, 1.0 unit 5 lines.
}

void FSTUFF_ImGuiMetal::KeyCallback(GLFWwindow*, int key, int, int action, int mods)
{
    FSTUFF_LOG_IMPLEMENT_ME("");
//    ImGuiIO& io = GetIO();
//    if (action == GLFW_PRESS)
//        io.KeysDown[key] = true;
//    if (action == GLFW_RELEASE)
//        io.KeysDown[key] = false;
//
//    (void)mods; // Modifiers are not reliable across systems
//    io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
//    io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
//    io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
//    io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
}

void FSTUFF_ImGuiMetal::CharCallback(GLFWwindow*, unsigned int c)
{
    FSTUFF_LOG_IMPLEMENT_ME("");
//
//    ImGuiIO& io = GetIO();
//    if (c > 0 && c < 0x10000)
//        io.AddInputCharacter((unsigned short)c);
}

bool FSTUFF_ImGuiMetal::CreateDeviceObjects()
{
    // Build texture atlas
    ImGuiIO& io = GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
    
//    this->mtlDevice = MTLCreateSystemDefaultDevice();

    if (!(this->sim && this->renderer && this->renderer->device)) {
        FSTUFF_Log(@"Metal is not available\n");
        return false;
    }
    
    FSTUFF_LOG_IMPLEMENT_ME(", set appropriate size for main texture");
    FSTUFF_Assert(this->sim);
    FSTUFF_Assert(this->sim->viewSize.widthPixels > 0);
    FSTUFF_Assert(this->sim->viewSize.heightPixels > 0);
    const FSTUFF_ViewSize & viewSize = this->sim->viewSize;
    MTLTextureDescriptor *mainTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                                     width:viewSize.widthPixels
                                                                                                    height:viewSize.heightPixels
                                                                                                 mipmapped:NO];
    mainTextureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
//    mainTextureDescriptor.label = @"ImGui Main Texture";
    this->metalTexture = [this->renderer->device newTextureWithDescriptor:mainTextureDescriptor];
    this->metalTexture.label = @"ImGui Main Texture";
//    FSTUFF_Log(@"%s, this->mtlMainTexture = %@\n", __FUNCTION__, this->mtlMainTexture);


//    [this->mtlLayer setDevice:this->mtlDevice];

    this->mtlCommandQueue = [this->renderer->device newCommandQueue];
    this->mtlCommandQueue.label = @"ImGui Command Queue";

    MTLTextureDescriptor *fontTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                                     width:width
                                                                                                    height:height
                                                                                                 mipmapped:NO];
    this->mtlFontTexture = [this->renderer->device newTextureWithDescriptor:fontTextureDescriptor];
    this->mtlFontTexture.label = @"ImGui Font Texture";
    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    [this->mtlFontTexture replaceRegion:region mipmapLevel:0 withBytes:pixels bytesPerRow:width * sizeof(uint8_t)];

    // Store our identifier
    io.Fonts->TexID = (void *)(intptr_t)this->mtlFontTexture;

    MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.label = @"ImGui Sampler State";

    this->mtlSamplerState = [this->renderer->device newSamplerStateWithDescriptor:samplerDescriptor];
    //this->mtlSamplerState.label = @"ImGui Sampler State";

    NSString *shaders = @"#include <metal_stdlib>\n\
    using namespace metal;                                                                  \n\
                                                                                            \n\
    struct vertex_t {                                                                       \n\
        float2 position [[attribute(0)]];                                                   \n\
        float2 tex_coords [[attribute(1)]];                                                 \n\
        uchar4 color [[attribute(2)]];                                                      \n\
    };                                                                                      \n\
                                                                                            \n\
    struct frag_data_t {                                                                    \n\
        float4 position [[position]];                                                       \n\
        float4 color;                                                                       \n\
        float2 tex_coords;                                                                  \n\
    };                                                                                      \n\
                                                                                            \n\
    vertex frag_data_t vertex_function(vertex_t vertex_in [[stage_in]],                     \n\
                                       constant float4x4 &proj_matrix [[buffer(1)]])        \n\
    {                                                                                       \n\
        float2 position = vertex_in.position;                                               \n\
                                                                                            \n\
        frag_data_t out;                                                                    \n\
        out.position = proj_matrix * float4(position.xy, 0, 1);                             \n\
        out.color = float4(vertex_in.color) * (1 / 255.0);                                  \n\
        out.tex_coords = vertex_in.tex_coords;                                              \n\
        return out;                                                                         \n\
    }                                                                                       \n\
                                                                                            \n\
    fragment float4 fragment_function(frag_data_t frag_in [[stage_in]],                     \n\
                                      texture2d<float, access::sample> tex [[texture(0)]],  \n\
                                      sampler tex_sampler [[sampler(0)]])                   \n\
    {                                                                                       \n\
        return frag_in.color * float4(tex.sample(tex_sampler, frag_in.tex_coords).r);       \n\
    }";

    NSError *error = nil;
    id<MTLLibrary> library = [this->renderer->device newLibraryWithSource:shaders options:nil error:&error];
    library.label = @"ImGui Library";
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_function"];
    vertexFunction.label = @"ImGui Vertex Function";
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_function"];
    fragmentFunction.label = @"ImGui Fragment Function";

    if (!library || !vertexFunction || !fragmentFunction) {
        FSTUFF_Log(@"Could not create library from shader source and retrieve functions\n");
        return false;
    }

#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
    MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.attributes[0].offset = OFFSETOF(ImDrawVert, pos);
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[1].offset = OFFSETOF(ImDrawVert, uv);
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[1].bufferIndex = 0;
    vertexDescriptor.attributes[2].offset = OFFSETOF(ImDrawVert, col);
    vertexDescriptor.attributes[2].format = MTLVertexFormatUChar4;
    vertexDescriptor.attributes[2].bufferIndex = 0;
    vertexDescriptor.layouts[0].stride = sizeof(ImDrawVert);
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
#undef OFFSETOF

    MTLRenderPipelineDescriptor *renderPipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    renderPipelineDescriptor.vertexFunction = vertexFunction;
    renderPipelineDescriptor.fragmentFunction = fragmentFunction;
    renderPipelineDescriptor.vertexDescriptor = vertexDescriptor;
    renderPipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    renderPipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    renderPipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    renderPipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    renderPipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    renderPipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    renderPipelineDescriptor.label = @"ImGui Render Pipeline State";

    this->mtlRenderPipelineState = [this->renderer->device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor error:&error];
    //this->mtlRenderPipelineState.label = @"ImGui Render Pipeline State";

    if (!this->mtlRenderPipelineState) {
        FSTUFF_Log(@"Error when creating pipeline state: %@\n", error);
        return false;
    }

    this->mtlBufferPool = [NSMutableArray array];

    return true;
}

void    FSTUFF_ImGuiMetal::InvalidateDeviceObjects()
{
    GetIO().Fonts->TexID = 0;

    this->mtlFontTexture = nil;
    this->mtlSamplerState = nil;
    this->mtlBufferPool = nil;
    this->mtlRenderPipelineState = nil;
    this->mtlCommandQueue = nil;
    //this->mtlDevice = nil;
}

FSTUFF_ImGuiMetal::FSTUFF_ImGuiMetal()
{
    FSTUFF_Log(@"%s, this:%p\n", __PRETTY_FUNCTION__, this);
    this->imGuiContext = ImGui::CreateContext();
    FSTUFF_Log(@"%s, this->imGuiContext:%p\n", __PRETTY_FUNCTION__, this->imGuiContext);
    this->imGuiContext->IO.Fonts = new ImFontAtlas();
    FSTUFF_Log(@"%s, this->imGuiContext->IO.Fonts:%p\n", __PRETTY_FUNCTION__, this->imGuiContext->IO.Fonts);
}

#include <execinfo.h>

FSTUFF_ImGuiMetal::~FSTUFF_ImGuiMetal()
{
//    FSTUFF_Log(@"%s, this:%p\n", __PRETTY_FUNCTION__, this);
//    FSTUFF_Log(@"%s, this->imGuiContext:%p\n", __PRETTY_FUNCTION__, this->imGuiContext);
    
//    static const int maxEntries = 1024;
//    void * callstack[maxEntries];
//    int numFrames = backtrace(callstack, maxEntries);
//    FSTUFF_Log(@"%s, numFrames:%d\n", __PRETTY_FUNCTION__, numFrames);
//    char ** frameNames = backtrace_symbols(callstack, numFrames);
//    for (int i = 0; i < numFrames; ++i) {
//        FSTUFF_Log(@"%s, frameNames[%d] = \"%s\"\n", __PRETTY_FUNCTION__, i, frameNames[i]);
//    }
//    free(frameNames);
    
    if (this->imGuiContext) {
        if (ImGui::GetCurrentContext() == this->imGuiContext) {
            ImGui::SetCurrentContext(NULL);
        }
        
        FSTUFF_Log(@"TODO, %s: destroy ImGui content, without causing a crash.\n", __FUNCTION__);
//        if (this->imGuiContext->IO.Fonts) {
//            delete this->imGuiContext->IO.Fonts;
//            this->imGuiContext->IO.Fonts = NULL;
//        }
//        ImGui::DestroyContext(this->imGuiContext);
    }
}

bool FSTUFF_ImGuiMetal::Init(void * _nativeView, bool install_callbacks)
{
    this->sim = (FSTUFF_Simulation *)_nativeView;
    this->renderer = (FSTUFF_AppleMetalRenderer *)this->sim->renderer;

//    this->g_Window = window;

//    this->mtlLayer = [CAMetalLayer layer];
//    FSTUFF_Log("IMPLEMENT ME: %s, setup nativeView\n", __FUNCTION__);
//    NSWindow *nativeWindow = glfwGetCocoaWindow(this->g_Window);
//    [[nativeWindow contentView] setLayer:this->mtlLayer];
//    [[nativeWindow contentView] setWantsLayer:YES];

    FSTUFF_LOG_IMPLEMENT_ME(", keymap");
    ImGuiIO& io = GetIO();
//    io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;                     // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
//    io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
//    io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
//    io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
//    io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
//    io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
//    io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
//    io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
//    io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
//    io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
//    io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
//    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
//    io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
//    io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
//    io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
//    io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
//    io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
//    io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
//    io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

    io.UserData = this;
    io.RenderDrawListsFn = FSTUFF_ImGuiMetal::RenderDrawLists;      // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
    io.SetClipboardTextFn = FSTUFF_ImGuiMetal::SetClipboardText;
    io.GetClipboardTextFn = FSTUFF_ImGuiMetal::GetClipboardText;
#ifdef _WIN32
    io.ImeWindowHandle = glfwGetWin32Window(this->g_Window);
#endif

    if (install_callbacks)
    {
        FSTUFF_LOG_IMPLEMENT_ME(", install callbacks");
//        glfwSetMouseButtonCallback(window, FSTUFF_ImGuiMetal::MouseButtonCallback);
//        glfwSetScrollCallback(window, FSTUFF_ImGuiMetal::ScrollCallback);
//        glfwSetKeyCallback(window, FSTUFF_ImGuiMetal::KeyCallback);
//        glfwSetCharCallback(window, FSTUFF_ImGuiMetal::CharCallback);
    }

    return true;
}

void FSTUFF_ImGuiMetal::Shutdown()
{
    this->InvalidateDeviceObjects();
    ImGui::Shutdown();
}

void FSTUFF_ImGuiMetal::EndFrame()
{
    [this->mtlCurrentCommandEncoder endEncoding];
    [this->mtlCurrentCommandBuffer commit];
}

void FSTUFF_ImGuiMetal::NewFrame(const FSTUFF_ViewSize & viewSize) // int widthPixels, int heightPixels)
{
    ImGui::SetCurrentContext(this->imGuiContext);

    if (!this->mtlFontTexture) {
        this->CreateDeviceObjects();
    }
    
    this->mtlCurrentCommandBuffer = [this->mtlCommandQueue commandBuffer];
    this->mtlCurrentCommandBuffer.label = @"ImGui Command Buffer";
    
    MTLRenderPassDescriptor *imguiRenderPass = [MTLRenderPassDescriptor renderPassDescriptor];
//    renderPassDescriptor.colorAttachments[0].texture = [(id<CAMetalDrawable>)this->mtlCurrentDrawable texture];
    imguiRenderPass.colorAttachments[0].texture = this->metalTexture;
//    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionLoad;
    imguiRenderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
    imguiRenderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    imguiRenderPass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
    this->mtlCurrentCommandEncoder = [this->mtlCurrentCommandBuffer renderCommandEncoderWithDescriptor:imguiRenderPass];
    this->mtlCurrentCommandEncoder.label = @"ImGui Command Encoder";

    ImGuiIO& io = GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
#if 0
//    glfwGetWindowSize(this->g_Window, &w, &h);
//    glfwGetFramebufferSize(this->g_Window, &display_w, &display_h);
    w = 1024;
    h = 768;
    display_w = 1024;
    display_h = 768;
    FSTUFF_LOG_IMPLEMENT_ME(", get display size");
#else
    w = viewSize.widthOS;
    h = viewSize.heightOS;
    display_w = viewSize.widthPixels;
    display_h = viewSize.heightPixels;
#endif
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

//    CGRect bounds = CGRectMake(0, 0, w, h);
//    CGRect nativeBounds = CGRectMake(0, 0, display_w, display_h);
//    [this->mtlLayer setFrame:bounds];
//    [this->mtlLayer setContentsScale:nativeBounds.size.width / bounds.size.width];
//    [this->mtlLayer setDrawableSize:nativeBounds.size];

//    FSTUFF_Log("IMPLEMENT ME: %s, check set of this->mtlCurrentDrawable\n", __FUNCTION__);
//    this->mtlCurrentDrawable = [((CAMetalLayer *)[this->g_Renderer->nativeView layer]) nextDrawable];
//    this->mtlCurrentDrawable = [this->mtlLayer nextDrawable];

    // Setup time step
    double current_time = this->timeElapsedS; // glfwGetTime();
    io.DeltaTime = this->timeElapsedS > 0.0 ? (float)(current_time - this->timeElapsedS) : (float)(1.0f/60.0f);
    this->timeElapsedS = current_time;
    //FSTUFF_Log("io.DeltaTime: %f\n", io.DeltaTime);

    // Setup inputs
    // (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
//    FSTUFF_LOG_IMPLEMENT_ME(", set cursor pos");

//    if (glfwGetWindowAttrib(this->g_Window, GLFW_FOCUSED))
//    {
//        double mouse_x, mouse_y;
//        glfwGetCursorPos(this->g_Window, &mouse_x, &mouse_y);
//        io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);   // Mouse position in screen coordinates (set to -1,-1 if no mouse / on another screen, etc.)
//    }
//    else
//    {
//        io.MousePos = ImVec2(-1,-1);
//    }

    io.MousePos = ImVec2((float)this->sim->cursorInfo.xOS, (float)this->sim->cursorInfo.yOS);

    //FSTUFF_LOG_IMPLEMENT_ME(", get cursor press state");
//    for (int i = 0; i < 3; i++)
//    {
////        io.MouseDown[i] = this->g_MousePressed[i] || glfwGetMouseButton(this->g_Window, i) != 0;    // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
//        //this->g_MousePressed[i] = false;
//    }

    io.MouseDown[0] = this->sim->cursorInfo.pressed;
//    this->g_MousePressed[1] = false;
//    this->g_MousePressed[2] = false;

    io.MouseWheel = this->mouseWheelState;
    this->mouseWheelState = 0.0f;

    // Hide OS mouse cursor if ImGui is drawing it
//    FSTUFF_LOG_IMPLEMENT_ME(", hide OS mouse cursor");
//    glfwSetInputMode(this->g_Window, GLFW_CURSOR, io.MouseDrawCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);

    // Start the frame
    ImGui::NewFrame();
}

IMGUI_API id          FSTUFF_ImGuiMetal::CommandQueue()
{
    return this->mtlCommandQueue;
}

//IMGUI_API id           ()
//{
//    //return this->mtlCurrentDrawable;
//}

IMGUI_API id          FSTUFF_ImGuiMetal::MainTexture()
{
    return this->metalTexture;
}

#endif	// __APPLE__
