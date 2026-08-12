#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace renamer {

// How the base name (the file name without its extension) should be transformed.
enum class CaseMode {
    None,
    Lower,
    Upper,
    Title
};

// A single, self-contained description of the renaming job. The same options
// object is used both to build a preview (dry run) and to execute the changes.
struct RenameOptions {
    // Root directory whose files are considered.
    std::filesystem::path root;

    // When true, descend into sub-directories; otherwise only the root is used.
    bool recursive = false;

    // Only files whose extension is in this list are selected. Extensions may be
    // given with or without a leading dot (".txt" or "txt"), case-insensitively.
    // An empty list means "every file".
    std::vector<std::string> extensions;

    // Text transformations applied to the base name, in this order:
    //  1. find/replace (literal or regex)
    //  2. case change
    //  3. prefix / suffix
    //  4. sequential numbering
    std::string find;                 // substring or regex to look for
    std::string replace;              // replacement text
    bool useRegex = false;            // interpret `find` as an ECMAScript regex
    bool replaceAll = true;           // replace every match, not just the first

    CaseMode caseMode = CaseMode::None;

    std::string prefix;
    std::string suffix;

    // Sequential numbering. When enabled, a zero-padded counter is appended to
    // each new base name, e.g. "photo_001".
    bool numbering = false;
    int numberStart = 1;
    int numberPadding = 3;
    std::string numberSeparator = "_";

    // Replace the extension of every selected file (e.g. "jpeg" -> "jpg").
    // Given with or without a leading dot. Empty means "leave extension alone".
    std::string newExtension;
};

// One planned rename: the file as it exists now and the full path it will get.
struct RenamePlanItem {
    std::filesystem::path from;
    std::filesystem::path to;
    // Populated with a human-readable reason when this item cannot be applied
    // (e.g. two source files would collapse onto the same target name).
    std::string conflict;
};

// Collect the files under `options.root` that match the recursion and extension
// filters, sorted for deterministic ordering.
std::vector<std::filesystem::path> collectFiles(const RenameOptions& options);

// Apply the text transformations of `options` to a single base name (the file
// name without extension). `index` is the zero-based position of the file in
// the selection and drives sequential numbering.
std::string transformBaseName(const std::string& baseName,
                              const RenameOptions& options,
                              int index);

// Build the full rename plan for the given files. Detects targets that would
// collide with each other and flags them via RenamePlanItem::conflict. Items
// whose target equals their source (no change) are omitted.
std::vector<RenamePlanItem> buildPlan(const std::vector<std::filesystem::path>& files,
                                     const RenameOptions& options);

// Execute a plan on disk. Items carrying a conflict are skipped. Returns the
// number of files successfully renamed. Errors for individual files are written
// to stderr and do not abort the remaining renames.
int applyPlan(const std::vector<RenamePlanItem>& plan);

}  // namespace renamer
