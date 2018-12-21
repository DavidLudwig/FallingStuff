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


static const FSTUFF_GL_ShaderCode FSTUFF_GL_ShaderCode_Core3 = {

    // Simulation, Vertex Shader
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
)",

    // Simulation, Fragment Shader
R"(#version 330 core
    precision mediump float;
    in vec4 midColor;
    out vec4 finalColor;
    void main()
    {
        // color is listed in code as (R,G,B,A)
        finalColor = midColor;
    }
)",

    // ImGui, Vertex Shader
R"(#version 330 core
    layout (location = 0) in vec2 Position;
    layout (location = 1) in vec2 UV;
    layout (location = 2) in vec4 Color;
    uniform mat4 ProjMtx;
    out vec2 Frag_UV;
    out vec4 Frag_Color;
    void main()
    {
        Frag_UV = UV;
        Frag_Color = Color;
        gl_Position = ProjMtx * vec4(Position.xy,0,1);
    }
)",

    // ImGui, Fragment Shader
R"(#version 330 core
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    uniform sampler2D Texture;
    layout (location = 0) out vec4 Out_Color;
    void main()
    {
        Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
    }
)"
};

static const FSTUFF_GL_ShaderCode FSTUFF_GL_ShaderCode_ES2 = {

    // Simulation, Vertex Shader
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
)",

    // Simulation, Fragment Shader
    R"(
    precision mediump float;
    varying vec4 midColor;
    void main()
    {
        // color is listed in code as (R,G,B,A)
        gl_FragColor = midColor;
    }
)",

    // ImGui, Vertex Shader
R"(
    uniform mat4 ProjMtx;
    attribute vec2 Position;
    attribute vec2 UV;
    attribute vec4 Color;
    varying vec2 Frag_UV;
    varying vec4 Frag_Color;
    void main()
    {
    	Frag_UV = UV;
    	Frag_Color = Color;
    	gl_Position = ProjMtx * vec4(Position.xy, 0, 1);
    }
)",

    // ImGui, Fragment Shader
R"(
    precision mediump float;
    uniform sampler2D Texture;
    varying vec2 Frag_UV;
    varying vec4 Frag_Color;
    void main()
    {
    	gl_FragColor = Frag_Color * texture2D(Texture, Frag_UV);
    }
)"};

static const FSTUFF_GL_ShaderCode FSTUFF_GL_ShaderCode_ES3 = {

    // Simulation, Vertex Shader
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
)",

    // Simulation, Fragment Shader
    R"(#version 300 es
    precision mediump float;
    in vec4 midColor;
    out vec4 finalColor;
    void main()
    {
        // color is listed in code as (R,G,B,A)
        finalColor = midColor;
    }
)",

    // ImGui, Vertex Shader
    nullptr,

    // ImGui, Fragment Shader
    nullptr
};

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

static GLuint FSTUFF_GL_CompileShader(GLenum shaderType, const GLbyte *shaderSrc, const char * debugName)
{
    // Create the shader object
    GLuint shader = glCreateShader(shaderType);
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
        const char * shaderTypeName = "unknown";
        switch (shaderType) {
            case GL_FRAGMENT_SHADER:
                shaderTypeName = "fragment";
                break;
            case GL_VERTEX_SHADER:
                shaderTypeName = "vertex";
                break;
        }
        debugName = (debugName ? debugName : "");
        FSTUFF_Log("Error compiling %s shader (%s):\n%s\n\n", shaderTypeName, debugName, infoLog.c_str());
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint FSTUFF_GL_CreateProgram(
    const char * vertexShaderSrc,
    const char * fragmentShaderSrc,
    const char * debugName
) {
    GLuint vertexShader = FSTUFF_GL_CompileShader(GL_VERTEX_SHADER, (const GLbyte *) vertexShaderSrc, debugName);
    GLuint fragmentShader = FSTUFF_GL_CompileShader(GL_FRAGMENT_SHADER, (const GLbyte *) fragmentShaderSrc, debugName);

    // Create the simulation's program object
    GLuint program = glCreateProgram();
    if ( ! program) {
        FSTUFF_Log("glCreateProgram failed!\n");
        return 0;
    }
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    // Link the simulation's program
    glLinkProgram(program);
    GLint didLink = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &didLink);
    if ( ! didLink) {
        std::string infoLog = FSTUFF_GL_GetInfoLog(program, glGetProgramiv, glGetProgramInfoLog);
        FSTUFF_Log("Error linking program:\n%s\n\n", infoLog.c_str());
        glDeleteProgram(program);
        return 0;
    }

    return program;
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
            this->glBindSampler = (decltype(this->glBindSampler)) this->getProcAddress("glBindSampler");
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
    glGenBuffers(1, &segmentMatricesBufID);
    glGenBuffers(1, &segmentColorsBufID);
    glGenBuffers(1, &debugShapeMatricesBufID);
    glGenBuffers(1, &debugShapeColorsBufID);
    FSTUFF_GLCheck();
    
    // Load the vertex/fragment shaders
    const FSTUFF_GL_ShaderCode * shadersSrc = nullptr;
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLCorev3:
            shadersSrc = &FSTUFF_GL_ShaderCode_Core3;
            break;
        case FSTUFF_GLVersion::GLESv2:
            shadersSrc = &FSTUFF_GL_ShaderCode_ES2;
            break;
        case FSTUFF_GLVersion::GLESv3:
            shadersSrc = &FSTUFF_GL_ShaderCode_ES3;
            break;
    }
    FSTUFF_Assert(shadersSrc != nullptr);
    FSTUFF_Assert(shadersSrc->simulationVertex != nullptr);
    FSTUFF_Assert(shadersSrc->simulationFragment != nullptr);
    FSTUFF_Assert(shadersSrc->imGuiVertex != nullptr);
    FSTUFF_Assert(shadersSrc->imGuiFragment != nullptr);

    this->simProgram = FSTUFF_GL_CreateProgram(
        shadersSrc->simulationVertex,
        shadersSrc->simulationFragment,
        "simulation"
    );
    this->simVS_position = glGetAttribLocation(this->simProgram, "position");
    this->simVS_colorRGBX = glGetAttribLocation(this->simProgram, "colorRGBX");
    this->simVS_alpha = glGetAttribLocation(this->simProgram, "alpha");
    this->simVS_modelMatrix = glGetAttribLocation(this->simProgram, "modelMatrix");

    this->imGuiProgram = FSTUFF_GL_CreateProgram(
        shadersSrc->imGuiVertex,
        shadersSrc->imGuiFragment,
        "ImGui"
    );
    this->imGui_tex = glGetUniformLocation(this->imGuiProgram, "Texture");
    this->imGui_projMtx = glGetUniformLocation(this->imGuiProgram, "ProjMtx");
    this->imGui_position = glGetAttribLocation(this->imGuiProgram, "Position");
    this->imGui_uv = glGetAttribLocation(this->imGuiProgram, "UV");
    this->imGui_color = glGetAttribLocation(this->imGuiProgram, "Color");
    glGenBuffers(1, &this->imGuiVBO);
    glGenBuffers(1, &this->imGuiElements);

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
    glUseProgram(this->simProgram);

    // Set the viewport
    const int uniform_viewMatrix = glGetUniformLocation(this->simProgram, "viewMatrix");
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

void FSTUFF_GLESRenderer::RenderImGuiDrawData(ImDrawData * drawData) {
    FSTUFF_GLCheck();
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO& io = ImGui::GetIO();
    int fb_width = (int)(drawData->DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height = (int)(drawData->DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;
    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    // Backup GL state
    GLenum last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture);
    glActiveTexture(GL_TEXTURE0);
    GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
#ifdef GL_SAMPLER_BINDING
    GLint last_sampler;
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLESv2:
            break;
        case FSTUFF_GLVersion::GLESv3:
        case FSTUFF_GLVersion::GLCorev3:
            glGetIntegerv(GL_SAMPLER_BINDING, &last_sampler);
            break;
    }
#endif
    FSTUFF_GLCheck();
    GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    GLint last_vertex_array;
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLESv2:
            break;
        case FSTUFF_GLVersion::GLESv3:
        case FSTUFF_GLVersion::GLCorev3:
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
            break;
    }
#ifdef GL_POLYGON_MODE
    GLint last_polygon_mode[2];
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLESv2:
        case FSTUFF_GLVersion::GLESv3:
            break;
        case FSTUFF_GLVersion::GLCorev3:
            glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
            break;
    }
#endif
    GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    GLenum last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
    GLenum last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
    GLenum last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
    GLenum last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
    GLenum last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
    GLenum last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);
    GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    bool clip_origin_lower_left = true;
#ifdef GL_CLIP_ORIGIN
    GLenum last_clip_origin = 0; glGetIntegerv(GL_CLIP_ORIGIN, (GLint*)&last_clip_origin); // Support for GL 4.5's glClipControl(GL_UPPER_LEFT)
    if (last_clip_origin == GL_UPPER_LEFT)
        clip_origin_lower_left = false;
#endif

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
#ifdef GL_POLYGON_MODE
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLESv2:
        case FSTUFF_GLVersion::GLESv3:
            break;
        case FSTUFF_GLVersion::GLCorev3:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;
    }
#endif

    // Setup viewport, orthographic projection matrix
    // Our visible imgui space lies from drawData->DisplayPos (top left) to drawData->DisplayPos+data_data->DisplaySize (bottom right). DisplayMin is typically (0,0) for single viewport apps.
    glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
    float L = drawData->DisplayPos.x;
    float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
    float T = drawData->DisplayPos.y;
    float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
    const float ortho_projection[4][4] =
    {
        { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
    };
    glUseProgram(this->imGuiProgram);
    glUniform1i(this->imGui_tex, 0);
    glUniformMatrix4fv(this->imGui_projMtx, 1, GL_FALSE, &ortho_projection[0][0]);
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLESv2:
            break;
        case FSTUFF_GLVersion::GLESv3:
        case FSTUFF_GLVersion::GLCorev3:
            if (this->glBindSampler) {
                this->glBindSampler(0, 0); // We use combined texture/sampler state. Applications using GL 3.3 may set that otherwise.
            }
            break;
    }

    // Recreate the VAO every time
    // (This is to easily allow multiple GL contexts. VAO are not shared among GL contexts, and we don't track creation/deletion of windows so we don't have an obvious key to use to cache them.)
    FSTUFF_GLCheck();
    GLuint vao_handle = 0;
    glGenVertexArrays(1, &vao_handle);
    glBindVertexArray(vao_handle);
    glBindBuffer(GL_ARRAY_BUFFER, this->imGuiVBO);
    glEnableVertexAttribArray(this->imGui_position);
    glEnableVertexAttribArray(this->imGui_uv);
    glEnableVertexAttribArray(this->imGui_color);
    glVertexAttribPointer(this->imGui_position, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(this->imGui_uv, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(this->imGui_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));

    // Draw
    ImVec2 pos = drawData->DisplayPos;
    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = drawData->CmdLists[n];
        const ImDrawIdx* idx_buffer_offset = 0;

        glBindBuffer(GL_ARRAY_BUFFER, this->imGuiVBO);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->imGuiElements);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                // User callback (registered via ImDrawList::AddCallback)
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                ImVec4 clip_rect = ImVec4(pcmd->ClipRect.x - pos.x, pcmd->ClipRect.y - pos.y, pcmd->ClipRect.z - pos.x, pcmd->ClipRect.w - pos.y);
                if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
                {
                    // Apply scissor/clipping rectangle
                    if (clip_origin_lower_left)
                        glScissor((int)clip_rect.x, (int)(fb_height - clip_rect.w), (int)(clip_rect.z - clip_rect.x), (int)(clip_rect.w - clip_rect.y));
                    else
                        glScissor((int)clip_rect.x, (int)clip_rect.y, (int)clip_rect.z, (int)clip_rect.w); // Support for GL 4.5's glClipControl(GL_UPPER_LEFT)

                    // Bind texture, Draw
                    glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                    glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
                }
            }
            idx_buffer_offset += pcmd->ElemCount;
        }
    }
    glDeleteVertexArrays(1, &vao_handle);

    // Restore modified GL state
    glUseProgram(last_program);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLESv2:
            break;
        case FSTUFF_GLVersion::GLESv3:
        case FSTUFF_GLVersion::GLCorev3:
            if (this->glBindSampler) {
                this->glBindSampler(0, last_sampler);
            }
            break;
    }
    glActiveTexture(last_active_texture);
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLESv2:
            break;
        case FSTUFF_GLVersion::GLESv3:
        case FSTUFF_GLVersion::GLCorev3:
            glBindVertexArray(last_vertex_array);
            break;
    }
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
    glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
    if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
#ifdef GL_POLYGON_MODE
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLESv2:
        case FSTUFF_GLVersion::GLESv3:
            break;
        case FSTUFF_GLVersion::GLCorev3:
            glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
            break;
    }
#endif
    glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);

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

FSTUFF_Texture FSTUFF_GLESRenderer::NewTexture(const uint8_t *srcRGBA32, int width, int height)
{
    GLuint id = 0;
    glGenTextures(1, &id);
    FSTUFF_Assert(id > 0 && "glGenTextures failed");
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    switch (this->glVersion) {
        case FSTUFF_GLVersion::GLCorev3:
        case FSTUFF_GLVersion::GLESv3:
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            break;
        case FSTUFF_GLVersion::GLESv2:
            break;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, srcRGBA32);
    FSTUFF_GLCheck();
    return (FSTUFF_Texture) (uintptr_t) id;
}

void FSTUFF_GLESRenderer::DestroyTexture(FSTUFF_Texture tex) {
    FSTUFF_AssertUnimplemented();
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
        case FSTUFF_ShapeSegment: {
            this->segmentMatrices[i] = matrix;
            this->segmentColors[i] = color;
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
        case FSTUFF_ShapeSegment:
            glBindBuffer(GL_ARRAY_BUFFER, this->segmentColorsBufID);
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(float) * 4, this->segmentColors + offset, GL_DYNAMIC_DRAW);
            break;
        case FSTUFF_ShapeDebug:
            glBindBuffer(GL_ARRAY_BUFFER, this->debugShapeColorsBufID);
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(float) * 4, this->debugShapeColors + offset, GL_DYNAMIC_DRAW);
            break;
    }
    glVertexAttribPointer(simVS_colorRGBX, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(simVS_colorRGBX);
    this->glVertexAttribDivisor(simVS_colorRGBX, 1);

    // Send to OpenGL: alpha
    glVertexAttrib1f(simVS_alpha, alpha);

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
        case FSTUFF_ShapeSegment:
            glBindBuffer(GL_ARRAY_BUFFER, this->segmentMatricesBufID);
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(gbMat4), this->segmentMatrices + offset, GL_DYNAMIC_DRAW);
            break;
        case FSTUFF_ShapeDebug:
            glBindBuffer(GL_ARRAY_BUFFER, this->debugShapeMatricesBufID);
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(gbMat4), this->debugShapeMatrices + offset, GL_DYNAMIC_DRAW);
            break;
    }
    for (int i = 0; i < 4; ++i) {
        const int location = simVS_modelMatrix;
        glVertexAttribPointer       (location + i, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void *)(sizeof(float) * 4 * i));
        glEnableVertexAttribArray   (location + i);
        this->glVertexAttribDivisor (location + i, 1);
    }

    // Send to OpenGL: position
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)(uintptr_t) shape->gpuVertexBuffer);
    glVertexAttribPointer(simVS_position, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(simVS_position);

    //
    // Draw!
    //
    FSTUFF_GLCheck();
    this->glDrawArraysInstanced(gpuPrimitiveType, 0, shape->numVertices, (int)count);
    FSTUFF_GLCheck();
}
