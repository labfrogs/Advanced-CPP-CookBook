#include "Renamer.h"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using renamer::CaseMode;
using renamer::RenameOptions;

void printUsage(const char* exe) {
    std::cout <<
        "FileRenamer - recursively rename multiple selected files\n\n"
        "Usage:\n"
        "  " << exe << " <directory> [options]\n\n"
        "Selection:\n"
        "  -r, --recursive          Descend into sub-directories\n"
        "  -e, --ext <list>         Only files with these extensions\n"
        "                           (comma separated, e.g. \"jpg,png\")\n\n"
        "Transformations (applied in this order):\n"
        "  -f, --find <text>        Substring (or regex) to find\n"
        "  -t, --replace <text>     Replacement text (default: empty)\n"
        "      --regex              Treat --find as an ECMAScript regex\n"
        "      --first-only         Replace only the first match\n"
        "      --case <mode>        lower | upper | title\n"
        "  -p, --prefix <text>      Text prepended to each base name\n"
        "  -s, --suffix <text>      Text appended to each base name\n"
        "  -n, --number             Append a sequential counter\n"
        "      --number-start <n>   First counter value (default: 1)\n"
        "      --number-pad <n>     Zero-padding width (default: 3)\n"
        "      --number-sep <text>  Separator before counter (default: \"_\")\n"
        "  -x, --new-ext <ext>      Replace the file extension\n\n"
        "Execution:\n"
        "      --apply              Perform the renames (default is dry run)\n"
        "  -h, --help               Show this help\n\n"
        "By default nothing is changed on disk: the planned renames are printed\n"
        "so you can review them. Add --apply to actually rename the files.\n";
}

std::vector<std::string> splitCsv(const std::string& value) {
    std::vector<std::string> parts;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
            parts.push_back(item);
        }
    }
    return parts;
}

// Fetch the argument that must follow an option flag.
bool nextValue(int argc, char** argv, int& i, const std::string& flag, std::string& out) {
    if (i + 1 >= argc) {
        std::cerr << "error: missing value for " << flag << "\n";
        return false;
    }
    out = argv[++i];
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    RenameOptions options;
    bool apply = false;
    bool rootSet = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        std::string value;

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-r" || arg == "--recursive") {
            options.recursive = true;
        } else if (arg == "-e" || arg == "--ext") {
            if (!nextValue(argc, argv, i, arg, value)) return 1;
            options.extensions = splitCsv(value);
        } else if (arg == "-f" || arg == "--find") {
            if (!nextValue(argc, argv, i, arg, value)) return 1;
            options.find = value;
        } else if (arg == "-t" || arg == "--replace") {
            if (!nextValue(argc, argv, i, arg, value)) return 1;
            options.replace = value;
        } else if (arg == "--regex") {
            options.useRegex = true;
        } else if (arg == "--first-only") {
            options.replaceAll = false;
        } else if (arg == "--case") {
            if (!nextValue(argc, argv, i, arg, value)) return 1;
            if (value == "lower") options.caseMode = CaseMode::Lower;
            else if (value == "upper") options.caseMode = CaseMode::Upper;
            else if (value == "title") options.caseMode = CaseMode::Title;
            else { std::cerr << "error: unknown case mode '" << value << "'\n"; return 1; }
        } else if (arg == "-p" || arg == "--prefix") {
            if (!nextValue(argc, argv, i, arg, value)) return 1;
            options.prefix = value;
        } else if (arg == "-s" || arg == "--suffix") {
            if (!nextValue(argc, argv, i, arg, value)) return 1;
            options.suffix = value;
        } else if (arg == "-n" || arg == "--number") {
            options.numbering = true;
        } else if (arg == "--number-start") {
            if (!nextValue(argc, argv, i, arg, value)) return 1;
            options.numberStart = std::stoi(value);
        } else if (arg == "--number-pad") {
            if (!nextValue(argc, argv, i, arg, value)) return 1;
            options.numberPadding = std::stoi(value);
        } else if (arg == "--number-sep") {
            if (!nextValue(argc, argv, i, arg, value)) return 1;
            options.numberSeparator = value;
        } else if (arg == "-x" || arg == "--new-ext") {
            if (!nextValue(argc, argv, i, arg, value)) return 1;
            options.newExtension = value;
        } else if (arg == "--apply") {
            apply = true;
        } else if (!arg.empty() && arg.front() == '-') {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return 1;
        } else if (!rootSet) {
            options.root = arg;
            rootSet = true;
        } else {
            std::cerr << "error: unexpected argument '" << arg << "'\n";
            return 1;
        }
    }

    if (!rootSet) {
        std::cerr << "error: no directory specified\n";
        return 1;
    }

    if (!std::filesystem::exists(options.root)) {
        std::cerr << "error: directory does not exist: " << options.root.string() << "\n";
        return 1;
    }
    if (!std::filesystem::is_directory(options.root)) {
        std::cerr << "error: not a directory: " << options.root.string() << "\n";
        return 1;
    }

    auto files = renamer::collectFiles(options);
    if (files.empty()) {
        std::cout << "No matching files under " << options.root.string() << "\n";
        return 0;
    }

    auto plan = renamer::buildPlan(files, options);
    if (plan.empty()) {
        std::cout << "Nothing to rename (" << files.size()
                  << " file(s) matched, none would change).\n";
        return 0;
    }

    std::cout << (apply ? "Applying" : "Dry run -") << " "
              << plan.size() << " rename(s):\n";
    for (const auto& item : plan) {
        std::cout << "  " << item.from.filename().string()
                  << "  ->  " << item.to.filename().string();
        if (!item.conflict.empty()) {
            std::cout << "   [SKIP: " << item.conflict << "]";
        }
        std::cout << "\n";
    }

    if (apply) {
        int renamed = renamer::applyPlan(plan);
        std::cout << "Renamed " << renamed << " file(s).\n";
    } else {
        std::cout << "\nThis was a dry run. Re-run with --apply to perform the renames.\n";
    }
    return 0;
}
