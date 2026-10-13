#version 440

// 已经是 RGB 的画面（Windows 上软件解码的 BGRA8 纹理）
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Parameters {
    mat4 mvp;
    float bt709;
    float opacity;
};
layout(binding = 1) uniform sampler2D pictureTexture;

void main() {
    fragColor = vec4(texture(pictureTexture, v_texcoord).rgb * opacity, opacity);
}
