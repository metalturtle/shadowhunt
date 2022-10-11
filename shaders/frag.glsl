#version 460 core
out vec4 FragColor;
in vec3 vertColor;
in vec2 texCoord;
uniform float uniColor;

uniform sampler2D tex0;

void main() {
    // FragColor = vertColor;
    vec4 texColor = texture(tex0, texCoord);
    //FragColor = texColor;
    // FragColor = vec4(texColor.rgb*0.6f, 1.0f);
    FragColor = texColor;
    // FragColor = vec4(1.0, 0.0, 0.0, 1.0f) + 0.0 * texColor;
}