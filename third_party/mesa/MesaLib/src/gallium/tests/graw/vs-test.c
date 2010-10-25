/* Display a cleared blue window.  This demo has no dependencies on
 * any utility code, just the graw interface and gallium.
 */

#include "state_tracker/graw.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_shader_tokens.h"
#include "pipe/p_state.h"
#include "pipe/p_defines.h"

#include <stdio.h>              /* for fread(), etc */

#include "util/u_debug.h"       /* debug_dump_surface_bmp() */
#include "util/u_inlines.h"
#include "util/u_memory.h"      /* Offset() */
#include "util/u_draw_quad.h"
#include "util/u_box.h"    

static const char *filename = NULL;
unsigned show_fps = 0;


static void usage(char *name)
{
   fprintf(stderr, "usage: %s [ options ] shader_filename\n", name);
#ifndef WIN32
   fprintf(stderr, "\n" );
   fprintf(stderr, "options:\n");
   fprintf(stderr, "    -fps  show frames per second\n");
#endif
}


enum pipe_format formats[] = {
   PIPE_FORMAT_R8G8B8A8_UNORM,
   PIPE_FORMAT_B8G8R8A8_UNORM,
   PIPE_FORMAT_NONE
};

static const int WIDTH = 250;
static const int HEIGHT = 250;

static struct pipe_screen *screen = NULL;
static struct pipe_context *ctx = NULL;
static struct pipe_resource *rttex = NULL;
static struct pipe_resource *constbuf = NULL;
static struct pipe_surface *surf = NULL;
static struct pipe_sampler_view *sv = NULL;
static void *sampler = NULL;
static void *window = NULL;
static struct pipe_resource *samptex = NULL;

struct vertex {
   float position[4];
   float color[3];
};

/* Draw a regular mesh
 */
#define MESH_SZ 16
static struct vertex vertices[MESH_SZ * MESH_SZ];

static float constants[] = 
{  0.4, 0, 0,  1,
   1,   1, 1,  1,
   2,   2, 2,  2,
   4,   8, 16, 32,

   3,  0, 0, 0,
   0, .5, 0, 0,
   0,  0, 1, 0,
   0,  0, 0, 1,

   1, 0, 0, 0.5,
   0, 1, 0, 0.5,
   0, 0, 1, 0,
   0, 0, 0, 1,
};

static void init_fs_constbuf( void )
{
   struct pipe_resource templat;
   struct pipe_box box;

   templat.target = PIPE_BUFFER;
   templat.format = PIPE_FORMAT_R8_UNORM;
   templat.width0 = sizeof(constants);
   templat.height0 = 1;
   templat.depth0 = 1;
   templat.last_level = 0;
   templat.nr_samples = 1;
   templat.bind = PIPE_BIND_CONSTANT_BUFFER;

   constbuf = screen->resource_create(screen,
                                      &templat);
   if (constbuf == NULL)
      exit(4);


   u_box_2d(0,0,sizeof(constants),1, &box);

   ctx->transfer_inline_write(ctx,
                              constbuf,
                              u_subresource(0,0),
                              PIPE_TRANSFER_WRITE,
                              &box,
                              constants,
                              sizeof constants,
                              sizeof constants);


   ctx->set_constant_buffer(ctx,
                            PIPE_SHADER_FRAGMENT, 0,
                            constbuf);
}


static void set_viewport( float x, float y,
                          float width, float height,
                          float near, float far)
{
   float z = far;
   float half_width = (float)width / 2.0f;
   float half_height = (float)height / 2.0f;
   float half_depth = ((float)far - (float)near) / 2.0f;
   struct pipe_viewport_state vp;

   vp.scale[0] = half_width;
   vp.scale[1] = half_height;
   vp.scale[2] = half_depth;
   vp.scale[3] = 1.0f;

   vp.translate[0] = half_width + x;
   vp.translate[1] = half_height + y;
   vp.translate[2] = half_depth + z;
   vp.translate[3] = 0.0f;

   ctx->set_viewport_state( ctx, &vp );
}

static void set_vertices( void )
{
   struct pipe_vertex_element ve[2];
   struct pipe_vertex_buffer vbuf;
   void *handle;
   int x,y;

   memset(ve, 0, sizeof ve);

   ve[0].src_offset = Offset(struct vertex, position);
   ve[0].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
   ve[1].src_offset = Offset(struct vertex, color);
   ve[1].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;

   handle = ctx->create_vertex_elements_state(ctx, 2, ve);
   ctx->bind_vertex_elements_state(ctx, handle);

   for (x = 0; x < MESH_SZ; x++) {
      for (y = 0; y < MESH_SZ; y++) {
         int i = y * MESH_SZ + x;
         vertices[i].position[0] = ((float)x)/MESH_SZ * 2.0 - 1.0;
         vertices[i].position[1] = ((float)y)/MESH_SZ * 2.0 - 1.0;
         vertices[i].position[2] = 0;
         vertices[i].position[3] = 1.0;

         vertices[i].color[0] = .5;
         vertices[i].color[1] = (float)x / (float)MESH_SZ;
         vertices[i].color[2] = (float)y / (float)MESH_SZ;
      }
   }

   vbuf.stride = sizeof( struct vertex );
   vbuf.max_index = sizeof(vertices) / vbuf.stride;
   vbuf.buffer_offset = 0;
   vbuf.buffer = screen->user_buffer_create(screen,
                                            vertices,
                                            sizeof(vertices),
                                            PIPE_BIND_VERTEX_BUFFER);

   ctx->set_vertex_buffers(ctx, 1, &vbuf);
}

static void set_vertex_shader( void )
{
   FILE *f;
   char buf[50000];
   void *handle;
   int sz;

   if ((f = fopen(filename, "r")) == NULL) {
      fprintf(stderr, "Couldn't open %s\n", filename);
      exit(1);
   }

   sz = fread(buf, 1, sizeof(buf), f);
   if (!feof(f)) {
      printf("file too long\n");
      exit(1);
   }
   printf("%.*s\n", sz, buf);
   buf[sz] = 0;

   handle = graw_parse_vertex_shader(ctx, buf);
   ctx->bind_vs_state(ctx, handle);
   fclose(f);
}

static void set_fragment_shader( void )
{
   void *handle;
   const char *text =
      "FRAG\n"
      "DCL IN[0], COLOR, LINEAR\n"
      "DCL OUT[0], COLOR\n"
      "  0: MOV OUT[0], IN[0]\n"
      "  1: END\n";

   handle = graw_parse_fragment_shader(ctx, text);
   ctx->bind_fs_state(ctx, handle);
}



static void draw( void )
{
   float clear_color[4] = {.1,.3,.5,0};

   ctx->clear(ctx, PIPE_CLEAR_COLOR, clear_color, 0, 0);
   util_draw_arrays(ctx, PIPE_PRIM_POINTS, 0, Elements(vertices));
   ctx->flush(ctx, PIPE_FLUSH_RENDER_CACHE, NULL);

#if 0
   /* At the moment, libgraw leaks out/makes available some of the
    * symbols from gallium/auxiliary, including these debug helpers.
    * Will eventually want to bless some of these paths, and lock the
    * others down so they aren't accessible from test programs.
    *
    * This currently just happens to work on debug builds - a release
    * build will probably fail to link here:
    */
   debug_dump_surface_bmp(ctx, "result.bmp", surf);
#endif

   screen->flush_frontbuffer(screen, surf, window);
}

#define SIZE 16

static void init_tex( void )
{ 
   struct pipe_sampler_view sv_template;
   struct pipe_sampler_state sampler_desc;
   struct pipe_resource templat;
   struct pipe_box box;
   ubyte tex2d[SIZE][SIZE][4];
   int s, t;

#if (SIZE != 2)
   for (s = 0; s < SIZE; s++) {
      for (t = 0; t < SIZE; t++) {
         if (0) {
            int x = (s ^ t) & 1;
	    tex2d[t][s][0] = (x) ? 0 : 63;
	    tex2d[t][s][1] = (x) ? 0 : 128;
	    tex2d[t][s][2] = 0;
	    tex2d[t][s][3] = 0xff;
         }
         else {
            int x = ((s ^ t) >> 2) & 1;
	    tex2d[t][s][0] = s*255/(SIZE-1);
	    tex2d[t][s][1] = t*255/(SIZE-1);
	    tex2d[t][s][2] = (x) ? 0 : 128;
	    tex2d[t][s][3] = 0xff;
         }
      }
   }
#else
   tex2d[0][0][0] = 0;
   tex2d[0][0][1] = 255;
   tex2d[0][0][2] = 255;
   tex2d[0][0][3] = 0;

   tex2d[0][1][0] = 0;
   tex2d[0][1][1] = 0;
   tex2d[0][1][2] = 255;
   tex2d[0][1][3] = 255;

   tex2d[1][0][0] = 255;
   tex2d[1][0][1] = 255;
   tex2d[1][0][2] = 0;
   tex2d[1][0][3] = 255;

   tex2d[1][1][0] = 255;
   tex2d[1][1][1] = 0;
   tex2d[1][1][2] = 0;
   tex2d[1][1][3] = 255;
#endif

   templat.target = PIPE_TEXTURE_2D;
   templat.format = PIPE_FORMAT_B8G8R8A8_UNORM;
   templat.width0 = SIZE;
   templat.height0 = SIZE;
   templat.depth0 = 1;
   templat.last_level = 0;
   templat.nr_samples = 1;
   templat.bind = PIPE_BIND_SAMPLER_VIEW;

   
   samptex = screen->resource_create(screen,
                                 &templat);
   if (samptex == NULL)
      exit(4);

   u_box_2d(0,0,SIZE,SIZE, &box);

   ctx->transfer_inline_write(ctx,
                              samptex,
                              u_subresource(0,0),
                              PIPE_TRANSFER_WRITE,
                              &box,
                              tex2d,
                              sizeof tex2d[0],
                              sizeof tex2d);

   /* Possibly read back & compare against original data:
    */
   if (0)
   {
      struct pipe_transfer *t;
      uint32_t *ptr;
      t = pipe_get_transfer(ctx, samptex,
                            0, 0, 0, /* face, level, zslice */
                            PIPE_TRANSFER_READ,
                            0, 0, SIZE, SIZE); /* x, y, width, height */

      ptr = ctx->transfer_map(ctx, t);

      if (memcmp(ptr, tex2d, sizeof tex2d) != 0) {
         assert(0);
         exit(9);
      }

      ctx->transfer_unmap(ctx, t);

      ctx->transfer_destroy(ctx, t);
   }

   memset(&sv_template, 0, sizeof sv_template);
   sv_template.format = samptex->format;
   sv_template.texture = samptex;
   sv_template.first_level = 0;
   sv_template.last_level = 0;
   sv_template.swizzle_r = 0;
   sv_template.swizzle_g = 1;
   sv_template.swizzle_b = 2;
   sv_template.swizzle_a = 3;
   sv = ctx->create_sampler_view(ctx, samptex, &sv_template);
   if (sv == NULL)
      exit(5);

   ctx->set_fragment_sampler_views(ctx, 1, &sv);
   

   memset(&sampler_desc, 0, sizeof sampler_desc);
   sampler_desc.wrap_s = PIPE_TEX_WRAP_REPEAT;
   sampler_desc.wrap_t = PIPE_TEX_WRAP_REPEAT;
   sampler_desc.wrap_r = PIPE_TEX_WRAP_REPEAT;
   sampler_desc.min_img_filter = PIPE_TEX_FILTER_NEAREST;
   sampler_desc.min_mip_filter = PIPE_TEX_MIPFILTER_NONE;
   sampler_desc.mag_img_filter = PIPE_TEX_FILTER_NEAREST;
   sampler_desc.compare_mode = PIPE_TEX_COMPARE_NONE;
   sampler_desc.compare_func = 0;
   sampler_desc.normalized_coords = 1;
   sampler_desc.max_anisotropy = 0;
   
   sampler = ctx->create_sampler_state(ctx, &sampler_desc);
   if (sampler == NULL)
      exit(6);

   ctx->bind_fragment_sampler_states(ctx, 1, &sampler);
   
}

static void init( void )
{
   struct pipe_framebuffer_state fb;
   struct pipe_resource templat;
   int i;

   /* It's hard to say whether window or screen should be created
    * first.  Different environments would prefer one or the other.
    *
    * Also, no easy way of querying supported formats if the screen
    * cannot be created first.
    */
   for (i = 0; 
        window == NULL && formats[i] != PIPE_FORMAT_NONE;
        i++) {
      
      screen = graw_create_window_and_screen(0,0,WIDTH,HEIGHT,
                                             formats[i],
                                             &window);
   }
   
   ctx = screen->context_create(screen, NULL);
   if (ctx == NULL)
      exit(3);

   templat.target = PIPE_TEXTURE_2D;
   templat.format = formats[i];
   templat.width0 = WIDTH;
   templat.height0 = HEIGHT;
   templat.depth0 = 1;
   templat.last_level = 0;
   templat.nr_samples = 1;
   templat.bind = (PIPE_BIND_RENDER_TARGET |
                   PIPE_BIND_DISPLAY_TARGET);
   
   rttex = screen->resource_create(screen,
                                 &templat);
   if (rttex == NULL)
      exit(4);

   surf = screen->get_tex_surface(screen, rttex, 0, 0, 0,
                                  PIPE_BIND_RENDER_TARGET |
                                  PIPE_BIND_DISPLAY_TARGET);
   if (surf == NULL)
      exit(5);

   memset(&fb, 0, sizeof fb);
   fb.nr_cbufs = 1;
   fb.width = WIDTH;
   fb.height = HEIGHT;
   fb.cbufs[0] = surf;

   ctx->set_framebuffer_state(ctx, &fb);
   
   {
      struct pipe_blend_state blend;
      void *handle;
      memset(&blend, 0, sizeof blend);
      blend.rt[0].colormask = PIPE_MASK_RGBA;
      handle = ctx->create_blend_state(ctx, &blend);
      ctx->bind_blend_state(ctx, handle);
   }

   {
      struct pipe_depth_stencil_alpha_state depthstencil;
      void *handle;
      memset(&depthstencil, 0, sizeof depthstencil);
      handle = ctx->create_depth_stencil_alpha_state(ctx, &depthstencil);
      ctx->bind_depth_stencil_alpha_state(ctx, handle);
   }

   {
      struct pipe_rasterizer_state rasterizer;
      void *handle;
      memset(&rasterizer, 0, sizeof rasterizer);
      rasterizer.cull_face = PIPE_FACE_NONE;
      rasterizer.point_size = 8.0;
      rasterizer.gl_rasterization_rules = 1;
      handle = ctx->create_rasterizer_state(ctx, &rasterizer);
      ctx->bind_rasterizer_state(ctx, handle);
   }

   set_viewport(0, 0, WIDTH, HEIGHT, 30, 1000);

   init_tex();
   init_fs_constbuf();

   set_vertices();
   set_vertex_shader();
   set_fragment_shader();
}

static void args(int argc, char *argv[])
{
   int i;

   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-fps") == 0) {
         show_fps = 1;
      }
      else if (i == argc - 1) {
	 filename = argv[i];
      }
      else {
	 usage(argv[0]);
	 exit(1);
      }
   }

   if (!filename) {
      usage(argv[0]);
      exit(1);
   }
}

int main( int argc, char *argv[] )
{
   args(argc,argv);
   init();

   graw_set_display_func( draw );
   graw_main_loop();
   return 0;
}
