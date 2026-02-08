#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

class Shader {
public:
    Shader() = default;
    Shader(const char* vertexSrc, const char* fragmentSrc);

    ~Shader();

    // Non-copyable (OpenGL resource owner)
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Movable
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void use() const;

    unsigned int id() const { return m_id; }

    // Uniform helpers (cached)
    void setInt(const std::string& name, int v);
    void setFloat(const std::string& name, float v);
    void setVec3(const std::string& name, const glm::vec3& v);
    void setMat4(const std::string& name, const glm::mat4& m);
    void setVec2(const std::string& name, const glm::vec2& v);


private:
    unsigned int m_id = 0;
    std::unordered_map<std::string, int> m_uniformCache;

    unsigned int compile(unsigned int type, const char* src);
    int uniformLocation(const std::string& name);
    void destroy();
};
