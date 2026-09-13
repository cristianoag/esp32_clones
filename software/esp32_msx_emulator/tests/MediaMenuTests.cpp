#include "MsxSettings.h"
#include "MsxMenuLayout.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <deque>
#include <string>
#include <vector>

static bool running, bootRequested, exitRequested, preferencesReady;
static bool soundEnabled, autoBoot;
static uint8_t selectedAudioProfile;
static bool audioAvailable;
static unsigned resolvedAudio;
static unsigned selectedProfile;
static int selectedRamPages;
static char selectedCartridges[2][MsxSdPathCapacity], selectedDisks[2][MsxSdPathCapacity];
static uint8_t selectedMappers[2];
static MsxCartridgeInfo cartridgeInfo[2];
static bool cartridgeInfoValid[2];
static char selectedTape[MsxSdPathCapacity], statusMessage[160];
struct Profile { const char *id; const char *error; };
static Profile MsxProfiles[] = {{"omega", "Invalid BIOS"}, {"fs-a1fx", "Invalid BIOS"}};
static bool validProfile, validCartridges, validMedia, validCandidate, attachOk, pauseOk, writeOk;
static unsigned saves, pauses, resumes, diskAttaches, tapeAttaches, rewinds, diskValidations, tapeValidations;
static unsigned lastDrive;
static std::string lastPath;
static std::string inspectedProfile;
static int detectedMapper = MsxKonamiScc;
static MsxBootSettings saved;
static std::deque<uint8_t> keys;
static std::deque<std::string> files;
static std::vector<std::string> filters, titles, labels;

struct PreferencesStub
{
    size_t putBytes(const char *, const void *value, size_t size)
    {
        ++saves;
        assert(size == sizeof(saved));
        if (!writeOk) return 0;
        memcpy(&saved, value, size);
        return size;
    }
} preferences;
struct SerialStub
{
    void println(const char *) {}
    template<typename... Args> void printf(const char *, Args...) {}
} Serial;
struct DisplayStub { void fillRect(int, int, int, int, int) {} } display;
struct VideoStub { void show() {} } video;
static void text(int, int, const char *label, int = 255)
{
    assert(strlen(label) <= 50);
    labels.emplace_back(label);
}
static void frame(const char *title) { titles.emplace_back(title); }
static void messageLines(const char *) {}
static void delay(unsigned) {}
static void MsxKeyboardClearEvents() {}
static uint8_t MsxKeyboardMenuKey()
{
    assert(!keys.empty()); // Every scripted interaction must return, not hang.
    const uint8_t key = keys.front();
    keys.pop_front();
    return key;
}
static const char *baseName(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}
static bool MsxJoystickHostPause() { ++pauses; return pauseOk; }
static void MsxJoystickHostResume() { ++resumes; }
static bool MsxValidateProfile(Profile &) { return validProfile; }
static bool validateCartridges() { return validCartridges; }
static bool validateMedia() { return validMedia; }
static bool result(bool ok, char *error, size_t size)
{
    snprintf(error, size, "%s", ok ? "" : "Test failure");
    return ok;
}
#define MsxResolveAudioProfile mockResolveAudioProfile
#define MsxAudioProfileName mockAudioProfileName
static bool mockResolveAudioProfile(unsigned profile, const char *, char *path, size_t,
                                    char *error, size_t size)
{
    resolvedAudio = profile;
    path[0] = 0;
    return result(audioAvailable || (profile != MsxAudioPsgFm && profile != MsxAudioPsgSccFm), error, size);
}
static const char *mockAudioProfileName(unsigned profile)
{
    const char *names[] = {"Auto", "PSG only", "PSG + SCC", "PSG + FM-PAC", "PSG + SCC + FM-PAC"};
    assert(profile < MsxAudioProfileCount);
    return names[profile];
}
static bool MsxInspectCartridge(const char *, const char *profile, MsxCartridgeInfo &info, char *error, size_t size)
{
    inspectedProfile = profile;
    if (validCandidate) info = {detectedMapper, "Embedded SHA-1 database"};
    return result(validCandidate, error, size);
}
static bool MsxValidateDisk(const char *, char *error, size_t size)
{
    ++diskValidations;
    return result(attachOk, error, size);
}
static bool MsxValidateTape(const char *, char *error, size_t size)
{
    ++tapeValidations;
    return result(attachOk, error, size);
}
static bool MsxAttachDisk(unsigned drive, const char *path, char *error, size_t size)
{
    ++diskAttaches; lastDrive = drive; lastPath = path;
    return result(attachOk, error, size);
}
static bool MsxAttachTape(const char *path, char *error, size_t size)
{
    ++tapeAttaches; lastPath = path;
    return result(attachOk, error, size);
}
static bool MsxDiskAvailable() { return true; }
static bool MsxRewindTape(char *error, size_t size) { ++rewinds; return result(attachOk, error, size); }
static bool chooseSdFile(const char *, const char *extension, bool allowEject,
                         char *selection, bool liveMedia = false)
{
    assert(allowEject);
    assert(liveMedia == (strcmp(extension, ".rom") != 0));
    assert(!files.empty());
    filters.emplace_back(extension);
    const std::string candidate = files.front();
    files.pop_front();
    if (candidate == "<cancel>") return false;
    snprintf(selection, MsxSdPathCapacity, "%s", candidate.c_str());
    return true;
}

#include "MediaMenuUnderTest.inc"

static void reset(bool active = true)
{
    running = active; bootRequested = exitRequested = false;
    preferencesReady = soundEnabled = autoBoot = true;
    selectedAudioProfile = MsxAudioAuto;
    audioAvailable = true;
    resolvedAudio = MsxAudioAuto;
    validProfile = validCartridges = validMedia = validCandidate = attachOk = pauseOk = writeOk = true;
    selectedProfile = 0; selectedRamPages = 32;
    memset(selectedCartridges, 0, sizeof(selectedCartridges));
    for (unsigned slot = 0; slot < 2; ++slot)
    {
        selectedMappers[slot] = MsxMapperAuto;
        cartridgeInfo[slot] = {-1, "not scanned"};
        cartridgeInfoValid[slot] = false;
    }
    detectedMapper = MsxKonamiScc;
    inspectedProfile.clear();
    memset(selectedDisks, 0, sizeof(selectedDisks));
    memset(selectedTape, 0, sizeof(selectedTape));
    memset(&saved, 0, sizeof(saved));
    statusMessage[0] = 0;
    saves = pauses = resumes = diskAttaches = tapeAttaches = rewinds = diskValidations = tapeValidations = 0;
    lastDrive = 0; lastPath.clear();
    keys.clear(); files.clear(); filters.clear(); titles.clear(); labels.clear();
}

static void moveTo(unsigned from, unsigned to)
{
    while (from != to)
    {
        keys.push_back(81);
        from = (from + 1) % MsxMediaCount;
    }
}

static void onlyMediaPages()
{
    for (const auto &title : titles)
        assert(title == "Media" || title == "Inspecting cartridge mapper" ||
               title == "Media > Audio - whole machine");
}

int main()
{
    const uint8_t enter = 40, esc = 41, f12 = 69, del = 76, down = 81, up = 82;
    reset();
    files = {"/games/one.rom", "/games/two.ROM"};
    keys = {enter, down, down, enter};
    moveTo(MsxMediaSlot2, MsxMediaSaveBoot);
    keys.push_back(enter);
    strcpy(selectedDisks[0], "/disks/a.dsk");
    strcpy(selectedDisks[1], "/disks/b.dsk");
    strcpy(selectedTape, "/tapes/game.cas");
    assert(mediaMenu() && bootRequested && exitRequested);
    assert(keys.empty() && files.empty());
    assert(saves == 1 && pauses == 1 && resumes == 1);
    assert(saved.machine.version == 4 && saved.machine.ramPages == 32 && saved.machine.autoBoot);
    assert(saved.mappers[0] == MsxMapperAuto && saved.mappers[1] == MsxMapperAuto);
    assert(saved.audioProfile == MsxAudioAuto);
    assert(!strcmp(saved.machine.profile, "omega") && saved.machine.sound);
    assert(!strcmp(saved.cartridges[0], "/games/one.rom") && !strcmp(saved.cartridges[1], "/games/two.ROM"));
    assert(!strcmp(saved.disks[0], selectedDisks[0]) && !strcmp(saved.disks[1], selectedDisks[1]));
    assert(!strcmp(saved.tape, selectedTape));
    assert(filters == std::vector<std::string>({".rom", ".rom"}));
    onlyMediaPages();

    reset();
    files = {"/game.rom"};
    keys = {enter, up, enter};
    assert(mediaMenu() && bootRequested && exitRequested);
    assert(!saves && !pauses && !strcmp(selectedCartridges[0], "/game.rom"));

    for (unsigned failure = 0; failure < 6; ++failure)
    {
        reset();
        if (failure == 0) preferencesReady = false;
        if (failure == 1) pauseOk = false;
        if (failure == 2) writeOk = false;
        if (failure == 3) validProfile = false;
        if (failure == 4) validCartridges = false;
        if (failure == 5) validMedia = false;
        assert(!requestBoot(true) && !bootRequested && !exitRequested);
        if (failure < 3) assert(statusMessage[0]);
        if (failure != 2) assert(!saves);
    }
    reset();
    writeOk = false;
    moveTo(MsxMediaSlot1, MsxMediaSaveBoot);
    keys.insert(keys.end(), {enter, f12});
    assert(mediaMenu() && !bootRequested && !exitRequested);
    assert(saves == 1 && resumes == 1); // Save failure keeps menus usable.

    reset();
    files = {"/a.dsk", "/b.DSK"};
    moveTo(MsxMediaSlot1, MsxMediaDiskA);
    keys.insert(keys.end(), {enter, down, enter, f12});
    assert(mediaMenu() && !bootRequested && !exitRequested && !saves);
    assert(diskAttaches == 2 && lastDrive == 1 && lastPath == "/sdcard/b.DSK");
    assert(!strcmp(selectedDisks[0], "/a.dsk") && !strcmp(selectedDisks[1], "/b.DSK"));
    assert(filters == std::vector<std::string>({".dsk", ".dsk"}));
    assert(!tapeAttaches && !diskValidations);
    onlyMediaPages();
    // Enter on the drive itself must launch the picker, with no category menu.
    assert(titles.size() == MsxMediaDiskA + 4);

    reset();
    files = {"/game.CAS"};
    moveTo(MsxMediaSlot1, MsxMediaTape);
    keys.insert(keys.end(), {enter, down, enter, up, del, f12});
    assert(mediaMenu() && !bootRequested && !exitRequested && !saves);
    assert(tapeAttaches == 2 && rewinds == 1 && lastPath.empty() && !selectedTape[0]);
    assert(filters == std::vector<std::string>({".cas"}));
    assert(!diskAttaches && !tapeValidations);
    onlyMediaPages();

    reset();
    strcpy(selectedDisks[0], "/old.dsk");
    attachOk = false;
    files = {"/bad.dsk"};
    moveTo(MsxMediaSlot1, MsxMediaDiskA);
    keys.insert(keys.end(), {enter, f12});
    assert(mediaMenu() && !strcmp(selectedDisks[0], "/old.dsk"));
    assert(!bootRequested && !saves);
    reset();
    strcpy(selectedTape, "/old.cas");
    attachOk = false;
    files = {"/bad.cas"};
    moveTo(MsxMediaSlot1, MsxMediaTape);
    keys.insert(keys.end(), {enter, f12});
    assert(mediaMenu() && !strcmp(selectedTape, "/old.cas"));

    reset();
    strcpy(selectedCartridges[0], "/old.rom");
    validCandidate = false;
    files = {"/bad.rom", "<cancel>"};
    keys = {enter, enter, f12};
    assert(mediaMenu() && !strcmp(selectedCartridges[0], "/old.rom"));
    assert(!bootRequested && !saves);
    reset();
    strcpy(selectedCartridges[0], "/old.rom");
    keys = {del, up, enter};
    assert(mediaMenu() && bootRequested && !selectedCartridges[0][0] && !saves);

    reset();
    files = {"/pending.rom"};
    keys = {enter, f12};
    assert(mediaMenu() && !bootRequested && !exitRequested && !saves);
    assert(!strcmp(selectedCartridges[0], "/pending.rom"));
    assert(cartridgeInfoValid[0] && cartridgeInfo[0].detectedMapper == MsxKonamiScc);

    reset();
    const uint8_t left = 80, right = 79;
    files = {"/one.rom", "/two.rom"};
    keys = {enter, down, right, right, right, down, enter, down, left, left, left};
    moveTo(MsxMediaMapper2, MsxMediaSaveBoot);
    keys.push_back(enter);
    assert(mediaMenu() && bootRequested && saves == 1);
    assert(selectedMappers[0] == MsxKonamiScc && selectedMappers[1] == MsxAscii16);
    assert(saved.mappers[0] == MsxKonamiScc && saved.mappers[1] == MsxAscii16);
    assert(inspectedProfile == "/sdcard/msx/bios/omega");
    bool shown = false;
    for (const auto &label : labels) if (label == "  Mapper: Auto - Konami SCC") shown = true;
    assert(shown);

    reset();
    strcpy(selectedCartridges[0], "/one.rom");
    selectedMappers[0] = MsxAscii8;
    files = {"/one.rom", "/new.rom"};
    keys = {enter, f12};
    assert(mediaMenu() && selectedMappers[0] == MsxAscii8);
    keys = {enter, f12};
    assert(mediaMenu() && selectedMappers[0] == MsxMapperAuto);
    selectedMappers[0] = MsxAscii16;
    validCandidate = false;
    files = {"/bad.rom"};
    keys = {enter, f12};
    assert(mediaMenu() && selectedMappers[0] == MsxAscii16 && !strcmp(selectedCartridges[0], "/new.rom"));

    reset();
    strcpy(selectedCartridges[0], "/one.rom");
    selectedMappers[0] = MsxKonami;
    keys = {down, del, f12};
    assert(mediaMenu() && selectedMappers[0] == MsxMapperAuto && !bootRequested);
    selectedMappers[0] = MsxKonami;
    keys = {del, f12};
    assert(mediaMenu() && !selectedCartridges[0][0] && selectedMappers[0] == MsxMapperAuto);
    keys = {down, right, f12};
    assert(mediaMenu() && selectedMappers[0] == MsxMapperAuto && statusMessage[0]);

    reset();
    selectedProfile = 1;
    detectedMapper = -12;
    files = {"/unsupported.rom"};
    keys = {enter, down, right, f12};
    assert(mediaMenu() && cartridgeInfo[0].detectedMapper == -12 && selectedMappers[0] == MsxGeneric8);
    assert(inspectedProfile == "/sdcard/msx/bios/fs-a1fx");

    reset();
    moveTo(MsxMediaSlot1, MsxMediaAudio);
    keys.insert(keys.end(), {enter, down, down, down, enter, down, down, enter});
    assert(mediaMenu() && bootRequested && exitRequested && saves == 1);
    assert(selectedAudioProfile == MsxAudioPsgFm && saved.audioProfile == MsxAudioPsgFm);
    assert(resolvedAudio == MsxAudioPsgFm);
    reset();
    audioAvailable = false;
    moveTo(MsxMediaSlot1, MsxMediaAudio);
    keys.insert(keys.end(), {enter, down, down, down, enter, f12});
    assert(mediaMenu() && selectedAudioProfile == MsxAudioAuto && !bootRequested && !saves && statusMessage[0]);
    selectedAudioProfile = MsxAudioPsgFm;
    assert(!requestBoot(true) && !bootRequested && !saves);
    reset();
    moveTo(MsxMediaSlot1, MsxMediaAudio);
    keys.insert(keys.end(), {enter, down, down, down, enter, esc, f12});
    assert(mediaMenu() && selectedAudioProfile == MsxAudioPsgFm && !bootRequested && !saves);
    onlyMediaPages();
    bool audioShown = false;
    for (const auto &title : titles) if (title == "Media > Audio - whole machine") audioShown = true;
    assert(audioShown && titles.back() == "Media");

    reset();
    keys = {esc};
    assert(!mediaMenu() && keys.empty() && !bootRequested && !exitRequested);
    assert(titles == std::vector<std::string>({"Media"}));
    for (const char *label : {"Slot 1: <empty>", "Slot 2: <empty>", "Drive A: <empty>",
                             "Drive B: <empty>", "Tape: <empty>", "Rewind tape",
                             "Audio: Auto", "Reboot and save configuration", "Reboot without saving"})
    {
        bool found = false;
        for (const auto &shown : labels) if (shown == label) found = true;
        assert(found);
    }

    reset();
    strcpy(selectedDisks[0], "/a.dsk");
    strcpy(selectedDisks[1], "/b.dsk");
    moveTo(MsxMediaSlot1, MsxMediaDiskA);
    keys.insert(keys.end(), {del, down, del, f12});
    assert(mediaMenu() && diskAttaches == 2 && !selectedDisks[0][0] && !selectedDisks[1][0]);
    assert(!bootRequested && !saves);

    reset(false);
    files = {"/boot.dsk", "/boot.cas"};
    moveTo(MsxMediaSlot1, MsxMediaDiskA);
    keys.insert(keys.end(), {enter, down, down, enter, down, enter, f12});
    assert(!mediaMenu() && !bootRequested && !exitRequested);
    assert(diskValidations == 1 && tapeValidations == 1 && !diskAttaches && !tapeAttaches && !rewinds);
    keys = {up, enter};
    assert(mediaMenu() && bootRequested && !exitRequested && !saves);
    puts("PASS: flat Media attachments, Audio-only submenu, mapper controls, save/no-save reboot, failures, live resume, eject, rewind and startup selection.");
}
