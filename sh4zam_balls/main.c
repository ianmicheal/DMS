#include <kos.h>
#include <sh4zam/shz_sh4zam.h>

#define mat_trans_nodiv_nomod(x, y, z, x2, y2, z2, w2) do { \
        register float __x __asm__("fr12") = (x); \
        register float __y __asm__("fr13") = (y); \
        register float __z __asm__("fr14") = (z); \
        register float __w __asm__("fr15") = 1.0f; \
        __asm__ __volatile__( "ftrv  xmtrx, fv12\n" \
                              : "=f" (__x), "=f" (__y), "=f" (__z), "=f" (__w) \
                              : "0" (__x), "1" (__y), "2" (__z), "3" (__w) ); \
        x2 = __x; y2 = __y; z2 = __z; w2 = __w; \
    } while(false)


shz_mat4x4_t stored_projection_view = {0};

#define NUM_BALLS 94
#define SPHERE_STACKS 20
#define SPHERE_SLICES 20

//  32-byte  aligned 
typedef struct {
    float x, y, z;
    float rx, ry;
    float drx, dry;
    float _pad[1];  // Pad to 32 bytes 
} BallInfo __attribute__((aligned(32)));

// Cached polygon header - compute once
static pvr_poly_hdr_t cached_poly_header;
static bool header_initialized = false;

void initCachedPolyHeader() {
    if (!header_initialized) {
        pvr_poly_cxt_t cxt;
        pvr_poly_cxt_col(&cxt, PVR_LIST_OP_POLY);
        pvr_poly_compile(&cached_poly_header, &cxt);
        header_initialized = true;
    }
}

void submitCachedHeader(pvr_dr_state_t* dr_state) {
    pvr_poly_hdr_t *hdr = (pvr_poly_hdr_t*)pvr_dr_target(*dr_state);
    *hdr = cached_poly_header;
    pvr_dr_commit(hdr);
}

void setup_projection_view() {
    float screen_width = 640.0f;
    float screen_height = 480.0f;
    float near_z = 1.0f;
    float fov = 60.0f * SHZ_F_PI / 180.0f;   
    float aspect = screen_width / screen_height;
    
    shz_xmtrx_init_identity();
    shz_xmtrx_apply_screen(screen_width, screen_height);
    shz_xmtrx_apply_perspective(fov, aspect, near_z);
    shz_xmtrx_store_4x4(&stored_projection_view);
}

void setupModelMatrix(float x, float y, float z, float rx, float ry) {
    shz_xmtrx_init_translation(x, y, -z);
    shz_xmtrx_apply_rotation_x(rx);
    shz_xmtrx_apply_rotation_y(ry);
    
    shz_mat4x4_t temp_model;
    shz_xmtrx_store_4x4(&temp_model);
    
    shz_xmtrx_load_4x4(&stored_projection_view);
    shz_xmtrx_apply_4x4(&temp_model);
}

void Render_sphere(float radius, pvr_dr_state_t* dr_state, uint32_t base_color) {
    float stackStep = SHZ_F_PI / SPHERE_STACKS;
    float sliceStep = SHZ_F_PI * 2.0f / SPHERE_SLICES;
    
    for (int stack = 0; stack < SPHERE_STACKS; stack++) {
        float stackAngle = SHZ_F_PI / 2.0f - stack * stackStep;
        float nextStackAngle = SHZ_F_PI / 2.0f - (stack + 1) * stackStep;
        
        shz_sincos_t sc1 = shz_sincosf(stackAngle);
        shz_sincos_t sc2 = shz_sincosf(nextStackAngle);
        
        float z1 = sc1.sin;
        float r1 = sc1.cos;
        float z2 = sc2.sin;
        float r2 = sc2.cos;
        
        for (int slice = 0; slice < SPHERE_SLICES; slice++) {
            float sliceAngle = slice * sliceStep;
            
            shz_sincos_t sc_slice = shz_sincosf(sliceAngle);
            float x = sc_slice.cos;
            float y = sc_slice.sin;
            
            pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
            vert->flags = PVR_CMD_VERTEX;
            vert->argb = base_color;
            
            float vx = x * r2 * radius;
            float vy = y * r2 * radius;
            float vz = z2 * radius;
            float w;
            
            mat_trans_nodiv_nomod(vx, vy, vz, vert->x, vert->y, vert->z, w);
            
            float inv_w = shz_invf_fsrra(w);
            vert->x *= inv_w;
            vert->y *= inv_w;
            vert->z *= inv_w;
            
            pvr_dr_commit(vert);
            
            vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
            vert->flags = PVR_CMD_VERTEX; 
            vert->argb = base_color;
            
            vx = x * r1 * radius;
            vy = y * r1 * radius;
            vz = z1 * radius;
            
            mat_trans_nodiv_nomod(vx, vy, vz, vert->x, vert->y, vert->z, w);
            
            inv_w = shz_invf_fsrra(w);
            vert->x *= inv_w;
            vert->y *= inv_w;
            vert->z *= inv_w;
            
            pvr_dr_commit(vert);
        }
        
        {
            float sliceAngle = SPHERE_SLICES * sliceStep;
            
            shz_sincos_t sc_slice = shz_sincosf(sliceAngle);
            float x = sc_slice.cos;
            float y = sc_slice.sin;
            
            pvr_vertex_t *vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
            vert->flags = PVR_CMD_VERTEX;
            vert->argb = base_color;
            
            float vx = x * r2 * radius;
            float vy = y * r2 * radius;
            float vz = z2 * radius;
            float w;
            
            mat_trans_nodiv_nomod(vx, vy, vz, vert->x, vert->y, vert->z, w);
            
            float inv_w = shz_invf_fsrra(w);
            vert->x *= inv_w;
            vert->y *= inv_w;
            vert->z *= inv_w;
            
            pvr_dr_commit(vert);
            
            vert = (pvr_vertex_t *)pvr_dr_target(*dr_state);
            vert->flags = PVR_CMD_VERTEX_EOL;  
            vert->argb = base_color;
            
            vx = x * r1 * radius;
            vy = y * r1 * radius;
            vz = z1 * radius;
            
            mat_trans_nodiv_nomod(vx, vy, vz, vert->x, vert->y, vert->z, w);
            
            inv_w = shz_invf_fsrra(w);
            vert->x *= inv_w;
            vert->y *= inv_w;
            vert->z *= inv_w;
            
            pvr_dr_commit(vert);
        }
    }
}

int main(int argc, char **argv) {
    pvr_init_params_t params = {
        { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        1024 * 1024 * 3.0,
        0, 0, 0, 4
    };
    
    pvr_init(&params);
    setup_projection_view();
    initCachedPolyHeader();  // Initialize  once
    
    // Aligned for cache performance
    BallInfo balls[NUM_BALLS] __attribute__((aligned(32)));
    
    const int GRID_COLS = 10;
    const int GRID_ROWS = 10;
    const float SPACING_X = 4.0f;
    const float SPACING_Y = 4.0f;
    const float START_X = -((GRID_COLS-1) * SPACING_X) / 2.0f;
    const float START_Y = -((GRID_ROWS-1) * SPACING_Y) / 2.0f;
    const float BASE_Z = 30.0f;
    
    int idx = 0;
    for (int y = 0; y < GRID_ROWS; y++) {
        for (int x = 0; x < GRID_COLS; x++) {
            if (idx < NUM_BALLS) {
                balls[idx].x = START_X + (x * SPACING_X);
                balls[idx].y = START_Y + (y * SPACING_Y);
                balls[idx].z = BASE_Z;
                balls[idx].rx = 0.0f;
                balls[idx].ry = 0.0f;
                balls[idx].drx = 0.01f + (idx % 5) * 0.002f;
                balls[idx].dry = 0.015f + (idx % 7) * 0.002f;
                idx++;
            }
        }
    }
    
    // Pre-compute triangle count  
    const int triangles_per_ball = SPHERE_STACKS * SPHERE_SLICES * 2;
    const int benchmark_triangles = triangles_per_ball * NUM_BALLS;
    
    printf("Rendering %d balls, %d triangles\n", NUM_BALLS, benchmark_triangles);


    
    uint32 frames = 0;
    uint32 fps_timer = timer_ms_gettime64();
    float fps = 0.0f;
    
    while(1) {
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        
        pvr_dr_state_t dr_state;
        pvr_dr_init(&dr_state);
        submitCachedHeader(&dr_state);   
        
        for (int i = 0; i < NUM_BALLS; i++) {
            //   Prefetch next ball while processing current
            if (i + 1 < NUM_BALLS) {
                SHZ_PREFETCH(&balls[i + 1]);
            }
            
            balls[i].rx += balls[i].drx;
            balls[i].ry += balls[i].dry;
            
            setupModelMatrix(balls[i].x, balls[i].y, balls[i].z, balls[i].rx, balls[i].ry);
            
            uint32_t color = 0xFF000000 | 
                           ((i * 5) % 256) | 
                           (((i * 7) % 256) << 8) | 
                           (((i * 11) % 256) << 16);
            
            Render_sphere(1.0f, &dr_state, color);
        }
        
        pvr_list_finish();
        pvr_scene_finish();
        
        frames++;
        uint32 current_time = timer_ms_gettime64();
        if (current_time - fps_timer >= 1000) {
            fps = frames / ((current_time - fps_timer) / 1000.0f);
            float pps = benchmark_triangles * fps;
            
            printf("FPS: %.2f | PPS: %.2fM | Tris: %d\n", 
                   fps, pps/1000000.0f, benchmark_triangles);
            
            frames = 0;
            fps_timer = current_time;
        }
        
        maple_device_t* cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (cont) {
            cont_state_t* state = (cont_state_t *)maple_dev_status(cont);
            if (state && (state->buttons & CONT_START)) {
                break;
            }
        }
    }
    
    pvr_shutdown();
    return 0;
}