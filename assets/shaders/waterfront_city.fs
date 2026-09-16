#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;

uniform sampler2D texture0;
uniform vec4 objectColor;
uniform int emissive;
uniform vec3 ambientColor;
uniform vec3 directionalColor;
uniform vec3 directionalDirection;
uniform float directionalIntensity;
uniform mat4 lightViewProjection;
uniform sampler2D shadowMap;
uniform int shadowsEnabled;
uniform vec3 cameraPosition;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform float fogOpacity;
uniform int atmosphereEnabled;
uniform int localLightCount;
uniform vec4 localLightPositionRadius[32];
uniform vec4 localLightColorIntensity[32];
uniform vec4 localLightDirection[32];
uniform vec4 localLightCone[32];

out vec4 finalColor;

vec3 apply_atmosphere(vec3 color)
{
    if (atmosphereEnabled == 0)
        return color;

    float distanceToCamera = length(fragWorldPosition - cameraPosition);
    float fog = smoothstep(fogStart, fogEnd, distanceToCamera) * fogOpacity;
    return mix(color, fogColor, fog);
}

float directional_shadow(vec3 normal)
{
    if (shadowsEnabled == 0)
        return 1.0;

    vec4 lightSpace = lightViewProjection * vec4(fragWorldPosition, 1.0);
    vec3 projected = lightSpace.xyz / max(lightSpace.w, 0.0001);
    projected = projected * 0.5 + 0.5;
    if (projected.x <= 0.0 || projected.x >= 1.0 || projected.y <= 0.0 || projected.y >= 1.0
        || projected.z <= 0.0 || projected.z >= 1.0)
        return 1.0;

    float incidence = max(0.0, dot(normal, normalize(directionalDirection)));
    float bias = max(0.0012 * (1.0 - incidence), 0.00018);
    // The city uses a coarse six-level ramp. A partial multiplier commonly quantizes to the same
    // level as lit surfaces, so fully remove direct fill here; ambient and local/emissive light
    // remain visible below.
    return projected.z - bias > texture(shadowMap, projected.xy).r ? 0.0 : 1.0;
}

void main()
{
    vec4 albedo = texture(texture0, fragTexCoord) * objectColor * fragColor;
    if (emissive != 0) {
        finalColor = vec4(apply_atmosphere(albedo.rgb), albedo.a);
        return;
    }

    vec3 normal = normalize(fragWorldNormal);
    float directional = max(0.0, dot(normal, normalize(directionalDirection)));
    vec3 illumination = ambientColor
        + directionalColor * directionalIntensity * directional * directional_shadow(normal);
    for (int index = 0; index < localLightCount; ++index) {
        vec3 delta = localLightPositionRadius[index].xyz - fragWorldPosition;
        float radius = localLightPositionRadius[index].w;
        float distance = length(delta);
        if (distance >= radius)
            continue;
        vec3 towardLight = delta / max(distance, 0.001);
        float incidence = max(0.0, dot(normal, towardLight));
        float attenuation = 1.0 - distance / radius;
        float spot = 1.0;
        float outerCone = localLightCone[index].y;
        if (outerCone >= 0.0) {
            float cone = dot(normalize(localLightDirection[index].xyz), -towardLight);
            float innerCone = localLightCone[index].x;
            spot = smoothstep(outerCone, innerCone, cone);
        }
        illumination += localLightColorIntensity[index].rgb
            * (localLightColorIntensity[index].a * incidence * attenuation * attenuation * spot);
    }

    // Preserve the full output palette while retaining the low-resolution framebuffer. Clamp
    // illumination before multiplying albedo, rather than adding white, so nearby lamps do not
    // bleach materials.
    vec3 lit = clamp(illumination, 0.0, 1.25);
    finalColor = vec4(apply_atmosphere(min(albedo.rgb * lit, vec3(1.0))), albedo.a);
}
