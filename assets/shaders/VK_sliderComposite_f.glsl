#version 450

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D tex0;

layout(std140, set = 3, binding = 0) uniform FragParams {
    int style;
    float bodyColorSaturation;
    float bodyAlphaMultiplier;
    float borderSizeMultiplier;
    float alphaMultiplier;
    float _pad0; float _pad1; float _pad2;
    vec3 colBorder;
    float _pad3;
    vec3 colBody;
    float _pad4;
    vec4 channel;  // which of tex0's channels holds this slider's field
    // the stored field value where the shadow, shadow -> border transition, border and border -> body transition end
    vec4 regionEnds;
    // the shadow, shadow -> border, border -> body and body ramps' position, field * rampScale + rampBias
    vec4 rampScale;
    vec4 rampBias;
} fu;

vec4 getInnerBodyColor(in vec4 bodyColor) {
    float brightnessMultiplier = 0.25;
    bodyColor.r = min(1.0, bodyColor.r * (1.0 + 0.5 * brightnessMultiplier) + brightnessMultiplier);
    bodyColor.g = min(1.0, bodyColor.g * (1.0 + 0.5 * brightnessMultiplier) + brightnessMultiplier);
    bodyColor.b = min(1.0, bodyColor.b * (1.0 + 0.5 * brightnessMultiplier) + brightnessMultiplier);
    return vec4(bodyColor);
}

vec4 getOuterBodyColor(in vec4 bodyColor) {
    float darknessMultiplier = 0.1;
    bodyColor.r = min(1.0, bodyColor.r / (1.0 + darknessMultiplier));
    bodyColor.g = min(1.0, bodyColor.g / (1.0 + darknessMultiplier));
    bodyColor.b = min(1.0, bodyColor.b / (1.0 + darknessMultiplier));
    return vec4(bodyColor);
}

void main() {
    // tex0 holds the slider's accumulated distance field (see the sliderField shader): the MAX-blend union of
    // every primitive's radial gradient, i.e. exactly the radial the old one-pass shaders computed per
    // primitive, but resolved to the nearest curve feature. shading it here runs the gradient once per pixel
    // no matter how much the body geometry self-overlaps, and radial ramping to 0 keeps the silhouette
    // analytically round at any tessellation density.
    // the field is stored encoded (see SliderRenderer.cpp), linear within each region below
    float field = dot(texture(tex0, tex_coord), fu.channel);
    vec4 delta = field * fu.rampScale + fu.rampBias;

    // output var
    vec4 out_color = vec4(0.0);

    // dynamic color calculations
    vec4 borderColor = vec4(fu.colBorder.x, fu.colBorder.y, fu.colBorder.z, 1.0);
    vec4 bodyColor = vec4(fu.colBody.x, fu.colBody.y, fu.colBody.z, 0.7 * fu.bodyAlphaMultiplier);
    vec4 outerShadowColor = vec4(0, 0, 0, 0.25);
    vec4 innerBodyColor = getInnerBodyColor(bodyColor);
    vec4 outerBodyColor = getOuterBodyColor(bodyColor);

    innerBodyColor.rgb *= fu.bodyColorSaturation;
    outerBodyColor.rgb *= fu.bodyColorSaturation;

    // osu!next style color modifications
    if (fu.style == 1) {
        outerBodyColor.rgb = bodyColor.rgb * fu.bodyColorSaturation;
        outerBodyColor.a = 1.0 * fu.bodyAlphaMultiplier;
        innerBodyColor.rgb = bodyColor.rgb * 0.5 * fu.bodyColorSaturation;
        innerBodyColor.a = 0.0;
    }

    // a bit of a hack, but better than rough edges
    if (fu.borderSizeMultiplier < 0.01)
        borderColor = outerShadowColor;

    // conditional variant

    if (field < fu.regionEnds.x) // just shadow
        out_color = mix(vec4(0), outerShadowColor, delta.x);
    else if (field < fu.regionEnds.y) // shadow + border
        out_color = mix(outerShadowColor, borderColor, delta.y);
    else if (field < fu.regionEnds.z) // just border
        out_color = borderColor;
    else if (field < fu.regionEnds.w) // border + outer body
        out_color = mix(borderColor, outerBodyColor, delta.z);
    else // outer body + inner body
        out_color = mix(outerBodyColor, innerBodyColor, delta.w);

    // the slider's overall fade percent
    out_color.a *= fu.alphaMultiplier;

    outColor = out_color;
}
