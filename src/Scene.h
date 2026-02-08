#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Shader.h"

class Scene {
public:
    void init();
    void destroy();

    // Draw the whole scene (floor, trees, rocks)
    void render(Shader& shader,
                const glm::mat4& view,
                const glm::mat4& proj,
                const glm::vec3& lightPos);

private:
    unsigned int mCubeVAO = 0, mCubeVBO = 0;
    unsigned int mPlaneVAO = 0, mPlaneVBO = 0;
    unsigned int mGroundVAO = 0;
    unsigned int mGroundVBO = 0;
    unsigned int mGroundEBO = 0;
    int mGroundIndexCount = 0;
    unsigned int mRiverVAO = 0;
    unsigned int mRiverVBO = 0;
    int mRiverVertexCount = 0; // number of vertices in the strip
    // Scene.h (private:)
    unsigned int mTexGrass = 0;
    unsigned int mTexWater = 0;
    unsigned int mTexRock  = 0;
    unsigned int mTexBark  = 0;
    unsigned int mTexLeaf  = 0;


private:
    void drawCube(Shader& shader, const glm::mat4& model, const glm::vec3& color);
    void drawPlane(Shader& shader, const glm::mat4& model, const glm::vec3& color);
};
