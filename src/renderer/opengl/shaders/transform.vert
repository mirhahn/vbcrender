// Uniforms
layout(shared) uniform TransformBlock {
    vec2 scale;
    vec2 translate;
};

#if (VERTEX_SHADER_MODE == VSMODE_PASSTHROUGH_EDGE)
layout(shared) uniform EdgeBlock {
    vec3 edge_color;
};
#elif (VERTEX_SHADER_MODE == VSMODE_PASSTHROUGH_NODE)
layout(shared) uniform NodeBlock {
    uint shape_table[NUM_STYLES];
    vec3 color_table[NUM_STYLES];
};
#endif

// Inputs
layout(location=0) in vec2 vertex;
layout(location=1) in uint style;

// Outputs
#if (VERTEX_SHADER_MODE != VSMODE_MARKER_NODE)
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
#if (VERTEX_SHADER_MODE == VSMODE_MARKER_NODE) 
    node_style = style;
#elif (VERTEX_SHADER_MODE == VSMODE_PASSTHROUGH_EDGE)
    color = edge_color;
#elif (VERTEX_SHADER_MODE == VSMODE_PASSTHROUGH_NODE)
    color = color_table[style];
#else
#error "Unimplemented vertex shader mode"
#endif
}
