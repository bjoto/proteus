#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;

uniform sampler2D texture0;
uniform sampler2D reflectionTexture;
uniform vec4 objectColor;
uniform mat4 reflectionViewProjection;
uniform vec3 cameraPosition;
uniform float time;
uniform int reflectionsEnabled;

out vec4 finalColor;

void main()
{
    vec3 base = texture(texture0, fragTexCoord).rgb * objectColor.rgb * fragColor.rgb;
    base *= vec3(0.20, 0.25, 0.48);
    if (reflectionsEnabled == 0) {
        finalColor = vec4(base, 1.0);
        return;
    }

    vec4 reflectedPosition = reflectionViewProjection * vec4(fragWorldPosition, 1.0);
    vec2 reflectionUv = reflectedPosition.xy / max(reflectedPosition.w, 0.0001) * 0.5 + 0.5;
    vec2 ripple = vec2(sin(fragWorldPosition.x * 0.09 + time * 0.80)
            + sin(fragWorldPosition.z * 0.05 - time * 0.55),
        sin(fragWorldPosition.z * 0.08 + time * 0.65)
            + sin(fragWorldPosition.x * 0.04 + time * 0.35)) * 0.0025;
    reflectionUv = clamp(reflectionUv + ripple, vec2(0.004), vec2(0.996));
    vec3 reflection = texture(reflectionTexture, reflectionUv).rgb;
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float viewDot = max(0.0, dot(normalize(fragWorldNormal), viewDirection));
    float reflectionAmount = mix(0.10, 0.42, (1.0 - viewDot) * (1.0 - viewDot));
    finalColor = vec4(mix(base, reflection * vec3(0.45, 0.56, 0.72), reflectionAmount), 1.0);
}
