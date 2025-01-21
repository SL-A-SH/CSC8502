#version 330 core
in vec3 position;
in vec2 texCoord;
out vec2 TexCoords;

void main() {
    TexCoords = texCoord;
    gl_Position = vec4(position, 1.0);
}