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
uniform int physicalWaves;
uniform vec4 waveParameters[3];
uniform int reflectionsEnabled;
uniform int shadowsEnabled;
uniform sampler2D shadowMap;
uniform mat4 lightViewProjection;

out vec4 finalColor;

float waterShadow()
{
    if (shadowsEnabled == 0) return 1.0;
    vec4 lightPosition = lightViewProjection * vec4(fragWorldPosition, 1.0);
    vec3 p = lightPosition.xyz / lightPosition.w * 0.5 + 0.5;
    if (any(lessThan(p, vec3(0.0))) || any(greaterThan(p, vec3(1.0)))) return 1.0;
    return p.z - 0.00018 > texture(shadowMap, p.xy).r ? 0.55 : 1.0;
}

void main()
{
    vec3 base = texture(texture0, fragTexCoord).rgb * objectColor.rgb * fragColor.rgb;
    vec3 normal = normalize(fragWorldNormal);
    if (physicalWaves != 0) {
        vec2 slope = vec2(0.0);
        for (int i = 0; i < 3; ++i) {
            vec4 w = waveParameters[i];
            slope += w.xy * w.z * cos(dot(w.xy, fragWorldPosition.xz) - w.w * time);
        }
        normal = normalize(vec3(-slope.x, 1.0, -slope.y));
        base *= vec3(0.55, 0.65, 0.78) * (0.95 + 0.35 * dot(normal.xz, vec2(0.7)));
    } else {
        base *= vec3(0.20, 0.25, 0.48);
    }
    if (reflectionsEnabled == 0) {
        finalColor = vec4(base * waterShadow(), 1.0);
        return;
    }

    vec4 reflectedPosition = reflectionViewProjection * vec4(fragWorldPosition, 1.0);
    vec2 reflectionUv = reflectedPosition.xy / max(reflectedPosition.w, 0.0001) * 0.5 + 0.5;
    vec2 ripple = vec2(sin(fragWorldPosition.x * 0.09 + time * 0.80)
            + sin(fragWorldPosition.z * 0.05 - time * 0.55),
        sin(fragWorldPosition.z * 0.08 + time * 0.65)
            + sin(fragWorldPosition.x * 0.04 + time * 0.35)) * 0.0025;
    if (physicalWaves != 0) ripple = normal.xz * 0.08;
    reflectionUv += ripple;
    vec2 edge = min(reflectionUv, vec2(1.0) - reflectionUv);
    float coverage = smoothstep(0.0, 0.025, min(edge.x, edge.y));
    if (reflectedPosition.w <= 0.0) coverage = 0.0;
    reflectionUv = clamp(reflectionUv, vec2(0.004), vec2(0.996));
    vec3 reflection = texture(reflectionTexture, reflectionUv).rgb;
    vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
    float viewDot = max(0.0, dot(normal, viewDirection));
    float reflectionAmount = mix(0.10, physicalWaves != 0 ? 0.30 : 0.42,
        (1.0 - viewDot) * (1.0 - viewDot));
    vec3 color = mix(base, reflection * vec3(0.65, 0.62, 0.72), reflectionAmount * coverage);
    if (physicalWaves != 0) {
        vec3 halfVector = normalize(viewDirection + normalize(vec3(-0.4, 0.85, -0.35)));
        color += vec3(0.12, 0.14, 0.16) * pow(max(dot(normal, halfVector), 0.0), 96.0);
    }
    finalColor = vec4(color * waterShadow(), 1.0);
}
