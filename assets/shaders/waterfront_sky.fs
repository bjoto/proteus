#version 330

in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec3 skyForward;
uniform vec3 skyRight;
uniform vec3 skyUp;
uniform vec2 skyScale;
out vec4 finalColor;

void main()
{
    vec2 plane = (fragTexCoord * 2.0 - 1.0) * skyScale;
    vec3 ray = normalize(skyForward + plane.x * skyRight - plane.y * skyUp);
    vec2 uv = vec2(fract(atan(ray.x, ray.z) / 6.28318530718 + 0.5),
        clamp((1.0 - ray.y) * 0.5, 0.0, 1.0));
    finalColor = texture(texture0, uv);
}
