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
uniform vec3 sunDirection;
uniform int physicalWaves;
uniform vec4 waveParameters[3];
uniform int reflectionsEnabled;
uniform int shadowsEnabled;
uniform sampler2D shadowMap;
uniform mat4 lightViewProjection;
uniform int waterPatchPass;
uniform vec4 wavePatch;
uniform int waterfrontWater; // Only playable Eriksberg; leave timeline/basin materials alone.
uniform float surfacePixelScale;

out vec4 finalColor;

float waterShadow()
{
    if (shadowsEnabled == 0) return 1.0;
    vec4 lightPosition = lightViewProjection * vec4(fragWorldPosition, 1.0);
    vec3 p = lightPosition.xyz / lightPosition.w * 0.5 + 0.5;
    if (any(lessThan(p, vec3(0.0))) || any(greaterThan(p, vec3(1.0)))) return 1.0;
    return p.z - 0.00018 > texture(shadowMap, p.xy).r ? 0.55 : 1.0;
}

// Authored shelter around the narrow dock. It opens gradually across the mouth;
// water elsewhere on the river remains exposed. This field changes shading only.
float exposure(vec2 p)
{
    float dockAxis = 12.0 - 0.09 * p.y;
    float inDock = 1.0 - smoothstep(28.0, 100.0, abs(p.x - dockAxis));
    float northOfMouth = smoothstep(-190.0, -55.0, p.y);
    return 1.0 - 0.82 * inDock * northOfMouth;
}

float resolved(float frequency, float footprint)
{
    return 1.0 - smoothstep(0.65, 2.5, frequency * footprint);
}

void main()
{
    float footprint = max(length(dFdx(fragWorldPosition.xz)),
        length(dFdy(fragWorldPosition.xz))) * surfacePixelScale;
    float boatDistance = length(fragWorldPosition.xz - wavePatch.xy);
    if (waterPatchPass == 1 && boatDistance < wavePatch.w) discard;
    if (waterPatchPass == 2 && boatDistance >= wavePatch.w) discard;
    vec3 base = texture(texture0, fragTexCoord).rgb * objectColor.rgb * fragColor.rgb;
    vec3 normal = normalize(fragWorldNormal);
    float exposed = exposure(fragWorldPosition.xz);
    if (physicalWaves != 0) {
        vec2 slope = vec2(0.0);
        for (int i = 0; i < 3; ++i) {
            vec4 w = waveParameters[i];
            float filtering = waterfrontWater != 0 ? resolved(length(w.xy), footprint) : 1.0;
            slope += w.xy * w.z * cos(dot(w.xy, fragWorldPosition.xz) - w.w * time) * filtering;
        }
        if (waterfrontWater != 0) slope *= mix(0.55, 1.0, exposed);
        normal = normalize(vec3(-slope.x, 1.0, -slope.y));
        base *= vec3(0.78, 0.94, 1.0) * (0.95 + 0.35 * dot(normal.xz, vec2(0.7)));
    } else {
        base *= vec3(0.72, 0.90, 1.0);
    }
    if (waterfrontWater != 0) {
        vec2 wind = vec2(-0.6, -0.8), crossing = vec2(0.8, -0.6);
        float longRipple = dot(wind, fragWorldPosition.xz) * 1.65 - time * 1.5;
        float shortRipple = dot(crossing, fragWorldPosition.xz) * 3.7 - time * 2.1;
        vec2 detail = wind * cos(longRipple) * resolved(1.65, footprint)
            + crossing * cos(shortRipple) * 0.45 * resolved(3.7, footprint);
        vec2 slope = normal.xz + detail * mix(0.004, 0.035, exposed);
        normal = normalize(vec3(slope.x, normal.y, slope.y));
        base *= mix(vec3(0.97, 1.01, 1.0), vec3(1.04, 1.07, 0.98), exposed);
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
    if (waterfrontWater != 0 && physicalWaves == 0)
        ripple = normal.xz * 0.06 + ripple * mix(0.25, 0.75, exposed);
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
    if (waterfrontWater != 0)
        reflectionAmount = mix(0.13, mix(0.38, 0.25, exposed),
            (1.0 - viewDot) * (1.0 - viewDot));
    vec3 color = mix(base, reflection * vec3(0.96, 0.99, 1.0), reflectionAmount * coverage);
    if (physicalWaves != 0) {
        vec3 halfVector = normalize(viewDirection + normalize(sunDirection));
        vec3 highlight = waterfrontWater != 0 ? vec3(0.09, 0.10, 0.085) * mix(0.25, 1.0, exposed)
            : vec3(0.28, 0.25, 0.17);
        color += highlight * pow(max(dot(normal, halfVector), 0.0), 96.0);
    }
    finalColor = vec4(color * waterShadow(), 1.0);
}
