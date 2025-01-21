#version 330 core

uniform vec3 cameraPos;

in Vertex {
    float intensity;
    vec3 worldPos;
} IN;

out vec4 fragColor;

void main() {
    vec3 boltColor = vec3(0.9, 0.95, 1.0);  // Almost white with slight blue tint
    vec3 glowColor = vec3(0.4, 0.6, 1.0);   // Bright blue for glow
    
    // Bright core with surrounding glow
    float coreIntensity = IN.intensity * 3.0;
    float glowIntensity = IN.intensity * 1.5;
    
    // Distance-based glow falloff
    float distToCamera = length(cameraPos - IN.worldPos);
    float distanceFalloff = 1.0 - clamp(distToCamera / 2000.0, 0.0, 1.0);
    
    // Flickering effect
    float flicker = sin(gl_FragCoord.x * 0.1) * 0.2 + 0.8;
    
    vec3 finalColor = mix(glowColor, boltColor, coreIntensity) * 2.5;
    finalColor *= flicker * distanceFalloff;
    
    float bloomFactor = max(0.0, coreIntensity - 1.0) * 0.5;
    finalColor += boltColor * bloomFactor;
    
    float alpha = min(1.0, coreIntensity * distanceFalloff);
    
    fragColor = vec4(finalColor, alpha);
}