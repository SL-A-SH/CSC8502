#version 330 core

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projMatrix;

in vec3 position;
in float intensity;

out Vertex {
    float intensity;
    vec3 worldPos;
} OUT;

void main() {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    OUT.worldPos = worldPos.xyz;
    OUT.intensity = intensity;
    gl_Position = projMatrix * viewMatrix * worldPos;
}