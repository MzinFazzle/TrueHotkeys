#pragma once


#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Hotkeys {
    class Locale {
    public:
        static Locale* GetSingleton();

        void Initialize(std::string_view a_languageName);

        void InitializeEarly();

        [[nodiscard]] std::vector<std::string> ListLanguages() const;

        bool SetLanguage(std::string_view a_languageName);

        [[nodiscard]] const std::string& GetCurrentLanguage() const noexcept { return m_currentLanguage; }

        [[nodiscard]] const char* T(std::string_view a_key) const;

    private:
        Locale() = default;
        Locale(const Locale&) = delete;
        Locale(Locale&&) = delete;

        [[nodiscard]] std::filesystem::path LanguagesDirectory() const;
        bool LoadLanguageFile(const std::filesystem::path& a_path);
        void WriteDefaultEnglishFile(const std::filesystem::path& a_path) const;

        std::string m_currentLanguage = "English";
        std::unordered_map<std::string, std::string> m_strings;
    };

    template <typename... Args>
    [[nodiscard]] std::string TF(std::string_view a_key, Args&&... args) {
        try {
            return std::vformat(Locale::GetSingleton()->T(a_key), std::make_format_args(args...));
        } catch (const std::format_error&) {
            return std::string(a_key);
        }
    }
}
