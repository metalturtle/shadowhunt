#version 460 core
layout (location=0) in vec3 aPos;
layout (location=1) in vec3 aCol;
layout (location=2) in vec2 aTex;

out vec3 vertColor;
out vec2 texCoord;

uniform mat4 transform;

void main() {
    // gl_Position = vec4(cosv*aPos.x - sinv*aPos.y, sinv*aPos.x + cosv*aPos.y, aPos.z, 1.0f);
    gl_Position = transform * vec4(aPos, 1.0f);
    vertColor = aCol;
    texCoord = aTex;
}
