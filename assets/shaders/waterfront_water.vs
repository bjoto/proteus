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

void main()
{
    vec3 position = vertexPosition;
    if (physicalWaves != 0 && waterPatchPass != 1) {
        position.y = 0.0;
        for (int i = 0; i < 3; ++i) {
            vec4 w = waveParameters[i];
            position.y += w.z * sin(dot(w.xy, position.xz) - w.w * time);
        }
        position.y += crossingWake(position.xz, time).x;
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
