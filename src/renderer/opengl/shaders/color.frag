// Inputs
in ColorBlock {
    flat vec3 color;
};

// Outputs
layout(location=0) out vec3 fragment_color;

void main() {
    fragment_color = color;
}
