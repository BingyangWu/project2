#include "IR.h"
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace Boost{
namespace Internal{

Group toGrad(const vector<string> & gradient_vec, const Group & origin_kernel);
}
}