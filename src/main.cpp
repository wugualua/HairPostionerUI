// SPDX-License-Identifier: GPL-3.0-or-later
#include "pch.h"
#include "SKSEMenuFramework.h"
#include <charconv>
#include <map>
#include "EditHistory.h"
#include "Language.h"
#include "EditorState.h"
namespace ImGui = ImGuiMCP;

namespace HPUI
{
    using Localization::L;
    constexpr char Script[] = "HPUIBridge";
    State state;
    std::mutex stateMutex;
    SKSEMenuFramework::Model::WindowInterface* window = nullptr;
    std::atomic<bool> inGame{false};
    constexpr std::uint32_t DefaultHotkey = RE::BSKeyboardDevice::Key::kF3;
    std::atomic<std::uint32_t> hotkey{DefaultHotkey};
    std::atomic<bool> capturingKey{false};
    std::atomic<int> precision{1};
    std::atomic<bool> manipulating{false};
    std::mutex settingsMutex;
    std::string settingsMessage;
    std::filesystem::path settingsPath;
    void Pump();

    auto Identity(const Target& target) { return std::pair{target.part, target.item}; }
    EditValues Values(const std::array<float, 15>& snapshot) {
        EditValues result{};
        std::copy_n(snapshot.begin(), 10, result.begin()); result[10] = snapshot[14];
        return result;
    }
    EditValues EffectiveValues(const State& source) {
        auto result = Values(source.snapshot);
        if (source.activeEdit) { result[source.activeEdit->first] = source.activeEdit->second; }
        for (std::size_t i = 0; i < Rows; ++i) { if (source.queued[i]) { result[i] = *source.queued[i]; } }
        return result;
    }
    double Now() { return std::chrono::duration<double>(Clock::now().time_since_epoch()).count(); }
    void QueueRestore(const Target& expected, bool undo) {
        std::scoped_lock lock(stateMutex);
        if (Pending(state) || state.beginSession || !state.valid || !state.selected.Same(expected)) { return; }
        auto& history = state.histories[Identity(expected)];
        if (!history.baseline || (undo && history.undo.empty())) { return; }
        const auto before = EffectiveValues(state);
        auto desired = undo ? history.undo.back() : *history.baseline;
        if (!undo && SameValues(before, desired)) { return; }
        for (std::size_t i = 0; i < Rows; ++i) { desired[i] = ClampEdit(desired[i], static_cast<int>(i), expected.part); }
        history.EndGesture();
        state.restoreQueued = State::Restore{desired, before, undo};
    }

    std::string KeyName(std::uint32_t code) {
        if (!code) { return L(1); }
        wchar_t wide[64]{};
        LONG scan = static_cast<LONG>((code & 0x7f) << 16);
        if (code & 0x80) { scan |= 1 << 24; }
        if (GetKeyNameTextW(scan, wide, 64) > 0) {
            char utf8[256]{};
            if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr)) { return utf8; }
        }
        return std::string(L(2)) + std::to_string(code);
    }

    bool SaveHotkey(std::uint32_t code) {
        std::scoped_lock lock(settingsMutex);
        auto value = std::to_wstring(code);
        if (!WritePrivateProfileStringW(L"Hotkey", L"Key", value.c_str(), settingsPath.c_str())) {
            settingsMessage = L(3);
            SKSE::log::error("Hotkey save failed, Windows error {}", GetLastError());
            return false;
        }
        hotkey = code;
        capturingKey = false;
        settingsMessage = std::string(L(4)) + KeyName(code);
        SKSE::log::info("Hotkey saved: {}", code);
        return true;
    }

    void LoadHotkey() {
        settingsPath = std::filesystem::absolute(L"Data/SKSE/Plugins/HairPositionerUI.ini");
        wchar_t language[64]{};
        GetPrivateProfileStringW(L"General", L"Language", L"english", language, 64, settingsPath.c_str());
        const auto selectedLanguage = Localization::NormalizeLanguage(language);
        const auto directory = settingsPath.parent_path() / "HairPositionerUI" / "translations";
        const auto result = Localization::Load(directory / (selectedLanguage + ".csv"));
        SKSE::log::info("UI language: {}; CSV: {}; loaded: {}; rejected: {}", selectedLanguage, result.status, result.loaded, result.rejected);
        auto code = GetPrivateProfileIntW(L"Hotkey", L"Key", DefaultHotkey, settingsPath.c_str());
        if (code > 255) { SKSE::log::warn("Invalid hotkey {}, using F3", code); code = DefaultHotkey; }
        hotkey = code;
        SKSE::log::info("Hotkey loaded: {} (0 = disabled)", code);
        auto level = GetPrivateProfileIntW(L"Editing", L"Precision", 1, settingsPath.c_str());
        precision = level <= 2 ? static_cast<int>(level) : 1;
    }

    void SavePrecision(int level) {
        std::scoped_lock lock(settingsMutex);
        auto value = std::to_wstring(level);
        if (WritePrivateProfileStringW(L"Editing", L"Precision", value.c_str(), settingsPath.c_str())) {
            precision = level; settingsMessage.clear();
        } else { settingsMessage = L(5); }
    }

    void RenderHotkeySettings() {
        ImGui::Separator();
        ImGui::Text(L(6), KeyName(hotkey.load()).c_str());
        if (capturingKey.load()) {
            ImGui::TextUnformatted(L(7));
            if (ImGui::Button(L(8))) { capturingKey = false; }
        } else if (ImGui::Button(L(9))) {
            capturingKey = true;
            std::scoped_lock lock(settingsMutex); settingsMessage.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button(L(10))) { SaveHotkey(0); }
        ImGui::SameLine();
        if (ImGui::Button(L(11))) { SaveHotkey(DefaultHotkey); }
        std::string message;
        { std::scoped_lock lock(settingsMutex); message = settingsMessage; }
        if (!message.empty()) { ImGui::TextWrapped("%s", message.c_str()); }
    }

    class Reply final : public RE::BSScript::IStackCallbackFunctor {
    public:
        Reply(std::uint64_t epoch, Target target) : epoch_(epoch), target_(std::move(target)) {}
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
        void operator()(RE::BSScript::Variable result) override {
            std::array<float, 15> data{};
            bool good = result.IsArray();
            auto values = good ? result.GetArray() : nullptr;
            good = values && values->size() == data.size();
            if (good) {
                for (std::size_t i = 0; i < data.size(); ++i) {
                    if (!(*values)[static_cast<std::uint32_t>(i)].IsFloat()) { good = false; break; }
                    data[i] = (*values)[static_cast<std::uint32_t>(i)].GetFloat();
                    if (!std::isfinite(data[i])) { good = false; break; }
                }
            }
            {
                std::scoped_lock lock(stateMutex);
                if (epoch_ != state.generation) { return; }
                if (good && StoreSnapshot(state, target_, data)) {
                    bool available = data[10] > .5f && data[11] > .5f && data[12] > .5f;
                    auto& history = state.histories[Identity(state.selected)];
                    if (available && !state.beginSession) { history.Observe(Values(data)); }
                    if (state.restoreActive) {
                        auto request = *state.restoreActive;
                        if (available && SameValues(Values(data), request.desired)) {
                            if (request.undo) { history.UndoSucceeded(); }
                            else { history.Record(request.before, -1, Now()); }
                        } else { state.error = L(12); }
                    }
                }
                else if (!good && target_.Same(state.selected)) { state.error = L(13); }
                state.busy = false; state.writeBusy = false;
                state.activeEdit.reset();
                state.restoreActive.reset();
                state.lastReply = Clock::now();
            }
            Pump(); // Flush already accepted edits even if the user closed the window.
        }
    private:
        std::uint64_t epoch_;
        Target target_;
    };

    class CatalogReply final : public RE::BSScript::IStackCallbackFunctor {
    public:
        explicit CatalogReply(std::uint64_t epoch) : epoch_(epoch) {}
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
        void operator()(RE::BSScript::Variable result) override {
            auto values = result.IsArray() ? result.GetArray() : nullptr;
            bool good = values && values->size() >= 6 && values->size() % 3 == 0;
            std::vector<Target> targets;
            if (good) {
                for (std::uint32_t i = 0; i < values->size(); i += 3) {
                    if (!(*values)[i].IsString() || !(*values)[i+1].IsString() || !(*values)[i+2].IsString()) {
                        good = false; break;
                    }
                    auto part = (*values)[i].GetString();
                    if (part.empty()) { continue; }
                    auto id = (*values)[i+1].GetString();
                    Target target;
                    auto p = std::from_chars(part.data(), part.data()+part.size(), target.part);
                    auto n = std::from_chars(id.data(), id.data()+id.size(), target.item);
                    if (p.ec != std::errc{} || p.ptr != part.data()+part.size() ||
                        n.ec != std::errc{} || n.ptr != id.data()+id.size() || target.part < 0 || target.part > 3) {
                        good = false; break;
                    }
                    auto name = std::string((*values)[i+2].GetString());
                    target.name = target.part == 0 ? L(0) : target.part == 1 ? L(14) :
                        (target.part == 2 ? L(15) : L(16)) + (name.empty() ? std::to_string(target.item) : name);
                    // Part + signed Int ID, rather than list position or name, owns every edit.
                    if (std::none_of(targets.begin(), targets.end(), [&](const auto& t) { return t.Same(target); })) {
                        targets.push_back(std::move(target));
                    }
                }
            }
            {
                std::scoped_lock lock(stateMutex);
                if (epoch_ != state.generation) { return; }
                state.busy = false; state.writeBusy = false; state.catalogBusy = false;
                state.lastCatalog = state.lastReply = Clock::now();
                if (good) {
                    bool changed = state.targets.size() != targets.size() ||
                        !std::equal(state.targets.begin(), state.targets.end(), targets.begin(), targets.end(),
                            [](const auto& a, const auto& b) { return a.Same(b) && a.name == b.name; });
                    if (changed) {
                        SKSE::log::info("Target catalog: {} entries", targets.size());
                        for (const auto& target : targets) {
                            SKSE::log::info("Target part={} id={:08X} name={}", target.part, static_cast<std::uint32_t>(target.item), target.name);
                        }
                    }
                    state.targets = std::move(targets);
                    auto current = std::find_if(state.targets.begin(), state.targets.end(), [](const auto& t) { return t.Same(state.selected); });
                    if (current != state.targets.end()) { state.selected = *current; }
                    else {
                        state.selected = state.targets.empty() ? Target{} : state.targets.front();
                        state.valid = false;
                        DiscardTargetEdits(state);
                    }
                    state.error.clear(); state.refresh = true;
                } else { state.error = L(17); }
            }
            Pump();
        }
    private:
        std::uint64_t epoch_;
    };

    void QueueValue(int row, float value, const Target& expected, int group = -1) {
        if (row < 0 || row >= static_cast<int>(Rows) || !std::isfinite(value)) { return; }
        std::scoped_lock lock(stateMutex);
        if (!state.valid || state.beginSession || state.restoreActive || state.restoreQueued || state.resetQueued || !state.selected.Same(expected)) { return; }
        value = ClampEdit(value, row, expected.part);
        auto before = EffectiveValues(state);
        if (std::abs(before[row] - value) < 0.000001f) { return; }
        state.histories[Identity(expected)].Record(before, group, Now());
        state.queued[row] = value; // Coalesce dragging; never enqueue an unbounded VM stack per frame.
    }

    void Pump() {
        std::uint64_t epoch;
        int row = -1;
        float value = 0;
        bool reset = false;
        bool catalog = false;
        std::optional<State::Restore> restore;
        Target target;
        {
            std::scoped_lock lock(stateMutex);
            if (state.busy) {
                if (Clock::now() - state.started > 10s) {
                    state.error = L(18);
                }
                return; // A timeout must not launch a duplicate write while an older VM stack still exists.
            }
            if (!inGame.load()) { return; }
            if (state.beginSession && !Pending(state)) {
                state.histories.clear(); state.valid = false;
                state.beginSession = false; state.refresh = true; state.catalogRefresh = true;
            }
            restore = state.restoreQueued;
            state.restoreQueued.reset();
            state.restoreActive = restore;
            reset = state.resetQueued;
            if (reset) { state.resetQueued = false; }
            else if (!restore) {
                for (std::size_t i = 0; i < Rows; ++i) {
                    if (state.queued[i]) {
                        row = static_cast<int>(i); value = *state.queued[i];
                        state.queued[i].reset(); break;
                    }
                }
            }
            bool visible = window && window->IsOpen.load();
            auto edited = state.histories.find(Identity(state.selected));
            bool recentEdit = edited != state.histories.end() && Now() - edited->second.lastChange < .4;
            catalog = visible && !recentEdit && !manipulating.load() && !restore && !reset && row < 0 &&
                (state.catalogRefresh || Clock::now() - state.lastCatalog >= 5s);
            if (!restore && !reset && row < 0 && !catalog && (!visible || (!state.refresh && Clock::now() - state.lastReply < 1s))) {
                return;
            }
            if (catalog) { state.catalogRefresh = false; }
            state.catalogBusy = catalog;
            state.refresh = false;
            state.busy = true;
            state.writeBusy = restore.has_value() || reset || row >= 0;
            state.started = Clock::now();
            epoch = state.generation;
            target = state.selected;
            if (row >= 0) { state.activeEdit = std::pair{row, value}; }
        }
        SKSE::GetTaskInterface()->AddTask([epoch, row, value, reset, catalog, target, restore] {
            {
                std::scoped_lock lock(stateMutex);
                if (epoch != state.generation || !inGame.load()) { return; }
            }
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            if (catalog) { callback.reset(new CatalogReply(epoch)); }
            else { callback.reset(new Reply(epoch, target)); }
            auto* args = catalog ? RE::MakeFunctionArguments() : restore ?
                RE::MakeFunctionArguments(std::int32_t(target.part), std::int32_t(target.item), std::vector<float>(restore->desired.begin(), restore->desired.end())) : row >= 0 ?
                RE::MakeFunctionArguments(std::int32_t(target.part), std::int32_t(target.item), std::int32_t(row), float(value)) :
                RE::MakeFunctionArguments(std::int32_t(target.part), std::int32_t(target.item));
            const char* function = catalog ? "TargetsV2" : restore ? "ApplyValuesV3" : reset ? "ResetTargetV2" : row >= 0 ? "SetValueV2" : "SnapshotV2";
            bool ok = vm && vm->DispatchStaticCall(Script, function, args, callback);
            if (!ok) {
                // The VM owns arguments after dispatch; no raw callbacks or DLL offsets are used.
                std::scoped_lock lock(stateMutex);
                if (epoch != state.generation) { return; }
                state.busy = false; state.writeBusy = false;
                state.catalogBusy = false;
                if (catalog) { state.lastCatalog = Clock::now(); }
                state.activeEdit.reset();
                state.restoreActive.reset();
                state.lastReply = Clock::now();
                state.error = L(19);
                SKSE::log::error("Papyrus dispatch failed: {}.{}", Script, function);
            }
        });
    }

    void __stdcall RenderWindow() {
        ImGui::SetNextWindowSize({510.f, 650.f}, ImGui::ImGuiCond_FirstUseEver);
        bool open = window->IsOpen.load();
        if (!ImGui::Begin(L(20), &open, ImGui::ImGuiWindowFlags_NoScrollWithMouse)) {
            manipulating = false;
            ImGui::End(); window->IsOpen = open; Pump(); return;
        }
        float wheel = ImGui::GetKeyData(ImGui::ImGuiKey_MouseWheelY)->AnalogValue;
        bool overValue = false;
        bool activeValue = false;
        State view;
        { std::scoped_lock lock(stateMutex); view = state; }
        ImGui::TextUnformatted(L(21));
        bool pending = Pending(view);
        ImGui::BeginDisabled(pending || view.targets.empty());
        ImGui::SetNextItemWidth(340.f);
        if (ImGui::BeginCombo(L(22), view.targets.empty() ? L(23) : view.selected.name.c_str())) {
            for (std::size_t index = 0; index < view.targets.size(); ++index) {
                const auto target = view.targets[index];
                ImGui::PushID(static_cast<int>(index));
                if (ImGui::Selectable(target.name.c_str(), target.Same(view.selected))) {
                    std::scoped_lock lock(stateMutex);
                    // A background refresh can finish between the frame snapshot and click.
                    if (!Pending(state) &&
                        std::any_of(state.targets.begin(), state.targets.end(), [&](const auto& t) { return t.Same(target); })) {
                        state.histories[Identity(state.selected)].EndGesture();
                        state.selected = target;
                        state.valid = false; state.refresh = true; state.error.clear();
                        view.selected = target; view.valid = false;
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (ImGui::Button(L(24))) {
            std::scoped_lock lock(stateMutex); state.catalogRefresh = true;
        }
        ImGui::TextUnformatted(L(25));
        if (!inGame.load()) { ImGui::TextUnformatted(L(26)); }
        else if (!view.valid) { ImGui::TextUnformatted(L(27)); }
        else if (view.snapshot[10] < .5f) { ImGui::TextUnformatted(L(28)); }
        else if (view.snapshot[12] < .5f) { ImGui::TextUnformatted(L(29)); }
        else if (view.snapshot[11] < .5f) { ImGui::TextUnformatted(L(30)); }
        if (!view.error.empty()) { ImGui::TextWrapped("%s", view.error.c_str()); }
        int level = precision.load();
        ImGui::SetNextItemWidth(240.f);
        if (ImGui::Combo(L(31), &level, L(32))) { SavePrecision(level); }
        level = precision.load();
        ImGui::Text(L(33), PrecisionStep(0, level), PrecisionStep(3, level));
        bool enabled = inGame.load() && !view.beginSession && !view.restoreQueued && !view.restoreActive && !view.resetQueued && !view.targets.empty() && view.valid && view.snapshot[10] > .5f &&
                       view.snapshot[11] > .5f && view.snapshot[12] > .5f && view.error.empty();
        ImGui::BeginDisabled(!enabled);
        const char* labels[] = {
            L(34), L(35), L(36), L(37), L(38), L(39), L(40), L(41), L(42)
        };
        for (int i = 0; i < 9; ++i) {
            if (i % 3 == 0) { ImGui::Separator(); ImGui::TextUnformatted(i == 0 ? L(43) : i == 3 ? L(44) : L(45)); }
            float value = view.snapshot[i];
            if (view.activeEdit && view.activeEdit->first == i) { value = view.activeEdit->second; }
            if (view.queued[i]) { value = *view.queued[i]; }
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(230.f);
            float step = PrecisionStep(i, level);
            float move = view.selected.part == 1 ? 10.f : 15.f;
            float lo = i < 3 ? -move : i < 6 ? -90.f : .5f;
            float hi = i < 3 ? move : i < 6 ? 90.f : 2.f;
            if (ImGui::DragFloat(labels[i], &value, step, lo, hi, i < 6 && i >= 3 ? "%.2f" : "%.3f",
                                 ImGui::ImGuiSliderFlags_AlwaysClamp)) { QueueValue(i, value, view.selected, i); }
            bool active = ImGui::IsItemActive();
            activeValue = activeValue || active;
            if (ImGui::IsItemDeactivated()) {
                std::scoped_lock lock(stateMutex); state.histories[Identity(view.selected)].EndGesture(i);
            }
            if (enabled && ImGui::IsItemHovered()) {
                overValue = true;
                if (!active && std::isfinite(wheel) && wheel != 0.f) {
                    // Wheel uses the latest pending value, so fast ticks cannot overwrite each other.
                    { std::scoped_lock lock(stateMutex); value = EffectiveValues(state)[i]; }
                    QueueValue(i, value + wheel * step, view.selected, 32 + i);
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(L(46))) { QueueValue(i, i >= 6 ? 1.f : 0.f, view.selected); }
            ImGui::PopID();
        }
        ImGui::TextUnformatted(L(47));
        float pivotValue = view.snapshot[9];
        if (view.activeEdit && view.activeEdit->first == 9) { pivotValue = view.activeEdit->second; }
        if (view.queued[9]) { pivotValue = *view.queued[9]; }
        int pivot = std::clamp(static_cast<int>(pivotValue), 0, 2);
        if (ImGui::Combo(L(48), &pivot, L(49))) { QueueValue(9, static_cast<float>(pivot), view.selected); }
        if (view.selected.part == 0 && view.snapshot[13] > .5f) {
            float worn = view.snapshot[14];
            if (view.activeEdit && view.activeEdit->first == 10) { worn = view.activeEdit->second; }
            if (view.queued[10]) { worn = *view.queued[10]; }
            bool follow = worn >= .5f;
            if (ImGui::Checkbox(L(50), &follow)) { QueueValue(10, follow ? 1.f : 0.f, view.selected); }
        }
        static Target resetTarget;
        if (ImGui::Button(L(51))) { resetTarget = view.selected; ImGui::OpenPopup(L(52)); }
        if (ImGui::BeginPopupModal(L(52), nullptr, ImGui::ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped(L(53), resetTarget.name.c_str());
            if (ImGui::Button(L(54))) {
                {
                    std::scoped_lock lock(stateMutex);
                    if (state.valid && resetTarget.Same(state.selected)) {
                        state.histories[Identity(state.selected)].Record(EffectiveValues(state), -1, Now());
                        state.queued.fill(std::nullopt); state.resetQueued = true;
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(L(55))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
        auto history = view.histories.find(Identity(view.selected));
        auto undoCount = history == view.histories.end() ? 0 : history->second.undo.size();
        bool canRecover = enabled && !pending && history != view.histories.end() && history->second.baseline.has_value();
        ImGui::BeginDisabled(!canRecover || undoCount == 0);
        if (ImGui::Button(L(56))) { QueueRestore(view.selected, true); }
        ImGui::EndDisabled();
        ImGui::SameLine();
        bool atStart = canRecover && SameValues(EffectiveValues(view), *history->second.baseline);
        ImGui::BeginDisabled(!canRecover || atStart);
        if (ImGui::Button(L(57))) { QueueRestore(view.selected, false); }
        ImGui::EndDisabled();
        ImGui::Text(L(58), static_cast<unsigned>(undoCount));
        if (ImGui::Button(L(59))) { std::scoped_lock lock(stateMutex); state.refresh = true; }
        ImGui::TextUnformatted(pending ? L(60) : L(61));
        RenderHotkeySettings();
        if (ImGui::Button(L(62))) { open = false; }
        // This window disables automatic wheel scrolling. Manually scroll only
        // away from editable values; never change the framework's global IO.
        if (!overValue && !activeValue && wheel != 0.f && ImGui::IsWindowHovered()) {
            ImGui::SetScrollY(ImGui::GetScrollY() - wheel * 45.f);
        }
        manipulating = open && activeValue;
        ImGui::End();
        window->IsOpen = open;
        Pump();
    }

    void __stdcall RenderPage() {
        ImGui::TextUnformatted(L(63));
        ImGui::TextWrapped(L(64));
        if (ImGui::Button(L(65))) {
            window->IsOpen = true;
            { std::scoped_lock lock(stateMutex); state.refresh = true; state.catalogRefresh = true; state.beginSession = true; state.valid = false; }
            if (auto* main = SKSEMenuFramework::GetMainWindow()) { main->IsOpen = false; }
        }
        ImGui::TextUnformatted(L(66));
        RenderHotkeySettings();
    }

    bool __stdcall OnInput(RE::InputEvent* event) {
        if (!window) { return false; }
        auto* button = event ? event->AsButtonEvent() : nullptr;
        auto* main = SKSEMenuFramework::GetMainWindow();
        bool visible = window->IsOpen.load() || (main && main->IsOpen.load());
        if (capturingKey.load() && !visible) { capturingKey = false; }
        if (capturingKey.load() && button && button->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
            if (button->IsDown()) {
                auto code = button->GetIDCode();
                if (code == RE::BSKeyboardDevice::Key::kEscape) { capturingKey = false; }
                else if (code > 0 && code <= 255) { SaveHotkey(code); }
            }
            return true; // Registration consumes the key; it must not also toggle the window.
        }
        const auto key = hotkey.load();
        if (!inGame.load() || !key) { return false; }
        if (button && button->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
            button->GetIDCode() == key && button->IsDown()) {
            auto* ui = RE::UI::GetSingleton();
            if (!window->IsOpen.load() && ui && (ui->GameIsPaused() || ui->IsMenuOpen("Console") || ui->IsMenuOpen("RaceSex Menu"))) {
                return false;
            }
            window->IsOpen = !window->IsOpen.load();
            if (window->IsOpen.load()) { std::scoped_lock lock(stateMutex); state.refresh = true; state.catalogRefresh = true; state.beginSession = true; state.valid = false; }
            else { manipulating = false; }
            return true;
        }
        return false; // Framework captures input for our blocking window; do not alter global pause/blur settings.
    }

    void __stdcall AfterRender(SKSEMenuFramework::Model::EventType) { Pump(); }

    void ResetSession(bool game) {
        inGame = game;
        capturingKey = false;
        manipulating = false;
        if (window) { window->IsOpen = false; }
        std::scoped_lock lock(stateMutex);
        auto epoch = state.generation + 1;
        state = State{}; state.generation = epoch;
    }

    void Message(SKSE::MessagingInterface::Message* message) {
        if (!message) { return; }
        switch (message->type) {
        case SKSE::MessagingInterface::kDataLoaded: {
            const auto module = GetModuleHandleW(L"SKSEMenuFramework.dll");
            constexpr const char* required[] = {"AddWindow", "AddSectionItem", "GetMainWindow", "RegisterInpoutEvent",
                "RegisterEventPriority", "igBegin", "igEnd", "igDragFloat", "igCombo_Str", "igBeginDisabled",
                "igBeginCombo", "igEndCombo", "igSelectable_Bool", "igCheckbox",
                "igGetKeyData_Key", "igIsItemHovered", "igIsItemActive", "igIsItemDeactivated",
                "igGetScrollY", "igSetScrollY_Float", "igIsWindowHovered"};
            for (auto name : required) {
                if (!module || !GetProcAddress(module, name)) { SKSE::log::error("Missing framework export: {}", name); return; }
            }
            window = SKSEMenuFramework::AddWindow(RenderWindow, true);
            if (!window) { SKSE::log::error("Failed to register UI window"); return; }
            SKSEMenuFramework::SetSection("HairPositioner");
            SKSEMenuFramework::AddSectionItem(L(67), RenderPage);
            static auto* input = SKSEMenuFramework::AddInputEvent(OnInput);
            static auto* render = SKSEMenuFramework::AddEvent(AfterRender, 0.f);
            (void)input; (void)render;
            SKSE::log::info("Registered HairPositioner UI; hotkey {}", hotkey.load());
            break;
        }
        case SKSE::MessagingInterface::kPreLoadGame: ResetSession(false); break;
        case SKSE::MessagingInterface::kPostLoadGame: ResetSession(true); break;
        case SKSE::MessagingInterface::kNewGame: ResetSession(true); break;
        default: break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    if (skse->IsEditor() || REL::Module::IsVR()) { return false; }
    auto path = SKSE::log::log_directory();
    if (!path) { return false; }
    *path /= "HairPositionerUI.log";
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto logger = std::make_shared<spdlog::logger>("HPUI", sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse, false);
    SKSE::log::info("HairPositionerUI 1.2.2: wheel, per-target undo, session restore, precision; default F3");
    HPUI::LoadHotkey();
    return SKSE::GetMessagingInterface()->RegisterListener(HPUI::Message);
}
