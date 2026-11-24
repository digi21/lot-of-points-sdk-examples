// Texts of the camera path commands in the nine languages of lop.
#pragma once

#include <lop/sdk.hpp>

enum class TextId
{
    utilities_section,
    record_name,
    play_name,
    stop_recording,
    pause,
    resume,
    slower,
    faster,
    loop,
    stop_playback,
    open,
    playback_banner,            // std::vformat arguments: {0} time mm:ss, {1} duration mm:ss, {2} speed multiplier.
    playback_banner_looping,    // The same arguments as playback_banner.
    recording_banner,           // {0} duration mm:ss, {1} sample count.
    recorded_log,               // {0} duration in seconds, {1} sample count, {2} file path.
    write_failed_log,           // {0} file path, {1} reason.
    camera_path_read_failed_log,    // {0} file path, {1} reason.
    camera_path_file_type,
    record_description,
    play_description,
};

// Returns the text of id in language, or the English text if there is none for that language.
[[nodiscard]] char const* text(lop::Language language, TextId id);
