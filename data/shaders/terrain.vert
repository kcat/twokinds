#version 130

uniform mat4 osg_ModelViewProjectionMatrix;
uniform mat4 osg_ModelViewMatrix;

uniform mat4 diffuseTexMtx;
uniform mat4 blendTexMtx;

in vec4 osg_Vertex;
in vec3 osg_Normal;
in vec4 osg_Color;
in vec4 osg_MultiTexCoord0;

out vec3 pos_viewspace;
out vec3 n_viewspace;
out vec3 t_viewspace;
out vec3 b_viewspace;
out vec4 TexCoords;
out vec4 Color;

void main()
{
    gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;
    TexCoords.xy = (diffuseTexMtx * osg_MultiTexCoord0).xy;
    TexCoords.zw = (blendTexMtx * osg_MultiTexCoord0).xy;
    Color = osg_Color;

    pos_viewspace = (osg_ModelViewMatrix * osg_Vertex).xyz;

    vec3 binormal = cross(osg_Normal, vec3(1.0, 0.0, 0.0));
    n_viewspace   = normalize(mat3(osg_ModelViewMatrix) * osg_Normal);
    t_viewspace   = normalize(mat3(osg_ModelViewMatrix) * cross(osg_Normal, binormal));
    b_viewspace   = normalize(mat3(osg_ModelViewMatrix) * binormal);
}
