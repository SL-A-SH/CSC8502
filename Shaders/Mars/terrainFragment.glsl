#version 330 core

uniform sampler2D baseSoilTex;
uniform sampler2D rockTex;
uniform sampler2D sedimentTex;
uniform sampler2D shadowTex;
uniform vec3 lightPos;
uniform vec4 lightColour;
uniform float lightRadius;
uniform vec3 cameraPos;
uniform float ambientStrength;

uniform vec3 beamPosition;
uniform vec3 beamDirection;
uniform vec4 beamColor;
uniform float beamIntensity;
uniform float beamCutoff;
uniform bool beamActive;

in Vertex {
    vec2 texCoord;
    vec3 normal;
    vec3 worldPos;
    vec4 shadowProj;
} IN;

out vec4 fragColor;

float calculateSpotlight(vec3 fragPos) {
    vec3 lightDir = normalize(beamPosition - fragPos);
    float theta = dot(lightDir, normalize(-beamDirection));
    float epsilon = beamCutoff - beamCutoff * 0.9; // Creates soft edges
    float intensity = clamp((theta - beamCutoff * 0.9) / epsilon, 0.0, 1.0);
    
    // Distance attenuation
    float distance = length(beamPosition - fragPos);
    float attenuation = 1.0 / (1.0 + 0.0014 * distance + 0.000007 * distance * distance);
    
    return intensity * attenuation * beamIntensity;
}

float ShadowCalculation() {
    vec3 shadowNDC = IN.shadowProj.xyz / IN.shadowProj.w;
    if(abs(shadowNDC.x) <= 1.0 && abs(shadowNDC.y) <= 1.0) {
        vec2 shadowUV = shadowNDC.xy * 0.5 + 0.5;
        float shadowZ = shadowNDC.z * 0.5 + 0.5;
        
        // PCF sampling for smoother shadows
        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(shadowTex, 0);
        for(int x = -1; x <= 1; ++x) {
            for(int y = -1; y <= 1; ++y) {
                float pcfDepth = texture(shadowTex, shadowUV + vec2(x, y) * texelSize).r;
                shadow += shadowZ > pcfDepth + 0.001 ? 0.3 : 1.0;  // Bias of 0.001 to reduce shadow acne
            }
        }
        shadow /= 9.0;
        return shadow;
    }
    return 1.0;
}

void main(void) {
    float height = IN.worldPos.y / 255.0;
    float steepness = 1.0 - dot(IN.normal, vec3(0, 1, 0));
    
    vec4 baseSoil = texture(baseSoilTex, IN.texCoord * 3.0);
    vec4 rock = texture(rockTex, IN.texCoord * 3.0);
    vec4 sediment = texture(sedimentTex, IN.texCoord * 3.0);
    
    vec4 finalColor;
    if(height < 0.25) {
        // Lower areas - blend between sediment and base soil
        float blend = smoothstep(0.0, 0.4, height);
        finalColor = mix(sediment, baseSoil, blend);
    } 
    else {
        // Rest of the area - blend between base soil and rock
        float blend = smoothstep(0.3, 0.8, height);
        finalColor = mix(baseSoil, rock * 0.4, blend);
    }
    
    float steepBlend = smoothstep(0.7, 0.85, steepness);
    finalColor = mix(finalColor, baseSoil, steepBlend);
    
    vec3 incident = normalize(lightPos - IN.worldPos);
    vec3 viewDir = normalize(cameraPos - IN.worldPos);
    vec3 halfDir = normalize(incident + viewDir);
    
    float lambert = max(0.0, dot(incident, IN.normal));
    float distance = length(lightPos - IN.worldPos);
    float attenuation = 1.0 - clamp(distance / lightRadius, 0.0, 1.0);
    
    float specPower = 32.0;
    float specFactor = pow(max(dot(halfDir, IN.normal), 0.0), specPower);
    
    float shadow = ShadowCalculation();
    
    vec3 surface = (finalColor.rgb * lightColour.rgb * lambert * attenuation * shadow) + (lightColour.rgb * specFactor * attenuation * shadow * 0.3);
    
    vec3 ambient = finalColor.rgb * ambientStrength;
    surface += ambient;

    if (beamActive) {
        float spotEffect = calculateSpotlight(IN.worldPos);
        surface += beamColor.rgb * spotEffect * (finalColor.rgb * 0.5);
    }
    
    fragColor = vec4(surface, 1.0);
}