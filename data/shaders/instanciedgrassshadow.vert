uniform int layer;

uniform vec3 windDir;
#if __VERSION__ >= 330
layout(location = 0) in vec3 Position;
layout(location = 2) in vec4 Color;
layout(location = 3) in vec2 Texcoord;

layout(location = 7) in vec3 Origin;
layout(location = 8) in vec3 Orientation;
layout(location = 9) in vec3 Scale;
#ifdef Use_Bindless_Texture
layout(location = 10) in uvec2 Handle;
#endif

#else
in vec3 Position;
in vec4 Color;
in vec2 Texcoord;

in vec3 Origin;
in vec3 Orientation;
in vec3 Scale;
#endif

#ifdef VSLayer
out vec2 uv;
#ifdef Use_Bindless_Texture
flat out uvec2 handle;
#endif
#else
out vec2 tc;
out int layerId;
#ifdef Use_Bindless_Texture
flat out uvec2 hdle;
#endif
#endif

mat4 getWorldMatrix(vec3 translation, vec3 rotation, vec3 scale);
mat4 getInverseWorldMatrix(vec3 translation, vec3 rotation, vec3 scale);

void main(void)
{
    mat4 ModelMatrix = getWorldMatrix(Origin, Orientation, Scale);
#ifdef VSLayer
    gl_Layer = layer;
    gl_Position = ShadowViewProjMatrixes[gl_Layer] * ModelMatrix * vec4(Position + windDir * Color.r, 1.);
    uv = Texcoord;
#ifdef Use_Bindless_Texture
    handle = Handle;
#endif
#else
    layerId = layer;
    gl_Position = ShadowViewProjMatrixes[layerId] * ModelMatrix * vec4(Position + windDir * Color.r, 1.);
    tc = Texcoord;
#ifdef Use_Bindless_Texture
    hdle = Handle;
#endif
#endif
}