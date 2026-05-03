#pragma once
#include "types.h"

namespace minidb {
// TODO: tracks free space per page for insert routing
// Mirrors Postgres FSM — avoids full page scans on INSERT
class FreeSpaceMap {
};
} // namespace minidb