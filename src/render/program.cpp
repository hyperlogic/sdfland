#include "program.h"
#include <stdio.h>
#include <string.h>

class ShaderVariable
{
public:
    enum Constants { StringSize = 64 };
    char name[StringSize];
    GLint size;
    GLenum type;
    GLint loc;
};

// helper used for shader loading
enum LoadFileToMemoryResult { CouldNotOpenFile = -1, CouldNotReadFile = -2 };
static int _LoadFileToMemory(const char *filename, char **result)
{
    int size = 0;
#ifdef _WIN32
    FILE *f = NULL;
    fopen_s(&f, filename, "rb");
#else
    FILE *f = fopen(filename, "rb");
#endif
    if (f == NULL)
    {
        *result = NULL;
        return CouldNotOpenFile;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *result = new char[size + 1];

    int newSize = (int)fread(*result, sizeof(char), size, f);
    if (size != newSize)
    {
        fprintf(stderr, "size mismatch, size = %d, newSize = %d\n", size, newSize);
        delete [] *result;
        return CouldNotReadFile;
    }
    fclose(f);
    (*result)[size] = 0;  // make sure it's null terminated.
    return size;
}

// helper used to debug shader problems
static void _DumpShaderInfoLog(GLint shader)
{
    GLint bufferLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &bufferLen);
    if (bufferLen > 1)
    {
        GLsizei len = 0;
        char* buffer = new char[bufferLen];
        glGetShaderInfoLog(shader, bufferLen, &len, buffer);
        printf("glGetShaderInfoLog()\n");
        printf("%s", buffer);
        delete [] buffer;
    }
}

static void _DumpProgramInfoLog(GLint prog)
{
    const GLint bufferLen = 4096;
    GLsizei len = 0;
    char* buffer = new char[bufferLen];
    glGetProgramInfoLog(prog, bufferLen, &len, buffer);
    if (len > 0)
    {
        printf("glGetProgramInfoLog()\n");
        printf("%s", buffer);
        delete [] buffer;
    }
}

std::string Program::m_searchPath;

// static method
Program* Program::FromFiles(const char* vertexShaderFilename,
                            const char* fragmentShaderFilename)
{
    Program* prog = new Program();

    bool success;
    success = prog->AddSourceFile(GL_VERTEX_SHADER, vertexShaderFilename);
    if (!success)
    {
        fprintf(stderr, "ERROR: AddSourceFile() failed, for vertex shader \"%s\"\n", vertexShaderFilename);
        return 0;
    }

    success = prog->AddSourceFile(GL_FRAGMENT_SHADER, fragmentShaderFilename);
    if (!success)
    {
        fprintf(stderr, "ERROR: AddSourceFile() failed, for fragment shader \"%s\"\n", fragmentShaderFilename);
        return 0;
    }

    success = prog->Link();
    if (!success)
    {
        fprintf(stderr, "ERROR: Link() failed, for vertex shader \"%s\" and \"%s\"\n", vertexShaderFilename, fragmentShaderFilename);
        return 0;
    }

    return prog;
}


Program::Program() : m_vertShader(0), m_fragShader(0), m_program(0), m_uniforms(0), m_attribs(0)
{

}

Program::~Program()
{
#ifdef DEBUG_DESTRUCTORS
    printf("~Program(\"%s\", \"%s\")\n", m_vertexShaderFilename.c_str(), m_fragmentShaderFilename.c_str());
#endif
    glDeleteShader(m_vertShader);
    glDeleteShader(m_fragShader);
    glDeleteProgram(m_program);
    delete [] m_attribs;
    delete [] m_uniforms;
}

bool Program::AddSourceFile(GLenum type, const char* filename)
{
    std::string fullPath;
    if (!FindFileInSearchPath(m_searchPath, std::string(filename), fullPath))
    {
        fprintf(stderr, "Could not find \"%s\"\n", filename);
        return false;
    }

    char* source = 0;
    int size = _LoadFileToMemory(fullPath.c_str(), &source);
    if (size < 0)
    {
        fprintf(stderr, "error reading shader %s\n", fullPath.c_str());
        return false;
    }
    else
    {
        bool retVal = AddSourceString(type, source);
        delete [] source;

        if (!retVal)
            fprintf(stderr, "error compiling shader %s\n", fullPath.c_str());
        else if (type == GL_VERTEX_SHADER)
            m_vertexShaderFilename = filename;
        else if (type == GL_FRAGMENT_SHADER)
            m_fragmentShaderFilename = filename;
            
        return retVal;
    }
}

bool Program::AddSourceString(GLenum type, const char* source)
{
    int size = strlen(source);

    GLint shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar**)&source, &size);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        fprintf(stderr, "shader compilation error\n");
        _DumpShaderInfoLog(shader);
        return false;
    }
    else
    {
        // success
        if (type == GL_VERTEX_SHADER)
            m_vertShader = shader;
        else
            m_fragShader = shader;
        return true;
    }
}

bool Program::Link()
{
    if (!m_vertShader || !m_fragShader)
    {
        fprintf(stderr, "error linking, invalid shaders!\n");
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, m_vertShader);
    glAttachShader(m_program, m_fragShader);
    glLinkProgram(m_program);

    GLint linked;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        _DumpProgramInfoLog(m_program);
        return false;
    }
    else
    {
        BuildVariableList();
        return true;
    }
}

int Program::GetUniformLocation(const char* uniformName) const
{
    return glGetUniformLocation(m_program, uniformName);
}

int Program::GetAttribLocation(const char* attribName) const
{
    return glGetAttribLocation(m_program, attribName);
}

std::string Program::GetUniformName(int loc) const
{
    int count;
    glGetProgramiv(m_program, GL_ACTIVE_UNIFORMS, &count);
    if (loc >= count)
        return "???";

    const size_t kNameSize = 64;
    char* name = new char[kNameSize];
    int uniformSize;
    GLenum uniformType;
    glGetActiveUniform(m_program, loc, kNameSize, NULL, &uniformSize, &uniformType, name);

    return std::string(name);
}

std::string Program::GetAttribName(int loc) const
{
    int count;
    glGetProgramiv(m_program, GL_ACTIVE_ATTRIBUTES, &count);
    if (loc >= count)
        return "???";

    const size_t kNameSize = 64;
    char* name = new char[kNameSize];
    int attribSize;
    GLenum attribType;
    glGetActiveAttrib(m_program, loc, kNameSize, NULL, &attribSize, &attribType, name);

    return std::string(name);
}

int Program::GetTextureUnit(int loc) const
{
    int count = 0;
    for (int i = 0; i < m_numUniforms; ++i)
    {
#ifdef GL_ES_VERSION_2_0
        if (m_uniforms[i].type == GL_SAMPLER_2D)
#else
        if (m_uniforms[i].type == GL_SAMPLER_2D || m_uniforms[i].type == GL_SAMPLER_2D_SHADOW)
#endif
        {
            if (m_uniforms[i].loc == loc)
                return count;
            count++;
        }
    }
    ASSERT(0); // bad texture loc
    return 0;
}

int Program::GetNumUniforms() const
{
    return m_numUniforms;
}

int Program::GetMaxUniformLoc() const
{
    return m_maxUniformLoc;
}

GLint Program::GetProgram() const
{
    return m_program;
}

void Program::Apply() const
{
#ifdef DEBUG
    glValidateProgram(m_program);
    GLint success;
    glGetProgramiv(m_program, GL_VALIDATE_STATUS, &success);
    if (!success)
        _DumpProgramInfoLog(m_program);
#endif
    glUseProgram(m_program);
}

static void _DumpVariable(const ShaderVariable* v)
{
    printf("    %s size = %d, ", v->name, v->size);
    switch (v->type)
    {
    case GL_FLOAT: printf("type = GL_FLOAT"); break;
    case GL_FLOAT_VEC2: printf("type = GL_FLOAT_VEC2"); break;
    case GL_FLOAT_VEC3: printf("type = GL_FLOAT_VEC3"); break;
    case GL_FLOAT_VEC4: printf("type = GL_FLOAT_VEC4"); break;
    case GL_FLOAT_MAT4: printf("type = GL_FLOAT_MAT4"); break;
    case GL_SAMPLER_2D: printf("type = GL_SAMPLER_2D"); break;
    default: printf("type = %d", v->type);
    }
    printf(", loc = %d\n", v->loc);
}

void Program::BuildVariableList()
{
    glGetProgramiv(m_program, GL_ACTIVE_ATTRIBUTES, &m_numAttribs);
    m_attribs = new ShaderVariable[m_numAttribs];
    for (int i = 0; i < m_numAttribs; ++i)
    {
        ShaderVariable* v = m_attribs + i;
        GLsizei strLen;
        glGetActiveAttrib(m_program, i, ShaderVariable::StringSize, &strLen, &v->size, &v->type, v->name);
        v->loc = glGetAttribLocation(m_program, v->name);
    }

    glGetProgramiv(m_program, GL_ACTIVE_UNIFORMS, &m_numUniforms);
    m_uniforms = new ShaderVariable[m_numUniforms];
    m_maxUniformLoc = -1;
    for (int i = 0; i < m_numUniforms; ++i)
    {
        ShaderVariable* v = m_uniforms + i;
        GLsizei strLen;
        glGetActiveUniform(m_program, i, ShaderVariable::StringSize, &strLen, &v->size, &v->type, v->name);
        int loc = glGetUniformLocation(m_program, v->name);
        v->loc = loc;
        if (loc > m_maxUniformLoc)
            m_maxUniformLoc = loc;
    }

#ifdef DUMP_SHADER_VARIABLES
    DumpShaderVariables();
#endif
}

void Program::DumpShaderVariables() const
{
    printf("shader GLuint = %d\n", m_program);
    printf("  vertexShader = \"%s\"\n", m_vertexShaderFilename.c_str());
    printf("  fragmentShader = \"%s\"\n", m_fragmentShaderFilename.c_str());
    printf("  attribs = \n");
    for (int i = 0; i < m_numAttribs; ++i)
    {
        ShaderVariable* v = m_attribs + i;
        _DumpVariable(v);
    }

    printf("  uniforms = \n");
    for (int i = 0; i < m_numUniforms; ++i)
    {
        ShaderVariable* v = m_uniforms + i;
        _DumpVariable(v);
    }
    printf("\n");
}
