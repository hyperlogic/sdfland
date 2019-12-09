#ifndef __IMAGE_H__
#define __IMAGE_H__

#include "texture.h"

class Image
{
public:
    Image();

    // creates a null image.
    Image(int width, int height, Texture::PixelFormat pixelFormat, bool allocBufferData = false);

    ~Image();

    bool Load(const char* filename);
    bool Save(const char* filename) const;

    bool ConvertPixelFormat(Texture::PixelFormat newPixelFormat);
    void FlipVertical();
    void PremultiplyAlpha();

    enum FilterType { Box = 0, Gaussian, WideGaussian, NumFilterTypes };
    enum Flags { SRepeatFlag = 1, TRepeatFlag = 2, SRGBFlag = 4 };
    void GenerateMipMaps(FilterType filterType, unsigned int flags);

    int GetPixelSize() const;
    Texture::PixelFormat GetPixelFormat() const { return m_pixelFormat; }

    // add a one pixel smudge border to help disguise artifacts at chart borders.
    void SmoothPixelBorder();

    struct Buffer
    {
        int width, height;
        unsigned char* data;
    };

    int GetNumMipMaps() const;
    const Buffer* GetMipMap(int i) const;

protected:
    void FreeImageData();

    Texture::PixelFormat m_pixelFormat;
    Buffer* m_mips;
    int m_numMips;
};

#endif

