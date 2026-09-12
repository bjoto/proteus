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
uniform int localLightCount;
uniform vec4 localLightPositionRadius[8];
uniform vec4 localLightColorIntensity[8];
uniform vec4 localLightDirection[8];
uniform vec4 localLightCone[8];

out vec4 finalColor;

void main()
{
    vec4 albedo = texture(texture0, fragTexCoord) * objectColor * fragColor;
    if (emissive != 0) {
        finalColor = albedo;
        return;
    }

    vec3 normal = normalize(fragWorldNormal);
    vec3 illumination = ambientColor
        + directionalColor * directionalIntensity
            * max(0.0, dot(normal, normalize(directionalDirection)));
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

    // Six levels retain hue and leave emissive colors untouched. Clamp illumination before
    // multiplying the albedo, rather than adding white, so nearby lamps do not bleach materials.
    vec3 stepped = floor(clamp(illumination, 0.0, 1.25) * 6.0 + 0.5) / 6.0;
    finalColor = vec4(min(albedo.rgb * stepped, vec3(1.0)), albedo.a);
}
