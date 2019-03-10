layout(points) in;
#if CLOSE_LOOP
layout(PRIM_TYPE, max_vertices=NUM_VERT_LOOP) out;
#else
layout(PRIM_TYPE, max_vertices=NUM_VERT) out;
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
    vec2 rel_pos[NUM_SHAPES * NUM_VERT];
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
    uint base_idx = NUM_VERT * shape_idx;

    vec2 input_pos = gl_in[0].gl_Position.xy;
    color = color_table[meta[0].node_style];
    gl_Position.zw = gl_in[0].gl_Position.zw;

#if CLOSE_LOOP
    vec2 start_pos = input_pos + scale * shapes.rel_pos[base_idx];
    gl_Position.xy = start_pos;
    EmitVertex();
    for(uint idx = base_idx + 1u; idx < base_idx + num_vert; ++idx) {
        gl_Position.xy = input_pos + scale * shapes.rel_pos[idx];
        EmitVertex();
    }
    gl_Position.xy = start_pos;
    EmitVertex();
#else
    for(uint idx = base_idx; idx < base_idx + num_vert; ++idx) {
        gl_Position.xy = input_pos + scale * shapes.rel_pos[idx];
        EmitVertex();
    }
#endif
    EndPrimitive();
}
