#include "scale.h"

using namespace synthux;

Scale::Scale():
_scale_index  { 0 },
_trans_index  { 12 }
{
  _PrepareScale();
};
