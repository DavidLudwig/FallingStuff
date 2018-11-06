// ImGui GLFW binding with OpenGL
// In this binding, ImTextureID is used to store an OpenGL 'GLuint' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include "FSTUFF.h"
#if FSTUFF_USE_METAL

#include "FSTUFF_AppleMetal.h"

struct GLFWwindow;

struct FSTUFF_ImGuiMetal {
    ImGuiContext * imGuiContext = NULL;
    FSTUFF_Simulation * sim = NULL;
    FSTUFF_AppleMetalRenderer * renderer = NULL;
    id <MTLTexture> metalTexture = nil;
    //GLFWwindow*  g_Window = NULL;
    double       timeElapsedS = 0.0f;
    bool         mouseButtonsPressed[3] = { false, false, false };
    float        mouseWheelState = 0.0f;

    //CAMetalLayer *mtlLayer;
    //id<MTLDevice> mtlDevice;
    id<MTLCommandQueue> mtlCommandQueue;
    id<MTLCommandBuffer> mtlCurrentCommandBuffer = nil;
    id<MTLRenderCommandEncoder> mtlCurrentCommandEncoder = nil;
    id<MTLRenderPipelineState> mtlRenderPipelineState;
    id<MTLTexture> mtlFontTexture;
    id<MTLSamplerState> mtlSamplerState;
    NSMutableArray<id<MTLBuffer>> *mtlBufferPool;
    //id<CAMetalDrawable> mtlCurrentDrawable;
    
    FSTUFF_ImGuiMetal();
    ~FSTUFF_ImGuiMetal();

    bool Init(void * nativeView, bool install_callbacks);
    void Shutdown();
    void NewFrame(const FSTUFF_ViewSize & viewSize); //int widthPixels, int heightPixels);
    void EndFrame();

	id   CommandQueue();
	//id   CurrentDrawable();
	id   MainTexture();

	// Use if you want to reset your rendering device without losing ImGui state.
	void InvalidateDeviceObjects();
	bool CreateDeviceObjects();

	// GLFW callbacks (installed by default if you enable 'install_callbacks' during initialization)
	// Provided here if you want to chain callbacks.
	// You can also handle inputs yourself and use those as a reference.
	void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
	void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	void CharCallback(GLFWwindow* window, unsigned int c);

    id<MTLBuffer> DequeueReusableBuffer(NSUInteger size);
    void EnqueueReusableBuffer(id<MTLBuffer> buffer);
    static void RenderDrawLists(ImDrawData * draw_data);
    void _RenderDrawLists(ImDrawData * draw_data);
    static const char * GetClipboardText(void * userdata);
    static void SetClipboardText(void * userdata, const char * text);
    
    ImGuiIO & GetIO();
};

#endif  // FSTUFF_USE_METAL
