#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform bool bloom;
uniform float exposure;
uniform float bloomThreshold;

void main()
{
    const float gamma = 0.96;
    vec3 hdrColor = texture(scene, TexCoords).rgb;
    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
    if(bloom){

       float brightness = dot(hdrColor, vec3(0.2126, 0.7152, 0.0722));
       if(brightness > bloomThreshold)
            bloomColor += vec3(1.0, 0.0, 0.0);
        hdrColor += bloomColor;
        vec3 result = vec3(1.0) - exp(-hdrColor * exposure);
        result = pow(result, vec3(1.0 / gamma));
    FragColor = vec4(result, 1.0);
    }else{
    vec3 result = pow(hdrColor, vec3(1.0/gamma));
    FragColor = vec4(result,1.0);
    }
}