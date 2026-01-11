// vim: sw=2 ts=2 expandtab smartindent
#ifndef cad_IMPLEMENTATION

typedef enum {
  cad_InputState_None,
  cad_InputState_PlacingFirst,
  cad_InputState_PlacingNext,
  cad_InputState_COUNT
} cad_InputState;

typedef struct cad_Post cad_Post;
struct cad_Post {
  cad_Post *next, *last;
  f2 pos;
};

typedef struct {
  bool placing_wall;

  cad_Post posts[10];
  int      post_count;

} cad_State;

static void cad_frame(void);
#endif

#ifdef cad_IMPLEMENTATION

/* massive hack. praying for forgiveness */
#define cad (jeux.cad)

static void cad_draw_wall(cad_Post post) {
  f2 p = post.pos;
  const float POST_HEIGHT = 1.4f;
  const float POST_THICK  = 0.2f;
  gl_geo_box3_outline(
    (f3) {        p.x,        p.y, POST_HEIGHT },
    (f3) { POST_THICK, POST_THICK, POST_HEIGHT },
    1.0f,
    (Color) { 200, 80, 20, 255 }
  );
}

static void cad_frame(void) {
  if (cad.placing_wall) {

    cad_Post post = {0};
    post.pos.x = jeux.mouse_ground.x;
    post.pos.y = jeux.mouse_ground.y;

    if (jeux.mouse_lmb_down) {
      cad.posts[cad.post_count++] = post;
    } else {
      cad_draw_wall(post);
    }
  }

  for (int i = 0; i < cad.post_count; i++) {
    cad_draw_wall(cad.posts[i]);
  }
}

#undef cad
#endif
