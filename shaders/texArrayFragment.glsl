#version 460 core
out vec4 FragColor;
in vec3 vertColor;
in vec2 texCoord;
uniform float uniColor;
uniform float currentLayer;

uniform sampler2DArray tex0;

void main()
{
    vec3 texCoordLayer = vec3(texCoord, currentLayer);
    vec4 texColor = texture(tex0, texCoordLayer);
    // FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
    // FragColor = texColor * 1.0 + vec4(1.0, 0.0, 0.0, 1.0);
    FragColor = texColor;
}
