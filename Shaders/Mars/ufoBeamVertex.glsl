#version 330 core

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projMatrix;
uniform float beamIntensity;

in vec3 position;
in vec4 colour;
in vec3 normal;
in vec2 texCoord;

out Vertex {
    vec4 colour;
    vec2 texCoord;
    vec3 worldPos;
    vec3 normal;
} OUT;

void main(void) {
    mat4 mvp = projMatrix * viewMatrix * modelMatrix;
    gl_Position = mvp * vec4(position, 1.0);
    
    OUT.colour = colour;
    OUT.texCoord = texCoord;
    OUT.worldPos = (modelMatrix * vec4(position, 1.0)).xyz;
    OUT.normal = (modelMatrix * vec4(normal, 0.0)).xyz;
}