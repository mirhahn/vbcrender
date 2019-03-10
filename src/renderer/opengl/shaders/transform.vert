// Uniforms
layout(shared) uniform TransformBlock {
    vec2 scale;
    vec2 translate;
};

#if SKIP_GEOM
layout(shared) uniform EdgeBlock {
    vec3 edge_color;
};
#endif

// Inputs
layout(location=0) in vec2 vertex;
layout(location=1) in uint style;

// Outputs
#if SKIP_GEOM
out ColorBlock {
    flat vec3 color;
};
#else
out NodeMetaBlock {
    uint node_style;
};
#endif

void main() {
    gl_Position.xy = scale * vertex + translate;
    gl_Position.z = 0.0;
    gl_Position.w = 1.0;
#if SKIP_GEOM
    color = edge_color;
#else
    node_style = style;
#endif
}
