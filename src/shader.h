/*
 * shader.h - Shader manager for NTSC artifact color post-processing
 *
 * This provides FBO-based post-processing support for applying authentic
 * CoCo NTSC artifact colors to the rendered game output.
 */

#ifndef DOD_SHADER_HEADER
#define DOD_SHADER_HEADER

#include "dod.h"

class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    // Initialize shader infrastructure
    bool init();

    // Clean up all resources
    void shutdown();

    // FBO operations for render-to-texture
    bool createRenderTarget(int width, int height);
    void beginRenderToTexture();
    void endRenderToTexture();

    // Artifact shader operations
    bool loadArtifactShader();
    void applyArtifactEffect(bool phaseFlip);

    // State queries
    bool isInitialized() const { return m_initialized; }

private:
    // Shader compilation helpers
    bool compileShader(GLuint shader, const char* source);
    bool linkProgram(GLuint program);
    void createFullscreenQuad();

    // FBO resources
    GLuint m_fbo;
    GLuint m_renderTexture;
    int m_texWidth;
    int m_texHeight;

    // Shader program
    GLuint m_artifactProgram;
    GLuint m_vertexShader;
    GLuint m_fragmentShader;

    // Uniform locations
    GLint m_textureLoc;
    GLint m_resolutionLoc;
    GLint m_phaseFlipLoc;

    // Fullscreen quad VBO
    GLuint m_quadVBO;

    // State
    bool m_initialized;
};

extern ShaderManager shaderMgr;

#endif // DOD_SHADER_HEADER
