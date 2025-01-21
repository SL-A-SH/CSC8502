#version 330 core

uniform sampler2D sourceScene;    // Ancient Mars
uniform sampler2D targetScene;    // Current Mars
uniform float transitionProgress;
uniform vec2 resolution;
uniform float noiseScale;
uniform float edgeSharpness;

in Vertex {
    vec2 texCoord;
} IN;

out vec4 fragColor;

float noise(vec2 uv) {
    vec2 s = vec2(12.9898, 78.233);
    vec2 p = floor(uv * 43758.5453);
    return fract(sin(dot(p, s)) * 43758.5453);
}

float fbm(vec2 uv) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < 6; i++) {
        value += amplitude * noise(uv * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    
    return value;
}

void main() {
    // Sample both scenes
    vec4 sourceColor = texture(sourceScene, IN.texCoord);
    vec4 targetColor = texture(targetScene, IN.texCoord);
    
    // Create a complex noise pattern
    vec2 noiseUV = IN.texCoord * noiseScale;
    float noiseValue = fbm(noiseUV);
    
    // Create a directional transition
    float transitionEdge = (IN.texCoord.x + noiseValue * 0.2 - transitionProgress) * edgeSharpness;
    float transitionFactor = clamp(transitionEdge, 0.0, 1.0);
    
    // Add atmosphere dissipation effect
    vec3 atmosphereColor = sourceColor.rgb * 0.5 + vec3(0.6, 0.4, 0.3) * 0.5;
    float atmosphereStrength = (1.0 - transitionFactor) * 0.3;
    
    // Smooth transition between scenes with atmospheric blend
    vec4 transitionColor = mix(sourceColor, targetColor, smoothstep(0.0, 1.0, transitionFactor));
    
    // Add atmospheric effect
    transitionColor.rgb = mix(transitionColor.rgb, atmosphereColor, atmosphereStrength * (1.0 - transitionFactor));
    
    // Add dust storm effect near transition boundary
    float dustBoundary = abs(transitionFactor - 0.5) * 2.0;
    vec3 dustColor = vec3(0.8, 0.6, 0.4);
    float dustStrength = (1.0 - dustBoundary) * 0.4 * smoothstep(0.0, 0.3, abs(transitionProgress - 0.5));
    transitionColor.rgb = mix(transitionColor.rgb, dustColor, dustStrength);
    
    fragColor = transitionColor;
}