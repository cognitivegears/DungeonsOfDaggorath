/*
 * artifact_shader.h - Embedded GLSL shaders for NTSC artifact color emulation
 *
 * These shaders are WebGL 1.0 / GLSL ES 1.0 compatible for Emscripten builds.
 * They implement phase-based artifact coloring that matches authentic CoCo
 * composite video output.
 */

#ifndef DOD_ARTIFACT_SHADER_HEADER
#define DOD_ARTIFACT_SHADER_HEADER

// Vertex Shader - Simple pass-through for fullscreen quad
static const char* ARTIFACT_VERTEX_SHADER =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

// Fragment Shader - NTSC artifact color emulation
// Converts white pixels to cyan or orange based on X-coordinate phase
static const char* ARTIFACT_FRAGMENT_SHADER =
    "precision mediump float;\n"
    "\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec2 u_resolution;\n"
    "uniform int u_phaseFlip;\n"
    "\n"
    "// CoCo artifact colors (tuned to match MC1372 composite output)\n"
    "const vec3 BLACK  = vec3(0.0, 0.0, 0.0);\n"
    "const vec3 CYAN   = vec3(0.0, 0.85, 0.90);\n"
    "const vec3 ORANGE = vec3(1.0, 0.45, 0.0);\n"
    "const vec3 WHITE  = vec3(1.0, 1.0, 1.0);\n"
    "\n"
    "// Get luminance of pixel (determine if white or black)\n"
    "float getLuma(vec2 coord) {\n"
    "    vec3 color = texture2D(u_texture, coord).rgb;\n"
    "    return step(0.5, dot(color, vec3(0.299, 0.587, 0.114)));\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 texelSize = 1.0 / u_resolution;\n"
    "    vec2 pixelCoord = v_texcoord * u_resolution;\n"
    "\n"
    "    // Calculate X position as integer\n"
    "    // GLSL ES 1.0 doesn't have integer modulo, use arithmetic\n"
    "    float x = floor(pixelCoord.x);\n"
    "\n"
    "    // Sample current pixel and horizontal neighbors\n"
    "    float center = getLuma(v_texcoord);\n"
    "    float left   = getLuma(v_texcoord - vec2(texelSize.x, 0.0));\n"
    "    float right  = getLuma(v_texcoord + vec2(texelSize.x, 0.0));\n"
    "\n"
    "    // Determine phase (0 or 1) based on X coordinate\n"
    "    float phase = x - floor(x / 2.0) * 2.0;\n"
    "\n"
    "    // Apply phase flip if enabled\n"
    "    if (u_phaseFlip == 1) {\n"
    "        phase = 1.0 - phase;\n"
    "    }\n"
    "\n"
    "    vec3 outputColor = BLACK;\n"
    "\n"
    "    if (center > 0.5) {\n"
    "        // This is a white pixel - determine artifact color\n"
    "\n"
    "        if (left > 0.5 && right > 0.5) {\n"
    "            // Surrounded by white on both sides - stays white\n"
    "            outputColor = WHITE;\n"
    "        } else if (phase < 0.5) {\n"
    "            // Even pixel phase - orange artifact\n"
    "            outputColor = ORANGE;\n"
    "            // Blend toward white if adjacent to another white pixel\n"
    "            if (left > 0.5 || right > 0.5) {\n"
    "                outputColor = mix(ORANGE, WHITE, 0.35);\n"
    "            }\n"
    "        } else {\n"
    "            // Odd pixel phase - cyan artifact\n"
    "            outputColor = CYAN;\n"
    "            // Blend toward white if adjacent to another white pixel\n"
    "            if (left > 0.5 || right > 0.5) {\n"
    "                outputColor = mix(CYAN, WHITE, 0.35);\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "\n"
    "    gl_FragColor = vec4(outputColor, 1.0);\n"
    "}\n";

#endif // DOD_ARTIFACT_SHADER_HEADER
