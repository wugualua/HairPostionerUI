// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorState.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

int main() {
    using namespace HPUI;
    State state;
    state.beginSession = false;
    state.valid = true;
    state.selected = Target{0, 0, "Hair"};
    const auto hair = state.selected;
    std::array<float, 15> values{};
    values[0] = 2.0f;
    state.busy = true; // A current-value read must not block edits or show Applying.
    assert(!Pending(state));
    state.queued[0] = 3.0f; // Input accepted while the read is in flight.
    assert(Pending(state));
    assert(StoreSnapshot(state, hair, values));
    assert(state.queued[0] == 3.0f); // A read reply must not erase accepted input.
    state.queued[0].reset();
    state.catalogBusy = true;
    assert(!Pending(state)); // A catalog read is also read-only.
    state.restoreQueued = State::Restore{};
    assert(Pending(state));
    state.restoreQueued.reset();
    state.resetQueued = true;
    assert(Pending(state));
    state.resetQueued = false;
    state.writeBusy = true; // Includes writes whose queue slot has already been consumed.
    assert(Pending(state));
    state.writeBusy = false;
    state.catalogBusy = false;
    state.selected = Target{1, 0, "Beard"};
    state.valid = false;
    auto previous = state.snapshot;
    values[0] = 9.0f;
    assert(!StoreSnapshot(state, hair, values));
    assert(!state.valid && state.snapshot == previous);
    assert(StoreSnapshot(state, state.selected, values));
    assert(state.valid && state.snapshot[0] == 9.0f);
    state.queued[0] = 4.0f;
    state.resetQueued = true;
    state.restoreQueued = State::Restore{};
    DiscardTargetEdits(state); // Catalog removal must not retarget queued writes.
    assert(!Pending(state) && !state.activeEdit && !state.restoreActive);
    std::cout << "Background refresh, pending edits, stale replies and removed-target safety passed\n";
}
