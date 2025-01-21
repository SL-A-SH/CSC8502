#version 330 core

uniform vec3 cameraPos;
uniform float beamIntensity;

in Vertex {
    vec4 colour;
    vec2 texCoord;
    vec3 worldPos;
    vec3 normal;
} IN;

out vec4 fragColor;

void main(void) {
    vec3 viewDir = normalize(cameraPos - IN.worldPos);
    float fresnel = 1.0 - max(dot(viewDir, normalize(IN.normal)), 0.0);
    fresnel = pow(fresnel, 2.0);
    
    float pattern = sin(IN.texCoord.y * 10.0 + beamIntensity * 2.0) * 0.5 + 0.5;
    
    float verticalFade = 1.0 - IN.texCoord.y;
    
    // Base color
    vec3 beamColor = vec3(0.2, 0.8, 0.1);
    
    float alpha = fresnel * verticalFade * 0.6;
    
    vec3 finalColor = beamColor * pattern * beamIntensity * 2.0;
    
    fragColor = vec4(finalColor, alpha);
}