#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform int physicalWaves;
uniform float wakeAmplitude;
uniform vec2 wakeOffset; // World X/Z translation of the scripted source.
uniform int playerWakeCount;
uniform vec4 playerWakeOrigins[64];
uniform vec4 playerWakeParameters[64];
uniform vec4 waveParameters[3]; // World X/Z wave vector, amplitude, angular speed.
uniform float time;
uniform int waterPatchPass; // 0: legacy, 1: planar far field, 2: clipped near patch.
uniform vec4 wavePatch; // Boat world X/Z, full displacement radius, zero displacement radius.

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;


// Same bounded packet and derivatives as boat_water.c, in reflected world X/Z.
vec3 crossingWake(vec2 world, float clock)
{
    world -= wakeOffset;
    float emitted = (-world.x + 24.0) / 4.0;
    float age = clock - emitted;
    if (wakeAmplitude == 0.0 || emitted <= 0.0 || emitted >= 12.0 || age <= 0.0 || age >= 12.0)
        return vec3(0.0);
    float radius = sqrt((world.y + 8.0) * (world.y + 8.0) + 0.25);
    float q = radius - 2.4 * age;
    if (abs(q) >= 5.0) return vec3(0.0);
    float pi = 3.14159265359, k = 2.0 * pi / 6.0;
    float edge = 1.0 - q * q / 25.0;
    float f = sin(k * q) * edge * edge * edge;
    float df = k * cos(k * q) * edge * edge * edge
        - sin(k * q) * 6.0 * q / 25.0 * edge * edge;
    float window = pow(sin(pi * emitted / 12.0), 2.0);
    float envelope = pow(sin(pi * age / 12.0), 2.0);
    float dw = pi / 12.0 * sin(2.0 * pi * emitted / 12.0);
    float de = pi / 12.0 * sin(2.0 * pi * age / 12.0);
    return wakeAmplitude * vec3(window * envelope * f,
        -((dw * envelope - window * de) * f + window * envelope * df * 2.4) / 4.0,
        window * envelope * df * (world.y + 8.0) / radius);
}


// Same owned path history and analytic derivatives as boat_water.c. Each entry is
// two vec4s: world centre/direction, support half-length/birth/amplitude/inverse speed.
vec3 playerWake(vec2 p, float clock)
{
    float sum = 0.0, weight = 0.0;
    vec2 sumG = vec2(0.0), weightG = vec2(0.0);
    for (int i = 0; i < playerWakeCount; ++i) {
        vec4 a = playerWakeOrigins[i], b = playerWakeParameters[i];
        vec2 delta = p - a.xy, direction = a.zw;
        vec2 normal = vec2(-direction.y, direction.x);
        float u = dot(delta, direction);
        if (abs(u) >= b.x) continue;
        float age = clock - b.y - u * b.w;
        if (age <= 0.4 || age >= 20.0) continue;
        float v = dot(delta, normal), radius = sqrt(v * v + 0.25);
        float q = radius - 0.9 - 1.6 * age;
        if (abs(q) >= 4.5) continue;
        float l = 1.0 - u * u / (b.x * b.x), r = 1.0 - q * q / 20.25;
        float wl = l * l * l, wq = r * r * r;
        float dl = -6.0 * u / (b.x * b.x) * l * l, dq = -6.0 * q / 20.25 * r * r;
        vec2 ageG = -direction * b.w;
        vec2 qG = normal * (v / radius) - 1.6 * ageG;
        float w = wl * wq;
        vec2 wg = direction * (dl * wq) + qG * (wl * dq);
        float riseT = clamp((age - 0.4), 0.0, 1.0), fallT = clamp((age - 14.0) / 6.0, 0.0, 1.0);
        float rise = riseT * riseT * (3.0 - 2.0 * riseT);
        float fall = 1.0 - fallT * fallT * (3.0 - 2.0 * fallT);
        float de = 6.0 * riseT * (1.0 - riseT) * fall - rise * fallT * (1.0 - fallT);
        float envelope = rise * fall, k = 6.28318530718 / 6.0;
        // Phase speed 3 m/s; the packet envelope spreads at 1.6 m/s.
        float phase = q - 1.4 * age;
        vec2 phaseG = qG - 1.4 * ageG;
        float wave = sin(k * phase), slope = k * cos(k * phase);
        float value = b.z * envelope * wave;
        vec2 valueG = b.z * (ageG * (de * wave) + phaseG * (envelope * slope));
        sum += w * value;
        sumG += wg * value + valueG * w;
        weight += w;
        weightG += wg;
    }
    float divisor = sqrt(1.0 + weight * weight);
    return vec3(sum, sumG - sum * weight / (1.0 + weight * weight) * weightG) / divisor;
}

void main()
{
    vec3 position = vertexPosition;
    if (physicalWaves != 0 && waterPatchPass != 1) {
        position.y = 0.0;
        for (int i = 0; i < 3; ++i) {
            vec4 w = waveParameters[i];
            position.y += w.z * sin(dot(w.xy, position.xz) - w.w * time);
        }
        position.y += crossingWake(position.xz, time).x + playerWake(position.xz, time).x;
        if (waterPatchPass == 2)
            position.y *= 1.0 - smoothstep(wavePatch.z, wavePatch.w,
                length(position.xz - wavePatch.xy));
    }
    vec4 worldPosition = matModel * vec4(position, 1.0);
    fragTexCoord = vertexTexCoord;
    // Both representations use the same boating albedo. The legacy city water is brighter.
    fragColor = physicalWaves != 0 && waterPatchPass != 0
        ? vec4(45.0, 79.0, 104.0, 255.0) / 255.0 : vertexColor;
    fragWorldPosition = worldPosition.xyz;
    fragWorldNormal = physicalWaves != 0 ? vec3(0.0, 1.0, 0.0)
        : normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(position, 1.0);
}
