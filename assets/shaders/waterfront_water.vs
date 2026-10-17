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

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;

void main()
{
    vec3 position = vertexPosition;
    if (physicalWaves != 0) {
        position.y = 0.0;
        for (int i = 0; i < 3; ++i) {
            vec4 w = waveParameters[i];
            position.y += w.z * sin(dot(w.xy, position.xz) - w.w * time);
        }
    }
    vec4 worldPosition = matModel * vec4(position, 1.0);
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragWorldPosition = worldPosition.xyz;
    fragWorldNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(position, 1.0);
}
