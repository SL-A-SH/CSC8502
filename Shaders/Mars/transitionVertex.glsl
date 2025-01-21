#version 330 core

in vec3 position;
in vec2 texCoord;

out Vertex {
    vec2 texCoord;
} OUT;

void main() {
    // Pass texture coordinates to fragment shader
    OUT.texCoord = texCoord;
    gl_Position = vec4(position, 1.0);
}