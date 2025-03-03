#include "common.slang"

struct fragment_t {
  float4 color : COLOR0;
};

[shader("fragment")]
fragment_t fragment_main(per_fragment_t per_fragment) : SV_Target {
  fragment_t o;

  o.color = float4(1, 1, 1, 1);

  return o;
}
