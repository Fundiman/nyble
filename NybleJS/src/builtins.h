#pragma once
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cfloat>
#include "env.h"

namespace nyble {

void installBuiltins(std::shared_ptr<Environment> env);

}
