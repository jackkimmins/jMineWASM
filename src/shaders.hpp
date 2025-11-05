// shaders.hpp

#ifndef SHADERS_HPP
#define SHADERS_HPP

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif

#include <iostream>

class Shader {
public:
    GLuint program;

    // Compile and link the shader
    Shader(const char* vertexSrc, const char* fragmentSrc) {
        GLuint vertex = compile(GL_VERTEX_SHADER, vertexSrc);
        GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentSrc);
        program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        checkLink();
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    void use() const { glUseProgram(program); }
    GLint getUniform(const char* name) const { return glGetUniformLocation(program, name); }
    ~Shader() { glDeleteProgram(program); }

private:
    // Compile GLSL source code
    GLuint compile(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        checkCompile(shader, type);
        return shader;
    }

    // Verify compilation status
    void checkCompile(GLuint shader, GLenum type) {
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char info[512];
            glGetShaderInfoLog(shader, 512, nullptr, info);
            std::cerr << "ERROR::SHADER::" 
                      << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") 
                      << "::COMPILATION_FAILED\n" << info << std::endl;
        }
    }

    // Verify linking status
    void checkLink() {
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char info[512];
            glGetProgramInfoLog(program, 512, nullptr, info);
            std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << info << std::endl;
        }
    }
};

#endif