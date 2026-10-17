#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform int physicalWaves;
uniform vec4 waveParameters[3]; // World X/Z wave vector, amplitude, angular speed.
uniform float time;
uniform int waterPatchPass; // 0: legacy, 1: planar far field, 2: clipped near patch.
uniform vec4 wavePatch; // Boat world X/Z, full displacement radius, zero displacement radius.

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;

void main()
{
    vec3 position = vertexPosition;
    if (physicalWaves != 0 && waterPatchPass != 1) {
        position.y = 0.0;
        for (int i = 0; i < 3; ++i) {
            vec4 w = waveParameters[i];
            position.y += w.z * sin(dot(w.xy, position.xz) - w.w * time);
        }
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
