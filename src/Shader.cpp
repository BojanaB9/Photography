#include "Shader.h"

#include <glad/glad.h>
#include <iostream>
#include <utility>
#include <fstream>
#include <sstream>
#include <stdexcept>

static std::string ReadTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

Shader::Shader(const char* vertexSrc, const char* fragmentSrc) {
    unsigned int vs = compile(GL_VERTEX_SHADER, vertexSrc);
    unsigned int fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);

    m_id = glCreateProgram();
    glAttachShader(m_id, vs);
    glAttachShader(m_id, fs);
    glLinkProgram(m_id);

    int success = 0;
    glGetProgramiv(m_id, GL_LINK_STATUS, &success);
    if (!success) {
        char info[1024];
        glGetProgramInfoLog(m_id, sizeof(info), nullptr, info);
        std::cerr << "Shader link error:\n" << info << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept
    : m_id(other.m_id), m_uniformCache(std::move(other.m_uniformCache)) {
    other.m_id = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        destroy();
        m_id = other.m_id;
        m_uniformCache = std::move(other.m_uniformCache);
        other.m_id = 0;
    }
    return *this;
}

void Shader::destroy() {
    if (m_id != 0) {
        glDeleteProgram(m_id);
        m_id = 0;
    }
    m_uniformCache.clear();
}

unsigned int Shader::compile(unsigned int type, const char* src) {
    unsigned int sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);

    int success = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[1024];
        glGetShaderInfoLog(sh, sizeof(info), nullptr, info);
        std::cerr << (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment")
                  << " shader compile error:\n" << info << "\n";
    }
    return sh;
}

void Shader::use() const {
    glUseProgram(m_id);
}

int Shader::uniformLocation(const std::string& name) {
    std::unordered_map<std::string, int>::iterator it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) {
        return it->second;
    }

    int loc = glGetUniformLocation(m_id, name.c_str());
    m_uniformCache[name] = loc;
    return loc;
}

void Shader::setInt(const std::string& name, int v) {
    glUniform1i(uniformLocation(name), v);
}

void Shader::setFloat(const std::string& name, float v) {
    glUniform1f(uniformLocation(name), v);
}

void Shader::setVec3(const std::string& name, const glm::vec3& v) {
    glUniform3f(uniformLocation(name), v.x, v.y, v.z);
}

void Shader::setMat4(const std::string& name, const glm::mat4& m) {
    glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, &m[0][0]);
}
void Shader::setVec2(const std::string& name, const glm::vec2& v) {
    glUniform2f(uniformLocation(name), v.x, v.y);
}

