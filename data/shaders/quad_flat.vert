#version 130

uniform mat4 osg_ProjectionMatrix;

in vec4 osg_Vertex;
in vec4 osg_Color;

out vec4 Color;

void main()
{
    gl_Position = osg_ProjectionMatrix * osg_Vertex;
    Color = osg_Color;
}
