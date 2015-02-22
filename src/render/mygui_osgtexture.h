#ifndef OSGTEXTURE_H
#define OSGTEXTURE_H

#include <MyGUI_ITexture.h>

#include <osg/ref_ptr>

namespace osg
{
    class Image;
    class Texture2D;
}

namespace TK
{

class OSGTexture : public MyGUI::ITexture {
    std::string mName;

    osg::ref_ptr<osg::Image> mLockedImage;
    osg::ref_ptr<osg::Texture2D> mTexture;
    MyGUI::PixelFormat mFormat;
    MyGUI::TextureUsage mUsage;
    size_t mNumElemBytes;

public:
    OSGTexture(const std::string &name);
    virtual ~OSGTexture();

    virtual const std::string& getName() const { return mName; }

    virtual void createManual(int width, int height, MyGUI::TextureUsage usage, MyGUI::PixelFormat format);
    virtual void loadFromFile(const std::string &fname);
    virtual void saveToFile(const std::string &fname);

    virtual void destroy();

    virtual void* lock(MyGUI::TextureUsage access);
    virtual void unlock();
    virtual bool isLocked();

    virtual int getWidth();
    virtual int getHeight();

    virtual MyGUI::PixelFormat getFormat() { return mFormat; }
    virtual MyGUI::TextureUsage getUsage() { return mUsage; }
    virtual size_t getNumElemBytes() { return mNumElemBytes; }

    virtual MyGUI::IRenderTarget *getRenderTarget();

/*internal:*/
    osg::Texture2D *getTexture() const { return mTexture.get(); }
};

} // namespace TK

#endif /* OSGTEXTURE_H */
