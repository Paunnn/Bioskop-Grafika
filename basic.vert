#version 330 core

layout(location = 0) in vec2 aPos;

uniform vec2 uPos;
uniform vec2 uSize;

void main() {
    vec2 scaledPos = aPos * uSize;
    vec2 finalPos = scaledPos + uPos;
    gl_Position = vec4(finalPos, 0.0, 1.0);
}