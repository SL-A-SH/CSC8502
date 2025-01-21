#version 330 core

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projMatrix;
uniform mat4 shadowMatrix;

in vec3 position;
in vec2 texCoord;
in vec3 normal;

out Vertex {
    vec2 texCoord;
    vec3 normal;
    vec3 worldPos;
    vec4 shadowProj;
} OUT;

void main(void) {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    OUT.worldPos = worldPos.xyz;
    
    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
    OUT.normal = normalize(normalMatrix * normal);
    
    OUT.texCoord = texCoord;
    OUT.shadowProj = shadowMatrix * vec4(worldPos.xyz, 1.0);
    
    gl_Position = projMatrix * viewMatrix * worldPos;
}