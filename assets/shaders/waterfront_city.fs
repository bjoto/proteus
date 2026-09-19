#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;

uniform sampler2D texture0;
uniform vec4 objectColor;
uniform int emissive;
uniform int groundPass;
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

// World-space, metre-scaled ground finishes. No bitmap tiling, painted windows,
// or camera-relative noise. Subpixel detail fades to its mean in both scene passes.
float ground_hash(vec2 cell)
{
    vec3 p = fract(vec3(cell.xyx) * vec3(.1031, .1030, .0973));
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float ground_noise(vec2 p)
{
    vec2 cell = floor(p), t = fract(p);
    t = t * t * (3.0 - 2.0 * t);
    return mix(mix(ground_hash(cell), ground_hash(cell + vec2(1, 0)), t.x),
        mix(ground_hash(cell + vec2(0, 1)), ground_hash(cell + vec2(1, 1)), t.x), t.y) - .5;
}

float ground_detail(vec2 p, float scale, float pixelWidth)
{
    float visible = 1.0 - smoothstep(scale * .25, scale, pixelWidth);
    return ground_noise(p / scale) * visible;
}

float joint_integral(float x, float coverage)
{
    return floor(x) * coverage + min(fract(x), coverage);
}

float ground_joint(float position, float period, float width, float pixelWidth)
{
    float coverage = width / period;
    float x = position / period + coverage * .5;
    float footprint = max(pixelWidth / period, .001);
    // Integrate the periodic seam over the pixel; do not inflate a 2 cm joint into
    // a full dark pixel. Fully unresolved patterns converge to average coverage.
    float line = (joint_integral(x + footprint * .5, coverage)
        - joint_integral(x - footprint * .5, coverage)) / footprint;
    return mix(clamp(line, 0.0, 1.0), coverage, smoothstep(.3, 1.0, footprint));
}

vec3 ground_albedo()
{
    vec2 p = fragWorldPosition.xz;
    vec2 footprint = max(abs(dFdx(p)), abs(dFdy(p)));
    float pixelWidth = max(footprint.x, footprint.y);
    int kind = int(fragTexCoord.x + .5); // WaterfrontGroundMaterial, constant per triangle.
    float weather = ground_detail(p, 7.0, pixelWidth);
    if (kind == 1) { // Asphalt: quiet aggregate and broad wear, no paving grid.
        float aggregate = ground_detail(p, .16, pixelWidth);
        float wear = ground_detail(p + vec2(19, 41), 1.7, pixelWidth);
        return fragColor.rgb * (.91 + .09 * weather + .07 * wear + .10 * aggregate);
    }
    if (kind == 4) { // Planted beds: irregular foliage/soil colour, not concrete slabs.
        float clumps = ground_detail(p, .65, pixelWidth);
        float patches = ground_detail(p + vec2(7, 13), 2.4, pixelWidth);
        return fragColor.rgb * (.97 + .20 * clumps + .14 * patches);
    }
    if (kind == 2) { // 80 x 40 cm staggered paving; two-centimetre joints.
        float row = floor(p.y / .4);
        float x = p.x + mod(row, 2.0) * .4;
        float joint = max(ground_joint(x, .8, .02, footprint.x),
            ground_joint(p.y, .4, .02, footprint.y));
        float tile = ground_hash(vec2(floor(x / .8), row)) - .5;
        tile *= 1.0 - smoothstep(.08, .3, pixelWidth);
        return fragColor.rgb * (.97 + .07 * weather + .08 * tile - .16 * joint);
    }
    // Quay/open concrete: 3 m construction panels, restrained seams and weathering.
    float joint = max(ground_joint(p.x, 3.0, .025, footprint.x),
        ground_joint(p.y, 3.0, .025, footprint.y));
    float grain = ground_detail(p, .3, pixelWidth);
    return fragColor.rgb * (.95 + .12 * weather + .05 * grain - .12 * joint);
}

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
    if (groundPass != 0)
        albedo = vec4(ground_albedo(), fragColor.a) * objectColor;
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
