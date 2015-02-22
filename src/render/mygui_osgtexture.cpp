
#include "mygui_osgtexture.h"

#include <osg/Texture2D>
#include <osgDB/ReadFile>
#include <MyGUI_Gui.h>

#include "log.hpp"

#include "mygui_osgdiagnostic.h"
#include "mygui_osgrendermanager.h"


namespace TK
{

OSGTexture::OSGTexture(const std::string &name)
  : mName(name)
  , mFormat(MyGUI::PixelFormat::Unknow)
  , mUsage(MyGUI::TextureUsage::Default)
  , mNumElemBytes(0)
{
}

OSGTexture::~OSGTexture()
{
}

void OSGTexture::createManual(int width, int height, MyGUI::TextureUsage usage, MyGUI::PixelFormat format)
{
    GLenum glfmt = GL_NONE;
    size_t numelems = 0;
    switch(format.getValue())
    {
        case MyGUI::PixelFormat::L8:
            glfmt = GL_LUMINANCE;
            numelems = 1;
            break;
        case MyGUI::PixelFormat::L8A8:
            glfmt = GL_LUMINANCE_ALPHA;
            numelems = 2;
            break;
        case MyGUI::PixelFormat::R8G8B8:
            glfmt = GL_RGB;
            numelems = 3;
            break;
        case MyGUI::PixelFormat::R8G8B8A8:
            glfmt = GL_RGBA;
            numelems = 4;
            break;
    }
    if(glfmt == GL_NONE)
        throw std::runtime_error("Texture format not supported");

    mTexture = new osg::Texture2D();
    mTexture->setTextureSize(width, height);
    mTexture->setSourceFormat(glfmt);
    mTexture->setSourceType(GL_UNSIGNED_BYTE);

    mTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
    mTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    mTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
    mTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

    mFormat = format;
    mUsage = usage;
    mNumElemBytes = numelems;
}

void OSGTexture::destroy()
{
    mTexture = nullptr;
    mFormat = MyGUI::PixelFormat::Unknow;
    mUsage = MyGUI::TextureUsage::Default;
    mNumElemBytes = 0;
}

void OSGTexture::loadFromFile(const std::string &fname)
{
    osg::ref_ptr<osg::Image> image = osgDB::readImageFile(fname);
    if(!image.valid())
        throw std::runtime_error("Failed to load image "+fname);
    if(image->getDataType() != GL_UNSIGNED_BYTE)
        throw std::runtime_error("Unsupported pixel type");

    MyGUI::PixelFormat format;
    size_t numelems;
    switch(image->getPixelFormat())
    {
        case GL_ALPHA:
            /* FIXME: Alpha as luminance? Or maybe convert to luminance+alpha
             * with full luminance? */
            image->setPixelFormat(GL_LUMINANCE);
            /* fall-through */
        case GL_LUMINANCE:
            format = MyGUI::PixelFormat::L8;
            numelems = 1;
            break;
        case GL_LUMINANCE_ALPHA:
            format = MyGUI::PixelFormat::L8A8;
            numelems = 2;
            break;
        case GL_RGB:
            format = MyGUI::PixelFormat::R8G8B8;
            numelems = 3;
            break;
        case GL_RGBA:
            format = MyGUI::PixelFormat::R8G8B8A8;
            numelems = 4;
            break;

        default:
            throw std::runtime_error("Unsupported pixel format");
    }

    image->flipVertical();
    mTexture = new osg::Texture2D(image.get());
    mTexture->setUnRefImageDataAfterApply(true);

    mTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
    mTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    mTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
    mTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

    mFormat = format;
    mUsage = MyGUI::TextureUsage::Static | MyGUI::TextureUsage::Write;
    mNumElemBytes = numelems;
}

void OSGTexture::saveToFile(const std::string &fname)
{
    Log::get().message("Would save image to file "+fname, Log::Level_Error);
}


int OSGTexture::getWidth()
{
    if(!mTexture.valid())
        return 0;
    osg::Image *image = mTexture->getImage();
    if(image) return image->s();
    return mTexture->getTextureWidth();
}

int OSGTexture::getHeight()
{
    if(!mTexture.valid())
        return 0;
    osg::Image *image = mTexture->getImage();
    if(image) return image->t();
    return mTexture->getTextureHeight();
}


void *OSGTexture::lock(MyGUI::TextureUsage /*access*/)
{
    MYGUI_PLATFORM_ASSERT(mTexture.valid(), "Texture is not created");
    MYGUI_PLATFORM_ASSERT(!mLockedImage.valid(), "Texture already locked");

    mLockedImage = mTexture->getImage();
    if(!mLockedImage.valid())
    {
        mLockedImage = new osg::Image();
        mLockedImage->allocateImage(
            mTexture->getTextureWidth(), mTexture->getTextureHeight(), mTexture->getTextureDepth(),
            mTexture->getSourceFormat(), mTexture->getSourceType()
        );
    }
    return mLockedImage->data();
}

void OSGTexture::unlock()
{
    MYGUI_PLATFORM_ASSERT(mLockedImage.valid(), "Texture not locked");

    // Tell the texture it can get rid of the image for static textures (since
    // they aren't expected to update much at all).
    mTexture->setImage(mLockedImage.get());
    mTexture->setUnRefImageDataAfterApply(mUsage.isValue(MyGUI::TextureUsage::Static) ? true : false);
    mTexture->dirtyTextureObject();

    mLockedImage = nullptr;
}

bool OSGTexture::isLocked()
{
    return mLockedImage.valid();
}


// FIXME: Render-to-texture not currently implemented.
MyGUI::IRenderTarget* OSGTexture::getRenderTarget()
{
    return nullptr;
}

} // namespace TK
