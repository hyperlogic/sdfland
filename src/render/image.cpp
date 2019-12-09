#include "image.h"
#include <algorithm>

extern "C" {
#include "png.h"
}

static int s_pixelFormatToPixelSize[Texture::NumPixelFormats] = {
    1, 2, 3, 4, 3, 4, 2
};
static unsigned int s_pixelFormatToAlphaMask[Texture::NumPixelFormats] = {
    0x0, 0x2, 0x0, 0x8, 0x0, 0x8, 0x0
};

static int _MipCount(int width, int height)
{
    int d = std::max(width, height);
    int i = 0;
    for (i = 0; d >= 1; ++i)
        d /= 2;
    return i;
}

Image::Image() : m_pixelFormat(Texture::Luminance), m_mips(0), m_numMips(0)
{

}

Image::Image(int width, int height, Texture::PixelFormat pixelFormat, bool allocBufferData) : m_pixelFormat(pixelFormat)
{
    // allocate pointer to the first mip level Buffer
    m_mips = new Buffer[1];
    m_mips[0].width = width;
    m_mips[0].height = height;

    // optionally allocate pixel data.
    if (allocBufferData)
        m_mips[0].data = new unsigned char[width * height * GetPixelSize()];
    else
        m_mips[0].data = 0;

    m_numMips = 1;
}

Image::~Image()
{
    FreeImageData();
}

bool Image::Load(const char* filename)
{
    FILE *fp = NULL;
    fopen_s(&fp, filename, "rb");
    //FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        fprintf(stderr, "Error: Failed to load texture \"%s\"\n", filename);
        return false;
    }

    unsigned char header[8];
    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8))
    {
        fprintf(stderr, "Error: Texture \"%s\" is not a valid PNG file\n", filename);
        return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fprintf(stderr, "Error: png_create_read_struct() failed\n");
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        fprintf(stderr, "Error: png_create_info_struct() failed\n");
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    png_byte** row_pointers = png_get_rows(png_ptr, info_ptr);

    png_uint_32 width, height;
    int bit_depth, color_type;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    /*
    printf("Texture \"%s\"\n", fullPath.c_str());
    printf("    windth = %d\n", (int)width);
    printf("    height = %d\n", (int)height);
    printf("    bit_depth = %d\n", bit_depth);
    const char* str = "unknown";
    switch (color_type)
    {
    case PNG_COLOR_TYPE_GRAY: str = "gray"; break;
    case PNG_COLOR_TYPE_PALETTE: str = "palette"; break;
    case PNG_COLOR_TYPE_RGB: str = "rbg"; break;
    case PNG_COLOR_TYPE_RGBA: str = "rbga"; break;
    case PNG_COLOR_TYPE_GA: str = "ga"; break;
    }
    printf("    color_type = %s\n", str);
    */

    bool loaded = false;
    if (bit_depth != 8)
    {
        fprintf(stderr, "Error: bad bit depth for texture \"%s\n", filename);
    }
    else
    {
        switch (color_type)
        {
        case PNG_COLOR_TYPE_GRAY:
            m_pixelFormat = Texture::Luminance;
            break;
        case PNG_COLOR_TYPE_GA:
            m_pixelFormat = Texture::LuminanceAlpha;
            break;
        case PNG_COLOR_TYPE_RGB:
            m_pixelFormat = Texture::RGB;
            break;
        case PNG_COLOR_TYPE_RGBA:
            m_pixelFormat = Texture::RGBA;
            break;
        default:
            fprintf(stderr, "bad pixel format for texture \"%s\n", filename);
            break;
        }

        int pixelSize = GetPixelSize();

        // free existing image data (if any)
        FreeImageData();

        // allocate pointer to the first mip level.
        m_mips = new Buffer[1];
        m_mips[0].width = width;
        m_mips[0].height = height;
        m_mips[0].data = new unsigned char[width * height * pixelSize];

        m_numMips = 1;

        // copy row pointers into top mip map level
        for (int i = 0; i < (int)height; ++i)
            memcpy(m_mips[0].data + i * width * pixelSize, row_pointers[height - 1 - i], width * pixelSize);

        loaded = true;
    }

    png_destroy_info_struct(png_ptr, &info_ptr);
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);

    return loaded;
}

bool Image::Save(const char* filename) const
{
    FILE *fp = NULL;
    fopen_s(&fp, filename, "wb");
    //FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        fprintf(stderr, "Error: Failed to save texture \"%s\"\n", filename);
        return false;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fprintf(stderr, "Error: png_create_write_struct() failed\n");
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        fprintf(stderr, "Error: png_create_info_struct() failed\n");
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
        return false;
    }

    png_init_io(png_ptr, fp);

    // TODO: uhh, error handling with setjump...
    // There's a bit of a conflict with Lua, which also uses setjmp for errors.

    // convert from pixelFormat to png color type
    static int s_pixelFormatToPNGColorType[Texture::NumPixelFormats] = {
        PNG_COLOR_TYPE_GRAY, PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_RGB,
        PNG_COLOR_TYPE_RGB_ALPHA, PNG_COLOR_TYPE_RGB, PNG_COLOR_TYPE_RGB_ALPHA,
        PNG_COLOR_TYPE_GRAY };

    const Buffer* buffer = GetMipMap(0);
    png_set_IHDR(png_ptr, info_ptr, buffer->width, buffer->height, 8,
                 s_pixelFormatToPNGColorType[m_pixelFormat], PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    // initialize row ptrs
    int pixelSize = GetPixelSize();
    //unsigned char* row_ptrs[buffer->height];
    unsigned char** row_ptrs = new unsigned char*[buffer->height];
    for (int i = 0; i < buffer->height; i++)
    {
        // png expects rows from top to bottom.
        row_ptrs[buffer->height - i - 1] = buffer->data + i * buffer->width * pixelSize;
    }

    png_set_rows(png_ptr, info_ptr, row_ptrs);

    unsigned int transform_flags = PNG_TRANSFORM_IDENTITY;
    if (m_pixelFormat == Texture::BGR || m_pixelFormat == Texture::BGRA)
        transform_flags |= PNG_TRANSFORM_BGR;
    png_write_png(png_ptr, info_ptr, transform_flags, NULL);

    delete [] row_ptrs;

    fclose(fp);

    return true;
}

typedef void (*ConvertFunc)(const unsigned char* src, unsigned char* dst, int width, int height);

static void rgb_lum(const unsigned char* src, unsigned char* dst, int width, int height)
{
    const int kSrcPixelSize = 3;
    const int kDstPixelSize = 1;
    const unsigned char* s = src;
    unsigned char* d = dst;
    for (int i = 0; i < height * width; i++, s += kSrcPixelSize, d += kDstPixelSize)
    {
        unsigned char r = *s;
        unsigned char g = *(s + 1);
        unsigned char b = *(s + 2);
        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        *d = (unsigned char)lum;
    }
    ASSERT(d == dst + (kDstPixelSize * (width * height)));
    ASSERT(s == src + (kSrcPixelSize * (width * height)));
}

static void rgb_luma(const unsigned char* src, unsigned char* dst, int width, int height)
{
    const int kSrcPixelSize = 3;
    const int kDstPixelSize = 2;
    const unsigned char* s = src;
    unsigned char* d = dst;
    for (int i = 0; i < height * width; i++, s += kSrcPixelSize, d += kDstPixelSize)
    {
        unsigned char r = *s;
        unsigned char g = *(s + 1);
        unsigned char b = *(s + 2);
        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        *d = (unsigned char)lum;
        *(d + 1) = 255;
    }
    ASSERT(d == dst + (kDstPixelSize * (width * height)));
    ASSERT(s == src + (kSrcPixelSize * (width * height)));
}

static void rgba_lum(const unsigned char* src, unsigned char* dst, int width, int height)
{
    const int kSrcPixelSize = 4;
    const int kDstPixelSize = 1;
    const unsigned char* s = src;
    unsigned char* d = dst;
    for (int i = 0; i < height * width; i++, s += kSrcPixelSize, d += kDstPixelSize)
    {
        unsigned char r = *s;
        unsigned char g = *(s + 1);
        unsigned char b = *(s + 2);
        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        *d = (unsigned char)lum;
    }
    ASSERT(d == dst + (kDstPixelSize * (width * height)));
    ASSERT(s == src + (kSrcPixelSize * (width * height)));
}

static void rgba_luma(const unsigned char* src, unsigned char* dst, int width, int height)
{
    const int kSrcPixelSize = 4;
    const int kDstPixelSize = 2;
    const unsigned char* s = src;
    unsigned char* d = dst;
    for (int i = 0; i < height * width; i++, s += kSrcPixelSize, d += kDstPixelSize)
    {
        unsigned char r = *s;
        unsigned char g = *(s + 1);
        unsigned char b = *(s + 2);
        unsigned char a = *(s + 3);
        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        *d = (unsigned char)lum;
        *(d + 1) = a;
    }
    ASSERT(d == dst + (kDstPixelSize * (width * height)));
    ASSERT(s == src + (kSrcPixelSize * (width * height)));
}

static void rgb_rgba(const unsigned char* src, unsigned char* dst, int width, int height)
{
    const int kSrcPixelSize = 3;
    const int kDstPixelSize = 4;
    const unsigned char* s = src;
    unsigned char* d = dst;
    for (int i = 0; i < height * width; i++, s += kSrcPixelSize, d += kDstPixelSize)
    {
        *d = *s;
        *(d + 1) = *(s + 1);
        *(d + 2) = *(s + 2);
        *(d + 3) = 255;
    }
    ASSERT(d == dst + (kDstPixelSize * (width * height)));
    ASSERT(s == src + (kSrcPixelSize * (width * height)));
}

static void rgba_rgb(const unsigned char* src, unsigned char* dst, int width, int height)
{
    const int kSrcPixelSize = 4;
    const int kDstPixelSize = 3;
    const unsigned char* s = src;
    unsigned char* d = dst;
    for (int i = 0; i < height * width; i++, s += kSrcPixelSize, d += kDstPixelSize)
    {
        *d = *s;
        *(d + 1) = *(s + 1);
        *(d + 2) = *(s + 2);
    }
    ASSERT(d == dst + (kDstPixelSize * (width * height)));
    ASSERT(s == src + (kSrcPixelSize * (width * height)));
}

// Row is the format to convert from.
// Column is the format to convert to.
// To convert from rgb to luminance use the function s_convertFuncMap[RGB][Luminance]
ConvertFunc s_convertFuncMap[Texture::NumPixelFormats][Texture::NumPixelFormats] = {
    /*                 lum       luma       rgb      rgba       bgr      bgra     depth */
    /* lum   */ {        0,         0,        0,        0,        0,        0,        0 },
    /* luma  */ {        0,         0,        0,        0,        0,        0,        0 },
    /* rgb   */ {  rgb_lum,  rgb_luma,        0, rgb_rgba,        0,        0,        0 },
    /* rgba  */ { rgba_lum, rgba_luma, rgba_rgb,        0,        0,        0,        0 },
    /* bgr   */ {        0,         0,        0,        0,        0,        0,        0 },
    /* bgra  */ {        0,         0,        0,        0,        0,        0,        0 },
    /* depth */ {        0,         0,        0,        0,        0,        0,        0 }
};

bool Image::ConvertPixelFormat(Texture::PixelFormat newPixelFormat)
{
    if (m_pixelFormat == newPixelFormat)
        return true;

    ConvertFunc convertFunc = s_convertFuncMap[m_pixelFormat][newPixelFormat];
    if (!convertFunc)
        return false;

    const int kNewBytesPerPixel = s_pixelFormatToPixelSize[newPixelFormat];
    for (int m = 0; m < m_numMips; ++m)
    {
        const unsigned char* data = m_mips[m].data;
        int width = m_mips[m].width;
        int height = m_mips[m].height;

        // allocate a new buffer
        unsigned char* newBuffer = new unsigned char[width * height * kNewBytesPerPixel];

        convertFunc(data, newBuffer, width, height);

        m_mips[m].data = newBuffer;
        delete [] data;
    }
    m_pixelFormat = newPixelFormat;
    return true;
}

void Image::FlipVertical()
{
    for (int m = 0; m < m_numMips; ++m)
    {
        const unsigned char* data = m_mips[m].data;
        int width = m_mips[m].width;
        int height = m_mips[m].height;
        int bytesPerPixel = GetPixelSize();

        unsigned char* flippedBuffer = new unsigned char[width * height * bytesPerPixel];

        for (int r = 0; r < height; r++)
        {
            memcpy(flippedBuffer + (r * width * bytesPerPixel),
                   data + ((height - 1 - r) * width * bytesPerPixel),
                   bytesPerPixel * width);
        }

        m_mips[m].data = flippedBuffer;
        delete [] data;
    }
}

void Image::PremultiplyAlpha()
{
    for (int m = 0; m < m_numMips; ++m)
    {
        unsigned char* bytes = m_mips[m].data;
        int width = m_mips[m].width;
        int height = m_mips[m].height;

        if (m_pixelFormat == Texture::LuminanceAlpha)
        {
            for (unsigned int i = 0; i < width * height * 2U; i += 2)
                bytes[i] = (unsigned char)((unsigned int)bytes[i] * (unsigned int)bytes[i+1] / 255);
        }
        else if (m_pixelFormat == Texture::RGBA || m_pixelFormat == Texture::BGRA)
        {
            for (unsigned int i = 0; i < width * height * 4U; i += 4)
            {
                bytes[i] = (unsigned char)((unsigned int)bytes[i] * (unsigned int)bytes[i+3] / 255);
                bytes[i+1] = (unsigned char)((unsigned int)bytes[i+1] * (unsigned int)bytes[i+3] / 255);
                bytes[i+2] = (unsigned char)((unsigned int)bytes[i+2] * (unsigned int)bytes[i+3] / 255);
            }
        }
    }
}

static float _SRGBDecode(float srgb)
{
    const float a = 0.055f;
    if (srgb <= 0.04045f)
        return srgb / 12.92f;
    else
        return pow((srgb + a) / (1 + a), 2.4f);
}

static float _SRGBEncode(float linear)
{
    const float a = 0.055f;
    if (linear <= 0.0031308f)
        return 12.92f * linear;
    else
        return pow((1 + a) * linear, 1 / 2.4f) - a;
}

const float kBoxCoeff = 1.0f / 9.0f;
static float s_boxKernel[] = {kBoxCoeff, kBoxCoeff, kBoxCoeff,
                              kBoxCoeff, kBoxCoeff, kBoxCoeff,
                              kBoxCoeff, kBoxCoeff, kBoxCoeff};

static float s_gaussianKernel[] = {0.044919223845156f, 0.12210310992677f, 0.044919223845156f,
                                   0.12210310992677f,  0.33191066491228f, 0.12210310992677f,
                                   0.044919223845156f, 0.12210310992677f, 0.044919223845156f};

static float s_wideGaussianKernel[] = {0.00010678874539336f, 0.0021449092885793f, 0.005830467942838f, 0.0021449092885793f, 0.00010678874539336f,
                                       0.0021449092885793f, 0.043081654712647f, 0.11710807914534f, 0.043081654712647f, 0.0021449092885793f,
                                       0.005830467942838f, 0.11710807914534f, 0.31833276350651f, 0.11710807914534f, 0.005830467942838f,
                                       0.0021449092885793f, 0.043081654712647f, 0.11710807914534f, 0.043081654712647f, 0.0021449092885793f,
                                       0.00010678874539336f, 0.0021449092885793f, 0.005830467942838f, 0.0021449092885793f, 0.00010678874539336f};

static float* s_kernels[Image::NumFilterTypes] = {s_boxKernel, s_gaussianKernel, s_wideGaussianKernel};

static int Index(int x, int y, int pixelSize, int width, int height, unsigned int flags)
{
    // wrap or clamp x
    if (x < 0)
        x = (flags & Image::SRepeatFlag) ? ((x + width) % width) : 0;
    else if (x >= width)
        x = (flags & Image::SRepeatFlag) ? (x % width) : (width - 1);

    // wrap or clamp y
    if (y < 0)
        y = (flags & Image::TRepeatFlag) ? ((y + height) % height) : 0;
    else if (y >= height)
        y = (flags & Image::TRepeatFlag) ? (y % height) : height - 1;

    return y * width * pixelSize + x * pixelSize;
}

void Image::GenerateMipMaps(FilterType type, unsigned int flags)
{
    if (m_numMips > 1 or m_numMips == 0)
        return;

    // allocate an array of Buffers that are large enough to hold all the mip levels.
    Buffer* oldMips = m_mips;
    m_numMips = _MipCount(oldMips[0].width, oldMips[0].height);
    m_mips = new Buffer[m_numMips];
    m_mips[0] = oldMips[0];
    delete [] oldMips;

    int pixelSize = GetPixelSize();
    float* kernel = s_kernels[type];

    for (int m = 1; m < m_numMips; ++m)
    {
        int prevWidth = m_mips[m-1].width;
        int prevHeight = m_mips[m-1].height;
        int width = std::max(prevWidth / 2, 1);
        int height = std::max(prevHeight / 2, 1);
        m_mips[m].width = width;
        m_mips[m].height = height;
        m_mips[m].data = new unsigned char[width * height * pixelSize];

        unsigned char* curr = m_mips[m].data;
        unsigned char* prev = m_mips[m-1].data;

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                const int xx = x * 2;
                const int yy = y * 2;

                if (type == Box || type == Gaussian)
                {
                    // get 3x3 indices from previous mip level.
                    int indices[9];
                    indices[0] = Index(xx - 1, yy - 1, pixelSize, prevWidth, prevHeight, flags);
                    indices[1] = Index(xx, yy - 1, pixelSize, prevWidth, prevHeight, flags);
                    indices[2] = Index(xx + 1, yy - 1, pixelSize, prevWidth, prevHeight, flags);
                    indices[3] = Index(xx - 1, yy, pixelSize, prevWidth, prevHeight, flags);
                    indices[4] = Index(xx, yy, pixelSize, prevWidth, prevHeight, flags);
                    indices[5] = Index(xx + 1, yy, pixelSize, prevWidth, prevHeight, flags);
                    indices[6] = Index(xx - 1, yy + 1, pixelSize, prevWidth, prevHeight, flags);
                    indices[7] = Index(xx, yy + 1, pixelSize, prevWidth, prevHeight, flags);
                    indices[8] = Index(xx + 1, yy + 1, pixelSize, prevWidth, prevHeight, flags);

                    // for each channel
                    for (int i = 0; i < pixelSize; ++i)
                    {
                        if ((flags & SRGBFlag) && !s_pixelFormatToAlphaMask[m_pixelFormat] & (1 << i))
                        {
                            // get 3x3 values from the previous mip level.
                            // channel is color which is srgb encoded.
                            float v = 0;
                            for (int j = 0; j < 9; ++j)
                                v += kernel[j] * _SRGBDecode(prev[indices[j] + i]);
                            curr[Index(x, y, pixelSize, width, height, 0) + i] = _SRGBEncode(v);
                        }
                        else
                        {
                            // get 3x3 values from the previous mip level.
                            // channel is alpha.  i.e. non-srgb encoded
                            float v = 0;
                            for (int j = 0; j < 9; ++j)
                                v += kernel[j] * prev[indices[j] + i];
                            curr[Index(x, y, pixelSize, width, height, 0) + i] = v;
                        }
                    }
                }
                else if (type == WideGaussian)
                {
                    // get 5x5 indices from previous mip level.
                    int indices[25];
                    int count = 0;
                    for (int yyy = yy - 2; yyy <= yy + 2; ++yyy)
                    {
                        for (int xxx = xx - 2; xxx <= xx + 2; ++xxx)
                        {
                            indices[count] = Index(xxx, yyy, pixelSize, prevWidth, prevHeight, flags);
                            count++;
                        }
                    }

                    // for each channel
                    for (int i = 0; i < pixelSize; ++i)
                    {
                        if ((flags & SRGBFlag) && !s_pixelFormatToAlphaMask[m_pixelFormat] & (1 << i))
                        {
                            // get 5x5 values from the previous mip level.
                            // channel is color which is srgb encoded.
                            float v = 0;
                            for (int j = 0; j < 25; ++j)
                                v += kernel[j] * _SRGBDecode(prev[indices[j] + i]);
                            curr[Index(x, y, pixelSize, width, height, 0) + i] = _SRGBEncode(v);
                        }
                        else
                        {
                            // get 5x5 values from the previous mip level.
                            // channel is alpha.  i.e. non-srgb encoded
                            float v = 0;
                            for (int j = 0; j < 25; ++j)
                                v += kernel[j] * prev[indices[j] + i];
                            curr[Index(x, y, pixelSize, width, height, 0) + i] = v;
                        }
                    }
                }
            }
        }
    }
}

void Image::SmoothPixelBorder()
{
    ASSERT(m_numMips == 1);
    ASSERT(m_pixelFormat == Texture::RGBA || m_pixelFormat == Texture::BGRA);

    Buffer* buffer = &m_mips[0];
    unsigned char* data = buffer->data;
    ASSERT(data);
    int pixelSize = GetPixelSize();
    int w = buffer->width;
    int h = buffer->height;

    // allocate a new bitmap
    unsigned char* newData = new unsigned char[w * h * pixelSize];

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int i = Index(x, y, pixelSize, w, h, 0);

            // if this pixel is not filled. i.e alpha is 0.
            if (data[i + 3] == 0)
            {
                // get 3x3 indices
                int indices[9];
                indices[0] = Index(x - 1, y - 1, pixelSize, w, h, 0);
                indices[1] = Index(x, y - 1, pixelSize, w, h, 0);
                indices[2] = Index(x + 1, y - 1, pixelSize, w, h, 0);
                indices[3] = Index(x - 1, y, pixelSize, w, h, 0);
                indices[4] = i;
                indices[5] = Index(x + 1, y, pixelSize, w, h, 0);
                indices[6] = Index(x - 1, y + 1, pixelSize, w, h, 0);
                indices[7] = Index(x, y + 1, pixelSize, w, h, 0);
                indices[8] = Index(x + 1, y + 1, pixelSize, w, h, 0);

                // average all the neighboring filled pixels
                Vector3f v(0, 0, 0);
                float count = 0;
                for (int j = 0; j < 9; ++j)
                {
                    if (data[indices[j] + 3] > 1)
                    {
                        float r = data[indices[j] + 0];
                        float g = data[indices[j] + 1];
                        float b = data[indices[j] + 2];
                        v += Vector3f(r, g, b);
                        count += 1.0f;
                    }
                }
                v = v / count;

                unsigned int newIndex = Index(x, y, pixelSize, w, h, 0);
                newData[newIndex] = v.x;
                newData[newIndex+1] = v.y;
                newData[newIndex+2] = v.z;
                newData[newIndex+3] = count ? 255 : 0;
            }
            else
            {
                // copy pixel as is.
                newData[i] = data[i];
                newData[i+1] = data[i+1];
                newData[i+2] = data[i+2];
                newData[i+3] = data[i+3];
            }

        }
    }
    delete [] data;
    buffer->data = newData;
}


int Image::GetNumMipMaps() const
{
    return m_numMips;
}

const Image::Buffer* Image::GetMipMap(int i) const
{
    return &m_mips[i];
}

int Image::GetPixelSize() const
{
    return s_pixelFormatToPixelSize[m_pixelFormat];
}

void Image::FreeImageData()
{
    for (int i = 0; i < m_numMips; ++i)
        delete [] m_mips[i].data;
    delete [] m_mips;
}
