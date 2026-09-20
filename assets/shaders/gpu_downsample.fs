#version 330

in vec2 fragTexCoord;
uniform sampler2D texture0;
out vec4 finalColor;

void main()
{
    // Average every source texel in this output pixel's 4x4 footprint. A single
    // bilinear lookup would only include the middle four samples.
    ivec2 size = textureSize(texture0, 0);
    ivec2 block = clamp(ivec2(fragTexCoord * vec2(size / 4)), ivec2(0), size / 4 - 1) * 4;
    vec4 sum = vec4(0.0);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            sum += texelFetch(texture0, block + ivec2(x, y), 0);
    finalColor = sum / 16.0;
}
