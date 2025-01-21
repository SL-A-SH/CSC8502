#version 330 core

in Vertex {
    vec2 texCoord;
    float alpha;
    float distFactor;
} IN;

out vec4 fragColor;

void main() {
    float gradient = 1.0 - IN.texCoord.y;
    gradient = pow(gradient, 0.5);
    
    vec3 rainColor = vec3(1, 1, 1);  // White color
    float brightness = 0.6 + (IN.distFactor * 0.4);
    
    // Add some variation to make it more interesting
    float variation = sin(IN.texCoord.y * 10.0) * 0.1 + 0.9;
    
    float finalAlpha = gradient * IN.alpha * 0.6;
    
    vec3 glowColor = vec3(0.8, 0.9, 1.0);
    vec3 finalColor = mix(rainColor, glowColor, gradient * 0.3) * brightness * variation;
    
    fragColor = vec4(finalColor, finalAlpha);
}