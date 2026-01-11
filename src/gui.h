/* UI */
Clay_Color paper       = { 150, 131, 107, 255 };
Clay_Color paper_hover = { 128, 101,  77, 255 };
Clay_Color wood        = {  27,  15,   7, 255 };
Clay_Color ink         = {   0,   0,   0, 255 };
Clay_Color blood       = {   255,   0,   0, 255 };
Clay_Color water       = {   0,   0,   255, 255 };

/* Macro to cast Clay's color type to our internal color type */
#define CLAY_TO_COL(c) ((Color){ .r = c.r, .g = c.g, .b = c.b, .a = c.a })

uint16_t text_body = 24;
uint16_t text_title = 24;

/* massive hack. praying for forgiveness */
#define gui (jeux.gui)

/* If you give the SVG Cooker an SVG like this
 *
 *   (0, 0)
 *      +==============+
 *      |   . .....    |
 *      |  . .         |
 *      +==============+  (230, 50)
 *
 * It normalizes the coordinates, like this
 *
 *   (0, 0)
 *      +==============+
 *      |              |
 *      +==============+
 *      |   . .....    |
 *      |  . .         |
 *      +==============+
 *      |              |
 *      +==============+  (1, 1)
 *
 * In doing so, instead of smushing into a box and breaking the aspect
 * ratio, it adds some space to the sides, preserving the aspect ratio
 * of the image.
 *
 * Most of the time, this is what you want - now you can just draw
 * the image, and you don't have to wory about supplying unique UVs
 * for it.
 *
 * But sometimes, we want to  take advantage of the fact that this
 * asset is actually a vector graphic, and we can scale it arbitrarily.
 *
 * When that's the case, it would be nice to know what portion of the
 * image actually gets used -- where the smaller rectangle is in that
 * drawing - so you can effectively "undo" the shrink-to-fit operation.
 *
 * What this macro does is supply a Clay_ImageElementConfig that undoes
 * the automatic aspect ratio correction; using this, you can warp your
 * images, so keep that in mind. */
#define UI_IMAGE_FIT(image) (Clay_ImageElementConfig) { \
  .sourceDimensions = { model_##image##_size_x, model_##image##_size_y }, \
  .imageData = gl_Model_##image, \
  .transform = f4x4_mul_f4x4( \
    f4x4_scale3((f3) { 1/model_##image##_size_x, 1/model_##image##_size_y, 1.0f }), \
    f4x4_move((f3) { -0.5f*(1 - model_##image##_size_x), -0.5f*(1 - model_##image##_size_y), 0.0f }) \
  ) \
}

/* MARK: START of "DUMB UI COMPONENTS" { */
static void ui_icon(gl_Model model, size_t size) {
  CLAY({
    .layout.sizing = { CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size) },
    .image = {
      .sourceDimensions = { size, size },
      .imageData = model,
      .transform = f4x4_scale(1)
    }
  });
}

static void ui_icon_f4x4(gl_Model model, size_t size, f4x4 mat) {
  CLAY({
    .layout.sizing = { CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size) },
    .image = {
      .sourceDimensions = { size, size },
      .imageData = model,
      .transform = mat
    }
  });
}

static void ui_checkbox(bool *state) {

  CLAY({
    .layout.sizing = { CLAY_SIZING_FIXED(text_body), CLAY_SIZING_FIXED(text_body) },
  }) {

    Clay_ElementDeclaration floating = {
      .floating.attachTo = CLAY_ATTACH_TO_PARENT,
      .floating.attachPoints.element = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.attachPoints.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
    };

    CLAY(floating) {
      size_t icon_size = 18;
      if (Clay_Hovered()) {
        jeux.sdl.cursor_next = jeux.sdl.cursor_doable;
        icon_size = 20;
        if (gui.lmb_click) *state ^= 1;
      }

      ui_icon(gl_Model_UiCheckBox, icon_size);

      floating.floating.offset.x =  4;
      floating.floating.offset.y = -4;
      if (*state) CLAY(floating) { ui_icon(gl_Model_UiCheck, icon_size + 4); }
    }
  };
}

/* returns true if released this frame */
static bool ui_slider(Clay_ElementId id, float *state, float min, float max) {
  Clay_ElementId handle_id = CLAY_IDI("SliderHandle", id.id);

  bool released = false;

  CLAY({
    .layout.sizing.width  = CLAY_SIZING_GROW(0),
    .layout.sizing.height = CLAY_SIZING_GROW(0),
    .layout.layoutDirection = CLAY_TOP_TO_BOTTOM,
    .id = id,
  }) {
    CLAY({ .layout.sizing.height = CLAY_SIZING_GROW(0) });
    CLAY({
      .layout.sizing.width  = CLAY_SIZING_GROW(0),
      .layout.sizing.height  = CLAY_SIZING_FIXED(2),
    });
    CLAY({ .layout.sizing.height = CLAY_SIZING_GROW(0) });

    Clay_BoundingBox bbox = Clay_GetElementData(id).boundingBox;

    CLAY({
      .floating.attachTo = CLAY_ATTACH_TO_PARENT,
      .floating.attachPoints.element = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.attachPoints.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH
    }) {
      ui_icon(gl_Model_UiSliderBody, bbox.width);
    }

    float prog = inv_lerp(min, max, *state);
    CLAY({
      .floating.attachTo = CLAY_ATTACH_TO_PARENT,
      .floating.attachPoints.element = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_CENTER,
      .floating.offset = { prog * bbox.width, 0 },
      .id = handle_id,
    }) {
      size_t icon_size = 15;

      bool held = gui.lmb_down_el.id == handle_id.id;

      if (Clay_Hovered() || held) {
        jeux.sdl.cursor_next = jeux.sdl.cursor_sideways;
        icon_size = 17;

        if (gui.lmb_down) {
          gui.lmb_down_el = handle_id;
        }
      }

      if (held) {
        float progress_px = jeux.mouse_ui_x - bbox.x;
        *state = lerp(min, max, clamp(0, 1, progress_px / bbox.width));
      }

      /* click is actually release - may be a stupid naming scheme on my part */
      if (held && gui.lmb_click) {
        released = true;
      }

      ui_icon(gl_Model_UiSliderHandle, icon_size);
    }
  }

  return released;
}

static bool ui_arrow_button(bool left) {

  bool click = false;

  CLAY({
    .layout.sizing = { CLAY_SIZING_FIXED(18), CLAY_SIZING_GROW(0) },
  }) {

    size_t icon_size = 14;

    CLAY({
      .floating.attachTo = CLAY_ATTACH_TO_PARENT,
      .floating.attachPoints.element = CLAY_ATTACH_POINT_CENTER_CENTER,
      .floating.attachPoints.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
    }) {
      if (Clay_Hovered()) {
        jeux.sdl.cursor_next = jeux.sdl.cursor_doable;

        icon_size = 17;
        if (gui.lmb_click) click = true;
      }

      f4x4 mvp = f4x4_move((f3) { 0.5, 0.5, 0 });
      /* you can also use f4x4_turn() here to rotate components around their center */
      mvp = f4x4_mul_f4x4(mvp, f4x4_scale3((f3) { left ? 1 : -1, 1, 1 }));
      mvp = f4x4_mul_f4x4(mvp, f4x4_move((f3) { -0.5f, -0.5f, 0 }));
      ui_icon_f4x4(gl_Model_UiArrowButton, icon_size, mvp);
    }
  };

  return click;
}

static bool ui_picker(size_t *state, size_t option_count, Clay_String *labels) {
  Clay_TextElementConfig text_conf = { .fontSize = text_body, .textColor = ink };

  bool changed = false;

  CLAY({
    .layout.sizing.width  = CLAY_SIZING_GROW(0),
    .layout.sizing.height = CLAY_SIZING_GROW(0),
  }) {
    if (ui_arrow_button(true)) changed = true, *state = (*state == 0 ? option_count : *state) - 1;
    CLAY({ .layout.sizing.width  = CLAY_SIZING_GROW(0) });
    CLAY_TEXT(labels[*state], CLAY_TEXT_CONFIG(text_conf));
    CLAY({ .layout.sizing.width  = CLAY_SIZING_GROW(0) });
    if (ui_arrow_button(false)) changed = true, *state = (*state + 1) % option_count;
  }

  return changed;
}

/* } MARK: END of "DUMB UI COMPONENTS" */

static void ui_wabisabi_window(
  gl_Model emblem,
  Clay_String window_title,
  ui_WabisabiWindow *window,
  void content_fn(void)
) {
#if 0
  Clay_BorderElementConfig debug_border = { .color = { 255, 0, 0, 255 }, .width = { 2, 2, 2, 2 } };
#else
  /* no debug for you! */
  Clay_BorderElementConfig debug_border = {0};
#endif

  /* currently window_title is used to make an ID for the draggable
   * area of the window, which needs to persist across frames so we can
   * know if they previously clicked on it and started dragging */

  if (!window->open) return;

  /* Wabisabi Window - Root */
  CLAY({
    .floating.attachTo = CLAY_ATTACH_TO_ROOT,
    .floating.offset = { window->x, window->y },
    .layout.sizing.width  = CLAY_SIZING_FIXED(400),
    .layout.sizing.height = CLAY_SIZING_FIT(100, 400),
    .border = debug_border,
  }) {

    /* Wabisabi Window - Background */
    CLAY({
      .layout.sizing.width  = CLAY_SIZING_GROW(0),
      .layout.sizing.height = CLAY_SIZING_GROW(0),

      // .layout.padding
      .floating.offset = { 5, 3 },

      .floating.zIndex = -1,
      .floating.attachTo = CLAY_ATTACH_TO_PARENT,
      .floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_TOP,
      .floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_TOP,
      .floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,

      .image = UI_IMAGE_FIT(UiWindowBg),
    });

    /* Wabisabi Window - Corner Emblem */
    size_t corner_size = 75;
    CLAY({
      .layout.sizing.width  = CLAY_SIZING_FIXED(corner_size),
      .layout.sizing.height = CLAY_SIZING_FIXED(corner_size),

      .floating.attachTo = CLAY_ATTACH_TO_PARENT,
      .floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_TOP,
      .floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_TOP,
      .floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,

      .image.sourceDimensions = { 1, 1 },
      .image.imageData = gl_Model_UiWindowCorner,
      .image.transform = f4x4_scale(1),

      .layout.childAlignment.x = CLAY_ALIGN_X_CENTER,
      .layout.childAlignment.y = CLAY_ALIGN_Y_CENTER,

      .border = debug_border,
    }) {
      ui_icon(gl_Model_UiOptions, 50);
    }

    CLAY({
      .layout.sizing.width  = CLAY_SIZING_GROW(100),
      .layout.sizing.height = CLAY_SIZING_GROW(55),
      .layout.padding.top = 7,
      .layout.padding.left = 7,
      .layout.padding.bottom = 2,
      .layout.layoutDirection = CLAY_TOP_TO_BOTTOM,
    }) {
      Clay_ElementId drag_bar_id = CLAY_SID(window_title);

      /* Wabisabi Window - Top Bar */
      CLAY({
        .layout.sizing.width  = CLAY_SIZING_GROW(0),
        .layout.sizing.height = CLAY_SIZING_FIXED(45),
        .layout.childAlignment.x = CLAY_ALIGN_X_CENTER,
        .layout.childAlignment.y = CLAY_ALIGN_Y_CENTER,
      }) {

        /* drawing the top bar itself */
        CLAY({
          .layout.sizing.width  = CLAY_SIZING_GROW(0),
          .layout.sizing.height = CLAY_SIZING_GROW(0),
          .layout.padding.left = 50,
          .floating.attachTo = CLAY_ATTACH_TO_PARENT,
          .floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_TOP,
          .floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_TOP,
          .floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
        }) {

          CLAY({
            .layout.sizing.width  = CLAY_SIZING_GROW(0),
            .layout.sizing.height = CLAY_SIZING_GROW(0),
            .image = UI_IMAGE_FIT(UiWindowTop),
            .border = debug_border,
          }) {
          }

        }

        /* click and drag to move behavior */
        {
          if (Clay_Hovered()) {
            jeux.sdl.cursor_next = jeux.sdl.cursor_move;
            if (gui.lmb_down) {
              window->lmb_down_x = window->x;
              window->lmb_down_y = window->y;
              gui.lmb_down_el = drag_bar_id;
            }
          }

          bool held = gui.lmb_down_el.id == drag_bar_id.id;

          if (held) {
            float dx = jeux.mouse_ui_x - jeux.mouse_ui_lmb_down_x;
            float dy = jeux.mouse_ui_y - jeux.mouse_ui_lmb_down_y;
            window->x = window->lmb_down_x + dx;
            window->y = window->lmb_down_y + dy;
          }
        }

        CLAY({ .layout.padding.top = 8 }) {
          CLAY_TEXT(window_title, CLAY_TEXT_CONFIG({ .fontSize = 30, .textColor = ink }));
        };

        CLAY({
          .floating.attachTo = CLAY_ATTACH_TO_PARENT,
          .floating.attachPoints.element = CLAY_ATTACH_POINT_RIGHT_CENTER,
          .floating.attachPoints.parent = CLAY_ATTACH_POINT_RIGHT_CENTER,
          .floating.offset.x = -15,
          .floating.offset.y =   5,
        }) {
          size_t icon_size = 20;
          if (Clay_Hovered()) {
            jeux.sdl.cursor_next = jeux.sdl.cursor_doable;
            icon_size = 22;
            if (gui.lmb_click) window->open ^= 1;
          }
          ui_icon(gl_Model_UiEcksButton, icon_size);
        };
      }

      /* Wabisabi Window - Content */
      CLAY({
        .layout.sizing.width  = CLAY_SIZING_GROW(0),
        .layout.sizing.height = CLAY_SIZING_GROW(0),

        .layout.padding.top = 16,
        .layout.padding.bottom = 24,
        .layout.padding.left = 24,
        .layout.padding.right = 24,

        .border = debug_border,
      }) {

        /* Wabisabi Window - Bottom Border */
        CLAY({
          .layout.sizing.width  = CLAY_SIZING_GROW(0),
          .layout.sizing.height = CLAY_SIZING_GROW(0),

          .floating.attachTo = CLAY_ATTACH_TO_PARENT,
          .floating.attachPoints.element = CLAY_ATTACH_POINT_LEFT_TOP,
          .floating.attachPoints.parent = CLAY_ATTACH_POINT_LEFT_TOP,
          .floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
          .floating.offset = { 1, 2 },

          .image = UI_IMAGE_FIT(UiWindowBorder),

        });

        CLAY({
          .layout.sizing.width  = CLAY_SIZING_GROW(0),
          .layout.sizing.height = CLAY_SIZING_GROW(0),

          .layout.padding.right = 12,

          .scroll = { .vertical = true },
        }) {
          content_fn();
        }
      }

    }

  }
}

static void ui_window_content_options(void) {

  CLAY({
      .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM },
      .layout.sizing.width = CLAY_SIZING_GROW(0)
  }) {

    Clay_ElementDeclaration pair = {
      .layout.sizing.width = CLAY_SIZING_GROW(0),
      .layout.padding.top = 20,
      .layout.padding.bottom = 20,
    };

    Clay_ElementDeclaration pair_inner = {
      .layout.sizing.width = CLAY_SIZING_GROW(0),
      .layout.sizing.height = CLAY_SIZING_GROW(0),
    };

    Clay_TextElementConfig label = {
      .fontSize = text_body,
      .textColor = ink
    };

    /* antialiasing approach dropdown */
    CLAY(pair) {
      CLAY(pair_inner) { CLAY_TEXT(CLAY_STRING("ANTIALIASING"), CLAY_TEXT_CONFIG(label)); };
      Clay_String labels[] = {
        [gl_AntiAliasingApproach_None  ] = CLAY_STRING("NONE"),
        [gl_AntiAliasingApproach_Linear] = CLAY_STRING("Linear"),
        [gl_AntiAliasingApproach_FXAA  ] = CLAY_STRING("FXAA"),
        [gl_AntiAliasingApproach_2XSSAA] = CLAY_STRING("2x SSAA"),
        [gl_AntiAliasingApproach_4XSSAA] = CLAY_STRING("4x SSAA"),
      };
      CLAY(pair_inner) {
        bool changed = ui_picker(&jeux.gl.pp.current_aa, gl_AntiAliasingApproach_COUNT, labels);
        if (changed) gl_set_antialiasing_approach(jeux.gl.pp.current_aa);
      };
    }

    /* ui scale slider */
    CLAY(pair) {
      CLAY(pair_inner) { CLAY_TEXT(CLAY_STRING("UI SCALE"), CLAY_TEXT_CONFIG(label)); }
      CLAY(pair_inner) {

        float ui_scale_min = 0.2f;
        float ui_scale_max = 2.0f;

        /* this is mostly for ZII */
        if (gui.options.ui_scale_tmp < ui_scale_min)
          gui.options.ui_scale_tmp = jeux.ui_scale;

        /* don't actually apply it until you let go because scaling something as you
         * move it around is WEIRD */
        bool released = ui_slider(
          CLAY_ID("UI SCALE SLIDER"),
          &gui.options.ui_scale_tmp,
          ui_scale_min,
          ui_scale_max
        );

        if (released) {
          f3 p = { gui.options.window.x, gui.options.window.y };
          p = f4x4_transform_f3(jeux.ui_transform, p);

          jeux.ui_scale = gui.options.ui_scale_tmp;
          gl_resize();

          p = f4x4_transform_f3(f4x4_invert(jeux.ui_transform), p);
          gui.options.window.x = fmaxf(p.x, 0);
          gui.options.window.y = fmaxf(p.y, 0);

          f3 ui = jeux_screen_to_ui((f3) { jeux.win_size_x, jeux.win_size_y, 0 });
          Clay_SetLayoutDimensions((Clay_Dimensions) { ui.x, ui.y });
        }
      }
    }

#if GAME_DEBUG

    /* "CAMERA" header */
    CLAY({ .layout.sizing.height = CLAY_SIZING_FIXED(30) });
    CLAY_TEXT(CLAY_STRING("CAMERA"), CLAY_TEXT_CONFIG({ .fontSize = 30, .textColor = ink }));
    CLAY({ .layout.sizing.height = CLAY_SIZING_FIXED(20) });

    /* perspective checkbox */
    CLAY(pair) {

      CLAY(pair_inner) {
        CLAY_TEXT(CLAY_STRING("PERSPECTIVE"), CLAY_TEXT_CONFIG(label));
      }

      CLAY({ .layout.sizing = { .width = CLAY_SIZING_GROW(0) } }) {
        CLAY({ .layout.sizing.width = CLAY_SIZING_GROW(0) });
        ui_checkbox(&jeux.gl.camera.perspective);
        CLAY({ .layout.sizing.width = CLAY_SIZING_GROW(0) });
      }

    }

    /* FOV slider */
    if (jeux.gl.camera.perspective) CLAY(pair) {
      CLAY(pair_inner) { CLAY_TEXT(CLAY_STRING("FOV"), CLAY_TEXT_CONFIG(label)); }
      CLAY(pair_inner) { ui_slider(CLAY_ID("FOV_SLIDER"), &jeux.gl.camera.fov, 35, 100); }
    }

    /* camera dist slider */
    if (jeux.gl.camera.perspective) CLAY(pair) {
      CLAY(pair_inner) { CLAY_TEXT(CLAY_STRING("DIST"), CLAY_TEXT_CONFIG(label)); }
      CLAY(pair_inner) { ui_slider(CLAY_ID("DIST_SLIDER"), &jeux.gl.camera.dist, 2, 10); }
    }

    /* camera angle slider */
    CLAY(pair) {
      CLAY(pair_inner) { CLAY_TEXT(CLAY_STRING("CAMERA ANGLE"), CLAY_TEXT_CONFIG(label)); }
      CLAY(pair_inner) { ui_slider(CLAY_ID("CAMERA_ANGLE_SLIDER"), &jeux.gl.camera.angle, -M_PI, M_PI); }
    }

    /* camera height slider */
    CLAY(pair) {
      CLAY(pair_inner) { CLAY_TEXT(CLAY_STRING("CAMERA HEIGHT"), CLAY_TEXT_CONFIG(label)); }
      CLAY(pair_inner) { ui_slider(CLAY_ID("CAMERA_HEIGHT_SLIDER"), &jeux.gl.camera.height, 0.1f, 4.0f); }
    }

    /* "LIGHTING" header */
    CLAY({ .layout.sizing.height = CLAY_SIZING_FIXED(30) });
    CLAY_TEXT(CLAY_STRING("LIGHTING"), CLAY_TEXT_CONFIG({ .fontSize = 30, .textColor = ink }));
    CLAY({ .layout.sizing.height = CLAY_SIZING_FIXED(20) });

    /* light angle slider */
    CLAY(pair) {
      CLAY(pair_inner) { CLAY_TEXT(CLAY_STRING("LIGHT ANGLE"), CLAY_TEXT_CONFIG(label)); }
      CLAY(pair_inner) { ui_slider(CLAY_ID("LIGHT_ANGLE_SLIDER"), &jeux.gl.light.angle, -M_PI, M_PI); }
    }

    /* camera height slider */
    CLAY(pair) {
      CLAY(pair_inner) { CLAY_TEXT(CLAY_STRING("LIGHT HEIGHT"), CLAY_TEXT_CONFIG(label)); }
      CLAY(pair_inner) { ui_slider(CLAY_ID("LIGHT_HEIGHT_SLIDER"), &jeux.gl.light.height, 0.1f, 4.0f); }
    }


#endif

  }
}


void gl_geo_box_rounded(f3 min, f3 max, Color color, float r) {
  /* corner arcs */
  {
    float q = M_PI * 0.5f;
    gl_geo_arc(2*q, 3*q, 16, (f3) { min.x + r, min.y + r, max.z }, r, color);
    gl_geo_arc(1*q, 2*q, 16, (f3) { min.x + r, max.y - r, max.z }, r, color);
    gl_geo_arc(3*q, 4*q, 16, (f3) { max.x - r, min.y + r, max.z }, r, color);
    gl_geo_arc(0*q, 1*q, 16, (f3) { max.x - r, max.y - r, max.z }, r, color);
  }

  /* min and max inset by radius */
  f3 rmin = { min.x + r, min.y + r, max.z };
  f3 rmax = { max.x - r, max.y - r, max.z };
  gl_geo_box(rmin, rmax, color);

  float h = r * 0.5f;
  rmin = (f3) { min.x + h, min.y + h, max.z };
  rmax = (f3) { max.x - h, max.y - h, max.z };
  gl_geo_line((f3) { rmin.x+h, rmin.y  , max.z }, (f3) { rmax.x-h, rmin.y  , max.z }, r, color);
  gl_geo_line((f3) { rmin.x  , rmax.y-h, max.z }, (f3) { rmin.x  , rmin.y+h, max.z }, r, color);
  gl_geo_line((f3) { rmax.x-h, rmax.y  , max.z }, (f3) { rmin.x+h, rmax.y  , max.z }, r, color);
  gl_geo_line((f3) { rmax.x  , rmin.y+h, max.z }, (f3) { rmax.x  , rmax.y-h, max.z }, r, color);
}

static void ui_main(void) {
  /* mouse_up resets lmb_down_el at the end of the frame so that elements
   * have a frame to clean up (e.g. gui.lmb_click && gui.lmb_down_el == me) */
  bool mouse_up = Clay_GetCurrentContext()->pointerInfo.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME;
  gui.lmb_click = mouse_up;
  gui.lmb_down = Clay_GetCurrentContext()->pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME;


  /* HUD */
  CLAY({
    .id = CLAY_ID("Root"),
    .layout = {
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
      .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }
    }
  }) {

    CLAY({
      .id = CLAY_ID("HeaderBar"),
      .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(64)},
        .padding = CLAY_PADDING_ALL(16),
        .childGap = 16,
        .childAlignment = { .x = CLAY_ALIGN_X_LEFT }
      },
    }) {
      CLAY({
        .id = CLAY_ID("OptionsToggle"),
        .border = { .color = wood, .width = { 2, 2, 2, 2 }},
        .backgroundColor = Clay_Hovered() ? paper_hover : paper,
        .layout.sizing = { CLAY_SIZING_FIXED(64), CLAY_SIZING_FIXED(64) },
        .cornerRadius = CLAY_CORNER_RADIUS(32),
      }) {
        if (Clay_Hovered()) {
          jeux.sdl.cursor_next = jeux.sdl.cursor_doable;
        }
        if (Clay_Hovered() && gui.lmb_click) {
          gui.options.window.open ^= 1;
        }

        /* icon here */
        CLAY({
          .layout.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
          .layout.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
        }) {
          ui_icon(gl_Model_UiOptions, 44);
        }
      }
    }

    CLAY({
      .id = CLAY_ID("Main"),
      .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
        .padding = CLAY_PADDING_ALL(16),
        .childGap = 16,
        .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_BOTTOM }
      },      
    }) { }

  }

  ui_wabisabi_window(
    gl_Model_UiOptions,
    CLAY_STRING("OPTIONS"),
    &gui.options.window,
    &ui_window_content_options
  );

  // static float t = 0;
  //item_grid(200, 20, 200+sinf(t)*50, 300);
  // t += .01f;

  if (mouse_up) gui.lmb_down_el = (Clay_ElementId) { 0 };
}

#undef gui
