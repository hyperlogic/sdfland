#ifndef __PROGRAM_H__
#define __PROGRAM_H__

#include "render.h"
#include <vector>
#include <string>

class ShaderVariable;

// Create a Program then add the shader source code through the use
// of AddSourceFile or AddSourceString.  You need to specify both a vertex
// and a fragment shader.  Once that has completed with no errors, call
// the Link method.  This performs the shader compile & link.
class Program
{
    friend class DrawContext;
public:
    static void SetSearchPath(const std::string& searchPath) { m_searchPath = searchPath; }

    static Program* FromFiles(const char* vertexShaderFilename,
                              const char* fragmentShaderFilename);

    Program();
    ~Program();
    bool AddSourceFile(GLenum type, const char* filename);
    bool AddSourceString(GLenum type, const char* source);
    bool Link();

    int GetUniformLocation(const char* uniformName) const;
    int GetAttribLocation(const char* attribName) const;

    std::string GetUniformName(int loc) const;
    std::string GetAttribName(int loc) const;

    int GetTextureUnit(int loc) const;
    int GetNumUniforms() const;
    int GetMaxUniformLoc() const;

    const std::string& GetVertexShaderFilename() const { return m_vertexShaderFilename; }
    const std::string& GetFragmentShaderFilename() const { return m_fragmentShaderFilename; }

    GLint GetProgram() const;
    void Apply() const;

private:

    void DumpShaderVariables() const;
    void BuildVariableList();

    GLint m_vertShader;
    GLint m_fragShader;
    GLint m_program;

    ShaderVariable* m_uniforms;
    int m_numUniforms;
    int m_maxUniformLoc;
    ShaderVariable* m_attribs;
    int m_numAttribs;
    std::string m_vertexShaderFilename;
    std::string m_fragmentShaderFilename;

    static std::string m_searchPath;
};

#endif
