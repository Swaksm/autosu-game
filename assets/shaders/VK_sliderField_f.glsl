#version 450

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D tex0;

layout(std140, set = 3, binding = 0) uniform FragParams {
    vec4 channel;  // this body's channel; the MAX blend keeps the other bodies' fields in the others
    // what the field stores for a fragment at length(tex_coord) = d: the sum of three clamped linear pieces, piece i
    // rising by encodeRise[i] as d * encodeScale[i] + encodeBias[i] goes from 0 to 1
    vec3 encodeScale;
    float _pad0;
    vec3 encodeBias;
    float _pad1;
    vec3 encodeRise;
    float _pad2;
} fu;

void main() {
    // tex_coord is the fragment's offset from the nearest curve feature (segment or cap center), normalized
    // by the body radius, so 1.0 - length(tex_coord) is that feature's exact radial gradient (1 at the
    // centerline, 0 at the edge). the MAX blend equation unions overlapping primitives into the field of the
    // nearest feature; the sliderComposite shader then shades the accumulated field once per pixel.
    // the radial is stored encoded so that the 8-bit target spends more of its levels where the shading changes
    // fastest (0 past the edge); the encoding is non-decreasing, so the MAX blend still picks the nearest feature.
    float field = dot(clamp(vec3(length(tex_coord)) * fu.encodeScale + fu.encodeBias, 0.0, 1.0), fu.encodeRise);
    outColor = fu.channel * field;
}
