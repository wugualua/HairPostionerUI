// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "EditHistory.h"
#include "Language.h"
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
namespace HPUI {
    using Clock = std::chrono::steady_clock;
    using Localization::L;
    constexpr std::size_t Rows = 11;
    struct Target {
        std::int32_t part = 0;
        std::int32_t item = 0;
        std::string name = L(0);
        bool Same(const Target& other) const { return part == other.part && item == other.item; }
    };
    struct State {
        std::array<float, 15> snapshot{};
        std::vector<Target> targets;
        Target selected;
        bool catalogRefresh = true;
        bool catalogBusy = false;
        Clock::time_point lastCatalog{};
        std::array<std::optional<float>, Rows> queued{};
        std::optional<std::pair<int, float>> activeEdit;
        bool valid = false;
        bool busy = false; // Any in-flight bridge request; serializes reads and writes.
        bool writeBusy = false; // Only mutations block selection/recovery or show Applying.
        bool resetQueued = false;
        bool refresh = true;
        bool beginSession = true;
        std::map<std::pair<std::int32_t, std::int32_t>, EditHistory> histories;
        struct Restore { EditValues desired; EditValues before; bool undo; };
        std::optional<Restore> restoreQueued;
        std::optional<Restore> restoreActive;
        std::uint64_t generation = 0;
        std::string error;
        Clock::time_point started{};
        Clock::time_point lastReply{};
    };
    inline bool Pending(const State& source) {
        return source.writeBusy || source.resetQueued || source.restoreQueued || source.restoreActive ||
            std::any_of(source.queued.begin(), source.queued.end(), [](const auto& v) { return v.has_value(); });
    }
    inline bool StoreSnapshot(State& state, const Target& target, const std::array<float, 15>& data) {
        if (!target.Same(state.selected)) { return false; }
        state.snapshot = data; state.valid = true; state.error.clear();
        return true;
    }
    inline void DiscardTargetEdits(State& state) {
        state.queued.fill(std::nullopt); state.resetQueued = false;
        state.restoreQueued.reset(); state.restoreActive.reset(); state.activeEdit.reset();
    }
}
