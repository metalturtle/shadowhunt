#version 460 core
out vec4 FragColor;
in vec3 vertColor;
in vec2 texCoord;
uniform float uniColor;

uniform sampler2D tex0;

void main()
{
    vec4 texColor = texture(tex0, texCoord);
    // FragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
    FragColor = texColor * 0.1;
}
