#version 440

// Android：解码器的硬件缓冲区做成的外部纹理，GPU 采样时已经按缓冲区标的格式转成 RGB（frame_textures_android.cpp）。
// 这是给别的图形接口编的那一份；OpenGL ES 用 external_gles.frag（samplerExternalOES，这里写不出来）。
// 块和采样器的名字两边要一样：Qt 的 OpenGL 后端按名字设 uniform
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Parameters {
    mat4 mvp;
    float bt709;
    float opacity;
} parameters;
layout(binding = 1) uniform sampler2D pictureTexture;

void main() {
    fragColor = vec4(texture(pictureTexture, v_texcoord).rgb * parameters.opacity, parameters.opacity);
}
