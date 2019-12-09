#ifndef __TEXTURE_H__
#define __TEXTURE_H__

#include "render.h"
#include <string>

class Image;

class Texture
{
    friend class DrawContext;
public:
    static void SetSearchPath(const std::string& searchPath) { m_searchPath = searchPath; }

    Texture();
    ~Texture();

    void SetMinFilter(GLenum minFilter);
    void SetMagFilter(GLenum magFilter);

    void SetSWrap(GLenum sWrap);
    void SetTWrap(GLenum tWrap);

    enum Flags {
        PremultiplyAlpha = 0x01,
        FlipVertical = 0x02,
        SRGB = 0x04
    };

    // There are many static arrays that use PixelFormat as an index.
    // So beware of changing the order.
    enum PixelFormat {
        Luminance = 0,
        LuminanceAlpha,
        RGB,
        RGBA,
        BGR,
        BGRA,
        Depth,
        NumPixelFormats
    };

    bool LoadFromImage(Image& image, bool srgb = false);
    bool LoadFromFile(const char* filename, unsigned int flags);
    GLuint GetTexture() const;

    bool Save(const char* filename) const;
    bool Save(const char* filename, PixelFormat pixelFormatOverride) const;

    const std::string& GetFilename() const { return m_filename; }

protected:

    // gen, bind & tex param, but no glTexImage2D()
    void Create(int width, int height);
    void Apply(int unit);

    GLuint m_texture;
    GLint m_minFilter;
    GLint m_magFilter;
    GLint m_sWrap;
    GLint m_tWrap;

    int m_width;
    int m_height;
    PixelFormat m_pixelFormat;

    std::string m_filename;
    static std::string m_searchPath;
};

#endif
