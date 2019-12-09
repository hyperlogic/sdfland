#include "texture.h"
#include <string.h>
#include "image.h"

// TODO: how to create cube maps?

std::string Texture::m_searchPath;

Texture::Texture() : m_texture(0), m_minFilter(GL_LINEAR_MIPMAP_LINEAR),
                     m_magFilter(GL_LINEAR), m_sWrap(GL_REPEAT), m_tWrap(GL_REPEAT)
{

}

Texture::~Texture()
{
#ifdef DEBUG_DESTRUCTORS
    printf("~Texture(\"%s\")\n", m_filename.c_str());
#endif
    glDeleteTextures(1, &m_texture);
}

void Texture::SetMinFilter(GLenum minFilter)
{
    m_minFilter = minFilter;
}

void Texture::SetMagFilter(GLenum magFilter)
{
    m_magFilter = magFilter;
}

void Texture::SetSWrap(GLenum sWrap)
{
    m_sWrap = sWrap;
}

void Texture::SetTWrap(GLenum tWrap)
{
    m_tWrap = tWrap;
}

// gen, bind & tex param, but no glTexImage2D()
void Texture::Create(int width, int height)
{
    ASSERT(IsPowerOfTwo(width) && IsPowerOfTwo(height));

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_sWrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_tWrap);
}

bool Texture::LoadFromImage(Image& image, bool srgb)
{
    m_width = image.GetMipMap(0)->width;
    m_height = image.GetMipMap(0)->height;
    m_pixelFormat = image.GetPixelFormat();

    Create(m_width, m_height);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int internalFormat = 0;
    GLenum pf = 0;
    switch (m_pixelFormat)
    {
    case Luminance:
        internalFormat = pf = GL_LUMINANCE;
#ifndef GL_ES_VERSION_2_0
        if (srgb)
            internalFormat = GL_SLUMINANCE;
#endif
        break;
    case LuminanceAlpha:
        internalFormat = pf = GL_LUMINANCE_ALPHA;
#ifndef GL_ES_VERSION_2_0
        if (srgb)
            internalFormat = GL_SLUMINANCE_ALPHA;
#endif
        break;
    case RGB:
        internalFormat = pf = GL_RGB;
#ifndef GL_ES_VERSION_2_0
        if (srgb)
            internalFormat = GL_SRGB;
#endif
        break;
    case RGBA:
        internalFormat = pf = GL_RGBA;
#ifndef GL_ES_VERSION_2_0
        if (srgb)
            internalFormat = GL_SRGB_ALPHA;
#endif
        break;
#ifndef GL_ES_VERSION_2_0
    case BGR:
        internalFormat = pf = GL_BGR;
        break;
#endif
    case BGRA:
        internalFormat = pf = GL_BGRA;
        break;
    case Depth:
        internalFormat = GL_DEPTH_COMPONENT16; pf = GL_DEPTH_COMPONENT;
        break;
    default: ASSERT(0); // illegal pixel format.
    }

    if (image.GetPixelFormat() == Depth)
    {
#ifndef GL_ES_VERSION_2_0
        // Shadow map setup.
        glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        const Image::Buffer* buffer = image.GetMipMap(0);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, buffer->width, buffer->height,
                     0, pf, GL_UNSIGNED_INT, buffer->data);

        GL_ERROR_CHECK("Texture::LoadFromImage(), pixelFormat == Depth");
        return true;
#else
        return false;
#endif
    }

    // generate mipmaps if needed
    unsigned int flags = 0;
    switch (m_minFilter)
    {
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_LINEAR:
        if (m_sWrap == GL_REPEAT)
            flags |= Image::SRepeatFlag;
        if (m_tWrap == GL_REPEAT)
            flags |= Image::TRepeatFlag;
        image.GenerateMipMaps(Image::Box, flags | Image::SRGBFlag);
        break;
    }

    for (int m = 0; m < image.GetNumMipMaps(); ++m)
    {
        const Image::Buffer* buffer = image.GetMipMap(m);
        glTexImage2D(GL_TEXTURE_2D, m, internalFormat, buffer->width, buffer->height,
                     0, pf, GL_UNSIGNED_BYTE, buffer->data);
    }

    GL_ERROR_CHECK("Texture::LoadFromImage()");

    return true;
}

bool Texture::LoadFromFile(const char* filename_cstr, unsigned int flags)
{
    std::string filename = filename_cstr;
    std::string fullPath = "";
    if (!FindFileInSearchPath(m_searchPath, filename, fullPath))
    {
        if (FileExists(filename))
        {
            fullPath = filename;
        }
    }

    if (fullPath == "")
    {
        fprintf(stderr, "Error: Could not find \"%s\"\n", filename_cstr);
        return false;
    }

    Image image;
    if (!image.Load(fullPath.c_str()))
    {
        fprintf(stderr, "Error: Could not load \"%s\"\n", fullPath.c_str());
        return false;
    }

    if (!IsPowerOfTwo(image.GetMipMap(0)->width) || !IsPowerOfTwo(image.GetMipMap(0)->height))
    {
        fprintf(stderr, "Error: image \"%s\" is not a power of two.\n", filename_cstr);
        return false;
    }

    if (flags & PremultiplyAlpha)
        image.PremultiplyAlpha();

    if (flags & FlipVertical)
        image.FlipVertical();

    bool srgb = flags & SRGB;
    LoadFromImage(image, srgb);

    // m_filename is the short filename
    m_filename = filename;

    return true;
}

GLuint Texture::GetTexture() const
{
    return m_texture;
}

void Texture::Apply(int unit)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_texture);
}

#ifndef GL_ES_VERSION_2_0
static GLenum s_pixelFormatToOpenGLFormat [] = {
    GL_LUMINANCE,
    GL_LUMINANCE_ALPHA,
    GL_RGB,
    GL_RGBA,
    GL_BGR,
    GL_BGRA,
    GL_RGB,
    GL_RGBA,
    GL_LUMINANCE };
#endif

bool Texture::Save(const char* filename) const
{
    return Save(filename, m_pixelFormat);
}

bool Texture::Save(const char* filename, PixelFormat pixelFormatOverride) const
{
#ifdef GL_ES_VERSION_2_0
    // No glGetTexImage in OpenGLES
    return false;
#else
    if (pixelFormatOverride < 0 && pixelFormatOverride >= NumPixelFormats)
        return false;
    if (m_pixelFormat < 0 && m_pixelFormat >= NumPixelFormats)
        return false;

    int pixelFormat = s_pixelFormatToOpenGLFormat[m_pixelFormat];

    // allocate temp image
    Image image(m_width, m_height, m_pixelFormat, true);

    // Read back texture into image using the native pixel format
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glGetTexImage(GL_TEXTURE_2D, 0, pixelFormat, GL_UNSIGNED_BYTE, image.GetMipMap(0)->data);
    GL_ERROR_CHECK("Texture::Save() glGetTexImage() failed!\n");

    bool success = image.ConvertPixelFormat(pixelFormatOverride);
    if (success)
        return image.Save(filename);
    else
        return false;
#endif
}
