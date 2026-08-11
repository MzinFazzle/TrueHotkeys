#include "Hotkeys/RelocDiagnostic.h"

#include "Hotkeys/DXScanCodes.h"

#include "versionlibdb.h"

#include <Windows.h>

namespace Hotkeys::RelocDiagnostic {
    namespace {
        constexpr unsigned long long kJournalTabSEId = 520167;
        constexpr unsigned long long kJournalTabAEId = 406697;

        [[nodiscard]] bool TryReadUint32(const void* a_address, std::uint32_t& a_outValue) {
            __try {
                a_outValue = *reinterpret_cast<const std::uint32_t*>(a_address);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        VersionDb& GetVersionDb() {
            static VersionDb db;
            static bool loadAttempted = false;
            if (!loadAttempted) {
                loadAttempted = true;
                db.Load();
            }
            return db;
        }

        void LogAddressAndValue(std::string_view a_label, const void* a_address) {
            if (!a_address) {
                SKSE::log::info("{}: null address, skipping read.", a_label);
                return;
            }
            SKSE::log::info("{}: 0x{:X}", a_label, reinterpret_cast<std::uintptr_t>(a_address));
            std::uint32_t value = 0;
            if (TryReadUint32(a_address, value)) {
                SKSE::log::info("{}: current uint32 value = {} (0x{:X})", a_label, value, value);
            } else {
                SKSE::log::info("{}: reading this address raised an exception - not safely readable memory.", a_label);
            }
        }

        void RunDiagnostic() {
            SKSE::log::info("---- RelocDiagnostic (CapsLock pressed) ----");

            auto& db = GetVersionDb();

            int major = 0, minor = 0, revision = 0, build = 0;
            if (db.GetExecutableVersion(major, minor, revision, build)) {
                SKSE::log::info("Detected Skyrim exe version: {}.{}.{}.{}", major, minor, revision, build);
            } else {
                SKSE::log::info("Could not detect Skyrim exe version via GetExecutableVersion().");
            }

            if (db.GetLoadedVersionString().empty()) {
                SKSE::log::info(
                    "VersionDb has no database loaded - either "
                    "Data\\SKSE\\Plugins\\versionlib-{}-{}-{}-{}.bin doesn't exist (the Address "
                    "Library AE download for this exact game version), or Load() failed for "
                    "another reason.",
                    major, minor, revision, build);
            } else {
                SKSE::log::info("VersionDb loaded: {} (module \"{}\")", db.GetLoadedVersionString(), db.GetModuleName());

                unsigned long long offset = 0;
                if (db.FindOffsetById(kJournalTabAEId, offset)) {
                    SKSE::log::info("ID {} (AE) -> offset 0x{:X} in this database.", kJournalTabAEId, offset);
                } else {
                    SKSE::log::info("ID {} (AE) not found in this database.", kJournalTabAEId);
                }

                LogAddressAndValue("VersionDb-resolved address", db.FindAddressById(kJournalTabAEId));
            }

            const std::uintptr_t ngAddress = REL::RelocationID(kJournalTabSEId, kJournalTabAEId).address();
            LogAddressAndValue("REL::RelocationID-resolved address", reinterpret_cast<const void*>(ngAddress));

            if (auto* controls = RE::PlayerControls::GetSingleton()) {
                SKSE::log::info("PlayerControlsData.running = {}, PlayerControlsData.autoMove = {}", controls->data.running,
                                 controls->data.autoMove);
                SKSE::log::info("PlayerControlsData.moveInputVec = ({:.3f}, {:.3f})", controls->data.moveInputVec.x,
                                 controls->data.moveInputVec.y);
            } else {
                SKSE::log::info("RE::PlayerControls::GetSingleton() returned null.");
            }

            SKSE::log::info("-----------------------------------------------");
        }

        class DiagnosticInputSink final : public RE::BSTEventSink<RE::InputEvent*> {
        public:
            static DiagnosticInputSink* GetSingleton() {
                static DiagnosticInputSink singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
                                                   RE::BSTEventSource<RE::InputEvent*>*) override {
                for (RE::InputEvent* event = (a_event && *a_event) ? *a_event : nullptr; event; event = event->next) {
                    if (auto* button = event->AsButtonEvent()) {
                        if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard && button->GetIDCode() == DXScanCode::kCapsLock &&
                            button->IsDown()) {
                            RunDiagnostic();
                        }
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            DiagnosticInputSink() = default;
        };
    }

    void Install() {
        auto* deviceManager = RE::BSInputDeviceManager::GetSingleton();
        if (!deviceManager) {
            SKSE::log::info("True Hotkeys: relocation diagnostic NOT installed - BSInputDeviceManager::GetSingleton() returned null.");
            return;
        }
        deviceManager->AddEventSink(DiagnosticInputSink::GetSingleton());
        SKSE::log::info("True Hotkeys: relocation diagnostic installed - press CapsLock in-game to log a snapshot.");
    }
}
