#pragma once

// NybleJS embedding header (Ant Silver backend).
//
// The preferred embedding surface is the SpiderMonkey-style API in
// engine/nyble_sm.h (C) and engine/nyble_sm.hpp (C++): Runtime, Context,
// rooted values and explicit evaluate/get/set/call operations.
//
// engine/ant_engine.h remains the small nyble::Engine façade used by the CLI.
// Ant's C-only headers are never exposed here; engine/ant_engine.c is the
// sole bridge.

#include "engine/nyble_sm.h"
#include "engine/nyble_sm.hpp"
#include "engine/ant_engine.h"
