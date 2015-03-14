#ifndef RENDER_PIPELINE_HPP
#define RENDER_PIPELINE_HPP

#include <string>

#include <osg/ref_ptr>
#include <osg/Camera>

#include "singleton.hpp"


namespace osg
{
    class Geode;

    class Texture;
    class StateSet;

    class Vec2f;
}

namespace TK
{

class Pipeline : public Singleton<Pipeline> {
    int mScreenWidth;
    int mScreenHeight;

    int mTextureWidth;
    int mTextureHeight;

    osg::ref_ptr<osg::Group> mGraph;
    osg::ref_ptr<osg::Camera> mClearPass;
    osg::ref_ptr<osg::Camera> mMainPass;
    osg::ref_ptr<osg::Camera> mLightPass;
    osg::ref_ptr<osg::Camera> mCombinerPass;
    osg::ref_ptr<osg::Camera> mOutputPass;

    osg::ref_ptr<osg::Texture> mGBufferColors;
    osg::ref_ptr<osg::Texture> mGBufferNormals;
    osg::ref_ptr<osg::Texture> mGBufferPositions;
    osg::ref_ptr<osg::Texture> mDepthStencil;

    osg::ref_ptr<osg::Texture> mDiffuseLight;
    osg::ref_ptr<osg::Texture> mSpecularLight;

    osg::ref_ptr<osg::Texture> mFinalBuffer;

    osg::ref_ptr<osg::Camera> mDebugMapDisplay;

    static osg::ref_ptr<osg::Geometry> createScreenGeometry(const osg::Vec2f &corner, float width, float height, int tex_width, int tex_height, const osg::Vec4ub &color=osg::Vec4ub(255,255,255,255));
    static osg::Geode *createScreenQuad(const osg::Vec2f &corner, float width, float height, int tex_width, int tex_height, const osg::Vec4ub &color=osg::Vec4ub(255,255,255,255));

    static osg::ref_ptr<osg::Texture> createTextureRect(int width, int height, GLenum internalFormat, GLenum format, GLenum type);
    static osg::ref_ptr<osg::Camera> createRTTCamera(osg::Camera::BufferComponent buffer, osg::Texture *tex);

    static osg::StateSet *setShaderProgram(osg::Node *node, std::string vert, std::string frag);

public:
    Pipeline(int width, int height)
      : mScreenWidth(width)
      , mScreenHeight(height)
      , mTextureWidth(width)
      , mTextureHeight(height)
    { }

    void init(osg::Group *scene);

    void setProjectionMatrix(const osg::Matrix &matrix);

    osg::Node *createDirectionalLight();
    void removeDirectionalLight(osg::Node *node);

    void toggleDebugMapDisplay();

    osg::StateSet *getLightingStateSet() { return mLightPass->getStateSet(); }

    osg::Group *getGraphRoot() const { return mGraph.get(); }
    osg::Texture *getColorTexture() const { return mGBufferColors.get(); }
    osg::Texture *getNormalsTexture() const { return mGBufferNormals.get(); }
    osg::Texture *getPositionsTexture() const { return mGBufferPositions.get(); }

    osg::Texture *getDiffuseTexture() const { return mDiffuseLight.get(); }
    osg::Texture *getSpecularTexture() const { return mSpecularLight.get(); }
};

} // namespace TK

#endif /* RENDER_PIPELINE_HPP */
