#version 330 core
uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float exposure;
uniform bool bloomEnabled;

in vec2 TexCoords;
out vec4 FragColor;

void main() {
    // Always sample the scene
    vec3 hdrColor = texture(scene, TexCoords).rgb;
    
    // Only add bloom if enabled
    if(bloomEnabled) {
        vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
        hdrColor += bloomColor;
    }
    
    vec3 result = vec3(1.0) - exp(-hdrColor * exposure);
    
    const float gamma = 2.2;
    result = pow(result, vec3(1.0 / gamma));
    
    FragColor = vec4(result, 1.0);
}