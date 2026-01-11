// vim: sw=2 ts=2 expandtab smartindent
#ifndef cad_IMPLEMENTATION

typedef struct cad_Post cad_Post;
struct cad_Post {
  cad_Post *next, *last;
  f2 pos;
};

typedef struct {
  bool placing_wall;
  cad_Post post;
} cad_State;

void cad_frame(void);
#endif

#ifdef cad_IMPLEMENTATION

/* massive hack. praying for forgiveness */
#define cad (jeux.cad)

void cad_frame(void) {
  if (cad.placing_wall) {

    f2 post = cad.post.pos;
    post.x = jeux.mouse_ground.x;
    post.y = jeux.mouse_ground.y;

    const float POST_HEIGHT = 1.4f;
    const float POST_THICK  = 0.2f;
    gl_geo_box3_outline(
      (f3) {     post.x,     post.y, POST_HEIGHT },
      (f3) { POST_THICK, POST_THICK, POST_HEIGHT },
      1.0f,
      (Color) { 200, 80, 20, 255 }
    );
  }
}

#undef cad
#endif
