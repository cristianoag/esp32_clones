#pragma once

enum MsxBootStage : unsigned
{
    MsxBootBuffers,
    MsxBootKeyboard,
    MsxBootAudio,
    MsxBootSd,
    MsxBootProfiles,
    MsxBootSavedSettings,
    MsxBootJoysticks,
    MsxBootStageCount
};

constexpr unsigned MsxBootPercent(unsigned completed)
{
    return completed >= MsxBootStageCount ? 100 : completed * 100 / MsxBootStageCount;
}

constexpr unsigned MsxProgressWidth(unsigned percent)
{
    return (percent > 100 ? 100 : percent) * 300 / 100;
}
