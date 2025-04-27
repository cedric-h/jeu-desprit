// vim: sw=2 ts=2 expandtab smartindent

typedef enum {
  animdata_JointKey_Hips,
  animdata_JointKey_RightArm,
  animdata_JointKey_RightForeArm,
  animdata_JointKey_RightHand,
  animdata_JointKey_Neck,
  animdata_JointKey_Head,
  animdata_JointKey_LeftArm,
  animdata_JointKey_LeftForeArm,
  animdata_JointKey_LeftHand,
  animdata_JointKey_RightLeg,
  animdata_JointKey_RightToeBase,
  animdata_JointKey_RightToe_End,
  animdata_JointKey_LeftLeg,
  animdata_JointKey_LeftToeBase,
  animdata_JointKey_LeftToe_End,
  animdata_JointKey_COUNT
} animdata_JointKey;

struct { animdata_JointKey from, to; } animdata_limb_connections[] = {
  { .from = animdata_JointKey_RightLeg,     .to = animdata_JointKey_RightToeBase },
  { .from = animdata_JointKey_LeftLeg,      .to = animdata_JointKey_LeftToeBase  },
  { .from = animdata_JointKey_Neck,         .to = animdata_JointKey_RightArm     },
  { .from = animdata_JointKey_Neck,         .to = animdata_JointKey_LeftArm      },
  { .from = animdata_JointKey_Hips,         .to = animdata_JointKey_RightLeg     },
  { .from = animdata_JointKey_Hips,         .to = animdata_JointKey_LeftLeg      },
  { .from = animdata_JointKey_Hips,         .to = animdata_JointKey_Neck         },
  { .from = animdata_JointKey_RightArm,     .to = animdata_JointKey_RightForeArm },
  { .from = animdata_JointKey_RightForeArm, .to = animdata_JointKey_RightHand    },
  { .from = animdata_JointKey_Neck,         .to = animdata_JointKey_Head         },
  { .from = animdata_JointKey_LeftArm,      .to = animdata_JointKey_LeftForeArm  },
  { .from = animdata_JointKey_LeftForeArm,  .to = animdata_JointKey_LeftHand     },
  { .from = animdata_JointKey_RightToeBase, .to = animdata_JointKey_RightToe_End },
  { .from = animdata_JointKey_LeftToeBase,  .to = animdata_JointKey_LeftToe_End  },
};

typedef struct {
    float time;
    f3 joint_pos[animdata_JointKey_COUNT];
} animdata_Frame;

f3 animdata_base_pose[animdata_JointKey_COUNT] = {
  [animdata_JointKey_Hips        ] = { -0.00031874649226665497, -0.015492378473281861, 1.0216461944580078 },
  [animdata_JointKey_RightArm    ] = { -0.15209491310914705, 0.019500644285726502, 1.418633234974889 },
  [animdata_JointKey_RightForeArm] = { -0.21488229405010736, 0.04400890470839535, 1.1484999113894279 },
  [animdata_JointKey_RightHand   ] = { -0.2576714998512554, -0.018782932989074644, 0.8755926581399418 },
  [animdata_JointKey_LeftArm     ] = { 0.14880437294046842, 0.015882980656392626, 1.4267212806945073 },
  [animdata_JointKey_LeftForeArm ] = { 0.208389707808596, 0.010840366599760091, 1.1548038703554349 },
  [animdata_JointKey_LeftHand    ] = { 0.22561565863509345, -0.061929426038347425, 0.8815641974858499 },
  [animdata_JointKey_Neck        ] = { -0.0023556812666045944, -0.01618214795797257, 1.485210109899372 },
  [animdata_JointKey_Head        ] = { -0.009614373670825484, -0.03974096063570164, 1.579690432650388 },
  [animdata_JointKey_RightLeg    ] = { -0.11980894201819188, -0.06771311581586936, 0.5182660476911155 },
  [animdata_JointKey_RightToeBase] = { -0.15308204531213618, -0.05995049340709224, 0.00000912873879903131 },
  [animdata_JointKey_RightToe_End] = { -0.17536306465528514, -0.14978910662272862, 0.006403950225510895 },
  [animdata_JointKey_LeftLeg     ] = { 0.15396560637208004, -0.09336890638921524, 0.5246280786652762 },
  [animdata_JointKey_LeftToeBase ] = { 0.21825566184358358, -0.1323508653775437, 0.000009560816774012438 },
  [animdata_JointKey_LeftToe_End ] = { 0.23055954096239675, -0.22386654206013057, 0.0090544371624689 },
};

static f3 animdata_sample(
  animdata_Frame    *animdata_frames,
  size_t             animdata_frame_count,
  float              animdata_duration,
  animdata_JointKey  joint,
  float              anim_t
) {
  size_t rhs_frame = -1;
  for (size_t i = 0; i < animdata_frame_count; i++)
    if (animdata_frames[i].time > anim_t) { rhs_frame = i; break; }
  bool last_frame = rhs_frame == -1;

  size_t lhs_frame = (last_frame ? animdata_frame_count : rhs_frame) - 1;
  rhs_frame = (lhs_frame + 1) % animdata_frame_count;
  float next_frame_t = last_frame ? animdata_duration : animdata_frames[rhs_frame].time;
  float this_frame_t = animdata_frames[lhs_frame].time;
  float tween_t = (anim_t - this_frame_t) / (next_frame_t - this_frame_t);

  return f3_lerp(
    animdata_frames[lhs_frame].joint_pos[joint],
    animdata_frames[rhs_frame].joint_pos[joint],
    tween_t
  );
}

#include "../animations/include/walk.h"
#include "../animations/include/turn90_right.h"
#include "../animations/include/turn90_left.h"

