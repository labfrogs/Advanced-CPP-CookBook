# FileRenamer

A small Visual Studio C++ console utility that recursively renames multiple
selected files. It is written in portable C++17 (`std::filesystem`), so it also
builds with g++/clang on Linux and macOS.

## Features

- **Recursive selection** of files under a directory (`-r`).
- **Extension filtering** so only the file types you want are touched (`-e jpg,png`).
- **Find & replace** on the base name, literal or ECMAScript **regex** (`-f`, `-t`, `--regex`).
- **Case conversion**: `lower`, `upper`, `title` (`--case`).
- **Prefix / suffix** text (`-p`, `-s`).
- **Sequential numbering** with configurable start, padding and separator (`-n`).
- **Extension replacement** (`-x jpeg`).
- **Dry run by default** — the plan is printed and nothing changes until you pass `--apply`.
- **Collision detection** — targets that would overwrite each other or existing
  files are flagged and skipped.

## Building

### Visual Studio (Windows)

Open `FileRenamer.sln`, pick a configuration (e.g. `Release|x64`) and build.
The output is a console executable `FileRenamer.exe`.

### Command line (any platform)

```sh
g++ -std=c++17 FileRenamer/src/Renamer.cpp FileRenamer/src/main.cpp -o FileRenamer
```

## Usage

```
FileRenamer <directory> [options]
```

Run with `--help` for the full option list.

### Examples

Preview renaming every `.jpg`/`.png` under a tree to a numbered `img_` series:

```sh
FileRenamer ./photos -r -e jpg,png -p img_ -n --number-pad 4
```

Replace spaces with underscores in all `.txt` files, then apply:

```sh
FileRenamer ./docs -e txt -f " " -t "_" --apply
```

Change every `.jpeg` extension to `.jpg`:

```sh
FileRenamer ./images -e jpeg -x jpg --apply
```

## Transformation order

For each selected file the new base name is built as:

1. find / replace
2. case change
3. prefix + name + suffix
4. sequential number

then the extension (original, or the one from `-x`) is appended.
