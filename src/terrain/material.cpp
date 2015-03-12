#include "material.hpp"

#include <cassert>
#include <sstream>

#include <osg/ref_ptr>
#include <osg/StateSet>
#include <osg/Material>
#include <osg/TexEnvCombine>
#include <osg/BlendFunc>
#include <osg/TexMat>
#include <osg/Depth>
#include <osg/CullFace>
#include <osg/Texture2D>

#include <osgDB/ReadFile>

#if TERRAIN_USE_SHADER
#include <boost/functional/hash.hpp>

#include <extern/shiny/Main/Factory.hpp>
#endif

namespace
{

std::string sampleColor(const Terrain::LayerInfo &layer, size_t layer_num)
{
    std::stringstream sstr;
    if(layer.mSpecular)
        sstr<< "texture2D(diffuseTex"<<layer_num<<", TexCoords.xy)";
    else
        sstr<< "vec4(texture2D(diffuseTex"<<layer_num<<", TexCoords.xy).rgb, 0.0)";
    return sstr.str();
}

std::string sampleNormal(const Terrain::LayerInfo &layer, size_t layer_num)
{
    std::stringstream sstr;
    if(layer.mNormalMap.empty())
        sstr<< "vec4(0.5, 0.5, 1.0, 1.0)";
    else if(layer.mParallax)
        sstr<< "texture2D(normalTex"<<layer_num<<", TexCoords.xy)";
    else
        sstr<< "vec4(texture2D(normalTex"<<layer_num<<", TexCoords.xy).rgb, 1.0)";
    return sstr.str();
}

void getShaderPreamble(std::ostream &stream, const std::vector<Terrain::LayerInfo> &layers)
{
    // Using GLSL 1.30, aka OpenGL 3
    stream<< "#version 130\n"<<
        "\n"<<
        "uniform vec4 illumination_color;\n"<<
        "\n";

    // Declare needed samplers
    for(size_t i = 0;i < layers.size();++i)
        stream<< "uniform sampler2D diffuseTex"<<i<<";\n";
    for(size_t i = 0;i < layers.size();++i)
    {
        if(!layers[i].mNormalMap.empty())
            stream<< "uniform sampler2D normalTex"<<i<<";\n";
    }
    if(layers.size() > 1)
    {
        // There is one blend texture for every 4 layers after the first
        for(size_t i = 0;i < (layers.size()-1+3)/4;++i)
            stream<< "uniform sampler2D blendTex"<<i<<";\n";
    }
    // Declare incoming attributes
    stream<< "\n"<<
        "in vec3 pos_viewspace;\n"<<
        "in vec3 n_viewspace;\n"<<
        "in vec3 t_viewspace;\n"<<
        "in vec3 b_viewspace;\n"<<
        "in vec4 TexCoords;\n"<<
        "in vec4 Color;\n"<<
        "\n";
    // Declare outputs
    // First output is diffuse color (rgb) and specular (a).
    // Second output is normal vector (rgb) and height from surface (a).
    // Third output is position vector (xyz) and clip space depth (w).
    // Fourth output is the illumination color.
    // Note that the normal vector values are supposed to be scaled and offset
    // to the 0...1 range, with 0.5 as the center.
    stream<<
        "out vec4 ColorData;\n"<<
        "out vec4 NormalData;\n"<<
        "out vec4 PositionData;\n"<<
        "out vec4 IlluminationData;\n"<<
        "\n";
}

void getShaderHeader(std::ostream &stream, const std::vector<Terrain::LayerInfo> &layers)
{
    // Get the diffuse and normal for the first/base layer
    stream<<
        "void main()\n"<<
        "{\n";
    stream<< "    vec4 color = "<<sampleColor(layers[0], 0)<<";\n";
    stream<< "    vec4 nn = "<<sampleNormal(layers[0], 0)<<";\n";
    if(layers.size() > 1)
        stream<< "    vec4 blend_amount;\n";
    stream<< "\n";
}

void getShaderForLayer(std::ostream &stream, const Terrain::LayerInfo &layer, size_t layer_num)
{
    // Every fourth layer after the first/base layer needs the next blend texture.
    if(((layer_num-1)&3) == 0)
        stream<< "    blend_amount = texture2D(blendTex"<<((layer_num-1)/4)<<", TexCoords.zw);\n";
    stream<< "    color = mix(color, "<<sampleColor(layer, layer_num)<<", blend_amount["<<((layer_num-1)&3)<<"]);\n";
    stream<< "    nn = mix(nn, "<<sampleNormal(layer, layer_num)<<", blend_amount["<<((layer_num-1)&3)<<"]);\n";
    stream<<"\n";
}

void getShaderFooter(std::ostream &stream)
{
    // Declare the normal rotation matrix to convert the calculated surface-
    // relative normal to view-space.
    stream<<
        "    mat3 nmat = mat3(normalize(t_viewspace),\n"<<
        "                     normalize(b_viewspace),\n"<<
        "                     normalize(n_viewspace));\n"<<
        "\n";

    // Write the view-space values to the g-buffer.
    stream<<
        "    ColorData    = color * vec4(Color.rgb, 1.0);\n"<<
        "    NormalData   = vec4(nmat*(nn.xyz - vec3(0.5)) + vec3(0.5), nn.w);\n"<<
        "    PositionData = vec4(pos_viewspace, gl_FragCoord.z);\n"<<
        "    IlluminationData = illumination_color;\n"<<
        "}\n";
}

}

namespace Terrain
{

// Used to map LayerInfo configurations to shader programs (does not depend on
// matching texture names, just the number of layers and the existence of
// normal, specular, and parallax data in each layer).
class LayerIdentifier {
public:
    struct LayerConfig {
        bool mHasNormalMap;
        bool mHasHeightInfo;
        bool mHasSpecInfo;
    };

private:
    std::vector<LayerConfig> mLayers;

public:
    LayerIdentifier(const std::vector<Terrain::LayerInfo> &layers)
    {
        mLayers.reserve(layers.size());
        for(size_t i = 0;i < layers.size();++i)
        {
            mLayers.push_back(LayerConfig{!layers[i].mNormalMap.empty(),
                                          !layers[i].mNormalMap.empty() && layers[i].mParallax,
                                          layers[i].mSpecular});
        }
    }

    bool operator<(const LayerIdentifier &rhs) const
    {
        if(mLayers.size() < rhs.mLayers.size())
            return true;
        if(rhs.mLayers.size() < mLayers.size())
            return false;

        for(size_t i = 0;i < mLayers.size();++i)
        {
            if(mLayers[i].mHasNormalMap < rhs.mLayers[i].mHasNormalMap)
                return true;
            if(rhs.mLayers[i].mHasNormalMap < mLayers[i].mHasNormalMap)
                return false;

            if(mLayers[i].mHasHeightInfo < rhs.mLayers[i].mHasHeightInfo)
                return true;
            if(rhs.mLayers[i].mHasHeightInfo < mLayers[i].mHasHeightInfo)
                return false;

            if(mLayers[i].mHasSpecInfo < rhs.mLayers[i].mHasSpecInfo)
                return true;
            if(rhs.mLayers[i].mHasSpecInfo < mLayers[i].mHasSpecInfo)
                return false;
        }
        return false;
    }
};


std::map<LayerIdentifier,osg::ref_ptr<osg::Program>> MaterialGenerator::mPrograms;


MaterialGenerator::MaterialGenerator(Storage *storage)
    : mShaders(true)
    , mShadows(false)
    , mSplitShadows(false)
    , mNormalMapping(true)
    , mParallaxMapping(true)
    , mStorage(storage)
{
}

osg::StateSet *MaterialGenerator::generate()
{
    assert(!mLayerList.empty() && "Can't create material with no layers");

    return create(false, nullptr, nullptr, 0);
}

osg::StateSet *MaterialGenerator::generateForCompositeMapRTT(int lodLevel)
{
    assert(!mLayerList.empty() && "Can't create material with no layers");

    return create(true, nullptr, nullptr, lodLevel);
}

osg::StateSet *MaterialGenerator::generateForCompositeMap(osg::Texture2D *compositeMap, osg::Texture2D *normalMap)
{
    return create(false, compositeMap, normalMap, 0);
}

osg::StateSet *MaterialGenerator::create(bool renderCompositeMap, osg::Texture2D *compositeMap, osg::Texture2D *normalMap, int lodLevel)
{
    assert(!renderCompositeMap || !compositeMap);

    osg::ref_ptr<osg::StateSet> state = new osg::StateSet();
    if(mShaders)
    {
        if(compositeMap)
        {
            std::vector<Terrain::LayerInfo> layerList;
            layerList.push_back(Terrain::LayerInfo{"dummy", "dummy", true, true});

            osg::ref_ptr<osg::Program> &prog = mPrograms[LayerIdentifier(layerList)];
            if(!prog.valid())
            {
                prog = new osg::Program();
                prog->addShader(osgDB::readShaderFile(osg::Shader::VERTEX, "shaders/terrain.vert"));

                std::stringstream sstr;
                getShaderPreamble(sstr, layerList);
                getShaderHeader(sstr, layerList);
                for(size_t i = 1;i < layerList.size();++i)
                    getShaderForLayer(sstr, layerList[i], i);
                getShaderFooter(sstr);

                prog->addShader(new osg::Shader(osg::Shader::FRAGMENT, sstr.str()));
            }
            state->setAttributeAndModes(prog.get());

            state->setTextureAttribute(0, compositeMap);
            state->setTextureAttribute(1, normalMap);
            state->addUniform(new osg::Uniform("diffuseTex0", 0));
            state->addUniform(new osg::Uniform("normalTex0", 1));
            state->addUniform(new osg::Uniform("diffuseTexMtx", osg::Matrixf::identity()));
            state->addUniform(new osg::Uniform("blendTexMtx", osg::Matrixf::identity()));
        }
        else
        {
            assert(mLayerList.size() == mBlendmapList.size()+1);

            osg::ref_ptr<osg::Program> &prog = mPrograms[LayerIdentifier(mLayerList)];
            if(!prog.valid())
            {
                prog = new osg::Program();
                prog->addShader(osgDB::readShaderFile(osg::Shader::VERTEX, "shaders/terrain.vert"));

                std::stringstream sstr;
                getShaderPreamble(sstr, mLayerList);
                getShaderHeader(sstr, mLayerList);
                for(size_t i = 1;i < mLayerList.size();++i)
                    getShaderForLayer(sstr, mLayerList[i], i);
                getShaderFooter(sstr);

                prog->addShader(new osg::Shader(osg::Shader::FRAGMENT, sstr.str()));
            }
            state->setAttributeAndModes(prog.get());

            unsigned int layerNum = 0;
            int texunit = 0;
            for(const LayerInfo &layer : mLayerList)
            {
                osg::ref_ptr<osg::Texture2D> tex = mStorage->getTextureImage(layer.mDiffuseMap);
                if(tex.valid())
                {
                    state->setTextureAttribute(texunit, tex.get());
                    std::stringstream sstr; sstr<<"diffuseTex"<<layerNum;
                    state->addUniform(new osg::Uniform(sstr.str().c_str(), texunit));
                }
                ++texunit;
                if(!layer.mNormalMap.empty())
                {
                    osg::ref_ptr<osg::Texture2D> normtex = mStorage->getTextureImage(layer.mNormalMap);
                    if(normtex.valid())
                    {
                        state->setTextureAttribute(texunit, normtex.get());
                        std::stringstream sstr; sstr<<"normalTex"<<layerNum;
                        state->addUniform(new osg::Uniform(sstr.str().c_str(), texunit));
                    }
                    ++texunit;
                }
                ++layerNum;
            }

            layerNum = 0;
            for(const osg::ref_ptr<osg::Image> &blend : mBlendmapList)
            {
                osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(blend.get());
                tex->setUnRefImageDataAfterApply(true);
                tex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
                tex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

                state->setTextureAttribute(texunit, tex.get());
                std::stringstream sstr; sstr<<"blendTex"<<layerNum;
                state->addUniform(new osg::Uniform(sstr.str().c_str(), texunit));

                ++texunit;
                ++layerNum;
            }

            double scale = 16.0 * double(1<<lodLevel);
            state->addUniform(new osg::Uniform("diffuseTexMtx", osg::Matrixf::scale(scale, scale, 1.0f)));

            scale /= (scale+1.0);
            state->addUniform(new osg::Uniform("blendTexMtx", osg::Matrixf::scale(scale, scale, 1.0f)));
        }
    }
    else if(compositeMap)
        state->setTextureAttributeAndModes(0, compositeMap);
    else
    {
        assert(mLayerList.size() == mBlendmapList.size()+1);

        const double scale = 16.0 * double(1<<lodLevel);
        unsigned int texunit = 0;
        bool first = true;
        std::vector<osg::ref_ptr<osg::Image>>::iterator blend = mBlendmapList.begin();
        for(const LayerInfo &layer : mLayerList)
        {
            if(!first)
            {
                osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(blend->get());
                tex->setUnRefImageDataAfterApply(true);
                tex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
                tex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

                state->setTextureAttributeAndModes(texunit, tex.get());

                //tus->setAlphaOperation(Ogre::LBX_BLEND_TEXTURE_ALPHA,
                //                       Ogre::LBS_TEXTURE, Ogre::LBS_TEXTURE);
                //tus->setColourOperationEx(Ogre::LBX_BLEND_DIFFUSE_ALPHA,
                //                          Ogre::LBS_TEXTURE, Ogre::LBS_TEXTURE);
                osg::ref_ptr<osg::TexEnvCombine> texcomb = new osg::TexEnvCombine();
                texcomb->setCombine_Alpha(osg::TexEnvCombine::INTERPOLATE);
                texcomb->setSource0_Alpha(osg::TexEnvCombine::TEXTURE);
                texcomb->setSource1_Alpha(osg::TexEnvCombine::TEXTURE);
                texcomb->setSource2_Alpha(osg::TexEnvCombine::TEXTURE);
                texcomb->setOperand0_Alpha(osg::TexEnvCombine::SRC_ALPHA);
                texcomb->setOperand1_Alpha(osg::TexEnvCombine::SRC_ALPHA);
                texcomb->setOperand2_Alpha(osg::TexEnvCombine::SRC_ALPHA);

                texcomb->setCombine_RGB(osg::TexEnvCombine::INTERPOLATE);
                texcomb->setSource0_RGB(osg::TexEnvCombine::TEXTURE);
                texcomb->setSource1_RGB(osg::TexEnvCombine::TEXTURE);
                texcomb->setSource2_RGB(osg::TexEnvCombine::PRIMARY_COLOR);
                texcomb->setOperand0_RGB(osg::TexEnvCombine::SRC_COLOR);
                texcomb->setOperand1_RGB(osg::TexEnvCombine::SRC_COLOR);
                texcomb->setOperand2_RGB(osg::TexEnvCombine::SRC_ALPHA);
                state->setTextureAttribute(texunit, texcomb.get());

                double tscale = scale / (scale+1.);
                state->setTextureAttribute(texunit, new osg::TexMat(osg::Matrix::scale(tscale, tscale, 1.0)));

                ++blend;
                ++texunit;
            }

            // Add the actual layer texture on top of the alpha map.
            osg::ref_ptr<osg::Texture2D> tex = mStorage->getTextureImage(layer.mDiffuseMap);
            if(!tex.valid())
                state->setTextureMode(texunit, GL_TEXTURE_2D, osg::StateAttribute::OFF);
            else
            {
                tex->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
                tex->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
                state->setTextureAttributeAndModes(texunit, tex.get());
            }
            if(!first)
            {
                //tus->setColourOperationEx(Ogre::LBX_BLEND_DIFFUSE_ALPHA,
                //                          Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT);
                osg::ref_ptr<osg::TexEnvCombine> texcomb = new osg::TexEnvCombine();
                texcomb->setCombine_Alpha(osg::TexEnvCombine::INTERPOLATE);
                texcomb->setSource0_Alpha(osg::TexEnvCombine::TEXTURE);
                texcomb->setSource1_Alpha(osg::TexEnvCombine::PREVIOUS);
                texcomb->setSource2_Alpha(osg::TexEnvCombine::PRIMARY_COLOR);
                texcomb->setOperand0_Alpha(osg::TexEnvCombine::SRC_ALPHA);
                texcomb->setOperand1_Alpha(osg::TexEnvCombine::SRC_ALPHA);
                texcomb->setOperand2_Alpha(osg::TexEnvCombine::SRC_ALPHA);

                texcomb->setCombine_RGB(osg::TexEnvCombine::INTERPOLATE);
                texcomb->setSource0_RGB(osg::TexEnvCombine::TEXTURE);
                texcomb->setSource1_RGB(osg::TexEnvCombine::PREVIOUS);
                texcomb->setSource2_RGB(osg::TexEnvCombine::PRIMARY_COLOR);
                texcomb->setOperand0_RGB(osg::TexEnvCombine::SRC_COLOR);
                texcomb->setOperand1_RGB(osg::TexEnvCombine::SRC_COLOR);
                texcomb->setOperand2_RGB(osg::TexEnvCombine::SRC_ALPHA);
                state->setTextureAttribute(texunit, texcomb.get());
            }
            state->setTextureAttribute(texunit, new osg::TexMat(osg::Matrix::scale(scale, scale, 1.0)));

            first = false;
            ++texunit;
        }
    }

    return state.release();
}

}
