#version 330 core

uniform float waterMovement;
uniform vec3 cameraPos;
uniform vec3 lightPos;
uniform vec4 lightColour;

in Vertex {
    vec2 texCoord;
    vec3 normal;
    vec3 worldPos;
} IN;

out vec4 fragColor;

float calculateFresnel(vec3 normal, vec3 viewDir, float baseReflectivity, float fresnelPower) {
    return baseReflectivity + (1.0 - baseReflectivity) * pow(1.0 - max(dot(normal, viewDir), 0.0), fresnelPower);
}

void main(void) {
    vec3 deepColor = vec3(0.0, 0.2, 0.4);
    vec3 shallowColor = vec3(0.1, 0.4, 0.5);
    
    vec3 normal = normalize(IN.normal);
    float wave1 = sin(IN.texCoord.x * 20.0 + waterMovement * 0.5) * 
                 cos(IN.texCoord.y * 20.0 + waterMovement * 0.5) * 0.8;
    float wave2 = sin(IN.texCoord.x * 10.0 - waterMovement * 0.3) * 
                 cos(IN.texCoord.y * 10.0 - waterMovement * 0.4) * 0.6;
    normal = normalize(normal + vec3(wave1 * 0.2, 0.0, wave2 * 0.2));
    
    // View and light vectors
    vec3 viewDir = normalize(cameraPos - IN.worldPos);
    vec3 lightDir = normalize(lightPos - IN.worldPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    
    // Fresnel effect
    float fresnel = calculateFresnel(normal, viewDir, 0.04, 4.0);
    
    // Specular lighting
    float specPower = 128.0;
    float spec = pow(max(dot(halfDir, normal), 0.0), specPower);
    vec3 specColor = lightColour.rgb * spec * 3.0;
    
    // Distance-based color blend with extended visibility
    float depth = length(cameraPos - IN.worldPos);
    float depthFactor = clamp(depth / 2000.0, 0.0, 1.0);
    vec3 waterColor = mix(shallowColor, deepColor, depthFactor);
    
    // Wave effect on color
    float waveFactor = (wave1 + wave2) * 0.5 + 0.5;
    waterColor = mix(waterColor, waterColor * 1.4, waveFactor);
    
    // Sun glitter effect
    float sunGlitter = pow(max(dot(normal, halfDir), 0.0), 512.0) * 2.0;
    
    vec3 finalColor = waterColor;
    finalColor += specColor;
    finalColor += vec3(sunGlitter);
    finalColor = mix(finalColor, vec3(1.0), fresnel * 0.7);
    
    float alpha = mix(0.85, 1.0, fresnel);
    alpha = mix(alpha, 1.0, spec * 0.7);
    
    float edgeFade = 1.0 - pow(depthFactor, 0.3);
    alpha *= edgeFade;
    
    finalColor *= 1.3;
    
    fragColor = vec4(finalColor, alpha);
}