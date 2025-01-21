#version 330 core

uniform mat4 modelMatrix;
uniform mat4 shadowMatrix;

in vec3 position;

void main() {
    gl_Position = shadowMatrix * modelMatrix * vec4(position, 1.0);
}