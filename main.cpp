#include <windows.h>
#include <shlwapi.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "User32.lib")

struct LaunchConfig {
    std::wstring executable;
    std::wstring workingDirectory;
    std::vector<std::wstring> arguments;
    std::wstring sourceDirectory;
};

namespace {

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }

    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return std::string();
    }

    std::string result(required, '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.empty() ? nullptr : &result[0],
        required,
        nullptr,
        nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }

    int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return L"";
    }

    std::wstring result(required, L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.empty() ? nullptr : &result[0],
        required);
    return result;
}

class LauncherException : public std::exception {
public:
    explicit LauncherException(std::wstring message)
        : message_(std::move(message)), narrow_(WideToUtf8(message_)) {}

    const char* what() const noexcept override {
        return narrow_.c_str();
    }

    const std::wstring& message() const noexcept {
        return message_;
    }

private:
    std::wstring message_;
    std::string narrow_;
};

std::wstring Trim(const std::wstring& value) {
    const std::wstring whitespace = L" \t\r\n";
    const auto begin = value.find_first_not_of(whitespace);
    if (begin == std::wstring::npos) {
        return L"";
    }
    const auto end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

std::wstring ToLower(const std::wstring& value) {
    std::wstring result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return result;
}

std::wstring StripQuotes(const std::wstring& value) {
    if (value.size() >= 2) {
        const wchar_t first = value.front();
        const wchar_t last = value.back();
        if ((first == L'"' && last == L'"') || (first == L'\'' && last == L'\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

std::vector<std::wstring> ParseInlineList(const std::wstring& value) {
    std::vector<std::wstring> results;

    if (value.size() < 2 || value.front() != L'[' || value.back() != L']') {
        if (!value.empty()) {
            results.push_back(StripQuotes(Trim(value)));
        }
        return results;
    }

    std::wstring current;
    bool inQuotes = false;
    wchar_t quoteChar = L'\0';
    bool escapeNext = false;

    for (size_t i = 1; i + 1 < value.size(); ++i) {
        const wchar_t ch = value[i];

        if (escapeNext) {
            current.push_back(ch);
            escapeNext = false;
            continue;
        }

        if (ch == L'\\') {
            escapeNext = true;
            continue;
        }

        if (inQuotes) {
            if (ch == quoteChar) {
                inQuotes = false;
            } else {
                current.push_back(ch);
            }
            continue;
        }

        if (ch == L'"' || ch == L'\'') {
            inQuotes = true;
            quoteChar = ch;
            continue;
        }

        if (ch == L',') {
            const auto trimmed = Trim(current);
            if (!trimmed.empty()) {
                results.push_back(StripQuotes(trimmed));
            }
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    const auto trimmed = Trim(current);
    if (!trimmed.empty()) {
        results.push_back(StripQuotes(trimmed));
    }

    return results;
}

std::wstring QuoteForCommandLine(const std::wstring& argument) {
    if (argument.empty()) {
        return L"\"\"";
    }

    bool needsQuotes = false;
    for (wchar_t ch : argument) {
        if (std::iswspace(ch) || ch == L'"' || ch == L'\\') {
            needsQuotes = true;
            break;
        }
    }

    if (!needsQuotes) {
        return argument;
    }

    std::wstring result;
    result.push_back(L'"');

    size_t backslashCount = 0;
    for (wchar_t ch : argument) {
        if (ch == L'\\') {
            ++backslashCount;
            continue;
        }

        if (ch == L'"') {
            result.append(backslashCount * 2 + 1, L'\\');
            result.push_back(ch);
            backslashCount = 0;
            continue;
        }

        if (backslashCount > 0) {
            result.append(backslashCount, L'\\');
            backslashCount = 0;
        }

        result.push_back(ch);
    }

    if (backslashCount > 0) {
        result.append(backslashCount * 2, L'\\');
    }

    result.push_back(L'"');
    return result;
}

std::wstring GetDirectoryName(const std::wstring& path) {
    DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (length == 0) {
        return L"";
    }

    std::vector<wchar_t> buffer(length);
    wchar_t* filePart = nullptr;
    if (GetFullPathNameW(path.c_str(), length, buffer.data(), &filePart) == 0) {
        return L"";
    }

    if (!filePart) {
        return Trim(std::wstring(buffer.data()));
    }

    *filePart = L'\0';
    return Trim(std::wstring(buffer.data()));
}

std::wstring CombinePath(const std::wstring& base, const std::wstring& target) {
    if (target.empty()) {
        return target;
    }

    if (PathIsRelativeW(target.c_str()) == FALSE) {
        return target;
    }

    std::wstring combined = base;
    if (!combined.empty() && combined.back() != L'\\' && combined.back() != L'/') {
        combined.push_back(L'\\');
    }
    combined.append(target);

    DWORD length = GetFullPathNameW(combined.c_str(), 0, nullptr, nullptr);
    if (length == 0) {
        return combined;
    }

    std::vector<wchar_t> buffer(length);
    if (GetFullPathNameW(combined.c_str(), length, buffer.data(), nullptr) == 0) {
        return combined;
    }

    return std::wstring(buffer.data());
}

std::wstring ReadFileUtf8(const std::wstring& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return L"";
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string content = buffer.str();

    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        return Utf8ToWide(content.substr(3));
    }

    return Utf8ToWide(content);
}

LaunchConfig ParseConfig(const std::wstring& path) {
    LaunchConfig config;
    config.sourceDirectory = GetDirectoryName(path);

    const std::wstring content = ReadFileUtf8(path);
    if (content.empty()) {
        throw LauncherException(L"无法读取配置文件或文件为空");
    }

    std::wistringstream stream(content);
    std::wstring line;
    bool inArguments = false;

    while (std::getline(stream, line)) {
        const std::wstring trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == L'#') {
            continue;
        }

        const size_t firstNonSpace = line.find_first_not_of(L" \t");
        const bool indented = firstNonSpace != std::wstring::npos && firstNonSpace > 0;

        if (!indented) {
            const size_t colon = line.find(L':', firstNonSpace);
            if (colon == std::wstring::npos) {
                continue;
            }

            std::wstring key = line.substr(firstNonSpace, colon - firstNonSpace);
            std::wstring value = line.substr(colon + 1);

            key = ToLower(Trim(key));
            value = Trim(value);

            if (key == L"executable") {
                config.executable = StripQuotes(value);
                inArguments = false;
            } else if (key == L"workingdirectory") {
                config.workingDirectory = StripQuotes(value);
                inArguments = false;
            } else if (key == L"arguments") {
                config.arguments.clear();
                inArguments = true;
                if (!value.empty()) {
                    const auto inlineArgs = ParseInlineList(value);
                    config.arguments.insert(config.arguments.end(), inlineArgs.begin(), inlineArgs.end());
                }
            } else {
                inArguments = false;
            }
        } else if (inArguments && trimmed.front() == L'-') {
            std::wstring value = Trim(trimmed.substr(1));
            value = StripQuotes(value);
            if (!value.empty()) {
                config.arguments.push_back(value);
            }
        }
    }

    return config;
}

std::wstring BuildErrorMessage(const std::wstring& text) {
    return text + L"\n程序将立即退出";
}

void ShowError(const std::wstring& message) {
    MessageBoxW(nullptr, message.c_str(), L"App Launcher", MB_OK | MB_ICONERROR);
}

void LaunchProcess(const LaunchConfig& config) {
    const std::wstring executable = CombinePath(config.sourceDirectory, config.executable);
    if (GetFileAttributesW(executable.c_str()) == INVALID_FILE_ATTRIBUTES) {
        throw LauncherException(L"目标程序不存在\n" + executable);
    }

    std::wstring commandLine = QuoteForCommandLine(executable);
    for (const auto& argument : config.arguments) {
        commandLine.append(L" ");
        commandLine.append(QuoteForCommandLine(argument));
    }

    std::wstring workingDirectory;
    if (!config.workingDirectory.empty()) {
        workingDirectory = CombinePath(config.sourceDirectory, config.workingDirectory);
    }

    STARTUPINFOW startupInfo;
    PROCESS_INFORMATION processInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    ZeroMemory(&processInfo, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;

    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');

    if (CreateProcessW(
            nullptr,
            commandBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
            &startupInfo,
            &processInfo) == FALSE) {
        DWORD error = GetLastError();
        LPWSTR buffer = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            0,
            reinterpret_cast<LPWSTR>(&buffer),
            0,
            nullptr);

        std::wstring message = L"启动进程失败。错误代码：" + std::to_wstring(error);
        if (buffer) {
            message.append(L"\n");
            message.append(buffer);
            LocalFree(buffer);
        }

        throw LauncherException(message);
    }

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
}

std::wstring GetExecutablePath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = 0;

    while (true) {
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw LauncherException(L"无法获取当前程序路径。");
        }

        if (length < buffer.size()) {
            break;
        }

        buffer.resize(buffer.size() * 2);
    }

    return std::wstring(buffer.data(), length);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        const std::wstring executablePath = GetExecutablePath();
        const std::wstring configDirectory = GetDirectoryName(executablePath);
        const std::wstring configPath = CombinePath(configDirectory, L"AppLauncher.yaml");

        if (GetFileAttributesW(configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            ShowError(BuildErrorMessage(L"找不到配置文件\n" + configPath));
            return EXIT_FAILURE;
        }

        LaunchConfig config = ParseConfig(configPath);
        if (config.executable.empty()) {
            ShowError(BuildErrorMessage(L"配置文件缺少 'executable' 字段。"));
            return EXIT_FAILURE;
        }

        LaunchProcess(config);
    } catch (const LauncherException& ex) {
        ShowError(BuildErrorMessage(ex.message()));
        return EXIT_FAILURE;
    } catch (const std::exception& ex) {
        std::wstring message = L"Unexpected error.";
        const std::string details = ex.what();
        if (!details.empty()) {
            message.append(L"\n");
            message.append(Utf8ToWide(details));
        }
        ShowError(BuildErrorMessage(message));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
