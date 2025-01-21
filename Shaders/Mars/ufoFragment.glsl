#version 330 core

uniform sampler2D diffuseTex;
uniform sampler2D shadowTex;
uniform vec3 lightPos;
uniform vec4 lightColour;
uniform vec3 cameraPos;

in Vertex {
    vec2 texCoord;
    vec3 normal;
    vec3 worldPos;
    vec4 shadowProj;
} IN;

out vec4 fragColor;

void main(void) {
    vec3 incident = normalize(lightPos - IN.worldPos);
    vec3 viewDir = normalize(cameraPos - IN.worldPos);
    vec3 halfDir = normalize(incident + viewDir);
    
    float lambert = max(0.0, dot(incident, IN.normal));
    float distance = length(lightPos - IN.worldPos);
    float attenuation = 1.0 - clamp(distance / 2000.0, 0.0, 1.0);
    
    float specFactor = pow(max(dot(halfDir, IN.normal), 0.0), 60.0);
    
    vec4 texCol = texture(diffuseTex, IN.texCoord);
    
    float shadow = 1.0;
    vec3 shadowNDC = IN.shadowProj.xyz / IN.shadowProj.w;
    if(abs(shadowNDC.x) <= 1.0 && abs(shadowNDC.y) <= 1.0) {
        vec2 shadowUV = shadowNDC.xy * 0.5 + 0.5;
        float shadowZ = shadowNDC.z * 0.5 + 0.5;
        float depth = texture(shadowTex, shadowUV).r;
        if(depth < shadowZ) {
            shadow = 0.2;
        }
    }
    
    float minAmbient = 0.3;
    vec3 ambient = texCol.rgb * minAmbient;
    

    vec3 surface = texCol.rgb * lightColour.rgb;
    fragColor.rgb = surface * lambert * attenuation * shadow;
    fragColor.rgb += ambient;
    fragColor.rgb += lightColour.rgb * specFactor * attenuation * shadow;
    
    // Emissive glow for lights in texture
    float luminance = dot(texCol.rgb, vec3(0.299, 0.587, 0.114));
    if(luminance > 0.7) {
        float emissive = 0.3;
        fragColor.rgb += texCol.rgb * emissive;
    }
    
    fragColor.a = texCol.a;
}