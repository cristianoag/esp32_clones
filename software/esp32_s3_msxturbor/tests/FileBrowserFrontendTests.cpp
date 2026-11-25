#include "MsxFileBrowser.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

struct Node
{
    std::string name;
    MsxFileKind kind;
};

static std::map<std::string, std::vector<Node>> directories;
static std::vector<uint8_t> keys;
static size_t keyIndex;
static unsigned scans, errors;

bool MsxReadDirectoryPage(const char* directory, const char* extension, bool allowEject,
                          size_t first, MsxFileEntry* entries, size_t& count, size_t& total,
                          char* error, size_t errorSize)
{
    ++scans;
    count = total = 0;
    error[0] = 0;
    const auto found = directories.find(directory);
    if (found == directories.end()) {
        std::snprintf(error, errorSize, "Cannot open SD directory.");
        return false;
    }
    auto append = [&](const char* name, MsxFileKind kind) {
        if (total >= first && count < MsxBrowserPageSize) {
            std::snprintf(entries[count].name, sizeof(entries[count].name), "%s", name);
            entries[count++].kind = kind;
        }
        ++total;
    };
    if (allowEject) append("<Eject>", MsxFileKind::Eject);
    if (std::strcmp(directory, "/")) append("..", MsxFileKind::Parent);
    for (const auto& node : found->second)
        if (node.kind == MsxFileKind::Directory || MsxHasExtension(node.name.c_str(), extension))
            append(node.name.c_str(), node.kind);
    return true;
}

static void reportError(const char* error) { assert(*error); ++errors; }
static void frame(const char*) {}
static void text(int, int, const char*, unsigned = 0) {}
static void messageLines(const char*) {}
static void showMenu() {}
static void MsxKeyboardClearEvents() {}
static uint8_t waitKey()
{
    assert(keyIndex < keys.size());
    return keys[keyIndex++];
}
static struct {
    void fillRect(int, int, int, int, unsigned) {}
} display;

#include "FileBrowserFrontend.inc"

static bool browse(std::initializer_list<uint8_t> input, char* selection,
                    bool eject = true, const char* extension = ".rom")
{
    keys = input;
    keyIndex = scans = errors = 0;
    const bool selected = chooseFile("ROMs", extension, eject, selection);
    assert(keyIndex == keys.size());
    return selected;
}

int main()
{
    directories["/"].push_back({"games", MsxFileKind::Directory});
    for (unsigned i = 0; i < 140; ++i)
        directories["/"].push_back({"game" + std::to_string(i) + ".rom", MsxFileKind::File});
    directories["/games"].push_back({"other.ROM", MsxFileKind::File});
    directories["/games"].push_back({"update.FLH", MsxFileKind::File});
    char selection[MsxSdPathCapacity] = {};
    assert(browse({81,81,81,81,81,81,81,81,81,81,81,81,
                   82,82,82,82,82,82,82,82,82,82,100,40}, selection));
    assert(!std::strcmp(selection, "/game0.rom"));
    assert(scans == 1);
    std::puts("PASS: 24 navigation/select events in a 142-entry directory require one scan.");

    selection[0] = 0;
    assert(browse({78,81,81,81,75,78,40}, selection));
    assert(!std::strcmp(selection, "/game17.rom") && scans == 4);
    selection[0] = 0;
    assert(browse({82,81,81,40,81,81,40}, selection));
    assert(!std::strcmp(selection, "/games/other.ROM") && scans == 4);
    assert(browse({81,40,81,81,40}, selection));
    assert(!std::strcmp(selection, "/game0.rom") && scans == 2);
    std::strcpy(selection, "/games/other.ROM");
    assert(browse({80,81,81,40}, selection));
    assert(!std::strcmp(selection, "/game0.rom") && scans == 2);
    assert(!browse({80,100,41}, selection) && scans == 1);
    assert(!std::strcmp(selection, "/game0.rom"));
    assert(!browse({69}, selection) && scans == 1);
    assert(browse({76}, selection) && !selection[0] && scans == 1);
    assert(browse({40}, selection) && !selection[0] && scans == 1);

    std::strcpy(selection, "/games/update.FLH");
    assert(browse({81,40}, selection, false, ".flh"));
    assert(!std::strcmp(selection, "/games/update.FLH") && scans == 1);
    assert(!browse({76,41}, selection, false, ".flh") && scans == 1);
    std::strcpy(selection, "/missing/file.rom");
    assert(!browse({}, selection) && scans == 1 && errors == 1);

    directories["/"].clear();
    selection[0] = 0;
    assert(!browse({40,81,82,78,75,41}, selection, false) && scans == 1);
    directories["/"].push_back({"new.rom", MsxFileKind::File});
    assert(browse({40}, selection, false) && !std::strcmp(selection, "/new.rom") && scans == 1);
    std::puts("PASS: page/folder changes, wraparound, parent, filters, eject, cancel, empty/error directories and reopening.");
}
