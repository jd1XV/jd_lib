#version 420 core

in vec3 fs_xyz;
in vec3 fs_uvw;
in vec4 fs_col;
in vec4 fs_rect;
in float fs_rounding;
in float fs_softness;
in float fs_thickness;

out vec4 out_col; 
uniform sampler2DArray tex;

// Implementation lifted from Ryan Fleury's UI tutorial
float RoundingSDF(vec2 sample_pos, vec2 rect_center, vec2 rect_half_size, float r) {
    vec2 d2 = (abs(rect_center - sample_pos) - rect_half_size + vec2(r, r));
    return min(max(d2.x, d2.y), 0.0) + length(max(d2, 0.0)) - r;
}

void main() {
    vec4 color = texture(tex, fs_uvw);
    
    float softness = fs_softness;
    vec2 softness_padding = vec2(max(0, softness * 2 - 1), max(0, softness * 2 - 1));
    
    vec2 p0 = vec2(fs_rect.x, fs_rect.z);
    vec2 p1 = vec2(fs_rect.y, fs_rect.w);
    
    vec2 rect_center = vec2(p1 + p0) / 2;
    vec2 rect_half_size = vec2(p1 - p0) / 2;
    
    float dist = RoundingSDF(fs_xyz.xy, rect_center, rect_half_size - softness_padding, fs_rounding);
    
    float border_factor = 1.0f;
    
    if (fs_thickness != 0) {
        vec2 interior_half_size = rect_half_size - vec2(fs_thickness);
        
        float interior_radius_reduce = min(interior_half_size.x / rect_half_size.x,
                                           interior_half_size.y / rect_half_size.y);
        float interior_corner_radius = (fs_rounding * interior_radius_reduce * interior_radius_reduce);
        
        float inside_d = RoundingSDF(fs_xyz.xy, rect_center, interior_half_size - softness_padding, interior_corner_radius);
        
        // map distance => factor
        float inside_f = smoothstep(0, 2 * softness, inside_d);
        border_factor = inside_f;
    }
    
    // map distance => a blend factor
    float sdf_factor = 1.0f - smoothstep(0, 2 * softness, dist);
    
    out_col = fs_col * color * sdf_factor * border_factor;
}