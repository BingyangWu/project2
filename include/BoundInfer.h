#include "IR.h"

namespace Boost{
namespace Internal{

Group bound_infer(const Group & origin_kernel, const Group & new_kernel);
Stmt loop_bound_infer(const Stmt & origin_loop);
}
}