#ifndef OSGVERTEXBUFFER_H
#define OSGVERTEXBUFFER_H

#include <vector>

#include <MyGUI_IVertexBuffer.h>

#include <osg/ref_ptr>
#include <osg/BufferObject>
#include <osg/Array>


namespace TK
{

class OSGVertexBuffer : public MyGUI::IVertexBuffer
{
    osg::ref_ptr<osg::VertexBufferObject> mBuffer;
    osg::ref_ptr<osg::Vec3Array> mPositionArray;
    osg::ref_ptr<osg::Vec4ubArray> mColorArray;
    osg::ref_ptr<osg::Vec2Array> mTexCoordArray;
    std::vector<MyGUI::Vertex> mLockedData;

    size_t mNeedVertexCount;

public:
    OSGVertexBuffer();
    virtual ~OSGVertexBuffer();

    virtual void setVertexCount(size_t count);
    virtual size_t getVertexCount();

    virtual MyGUI::Vertex *lock();
    virtual void unlock();

/*internal:*/
    void destroy();
    void create();

    osg::VertexBufferObject *getBuffer() const { return mBuffer.get(); }
};

} // namespace TK

#endif /* OSGVERTEXBUFFER_H */
