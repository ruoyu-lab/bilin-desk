#extension GL_OES_EGL_image_external : require

// external.frag 的 OpenGL ES 2.0 写法（qsb 换进 .qsb 的 GLSL 100 es 那一份）：外部纹理只能用 samplerExternalOES 采样
precision highp float;

struct Parameters
{
    mat4 mvp;
    float bt709;
    float opacity;
};

uniform Parameters parameters;
uniform samplerExternalOES pictureTexture;

varying vec2 v_texcoord;

void main()
{
    gl_FragColor = vec4(texture2D(pictureTexture, v_texcoord).rgb * parameters.opacity, parameters.opacity);
}
