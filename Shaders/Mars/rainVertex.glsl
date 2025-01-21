#version 330 core

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projMatrix;
uniform float rainLength;
uniform float distToCamera;

in vec3 position;
in vec2 texCoord;

out Vertex {
    vec2 texCoord;
    float alpha;
    float distFactor;
} OUT;

void main() {
    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vec4 viewPos = viewMatrix * worldPos;
    
    viewPos.y -= texCoord.y * rainLength;
    
    gl_Position = projMatrix * viewPos;
    OUT.texCoord = texCoord;
    
    float distFade = clamp(1.0 - (distToCamera / 2000.0), 0.2, 1.0);
    float heightFade = clamp(1.0 - (worldPos.y / 500.0), 0.0, 1.0);
    OUT.alpha = distFade * heightFade;
    OUT.distFactor = distFade;
}