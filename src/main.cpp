#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <vector>
#include <iostream>
#include "Shader.h"
#include "Camera.h"

const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 1200;

int gFbWidth  = SCR_WIDTH;
int gFbHeight = SCR_HEIGHT;

// FBO stuff must be accessible in the callback
unsigned int gFBO = 0;
unsigned int gColorTex = 0;
unsigned int gRBO = 0;

Camera gCamera(glm::vec3(0,0,3), glm::vec3(0,1,0), -90.0f, 0.0f);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool lightAnimate = true;      // toggle animation on/off
float lightOrbitRadius = 2.0f; // orbit radius
float lightOrbitSpeed  = 1.0f; // radians/sec (speed)
float lightHeight      = 2.0f; // Y height
float lightAngle       = 0.0f; // current angle (accumulates)
float brightness = 0.0f; // [-1..1] typical
float contrast   = 1.0f; // [0..3] typical
float exposure = 0.0f;          // stops: [-3..+3] nice range
float saturation = 1.0f;        // [0..2] (0=gray, 1=normal, 2=extra)
float vignette = 0.0f;          // [0..1]
float vignetteSoftness = 0.35f; // [0.1..0.8]
bool  bloomEnabled = true;
float bloomThreshold = 1.0f;   // tweak: 0.8..2.0
float bloomStrength  = 0.8f;   // 0..2

unsigned int gBrightTex = 0;
unsigned int pingpongFBO[2] = {0,0};
unsigned int pingpongTex[2] = {0,0};
bool bPressedLastFrame = false;

bool lPressedLastFrame = false;
bool debugPrint = false;
bool pPressedLastFrame = false;
bool mouseCaptured = true;
bool altPressedLastFrame = false;

bool lightSnapMode = true;   // if true: use 8-direction snapping
int  lightSnapSteps = 8;     // 4 or 8
int  lightSnapIndex = 0;     // which direction (0..steps-1)
bool leftPressedLastFrame = false;
bool rightPressedLastFrame = false;
bool tPressedLastFrame = false;
bool fourPressedLastFrame = false;
bool eightPressedLastFrame = false;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);


// Shader sources
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 FragPos;
out vec3 Normal;

void main() {
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    Normal  = mat3(transpose(inverse(uModel))) * aNormal;

    gl_Position = uProj * uView * vec4(FragPos, 1.0);
})";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uObjectColor;

void main() {
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightPos - FragPos);

    float diff = max(dot(norm, lightDir), 0.0);

    vec3 ambient = 0.15 * uLightColor;
    vec3 diffuse = diff * uLightColor;

    vec3 result = (ambient + diffuse) * uObjectColor;
    FragColor = vec4(result, 1.0);
})";

const char* ppVertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* ppFragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScene;

// existing
uniform float uBrightness; // 0 = no change
uniform float uContrast;   // 1 = no change

// new
uniform float uExposure;   // 0 = no change (stops)
uniform float uSaturation; // 1 = no change
uniform float uVignette;   // 0 = off, 1 = strong
uniform float uVignetteSoftness; // 0.1..0.8 typical
uniform sampler2D uBloom;
uniform float uBloomStrength;
uniform bool  uBloomEnabled;


void main() {
    vec3 color = texture(uScene, vUV).rgb;

    // --- exposure (photographic) ---
    // exposure in "stops": +1 doubles brightness, -1 halves
    color *= exp2(uExposure);

    // --- brightness ---
    color += vec3(uBrightness);

    // --- contrast (pivot around 0.5) ---
    color = (color - 0.5) * uContrast + 0.5;

    // --- saturation ---
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 gray = vec3(luma);
    color = mix(gray, color, uSaturation);

    // --- vignette ---
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(vUV, center); // 0 at center, ~0.707 at corner
    // smooth darkening curve
    float vig = smoothstep(0.707 - uVignetteSoftness, 0.707, dist);
    color *= (1.0 - uVignette * vig);

    if (uBloomEnabled) {
    vec3 bloom = texture(uBloom, vUV).rgb;
    color += bloom * uBloomStrength;
    }

    FragColor = vec4(color, 1.0);
}
)";

const char* brightFragSrc = R"(
#version 330 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScene;
uniform float uThreshold;

void main() {
    vec3 c = texture(uScene, vUV).rgb;
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    vec3 outC = (luma > uThreshold) ? c : vec3(0.0);
    FragColor = vec4(outC, 1.0);
}
)";

const char* blurFragSrc = R"(
#version 330 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uImage;
uniform bool uHorizontal;
uniform vec2 uTexelSize; // 1/width, 1/height

void main() {
    // 5-tap gaussian (small + fast)
    float w0 = 0.227027;
    float w1 = 0.1945946;
    float w2 = 0.1216216;

    vec2 off = uHorizontal ? vec2(uTexelSize.x, 0.0) : vec2(0.0, uTexelSize.y);

    vec3 result = texture(uImage, vUV).rgb * w0;
    result += texture(uImage, vUV + off * 1.0).rgb * w1;
    result += texture(uImage, vUV - off * 1.0).rgb * w1;
    result += texture(uImage, vUV + off * 2.0).rgb * w2;
    result += texture(uImage, vUV - off * 2.0).rgb * w2;

    FragColor = vec4(result, 1.0);
}
)";



void recreateFramebufferAttachments(int width, int height) {
    gFbWidth = width;
    gFbHeight = height;

    // Resize color texture
    glBindTexture(GL_TEXTURE_2D, gColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    // Resize depth-stencil renderbuffer
    glBindRenderbuffer(GL_RENDERBUFFER, gRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    if (gBrightTex != 0) {
        glBindTexture(GL_TEXTURE_2D, gBrightTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    }

    for (int i = 0; i < 2; i++) {
        if (pingpongTex[i] != 0) {
            glBindTexture(GL_TEXTURE_2D, pingpongTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        }
    }

    // Optional: sanity check
    glBindFramebuffer(GL_FRAMEBUFFER, gFBO);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "ERROR: Framebuffer not complete after resize!\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



int main() {
    // GLFW and OpenGL setup
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Manual Aperture Blades", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }


    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    mouseCaptured = true;
    gCamera.setMouseSensitivity(0.1f);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }
    // ENABLE DEPTH TESTING (3D rendering)
    glEnable(GL_DEPTH_TEST);

    // Compile shaders
    Shader lightingShader(vertexShaderSource, fragmentShaderSource);

    //Example cube vertex data (minimal):
    float cubeVertices[] = {
        // positions           // normals
        // back face
        -0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,
         0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f,
         0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,
         0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f,
        -0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,
        -0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f,

        // front face
        -0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,
         0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,
         0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f,
         0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,

        // left face
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, 0.5f,-0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f,-0.5f,-0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f,-0.5f,-0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f,-0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,

        // right face
         0.5f, 0.5f, 0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,-0.5f,-0.5f,  1.0f, 0.0f, 0.0f,
         0.5f, 0.5f,-0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,-0.5f,-0.5f,  1.0f, 0.0f, 0.0f,
         0.5f, 0.5f, 0.5f,  1.0f, 0.0f, 0.0f,
         0.5f,-0.5f, 0.5f,  1.0f, 0.0f, 0.0f,

        // bottom face
        -0.5f,-0.5f,-0.5f,  0.0f,-1.0f, 0.0f,
         0.5f,-0.5f,-0.5f,  0.0f,-1.0f, 0.0f,
         0.5f,-0.5f, 0.5f,  0.0f,-1.0f, 0.0f,
         0.5f,-0.5f, 0.5f,  0.0f,-1.0f, 0.0f,
        -0.5f,-0.5f, 0.5f,  0.0f,-1.0f, 0.0f,
        -0.5f,-0.5f,-0.5f,  0.0f,-1.0f, 0.0f,

        // top face
        -0.5f, 0.5f,-0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, 0.5f,-0.5f,  0.0f, 1.0f, 0.0f,
         0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f, 0.5f,-0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f
    };

    //This is the floor
    float planeVertices[] = {
        // positions            // normals
        -5.0f, 0.0f, -5.0f,     0.0f, 1.0f, 0.0f,
         5.0f, 0.0f, -5.0f,     0.0f, 1.0f, 0.0f,
         5.0f, 0.0f,  5.0f,     0.0f, 1.0f, 0.0f,

         5.0f, 0.0f,  5.0f,     0.0f, 1.0f, 0.0f,
        -5.0f, 0.0f,  5.0f,     0.0f, 1.0f, 0.0f,
        -5.0f, 0.0f, -5.0f,     0.0f, 1.0f, 0.0f
    };


    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    //
    //this is for the floor
    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);

    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ----- Compile post-process shader program -----
    Shader postShader(ppVertexShaderSrc, ppFragmentShaderSrc);
    postShader.use();
    postShader.setInt("uScene", 0); // texture unit 0 once
    Shader brightShader(ppVertexShaderSrc, brightFragSrc);
    brightShader.use();
    brightShader.setInt("uScene", 0);

    Shader blurShader(ppVertexShaderSrc, blurFragSrc);
    blurShader.use();
    blurShader.setInt("uImage", 0);



    //for the birghtness and contrast
    glGenFramebuffers(1, &gFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gFBO);

    // color texture
    glGenTextures(1, &gColorTex);
    glBindTexture(GL_TEXTURE_2D, gColorTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gColorTex, 0);

    // depth + stencil
    glGenRenderbuffers(1, &gRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, gRBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gRBO);

    // allocate initial storage using current framebuffer size
    recreateFramebufferAttachments(SCR_WIDTH, SCR_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //BLOOM
    unsigned int brightFBO = 0;
    glGenFramebuffers(1, &brightFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, brightFBO);

    glGenTextures(1, &gBrightTex);
    glBindTexture(GL_TEXTURE_2D, gBrightTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, gFbWidth, gFbHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gBrightTex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "ERROR: Bright FBO incomplete!\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongTex);

    for (int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, gFbWidth, gFbHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongTex[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "ERROR: Pingpong FBO " << i << " incomplete!\n";
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);





    float quadVertices[] = {
        // positions   // uvs
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,

         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f
    };

    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);





    // --- Render Loop ---
    while (!glfwWindowShouldClose(window)) {

        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glBindFramebuffer(GL_FRAMEBUFFER, gFBO);
        glEnable(GL_DEPTH_TEST);
        glClearColor(1,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // use your lighting shader and draw floor + cubes (your existing code)

        lightingShader.use();

        // ----- COMMON MATRICES -----
        glm::mat4 view = gCamera.getViewMatrix();
        glm::mat4 proj = glm::perspective(glm::radians(gCamera.fov()), (float)gFbWidth / (float)gFbHeight, 0.1f, 100.0f);

        lightingShader.setMat4("uView", view);
        lightingShader.setMat4("uProj", proj);

        // ----- LIGHT (common to all objects) -----
        glm::vec3 lightPos;

        if (lightSnapMode) {
            const float step = glm::two_pi<float>() / (float)lightSnapSteps;
            float ang = (float)lightSnapIndex * step;

            lightPos = glm::vec3(
                cos(ang) * lightOrbitRadius,
                lightHeight,
                sin(ang) * lightOrbitRadius
            );
        } else {
            if (lightAnimate) {
                lightAngle += lightOrbitSpeed * deltaTime;
            }

            lightPos = glm::vec3(
                cos(lightAngle) * lightOrbitRadius,
                lightHeight,
                sin(lightAngle) * lightOrbitRadius
            );
        }

        lightingShader.setVec3("uLightPos", lightPos);
        lightingShader.setVec3("uLightColor", glm::vec3(1.0f));

        // =========================================
        // DRAW FLOOR
        // =========================================

        glm::mat4 floorModel = glm::mat4(1.0f);
        lightingShader.setMat4("uModel", floorModel);
        lightingShader.setVec3("uObjectColor", glm::vec3(0.7f));

        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // =========================================
        // DRAW CUBE
        // =========================================

        glm::mat4 cubeModel = glm::mat4(1.0f);
        cubeModel = glm::translate(cubeModel, glm::vec3(0.0f, 0.5f, 0.0f));
        cubeModel = glm::rotate(cubeModel, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));

        lightingShader.setMat4("uModel", cubeModel);
        lightingShader.setVec3("uObjectColor", glm::vec3(0.2f));

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // =========================================
        // DRAW SECOND OBJECT (another cube)
        // =========================================
        glm::mat4 cube2Model = glm::mat4(1.0f);

        // put it somewhere else in the scene
        cube2Model = glm::translate(cube2Model, glm::vec3(2.0f, 0.5f, -1.0f));

        // make it taller like a simple “tree trunk”
        cube2Model = glm::scale(cube2Model, glm::vec3(0.5f, 2.0f, 0.5f));

        // (optional) rotate it a bit so it’s not identical
        cube2Model = glm::rotate(cube2Model, glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        lightingShader.setMat4("uModel", cube2Model);
        lightingShader.setVec3("uObjectColor", glm::vec3(0.25f, 0.18f, 0.10f));

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindFramebuffer(GL_FRAMEBUFFER, brightFBO);
        glDisable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT);

        brightShader.use();
        brightShader.setFloat("uThreshold", bloomThreshold);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gColorTex);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        bool horizontal = true;
        bool firstIteration = true;
        int blurPasses = 10;

        blurShader.use();
        blurShader.setVec2("uTexelSize", glm::vec2(1.0f / gFbWidth, 1.0f / gFbHeight));

        for (int i = 0; i < blurPasses; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal ? 0 : 1]);
            blurShader.setInt("uHorizontal", horizontal ? 1 : 0);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, firstIteration ? gBrightTex : pingpongTex[horizontal ? 1 : 0]);

            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            horizontal = !horizontal;
            firstIteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // final blurred result:
        unsigned int blurredBloomTex = pingpongTex[horizontal ? 1 : 0];





        // =========================
        // PASS B: render quad to screen
        // =========================
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT);

        postShader.use();

        // post params
        postShader.setFloat("uBrightness", brightness);
        postShader.setFloat("uContrast", contrast);
        postShader.setFloat("uExposure", exposure);
        postShader.setFloat("uSaturation", saturation);
        postShader.setFloat("uVignette", vignette);
        postShader.setFloat("uVignetteSoftness", vignetteSoftness);

        // bloom params
        postShader.setInt("uScene", 0);
        postShader.setInt("uBloom", 1);
        postShader.setFloat("uBloomStrength", bloomStrength);
        postShader.setInt("uBloomEnabled", bloomEnabled ? 1 : 0);


        // textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gColorTex);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, blurredBloomTex);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);



        if (debugPrint) {
            static float t = 0.0f;
            t += deltaTime;
            if (t > 0.5f) {
                t = 0.0f;
                std::cout << "lightAnimate=" << lightAnimate
                          << " radius=" << lightOrbitRadius
                          << " speed=" << lightOrbitSpeed
                          << " height=" << lightHeight
                          << " contrast=" << contrast
                          << " brightness=" << brightness
                          << " exposure=" << exposure
                          << " saturation=" << saturation
                          << " vignette=" << vignette
                          << " vignetteSoftness=" << vignetteSoftness
                          << "\n";
            }
        }


        glfwSwapBuffers(window);
        glfwPollEvents();
    }


    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 2.5f * deltaTime;

    bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

    gCamera.processKeyboard(w, s, a, d, deltaTime);

    //for alt tabiing out
    bool altPressed = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS;
    if (altPressed && !altPressedLastFrame) {
        mouseCaptured = !mouseCaptured;

        glfwSetInputMode(window,
            GLFW_CURSOR,
            mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
        );

        // Important: prevent a huge jump when recapturing
        gCamera.resetFirstMouse();
    }
    altPressedLastFrame = altPressed;
    if (!mouseCaptured && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        mouseCaptured = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        gCamera.resetFirstMouse();
    }


    bool lPressed = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    if (lPressed && !lPressedLastFrame) {
        lightAnimate = !lightAnimate;
    }
    lPressedLastFrame = lPressed;

    // radius: J/K
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
        lightOrbitRadius = std::max(0.2f, lightOrbitRadius - 1.5f * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
        lightOrbitRadius += 1.5f * deltaTime;

    // speed: U/I
    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
        lightOrbitSpeed = std::max(0.0f, lightOrbitSpeed - 2.0f * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
        lightOrbitSpeed += 2.0f * deltaTime;

    // height: N/M
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS)
        lightHeight -= 2.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS)
        lightHeight += 2.0f * deltaTime;
    bool pPressed = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
    if (pPressed && !pPressedLastFrame) debugPrint = !debugPrint;
    pPressedLastFrame = pPressed;

    //brightness: []
    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
        brightness -= 0.8f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
        brightness += 0.8f * deltaTime;

    //contrast: ;'
    if (glfwGetKey(window, GLFW_KEY_SEMICOLON) == GLFW_PRESS)
        contrast = std::max(0.0f, contrast - 1.5f * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_APOSTROPHE) == GLFW_PRESS)
        contrast += 1.5f * deltaTime;

    // exposure: O/P
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
        exposure -= 1.5f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
        exposure += 1.5f * deltaTime;

    // saturation: , .
    if (glfwGetKey(window, GLFW_KEY_COMMA) == GLFW_PRESS)
        saturation = std::max(0.0f, saturation - 1.5f * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_PERIOD) == GLFW_PRESS)
        saturation += 1.5f * deltaTime;

    // vignette: V/B
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS)
        vignette = std::max(0.0f, vignette - 1.0f * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
        vignette = std::min(1.0f, vignette + 1.0f * deltaTime);

    // vignette softness: G/H
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
        vignetteSoftness = std::max(0.05f, vignetteSoftness - 0.8f * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
        vignetteSoftness = std::min(0.9f, vignetteSoftness + 0.8f * deltaTime);

    // Toggle snap mode (T)
    bool tPressed = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
    if (tPressed && !tPressedLastFrame) {
        lightSnapMode = !lightSnapMode;
    }
    tPressedLastFrame = tPressed;

    // Switch 4/8 directions
    bool fourPressed = glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS;
    if (fourPressed && !fourPressedLastFrame) {
        lightSnapSteps = 4;
        lightSnapIndex %= lightSnapSteps;
    }
    fourPressedLastFrame = fourPressed;

    bool eightPressed = glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS;
    if (eightPressed && !eightPressedLastFrame) {
        lightSnapSteps = 8;
        lightSnapIndex %= lightSnapSteps;
    }
    eightPressedLastFrame = eightPressed;

    // Step direction with arrows (single-step per press)
    bool leftPressed = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
    if (leftPressed && !leftPressedLastFrame) {
        lightSnapIndex = (lightSnapIndex - 1 + lightSnapSteps) % lightSnapSteps;
    }
    leftPressedLastFrame = leftPressed;

    bool rightPressed = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    if (rightPressed && !rightPressedLastFrame) {
        lightSnapIndex = (lightSnapIndex + 1) % lightSnapSteps;
    }
    rightPressedLastFrame = rightPressed;

    bool bPressed = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
    if (bPressed && !bPressedLastFrame) bloomEnabled = !bloomEnabled;
    bPressedLastFrame = bPressed;

    // threshold: X/C
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) bloomThreshold = std::max(0.0f, bloomThreshold - 1.0f * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) bloomThreshold += 1.0f * deltaTime;

    // strength: R/F
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) bloomStrength = std::max(0.0f, bloomStrength - 1.0f * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) bloomStrength += 1.0f * deltaTime;





}


void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);

    // Avoid zero-sized resize (can happen when minimizing)
    if (width > 0 && height > 0 && gFBO != 0) {
        recreateFramebufferAttachments(width, height);
    }
}


void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    gCamera.processMouse((float)xpos, (float)ypos, mouseCaptured);
}


void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;
    gCamera.processScroll((float)yoffset);
}


