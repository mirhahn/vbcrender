layout(points) in;

#if (GEOMETRY_SHADER_MODE == GSMODE_MARKER_FILL)
layout(triangle_strip, max_vertices=NUM_VERTICES) out;
#elif (GEOMETRY_SHADER_MODE == GSMODE_MARKER_STROKE)
layout(line_strip, max_vertices=NUM_LOOP_VERTICES) out;
#else
#error "Unimplemented geometry shader mode"
#endif

// Uniforms
layout(shared) uniform TransformBlock {
    vec2 scale;
    vec2 translate;
};

layout(shared) uniform NodeBlock {
    uint shape_table[NUM_STYLES];
    vec3 color_table[NUM_STYLES];
};

layout(shared) uniform ShapeBlock {
    uint num_vert[NUM_SHAPES];
    vec2 rel_pos[NUM_SHAPES * NUM_VERTICES];
} shapes;

// Inputs
in NodeMetaBlock {
    uint node_style;
} meta[];

// Outputs
out ColorBlock {
    flat vec3 color;
};


void main() {
    uint shape_idx = shape_table[meta[0].node_style];
    uint num_vert = shapes.num_vert[shape_idx];
    uint base_idx = NUM_VERTICES * shape_idx;

    vec2 input_pos = gl_in[0].gl_Position.xy;
    color = color_table[meta[0].node_style];
    gl_Position.zw = gl_in[0].gl_Position.zw;

#if (GEOMETRY_SHADER_MODE == GSMODE_MARKER_FILL)
    for(uint idx = base_idx; idx < base_idx + num_vert; ++idx) {
        gl_Position.xy = input_pos + scale * shapes.rel_pos[idx];
        EmitVertex();
    }
    EndPrimitive();
#elif (GEOMETRY_SHADER_MODE == GSMODE_MARKER_STROKE)
    vec2 start_pos = input_pos + scale * shapes.rel_pos[base_idx];
    gl_Position.xy = start_pos;
    EmitVertex();
    for(uint idx = base_idx + 1u; idx < base_idx + num_vert; ++idx) {
        gl_Position.xy = input_pos + scale * shapes.rel_pos[idx];
        EmitVertex();
    }
    gl_Position.xy = start_pos;
    EmitVertex();
    EndPrimitive();
#else
#error "Unimplemented geometry shader mode"
#endif
}
