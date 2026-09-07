#include "MsxProfiles.h"
#include <stdio.h>
#include <string.h>

static bool parseError(MsxProfile &profile, const char *message)
{
    snprintf(profile.error, sizeof(profile.error), "%s", message);
    return false;
}

static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r') ++text;
    size_t length = strlen(text);
    while (length && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r'))
        text[--length] = '\0';
    return text;
}

bool MsxParseProfile(const char *text, MsxProfile &profile)
{
    profile.available = false;
    profile.error[0] = '\0';
    if (!text || strlen(text) > 1024) return parseError(profile, "profile.ini exceeds 1024 bytes.");
    bool hasName = false, hasModel = false, hasRam = false;
    while (*text)
    {
        const char *end = strchr(text, '\n');
        size_t length = end ? static_cast<size_t>(end - text) : strlen(text);
        char line[128];
        if (length >= sizeof(line)) return parseError(profile, "profile.ini line exceeds 127 bytes.");
        memcpy(line, text, length);
        line[length] = '\0';
        text += length + (end ? 1 : 0);
        char *key = trim(line);
        if (!*key || *key == '#') continue;
        char *value = strchr(key, '=');
        if (!value) return parseError(profile, "Malformed profile.ini line.");
        *value++ = '\0';
        key = trim(key);
        value = trim(value);
        if (!strcmp(key, "name") && !hasName)
        {
            length = strlen(value);
            if (!length || length > 40) return parseError(profile, "Name must contain 1-40 ASCII characters.");
            for (size_t i = 0; i < length; ++i)
                if (value[i] < 32 || value[i] > 126) return parseError(profile, "Profile name must be printable ASCII.");
            snprintf(profile.name, sizeof(profile.name), "%s", value);
            hasName = true;
        }
        else if (!strcmp(key, "model") && !hasModel)
        {
            if (!strcmp(value, "MSX1")) profile.model = 0;
            else if (!strcmp(value, "MSX2")) profile.model = 1;
            else if (!strcmp(value, "MSX2+")) profile.model = 2;
            else return parseError(profile, "Model must be MSX1, MSX2, or MSX2+.");
            hasModel = true;
        }
        else if (!strcmp(key, "ram") && !hasRam)
        {
            if (!strcmp(value, "64")) profile.ramPages = 4;
            else if (!strcmp(value, "128")) profile.ramPages = 8;
            else if (!strcmp(value, "256")) profile.ramPages = 16;
            else if (!strcmp(value, "512")) profile.ramPages = 32;
            else return parseError(profile, "RAM must be 64, 128, 256, or 512 KiB.");
            hasRam = true;
        }
        else return parseError(profile, "Unknown or duplicate profile.ini setting.");
    }
    if (!hasName || !hasModel || !hasRam) return parseError(profile, "profile.ini needs name, model, and ram.");
    return true;
}
