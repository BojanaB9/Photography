#include "Scene.h"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <iostream>
#include <stb_image.h>

static unsigned int loadTexture2D(const std::string& path)
{
    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // wrapping + filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);

    int w, h, n;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 0);
    if (!data) {
        std::cout << "Failed to load texture: " << path << "\n";
        return 0;
    }

    GLenum format = (n == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return tex;
}
// Cube (same layout you already use): pos(3) + normal(3)
static float kCubeVertices[] = {
    // back
    -0.5f,-0.5f,-0.5f,  0,0,-1,
     0.5f, 0.5f,-0.5f,  0,0,-1,
     0.5f,-0.5f,-0.5f,  0,0,-1,
     0.5f, 0.5f,-0.5f,  0,0,-1,
    -0.5f,-0.5f,-0.5f,  0,0,-1,
    -0.5f, 0.5f,-0.5f,  0,0,-1,

    // front
    -0.5f,-0.5f, 0.5f,  0,0, 1,
     0.5f,-0.5f, 0.5f,  0,0, 1,
     0.5f, 0.5f, 0.5f,  0,0, 1,
     0.5f, 0.5f, 0.5f,  0,0, 1,
    -0.5f, 0.5f, 0.5f,  0,0, 1,
    -0.5f,-0.5f, 0.5f,  0,0, 1,

    // left
    -0.5f, 0.5f, 0.5f, -1,0,0,
    -0.5f, 0.5f,-0.5f, -1,0,0,
    -0.5f,-0.5f,-0.5f, -1,0,0,
    -0.5f,-0.5f,-0.5f, -1,0,0,
    -0.5f,-0.5f, 0.5f, -1,0,0,
    -0.5f, 0.5f, 0.5f, -1,0,0,

    // right
     0.5f, 0.5f, 0.5f,  1,0,0,
     0.5f,-0.5f,-0.5f,  1,0,0,
     0.5f, 0.5f,-0.5f,  1,0,0,
     0.5f,-0.5f,-0.5f,  1,0,0,
     0.5f, 0.5f, 0.5f,  1,0,0,
     0.5f,-0.5f, 0.5f,  1,0,0,

    // bottom
    -0.5f,-0.5f,-0.5f,  0,-1,0,
     0.5f,-0.5f,-0.5f,  0,-1,0,
     0.5f,-0.5f, 0.5f,  0,-1,0,
     0.5f,-0.5f, 0.5f,  0,-1,0,
    -0.5f,-0.5f, 0.5f,  0,-1,0,
    -0.5f,-0.5f,-0.5f,  0,-1,0,

    // top
    -0.5f, 0.5f,-0.5f,  0, 1,0,
     0.5f, 0.5f, 0.5f,  0, 1,0,
     0.5f, 0.5f,-0.5f,  0, 1,0,
     0.5f, 0.5f, 0.5f,  0, 1,0,
    -0.5f, 0.5f,-0.5f,  0, 1,0,
    -0.5f, 0.5f, 0.5f,  0, 1,0
};

// Big plane, same layout: pos(3) + normal(3)
static float kPlaneVertices[] = {
    -1,0,-1,  0,1,0,
     1,0,-1,  0,1,0,
     1,0, 1,  0,1,0,

     1,0, 1,  0,1,0,
    -1,0, 1,  0,1,0,
    -1,0,-1,  0,1,0
};

static float heightAt(float x, float z)
{
    const float amp  = 0.35f;   // hill height
    const float freq = 0.35f;   // hill frequency
    return amp * std::sin(x * freq) * std::cos(z * freq);
}


static glm::vec3 normalAt(float x, float z)
{
    const float eps = 0.05f;
    float hL = heightAt(x - eps, z);
    float hR = heightAt(x + eps, z);
    float hD = heightAt(x, z - eps);
    float hU = heightAt(x, z + eps);

    // slope vectors
    glm::vec3 dx(2.0f * eps, hR - hL, 0.0f);
    glm::vec3 dz(0.0f, hU - hD, 2.0f * eps);

    glm::vec3 n = glm::normalize(glm::cross(dz, dx));
    return n;
}


void Scene::init() {
    // Cube
    glGenVertexArrays(1, &mCubeVAO);
    glGenBuffers(1, &mCubeVBO);
    glBindVertexArray(mCubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mCubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVertices), kCubeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    mTexGrass = loadTexture2D(std::string(PROJECT_SOURCE_DIR) + "/src/textures/grass.jpg");
    mTexWater = loadTexture2D(std::string(PROJECT_SOURCE_DIR) + "/src/textures/water.jpg");
    mTexRock  = loadTexture2D(std::string(PROJECT_SOURCE_DIR) + "/src/textures/rock.jpg");
    mTexBark  = loadTexture2D(std::string(PROJECT_SOURCE_DIR) + "/src/textures/bark.jpg");
    mTexLeaf  = loadTexture2D(std::string(PROJECT_SOURCE_DIR) + "/src/textures/leaves.jpg");


    // Plane
    glGenVertexArrays(1, &mPlaneVAO);
    glGenBuffers(1, &mPlaneVBO);
    glBindVertexArray(mPlaneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mPlaneVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kPlaneVertices), kPlaneVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    // --- Hills ground mesh (grid) ---
    const int N = 120;              // grid resolution (try 80–200)
    const float size = 40.0f;       // world size (matches your earlier scale)
    const float half = size * 0.5f;

    std::vector<float> verts;       // pos(3) + normal(3)
    std::vector<unsigned int> idx;

    verts.reserve(N * N * 6);

    for (int z = 0; z < N; z++) {
        for (int x = 0; x < N; x++) {
            float u = (float)x / (N - 1);
            float v = (float)z / (N - 1);

            float worldX = -half + u * size;
            float worldZ = -half + v * size;

            float y = heightAt(worldX, worldZ);
            glm::vec3 n = normalAt(worldX, worldZ);

            verts.push_back(worldX);
            verts.push_back(y);
            verts.push_back(worldZ);

            verts.push_back(n.x);
            verts.push_back(n.y);
            verts.push_back(n.z);
        }
    }

    // indices (two triangles per cell)
    for (int z = 0; z < N - 1; z++) {
        for (int x = 0; x < N - 1; x++) {
            unsigned int i0 = (unsigned int)(z * N + x);
            unsigned int i1 = i0 + 1;
            unsigned int i2 = i0 + (unsigned int)N;
            unsigned int i3 = i2 + 1;

            // triangle 1
            idx.push_back(i0);
            idx.push_back(i2);
            idx.push_back(i1);

            // triangle 2
            idx.push_back(i1);
            idx.push_back(i2);
            idx.push_back(i3);
        }
    }

    mGroundIndexCount = (int)idx.size();

    glGenVertexArrays(1, &mGroundVAO);
    glGenBuffers(1, &mGroundVBO);
    glGenBuffers(1, &mGroundEBO);

    glBindVertexArray(mGroundVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mGroundVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mGroundEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);

    // layout: location 0 = position, location 1 = normal
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    // --- River ribbon mesh ---
{
    const int S = 220;          // samples along the river (smoothness)
    const float zMin = -18.0f;
    const float zMax =  18.0f;

    const float halfWidth = 1.6f;   // half river width
    const float yOffset   = 0.03f;  // lift slightly above ground to avoid z-fighting

    // vertex layout = pos(3) + normal(3) like your shaders expect
    std::vector<float> rv;
    rv.reserve(S * 2 * 6); // 2 verts per sample, 6 floats per vert

    auto riverCenterX = [](float z) {
        // Simple curve: sine meander
        return 3.0f * std::sin(z * 0.25f);
    };

    auto riverCenterDxDz = [](float z) {
        // derivative of 3*sin(0.25z) => 3*0.25*cos(0.25z)
        return 0.75f * std::cos(z * 0.25f);
    };

    for (int i = 0; i < S; i++) {
        float t = (float)i / (float)(S - 1);
        float z = zMin + t * (zMax - zMin);

        float x = riverCenterX(z);

        // Tangent in XZ: (dx/dz, 1)
        float dx = riverCenterDxDz(z);
        glm::vec2 tangent(dx, 1.0f);
        tangent = glm::normalize(tangent);

        // Perpendicular (left) in XZ
        glm::vec2 perp(-tangent.y, tangent.x);

        // Left/right positions in world
        glm::vec2 leftXZ  = glm::vec2(x, z) + perp * halfWidth;
        glm::vec2 rightXZ = glm::vec2(x, z) - perp * halfWidth;

        float yL = heightAt(leftXZ.x,  leftXZ.y)  + yOffset;
        float yR = heightAt(rightXZ.x, rightXZ.y) + yOffset;

        // Normals: for water you can just use up (looks fine for now)
        glm::vec3 n(0,1,0);

        // left vertex
        rv.push_back(leftXZ.x);
        rv.push_back(yL);
        rv.push_back(leftXZ.y);
        rv.push_back(n.x); rv.push_back(n.y); rv.push_back(n.z);

        // right vertex
        rv.push_back(rightXZ.x);
        rv.push_back(yR);
        rv.push_back(rightXZ.y);
        rv.push_back(n.x); rv.push_back(n.y); rv.push_back(n.z);
    }

    mRiverVertexCount = S * 2;

    glGenVertexArrays(1, &mRiverVAO);
    glGenBuffers(1, &mRiverVBO);

    glBindVertexArray(mRiverVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mRiverVBO);
    glBufferData(GL_ARRAY_BUFFER, rv.size() * sizeof(float), rv.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

}

void Scene::destroy() {
    if (mCubeVBO) glDeleteBuffers(1, &mCubeVBO);
    if (mCubeVAO) glDeleteVertexArrays(1, &mCubeVAO);
    if (mPlaneVBO) glDeleteBuffers(1, &mPlaneVBO);
    if (mPlaneVAO) glDeleteVertexArrays(1, &mPlaneVAO);
    mCubeVBO = mCubeVAO = mPlaneVBO = mPlaneVAO = 0;
    if (mGroundEBO) glDeleteBuffers(1, &mGroundEBO);
    if (mGroundVBO) glDeleteBuffers(1, &mGroundVBO);
    if (mGroundVAO) glDeleteVertexArrays(1, &mGroundVAO);
    mGroundEBO = mGroundVBO = mGroundVAO = 0;
    mGroundIndexCount = 0;
    if (mRiverVBO) glDeleteBuffers(1, &mRiverVBO);
    if (mRiverVAO) glDeleteVertexArrays(1, &mRiverVAO);
    mRiverVBO = mRiverVAO = 0;
    mRiverVertexCount = 0;
    if (mTexGrass) glDeleteTextures(1, &mTexGrass);
    if (mTexWater) glDeleteTextures(1, &mTexWater);
    if (mTexRock)  glDeleteTextures(1, &mTexRock);
    if (mTexBark)  glDeleteTextures(1, &mTexBark);
    if (mTexLeaf)  glDeleteTextures(1, &mTexLeaf);

    mTexGrass = mTexWater = mTexRock = mTexBark = mTexLeaf = 0;


}

void Scene::drawCube(Shader& shader, const glm::mat4& model, const glm::vec3& color) {
    shader.setMat4("uModel", model);
    shader.setVec3("uObjectColor", color);
    glBindVertexArray(mCubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void Scene::drawPlane(Shader& shader, const glm::mat4& model, const glm::vec3& color) {
    shader.setMat4("uModel", model);
    shader.setVec3("uObjectColor", color);
    glBindVertexArray(mPlaneVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Scene::render(Shader& shader,
                   const glm::mat4& view,
                   const glm::mat4& proj,
                   const glm::vec3& lightPos)
{
    shader.use();
    shader.setInt("uTex", 0);
    shader.setMat4("uView", view);
    shader.setMat4("uProj", proj);
    shader.setVec3("uLightPos", lightPos);
    shader.setVec3("uLightColor", glm::vec3(1.0f));

    // -------------------------
    // Grass floor
    // -------------------------
    glm::mat4 ground = glm::mat4(1.0f);
    shader.setMat4("uModel", ground);
    shader.setVec3("uObjectColor", glm::vec3(0.15f, 0.55f, 0.18f));

    glBindVertexArray(mGroundVAO);
    glDrawElements(GL_TRIANGLES, mGroundIndexCount, GL_UNSIGNED_INT, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexGrass);
    shader.setVec2("uTexScale", glm::vec2(2.5f));
    shader.setInt("uUseTexture", 1);

    glBindVertexArray(mGroundVAO);
    glDrawElements(GL_TRIANGLES, mGroundIndexCount, GL_UNSIGNED_INT, 0);

    /// -------------------------
    // River (curved ribbon)
    // -------------------------
    shader.setMat4("uModel", glm::mat4(1.0f));
    shader.setVec3("uObjectColor", glm::vec3(0.08f, 0.35f, 0.65f));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexWater);
    shader.setInt("uUseTexture", 1);
    shader.setVec2("uTexScale", glm::vec2(2.5f));

    glBindVertexArray(mRiverVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, mRiverVertexCount);

    // -------------------------
    // A few trees (trunk + foliage)
    // -------------------------




    auto drawTree = [&](glm::vec3 pos, float trunkH, float crownSize) {
        // trunk
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexBark);
        shader.setVec2("uTexScale", glm::vec2(2.5f));
        shader.setInt("uUseTexture", 1);
        shader.setVec3("uObjectColor", glm::vec3(1.0f));
        glm::mat4 trunk = glm::mat4(1.0f);
        trunk = glm::translate(trunk, pos + glm::vec3(0, trunkH * 0.5f, 0));
        trunk = glm::scale(trunk, glm::vec3(0.4f, trunkH, 0.4f));
        drawCube(shader, trunk, glm::vec3(0.35f, 0.22f, 0.12f));

        // crown (3 stacked cubes looks surprisingly decent)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTexLeaf);
        shader.setVec2("uTexScale", glm::vec2(1.5f));
        shader.setInt("uUseTexture", 1);
        shader.setVec3("uObjectColor", glm::vec3(1.0f));
        for (int i = 0; i < 3; i++) {
            float y = trunkH + (float)i * (crownSize * 0.45f);
            glm::mat4 crown = glm::mat4(1.0f);
            crown = glm::translate(crown, pos + glm::vec3(0, y, 0));
            float s = crownSize * (1.0f - 0.18f * i);
            crown = glm::scale(crown, glm::vec3(s, s, s));
            drawCube(shader, crown, glm::vec3(0.10f, 0.45f, 0.12f));
        }
    };

    drawTree(glm::vec3(-3.0f, 0, -4.0f), 2.6f, 1.8f);
    drawTree(glm::vec3(-1.2f, 0, -5.5f), 3.2f, 2.2f);
    drawTree(glm::vec3( 1.0f, 0, -4.2f), 2.8f, 2.0f);
    drawTree(glm::vec3( 2.6f, 0, -5.0f), 2.4f, 1.7f);
    drawTree(glm::vec3( 0.5f, 0, -6.8f), 2.9f, 2.0f);
    drawTree(glm::vec3(-2.2f, 0, -6.3f), 2.5f, 1.8f);



    // -------------------------
    // Rocks
    // -------------------------
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexRock);
    shader.setVec2("uTexScale", glm::vec2(1.0f));
    shader.setInt("uUseTexture", 1);

    auto drawRock = [&](glm::vec3 pos, glm::vec3 scale) {
        glm::mat4 m = glm::mat4(1.0f);
        m = glm::translate(m, pos + glm::vec3(0, scale.y * 0.5f, 0));
        m = glm::scale(m, scale);
        drawCube(shader, m, glm::vec3(0.45f, 0.45f, 0.48f));
    };

    drawRock(glm::vec3(3,0,-4), glm::vec3(1.6f, 0.8f, 1.2f));
    drawRock(glm::vec3(5,0,-3), glm::vec3(0.9f, 0.6f, 0.7f));
    drawRock(glm::vec3(-8,0, 2), glm::vec3(1.2f, 0.7f, 1.1f));

    glBindVertexArray(0);
}




