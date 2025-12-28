/*
 * shader.cpp - Shader manager implementation for NTSC artifact colors
 *
 * Implements FBO-based post-processing for authentic CoCo artifact
 * color emulation using WebGL 1.0 compatible shaders.
 */

#include "shader.h"
#include "artifact_shader.h"
#include "oslink.h"
#include <cstdio>
#include <cstring>

// Global shader manager instance
ShaderManager shaderMgr;

// External reference to OS_Link for window dimensions
extern OS_Link oslink;

ShaderManager::ShaderManager()
    : m_fbo(0)
    , m_renderTexture(0)
    , m_texWidth(0)
    , m_texHeight(0)
    , m_artifactProgram(0)
    , m_vertexShader(0)
    , m_fragmentShader(0)
    , m_textureLoc(-1)
    , m_resolutionLoc(-1)
    , m_phaseFlipLoc(-1)
    , m_quadVBO(0)
    , m_initialized(false)
{
}

ShaderManager::~ShaderManager()
{
    shutdown();
}

bool ShaderManager::init()
{
    if (m_initialized) {
        return true;
    }

    // Create render target at native CoCo resolution (256x192)
    if (!createRenderTarget(256, 192)) {
        fprintf(stderr, "ShaderManager: Failed to create render target\n");
        return false;
    }

    // Load and compile artifact shader
    if (!loadArtifactShader()) {
        fprintf(stderr, "ShaderManager: Failed to load artifact shader\n");
        // Clean up partial initialization
        if (m_renderTexture) {
            glDeleteTextures(1, &m_renderTexture);
            m_renderTexture = 0;
        }
        if (m_fbo) {
            glDeleteFramebuffers(1, &m_fbo);
            m_fbo = 0;
        }
        return false;
    }

    // Create fullscreen quad for post-processing
    createFullscreenQuad();

    m_initialized = true;
    fprintf(stdout, "ShaderManager: Initialized successfully\n");
    return true;
}

void ShaderManager::shutdown()
{
    if (m_quadVBO) {
        glDeleteBuffers(1, &m_quadVBO);
        m_quadVBO = 0;
    }
    if (m_artifactProgram) {
        glDeleteProgram(m_artifactProgram);
        m_artifactProgram = 0;
    }
    if (m_vertexShader) {
        glDeleteShader(m_vertexShader);
        m_vertexShader = 0;
    }
    if (m_fragmentShader) {
        glDeleteShader(m_fragmentShader);
        m_fragmentShader = 0;
    }
    if (m_renderTexture) {
        glDeleteTextures(1, &m_renderTexture);
        m_renderTexture = 0;
    }
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    m_initialized = false;
}

bool ShaderManager::createRenderTarget(int width, int height)
{
    m_texWidth = width;
    m_texHeight = height;

    // Create framebuffer object
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Create texture to render to
    glGenTextures(1, &m_renderTexture);
    glBindTexture(GL_TEXTURE_2D, m_renderTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, NULL);

    // Use NEAREST filtering to preserve pixel-perfect rendering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Attach texture to framebuffer as color attachment
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_renderTexture, 0);

    // Check framebuffer completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "ShaderManager: Framebuffer not complete: 0x%x\n", status);
        return false;
    }

    return true;
}

bool ShaderManager::compileShader(GLuint shader, const char* source)
{
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "ShaderManager: Shader compilation failed:\n%s\n", infoLog);
        return false;
    }
    return true;
}

bool ShaderManager::linkProgram(GLuint program)
{
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        fprintf(stderr, "ShaderManager: Program linking failed:\n%s\n", infoLog);
        return false;
    }
    return true;
}

bool ShaderManager::loadArtifactShader()
{
    // Create shader objects
    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    // Compile vertex shader
    if (!compileShader(m_vertexShader, ARTIFACT_VERTEX_SHADER)) {
        fprintf(stderr, "ShaderManager: Vertex shader compilation failed\n");
        return false;
    }

    // Compile fragment shader
    if (!compileShader(m_fragmentShader, ARTIFACT_FRAGMENT_SHADER)) {
        fprintf(stderr, "ShaderManager: Fragment shader compilation failed\n");
        return false;
    }

    // Create program and attach shaders
    m_artifactProgram = glCreateProgram();
    glAttachShader(m_artifactProgram, m_vertexShader);
    glAttachShader(m_artifactProgram, m_fragmentShader);

    // Bind attribute locations before linking
    glBindAttribLocation(m_artifactProgram, 0, "a_position");
    glBindAttribLocation(m_artifactProgram, 1, "a_texcoord");

    // Link program
    if (!linkProgram(m_artifactProgram)) {
        fprintf(stderr, "ShaderManager: Program linking failed\n");
        return false;
    }

    // Get uniform locations
    m_textureLoc = glGetUniformLocation(m_artifactProgram, "u_texture");
    m_resolutionLoc = glGetUniformLocation(m_artifactProgram, "u_resolution");
    m_phaseFlipLoc = glGetUniformLocation(m_artifactProgram, "u_phaseFlip");

    return true;
}

void ShaderManager::createFullscreenQuad()
{
    // Fullscreen quad vertices: position (x,y) and texcoord (u,v)
    // Triangle strip order: bottom-left, bottom-right, top-left, top-right
    float quadVertices[] = {
        // Position    // TexCoord
        -1.0f, -1.0f,  0.0f, 0.0f,  // bottom-left
         1.0f, -1.0f,  1.0f, 0.0f,  // bottom-right
        -1.0f,  1.0f,  0.0f, 1.0f,  // top-left
         1.0f,  1.0f,  1.0f, 1.0f,  // top-right
    };

    glGenBuffers(1, &m_quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ShaderManager::beginRenderToTexture()
{
    // Bind our FBO as the render target
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Set viewport to native CoCo resolution
    glViewport(0, 0, m_texWidth, m_texHeight);
}

void ShaderManager::endRenderToTexture()
{
    // Unbind FBO, return to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShaderManager::applyArtifactEffect(bool phaseFlip)
{
    // Restore viewport to window size (4:3 aspect ratio)
    int windowHeight = (int)(oslink.width * 0.75);
    glViewport(0, 0, oslink.width, windowHeight);

    // Clear the default framebuffer
    glClear(GL_COLOR_BUFFER_BIT);

    // Use the artifact shader program
    glUseProgram(m_artifactProgram);

    // Bind the rendered texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_renderTexture);

    // Set uniforms
    glUniform1i(m_textureLoc, 0);
    glUniform2f(m_resolutionLoc, (float)m_texWidth, (float)m_texHeight);
    glUniform1i(m_phaseFlipLoc, phaseFlip ? 1 : 0);

    // Set up vertex attributes and draw fullscreen quad
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // Position attribute (location 0): 2 floats, stride 4 floats, offset 0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    // Texcoord attribute (location 1): 2 floats, stride 4 floats, offset 2 floats
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));

    // Draw the fullscreen quad as a triangle strip
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Clean up state
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}
