#include "Renamer.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace renamer {

namespace {

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

// Normalise an extension to the lowercase, dot-less form used for comparison.
std::string normalizeExtension(const std::string& ext) {
    std::string trimmed = ext;
    if (!trimmed.empty() && trimmed.front() == '.') {
        trimmed.erase(trimmed.begin());
    }
    return toLower(trimmed);
}

bool extensionMatches(const fs::path& file, const std::vector<std::string>& extensions) {
    if (extensions.empty()) {
        return true;
    }
    std::string fileExt = normalizeExtension(file.extension().string());
    for (const auto& ext : extensions) {
        if (normalizeExtension(ext) == fileExt) {
            return true;
        }
    }
    return false;
}

std::string applyCase(const std::string& value, CaseMode mode) {
    std::string result = value;
    switch (mode) {
        case CaseMode::None:
            break;
        case CaseMode::Lower:
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            break;
        case CaseMode::Upper:
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            break;
        case CaseMode::Title: {
            bool atWordStart = true;
            for (char& ch : result) {
                unsigned char uc = static_cast<unsigned char>(ch);
                if (std::isalnum(uc)) {
                    ch = atWordStart ? static_cast<char>(std::toupper(uc))
                                     : static_cast<char>(std::tolower(uc));
                    atWordStart = false;
                } else {
                    atWordStart = true;
                }
            }
            break;
        }
    }
    return result;
}

std::string applyFindReplace(const std::string& value, const RenameOptions& options) {
    if (options.find.empty()) {
        return value;
    }
    if (options.useRegex) {
        std::regex re(options.find);
        auto flags = options.replaceAll ? std::regex_constants::format_default
                                        : std::regex_constants::format_first_only;
        return std::regex_replace(value, re, options.replace, flags);
    }

    std::string result;
    result.reserve(value.size());
    std::size_t pos = 0;
    bool replacedOnce = false;
    while (pos <= value.size()) {
        std::size_t found = value.find(options.find, pos);
        if (found == std::string::npos || (!options.replaceAll && replacedOnce)) {
            result.append(value, pos, std::string::npos);
            break;
        }
        result.append(value, pos, found - pos);
        result.append(options.replace);
        pos = found + options.find.size();
        replacedOnce = true;
    }
    return result;
}

std::string formatNumber(int value, int padding) {
    std::ostringstream oss;
    oss << std::setw(std::max(0, padding)) << std::setfill('0') << value;
    return oss.str();
}

}  // namespace

std::vector<fs::path> collectFiles(const RenameOptions& options) {
    std::vector<fs::path> files;
    if (!fs::exists(options.root) || !fs::is_directory(options.root)) {
        return files;
    }

    auto consider = [&](const fs::directory_entry& entry) {
        if (entry.is_regular_file() && extensionMatches(entry.path(), options.extensions)) {
            files.push_back(entry.path());
        }
    };

    if (options.recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(options.root)) {
            consider(entry);
        }
    } else {
        for (const auto& entry : fs::directory_iterator(options.root)) {
            consider(entry);
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

std::string transformBaseName(const std::string& baseName,
                              const RenameOptions& options,
                              int index) {
    std::string result = applyFindReplace(baseName, options);
    result = applyCase(result, options.caseMode);
    result = options.prefix + result + options.suffix;
    if (options.numbering) {
        result += options.numberSeparator +
                  formatNumber(options.numberStart + index, options.numberPadding);
    }
    return result;
}

std::vector<RenamePlanItem> buildPlan(const std::vector<fs::path>& files,
                                     const RenameOptions& options) {
    std::vector<RenamePlanItem> plan;
    std::set<fs::path> targets;

    int index = 0;
    for (const auto& file : files) {
        std::string base = file.stem().string();
        std::string newBase = transformBaseName(base, options, index);
        ++index;

        std::string ext = options.newExtension.empty()
                              ? file.extension().string()
                              : "." + normalizeExtension(options.newExtension);

        fs::path target = file.parent_path() / (newBase + ext);
        if (target == file) {
            continue;  // nothing changed for this file
        }

        RenamePlanItem item{file, target, {}};
        if (targets.count(target)) {
            item.conflict = "target name collides with another renamed file";
        } else if (fs::exists(target)) {
            item.conflict = "target already exists on disk";
        }
        targets.insert(target);
        plan.push_back(std::move(item));
    }
    return plan;
}

int applyPlan(const std::vector<RenamePlanItem>& plan) {
    int renamed = 0;
    for (const auto& item : plan) {
        if (!item.conflict.empty()) {
            std::cerr << "skip: " << item.from.string() << " (" << item.conflict << ")\n";
            continue;
        }
        std::error_code ec;
        fs::rename(item.from, item.to, ec);
        if (ec) {
            std::cerr << "error: " << item.from.string() << " -> " << item.to.string()
                      << " (" << ec.message() << ")\n";
        } else {
            ++renamed;
        }
    }
    return renamed;
}

}  // namespace renamer
