#version 330 core

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projMatrix;
uniform float waterMovement;

in vec3 position;
in vec2 texCoord;
in vec3 normal;

out Vertex {
    vec2 texCoord;
    vec3 normal;
    vec3 worldPos;
} OUT;

void main(void) {
    vec3 pos = position;
    float wave1 = sin(texCoord.x * 10.0 + waterMovement * 0.5) * cos(texCoord.y * 10.0 + waterMovement * 0.5);
    float wave2 = sin(texCoord.x * 5.0 - waterMovement * 0.3) * cos(texCoord.y * 5.0 - waterMovement * 0.4);
    pos.y += (wave1 + wave2) * 4.0;
    
    vec4 worldPos = modelMatrix * vec4(pos, 1.0);
    
    OUT.texCoord = texCoord;
    OUT.normal = normalize(mat3(modelMatrix) * normal);
    OUT.worldPos = worldPos.xyz;
    
    gl_Position = projMatrix * viewMatrix * worldPos;
}