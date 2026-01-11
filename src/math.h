// vim: sw=2 ts=2 expandtab smartindent

typedef struct { float x, y; } f2;
typedef struct { float x, y, z; } f3;
typedef union { float arr[4]; struct { float x, y, z, w; } p; f3 xyz; } f4;
typedef union { float arr[4][4]; f4 rows[4]; float floats[16]; } f4x4;

typedef struct { f2 min, max; } Box2;
#define BOX2_UNCONSTRAINED (Box2) { { -INFINITY, -INFINITY }, {  INFINITY,  INFINITY } }
#define BOX2_CLOSED        (Box2) { {  INFINITY,  INFINITY }, { -INFINITY, -INFINITY } }

static float lerp(float v0, float v1, float t) { return (1.0f - t) * v0 + t * v1; }
static float clamp(float min, float max, float t) { return fminf(max, fmaxf(min, t)); }
static float inv_lerp(float min, float max, float p) { return (p - min) / (max - min); }
static float rads_distance(float a, float b) {
  float difference = fmodf(b - a, M_PI*2.0);
  return fmodf(2.0 * difference, M_PI*2.0) - difference;
}
static float rads_lerp(float a, float b, float t) {
    return a + rads_distance(a, b) * t;
}

static float f2_length(f2 f) { return sqrtf(f.x*f.x + f.y*f.y); }
static f2 f2_norm(f2 f) {
  float len = f2_length(f);
  return (f2) { f.x / len, f.y / len };
}
static bool f2_line_hits_line(f2 from0, f2 to0, f2 from1, f2 to1, f2 *out) {
  float a = from0.x, b = from0.y,
        c =   to0.x, d =   to0.y,
        p = from1.x, q = from1.y,
        r =   to1.x, s =   to1.y;
  float det = (c - a) * (s - q) - (r - p) * (d - b);
  if (det < 0.001) {
    return false;
  } else {
    float lambda = ((s - q) * (r - a) + (p - r) * (s - b)) / det;
    float gamma = ((b - d) * (r - a) + (c - a) * (s - b)) / det;

    if ((0 < lambda && lambda < 1) && (0 < gamma && gamma < 1)) {
      if (out) out->x = lerp(from0.x, to0.x, lambda);
      if (out) out->y = lerp(from0.y, to0.y, lambda);
      return true;
    }

    return false;
  }
}

static f3 f3_lerp(f3 a, f3 b, float t) {
  return (f3) {
    .x = lerp(a.x, b.x, t),
    .y = lerp(a.y, b.y, t),
    .z = lerp(a.z, b.z, t)
  };
}
static float f3_length(f3 f) { return sqrtf(f.x*f.x + f.y*f.y + f.z*f.z); }
static f3 f3_norm(f3 f) {
  float len = f3_length(f);
  return (f3) { f.x / len, f.y / len, f.z / len };
}

static f3 ray_hit_plane(f3 ray_origin, f3 ray_vector, f3 plane_origin, f3 plane_vector) {
  float delta_x = plane_origin.x - ray_origin.x;
  float delta_y = plane_origin.y - ray_origin.y;
  float delta_z = plane_origin.z - ray_origin.z;

  float ldot = delta_x*plane_vector.x +
               delta_y*plane_vector.y +
               delta_z*plane_vector.z ;

  float rdot = ray_vector.x*plane_vector.x +
               ray_vector.y*plane_vector.y +
               ray_vector.z*plane_vector.z ;

  float d = ldot / rdot;
  return (f3) {
    ray_origin.x + ray_vector.x * d,
    ray_origin.y + ray_vector.y * d,
    ray_origin.z + ray_vector.z * d,
  };
}

static f4x4 f4x4_ortho(float left, float right, float bottom, float top, float near, float far) {
    f4x4 res = {0};

    res.arr[0][0] = 2.0f / (right - left);
    res.arr[1][1] = 2.0f / (top - bottom);
    res.arr[2][2] = 2.0f / (far - near);
    res.arr[3][3] = 1.0f;

    res.arr[3][0] = (left + right) / (left - right);
    res.arr[3][1] = (bottom + top) / (bottom - top);
    res.arr[3][2] = (far + near) / (near - far);

    return res;
}

static f4x4 f4x4_perspective(float fovy, float aspect, float near, float far) {
  float f = 1.0 / tanf(fovy / 2);
  f4x4 out = { .floats = {
    f,          0, 0, 0,
    0, f / aspect, 0, 0,
    0,          0, 0, -1,
    0,          0, 0, 0,
  }};

  {
    float nf = 1 / (far - near);
    out.floats[10] = (far + near) * nf;
    out.floats[14] = 2 * far * near * nf;
  }

  /* theoretically you can make an infinite frustum like so?
    out.floats[10] = -1, out.floats[14] = -2 * near; */

  return out;
}

static f4x4 f4x4_target_to(f3 eye, f3 target, f3 up) {
  float z0 = eye.x - target.x,
        z1 = eye.y - target.y,
        z2 = eye.z - target.z;
  float len = z0 * z0 + z1 * z1 + z2 * z2;
  if (len > 0) {
      len = 1.0f / sqrtf(len);
      z0 *= len;
      z1 *= len;
      z2 *= len;
  }
  float x0 = up.y * z2 - up.z * z1,
        x1 = up.z * z0 - up.x * z2,
        x2 = up.x * z1 - up.y * z0;
  len = x0 * x0 + x1 * x1 + x2 * x2;
  if (len > 0) {
      len = 1 / sqrtf(len);
      x0 *= len;
      x1 *= len;
      x2 *= len;
  }

  return (f4x4) { .arr = {
    { x0, x1, x2, 0 },
    {
      z1 * x2 - z2 * x1,
      z2 * x0 - z0 * x2,
      z0 * x1 - z1 * x0,
      0,
    },
    { z0, z1, z2, 0 },
    { eye.x, eye.y, eye.z, 1 },
  } };
}

static f4x4 f4x4_invert(f4x4 a) {
    float b00 = a.arr[0][0] * a.arr[1][1] - a.arr[0][1] * a.arr[1][0];
    float b01 = a.arr[0][0] * a.arr[1][2] - a.arr[0][2] * a.arr[1][0];
    float b02 = a.arr[0][0] * a.arr[1][3] - a.arr[0][3] * a.arr[1][0];
    float b03 = a.arr[0][1] * a.arr[1][2] - a.arr[0][2] * a.arr[1][1];
    float b04 = a.arr[0][1] * a.arr[1][3] - a.arr[0][3] * a.arr[1][1];
    float b05 = a.arr[0][2] * a.arr[1][3] - a.arr[0][3] * a.arr[1][2];
    float b06 = a.arr[2][0] * a.arr[3][1] - a.arr[2][1] * a.arr[3][0];
    float b07 = a.arr[2][0] * a.arr[3][2] - a.arr[2][2] * a.arr[3][0];
    float b08 = a.arr[2][0] * a.arr[3][3] - a.arr[2][3] * a.arr[3][0];
    float b09 = a.arr[2][1] * a.arr[3][2] - a.arr[2][2] * a.arr[3][1];
    float b10 = a.arr[2][1] * a.arr[3][3] - a.arr[2][3] * a.arr[3][1];
    float b11 = a.arr[2][2] * a.arr[3][3] - a.arr[2][3] * a.arr[3][2];

    /* Calculate the determinant */
    float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    if (det == 0.0f) {
      SDL_Log("Couldn't invert matrix!\n");
      return (f4x4) {0};
    }

    det = 1.0 / det;
    return (f4x4) { .floats = {
      (a.arr[1][1] * b11 - a.arr[1][2] * b10 + a.arr[1][3] * b09) * det,
      (a.arr[0][2] * b10 - a.arr[0][1] * b11 - a.arr[0][3] * b09) * det,
      (a.arr[3][1] * b05 - a.arr[3][2] * b04 + a.arr[3][3] * b03) * det,
      (a.arr[2][2] * b04 - a.arr[2][1] * b05 - a.arr[2][3] * b03) * det,
      (a.arr[1][2] * b08 - a.arr[1][0] * b11 - a.arr[1][3] * b07) * det,
      (a.arr[0][0] * b11 - a.arr[0][2] * b08 + a.arr[0][3] * b07) * det,
      (a.arr[3][2] * b02 - a.arr[3][0] * b05 - a.arr[3][3] * b01) * det,
      (a.arr[2][0] * b05 - a.arr[2][2] * b02 + a.arr[2][3] * b01) * det,
      (a.arr[1][0] * b10 - a.arr[1][1] * b08 + a.arr[1][3] * b06) * det,
      (a.arr[0][1] * b08 - a.arr[0][0] * b10 - a.arr[0][3] * b06) * det,
      (a.arr[3][0] * b04 - a.arr[3][1] * b02 + a.arr[3][3] * b00) * det,
      (a.arr[2][1] * b02 - a.arr[2][0] * b04 - a.arr[2][3] * b00) * det,
      (a.arr[1][1] * b07 - a.arr[1][0] * b09 - a.arr[1][2] * b06) * det,
      (a.arr[0][0] * b09 - a.arr[0][1] * b07 + a.arr[0][2] * b06) * det,
      (a.arr[3][1] * b01 - a.arr[3][0] * b03 - a.arr[3][2] * b00) * det,
      (a.arr[2][0] * b03 - a.arr[2][1] * b01 + a.arr[2][2] * b00) * det,
    } };
}

static f4x4 f4x4_mul_f4x4(f4x4 a, f4x4 b) {
  f4x4 out = {0};

  /* cache only the current line of the second matrix */
  f4 bb = b.rows[0];
  out.arr[0][0] = bb.arr[0] * a.arr[0][0] + bb.arr[1] * a.arr[1][0] + bb.arr[2] * a.arr[2][0] + bb.arr[3] * a.arr[3][0];
  out.arr[0][1] = bb.arr[0] * a.arr[0][1] + bb.arr[1] * a.arr[1][1] + bb.arr[2] * a.arr[2][1] + bb.arr[3] * a.arr[3][1];
  out.arr[0][2] = bb.arr[0] * a.arr[0][2] + bb.arr[1] * a.arr[1][2] + bb.arr[2] * a.arr[2][2] + bb.arr[3] * a.arr[3][2];
  out.arr[0][3] = bb.arr[0] * a.arr[0][3] + bb.arr[1] * a.arr[1][3] + bb.arr[2] * a.arr[2][3] + bb.arr[3] * a.arr[3][3];
  bb = b.rows[1];
  out.arr[1][0] = bb.arr[0] * a.arr[0][0] + bb.arr[1] * a.arr[1][0] + bb.arr[2] * a.arr[2][0] + bb.arr[3] * a.arr[3][0];
  out.arr[1][1] = bb.arr[0] * a.arr[0][1] + bb.arr[1] * a.arr[1][1] + bb.arr[2] * a.arr[2][1] + bb.arr[3] * a.arr[3][1];
  out.arr[1][2] = bb.arr[0] * a.arr[0][2] + bb.arr[1] * a.arr[1][2] + bb.arr[2] * a.arr[2][2] + bb.arr[3] * a.arr[3][2];
  out.arr[1][3] = bb.arr[0] * a.arr[0][3] + bb.arr[1] * a.arr[1][3] + bb.arr[2] * a.arr[2][3] + bb.arr[3] * a.arr[3][3];
  bb = b.rows[2];
  out.arr[2][0] = bb.arr[0] * a.arr[0][0] + bb.arr[1] * a.arr[1][0] + bb.arr[2] * a.arr[2][0] + bb.arr[3] * a.arr[3][0];
  out.arr[2][1] = bb.arr[0] * a.arr[0][1] + bb.arr[1] * a.arr[1][1] + bb.arr[2] * a.arr[2][1] + bb.arr[3] * a.arr[3][1];
  out.arr[2][2] = bb.arr[0] * a.arr[0][2] + bb.arr[1] * a.arr[1][2] + bb.arr[2] * a.arr[2][2] + bb.arr[3] * a.arr[3][2];
  out.arr[2][3] = bb.arr[0] * a.arr[0][3] + bb.arr[1] * a.arr[1][3] + bb.arr[2] * a.arr[2][3] + bb.arr[3] * a.arr[3][3];
  bb = b.rows[3];
  out.arr[3][0] = bb.arr[0] * a.arr[0][0] + bb.arr[1] * a.arr[1][0] + bb.arr[2] * a.arr[2][0] + bb.arr[3] * a.arr[3][0];
  out.arr[3][1] = bb.arr[0] * a.arr[0][1] + bb.arr[1] * a.arr[1][1] + bb.arr[2] * a.arr[2][1] + bb.arr[3] * a.arr[3][1];
  out.arr[3][2] = bb.arr[0] * a.arr[0][2] + bb.arr[1] * a.arr[1][2] + bb.arr[2] * a.arr[2][2] + bb.arr[3] * a.arr[3][2];
  out.arr[3][3] = bb.arr[0] * a.arr[0][3] + bb.arr[1] * a.arr[1][3] + bb.arr[2] * a.arr[2][3] + bb.arr[3] * a.arr[3][3];

  return out;
}

static f4 f4x4_mul_f4(f4x4 m, f4 v) {
  f4 res = {0};
  for (int x = 0; x < 4; x++) {
    float sum = 0;
    for (int y = 0; y < 4; y++)
      sum += m.arr[y][x] * v.arr[y];

    res.arr[x] = sum;
  }
  return res;
}

static f3 f4x4_transform_f3(f4x4 m, f3 v) {
  f4 res = { .xyz = v };
  res.p.w = 1.0f;
  res = f4x4_mul_f4(m, res);
  res.p.x /= res.p.w;
  res.p.y /= res.p.w;
  res.p.z /= res.p.w;
  return res.xyz;
}

static f4x4 f4x4_scale(float scale) {
  f4x4 res = {0};
  res.arr[0][0] = scale;
  res.arr[1][1] = scale;
  res.arr[2][2] = scale;
  res.arr[3][3] = 1.0f;
  return res;
}

static f4x4 f4x4_scale3(f3 scale) {
  f4x4 res = {0};
  res.arr[0][0] = scale.x;
  res.arr[1][1] = scale.y;
  res.arr[2][2] = scale.z;
  res.arr[3][3] = 1.0f;
  return res;
}

/* 2D rotation around the Z axis */
static f4x4 f4x4_turn(float radians) {
  f4x4 res = {0};

  res.arr[0][0] = cosf(radians);
  res.arr[0][1] = sinf(radians);

  res.arr[1][0] = -res.arr[0][1];
  res.arr[1][1] =  res.arr[0][0];

  res.arr[2][2] = 1.0f;
  res.arr[3][3] = 1.0f;
  return res;
}

static f4x4 f4x4_move(f3 pos) {
  f4x4 res = {0};
  res.arr[0][0] = 1.0f;
  res.arr[1][1] = 1.0f;
  res.arr[2][2] = 1.0f;
  res.arr[3][0] = pos.x;
  res.arr[3][1] = pos.y;
  res.arr[3][2] = pos.z;
  res.arr[3][3] = 1.0f;
  return res;
}
