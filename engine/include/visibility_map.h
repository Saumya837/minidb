#pragma once
#include "types.h"

namespace minidb {
// TODO: tracks pages where all tuples are visible to all transactions
// Mirrors Postgres VM — allows vacuum to skip clean pages
class VisibilityMap {
};
} // namespace minidb