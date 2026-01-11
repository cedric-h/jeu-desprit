// vim: sw=2 ts=2 expandtab smartindent

/* This file is used as an index of all the 2D and 3D vector-based assets we import and use */

#include "../models/include/Head.h"
#include "../models/include/HornedHelmet.h"
#include "../svg/include/UiOptions.svg.h"
#include "../svg/include/UiArrowButton.svg.h"
#include "../svg/include/UiCheck.svg.h"
#include "../svg/include/UiCheckBox.svg.h"
#include "../svg/include/UiEcksButton.svg.h"
#include "../svg/include/UiSliderBody.svg.h"
#include "../svg/include/UiSliderHandle.svg.h"
#include "../svg/include/UiWindowCorner.svg.h"
#include "../svg/include/UiWindowTop.svg.h"
#include "../svg/include/UiWindowBorder.svg.h"
#include "../svg/include/UiWindowBg.svg.h"

struct {
  gl_geo_Vtx *vtx;
  gl_Tri *tri;
  size_t vtx_count;
  size_t tri_count;
} gl_modeldata[gl_Model_COUNT] = {
  [gl_Model_Head] = {
      .vtx = model_vtx_Head, .vtx_count = jx_COUNT(model_vtx_Head),
      .tri = model_tri_Head, .tri_count = jx_COUNT(model_tri_Head),
  },
  [gl_Model_HornedHelmet] = {
      .vtx = model_vtx_HornedHelmet, .vtx_count = jx_COUNT(model_vtx_HornedHelmet),
      .tri = model_tri_HornedHelmet, .tri_count = jx_COUNT(model_tri_HornedHelmet),
  },

  /* TODO: In retrospect, the asset cookers really should output these sized fat pointer structs. You live and you learn. */
  [gl_Model_UiOptions     ] = { .vtx = model_vtx_UiOptions     , .vtx_count = jx_COUNT(model_vtx_UiOptions     ), .tri = model_tri_UiOptions     , .tri_count = jx_COUNT(model_tri_UiOptions     ) },
  [gl_Model_UiArrowButton ] = { .vtx = model_vtx_UiArrowButton , .vtx_count = jx_COUNT(model_vtx_UiArrowButton ), .tri = model_tri_UiArrowButton , .tri_count = jx_COUNT(model_tri_UiArrowButton ) },
  [gl_Model_UiCheck       ] = { .vtx = model_vtx_UiCheck       , .vtx_count = jx_COUNT(model_vtx_UiCheck       ), .tri = model_tri_UiCheck       , .tri_count = jx_COUNT(model_tri_UiCheck       ) },
  [gl_Model_UiCheckBox    ] = { .vtx = model_vtx_UiCheckBox    , .vtx_count = jx_COUNT(model_vtx_UiCheckBox    ), .tri = model_tri_UiCheckBox    , .tri_count = jx_COUNT(model_tri_UiCheckBox    ) },
  [gl_Model_UiEcksButton  ] = { .vtx = model_vtx_UiEcksButton  , .vtx_count = jx_COUNT(model_vtx_UiEcksButton  ), .tri = model_tri_UiEcksButton  , .tri_count = jx_COUNT(model_tri_UiEcksButton  ) },
  [gl_Model_UiSliderBody  ] = { .vtx = model_vtx_UiSliderBody  , .vtx_count = jx_COUNT(model_vtx_UiSliderBody  ), .tri = model_tri_UiSliderBody  , .tri_count = jx_COUNT(model_tri_UiSliderBody  ) },
  [gl_Model_UiSliderHandle] = { .vtx = model_vtx_UiSliderHandle, .vtx_count = jx_COUNT(model_vtx_UiSliderHandle), .tri = model_tri_UiSliderHandle, .tri_count = jx_COUNT(model_tri_UiSliderHandle) },
  [gl_Model_UiWindowCorner] = { .vtx = model_vtx_UiWindowCorner, .vtx_count = jx_COUNT(model_vtx_UiWindowCorner), .tri = model_tri_UiWindowCorner, .tri_count = jx_COUNT(model_tri_UiWindowCorner) },
  [gl_Model_UiWindowTop   ] = { .vtx = model_vtx_UiWindowTop   , .vtx_count = jx_COUNT(model_vtx_UiWindowTop   ), .tri = model_tri_UiWindowTop   , .tri_count = jx_COUNT(model_tri_UiWindowTop   ) },
  [gl_Model_UiWindowBorder] = { .vtx = model_vtx_UiWindowBorder, .vtx_count = jx_COUNT(model_vtx_UiWindowBorder), .tri = model_tri_UiWindowBorder, .tri_count = jx_COUNT(model_tri_UiWindowBorder) },
  [gl_Model_UiWindowBg    ] = { .vtx = model_vtx_UiWindowBg    , .vtx_count = jx_COUNT(model_vtx_UiWindowBg    ), .tri = model_tri_UiWindowBg    , .tri_count = jx_COUNT(model_tri_UiWindowBg    ) },
};

