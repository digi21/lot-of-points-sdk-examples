// Declares Record camera path and Play camera path. Recording samples camera poses, enforces a minimum interval between regular samples and
// retains only the first and last sample of consecutive identical poses; the playback assigns the pose of the path in each frame.
#include "camera_path.hpp"
#include "camera_path_texts.hpp"
#include "commands.hpp"
// Regenerate the embedded PNG files with:
// tools/png_to_header.ps1 cpp/icons/record_camera_path.png cpp/record_camera_path_icon.h record_camera_path_icon
// tools/png_to_header.ps1 cpp/icons/play_camera_path.png cpp/play_camera_path_icon.h play_camera_path_icon
#include "play_camera_path_icon.h"
#include "record_camera_path_icon.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    constexpr auto record_command_id = "lop.example.record_camera_path";
    constexpr auto play_command_id = "lop.example.play_camera_path";
    constexpr auto camera_path_extension = ".lopcamerapath";
    // Longest real interval that one frame advances the playback, in seconds; Rendering stops while the window is minimized.
    constexpr double maximum_playback_step_seconds = 0.1;
    constexpr double banner_refresh_interval_seconds = 0.25;

    // The last path recorded in this process; only the user interface thread reads and assigns it.
    std::optional<CameraPath>& last_recorded_camera_path()
    {
        static std::optional<CameraPath> path;
        return path;
    }

    template<typename... Arguments>
    [[nodiscard]] std::string format_text(lop::Language const language, TextId const id, Arguments const&... arguments)
    {
        return std::vformat(text(language, id), std::make_format_args(arguments...));
    }

    // Returns "mm:ss" for a time in seconds, truncated; the minutes have as many digits as needed.
    [[nodiscard]] std::string minutes_and_seconds(double const seconds)
    {
        auto const whole_seconds = static_cast<long long>(std::max(seconds, 0.0));
        return std::format("{:02}:{:02}", whole_seconds / 60, whole_seconds % 60);
    }

    [[nodiscard]] std::filesystem::path path_from_utf8(std::string const& text_utf8)
    {
        return std::filesystem::path{ std::u8string{ reinterpret_cast<char8_t const*>(text_utf8.data()), text_utf8.size() } };
    }

    [[nodiscard]] std::string utf8_from_path(std::filesystem::path const& path)
    {
        auto const text_utf8 = path.u8string();
        return std::string{ reinterpret_cast<char const*>(text_utf8.data()), text_utf8.size() };
    }

    // Returns the local time as YYYYMMDD-HHMMSS.
    [[nodiscard]] std::string local_timestamp()
    {
        auto const now = std::time(nullptr);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        return std::format("{:04}{:02}{:02}-{:02}{:02}{:02}", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min,
            local.tm_sec);
    }

    class RecordCameraPathCommand final : public lop::Command
    {
    public:
        explicit RecordCameraPathCommand(lop::Language const language) : language_{ language } {}

        lop::Task<> on_start() override
        {
            // Stores the extension data directory path during on_start for the automatic save in on_finish.
            automatic_save_directory_path_ = host().get_extension_data_directory_path();
            host().set_kept_below_new_commands(true);
            host().set_view_frame_notices(lop::ViewFrameNotices::every_frame);
            show_banner();
            co_return;
        }

        void on_view_frame(lop::ViewFrame const& frame) override
        {
            if (stopped_)
                return;
            recorder_.record_pose(frame.frame_time_seconds, frame.camera_pose);
            last_frame_ = frame;
            if (frame.frame_time_seconds - last_banner_time_seconds_ >= banner_refresh_interval_seconds)
            {
                last_banner_time_seconds_ = frame.frame_time_seconds;
                show_banner();
            }
        }

        lop::Task<> on_banner_action(std::string const action) override
        {
            if (action != "stop" || stopped_)
                co_return;
            stop_recording();
            if (!recorded_path_)
            {
                finish();
                co_return;
            }
            std::optional<std::string> file_path;
            try
            {
                file_path = co_await host().ask_save_file_path(suggested_file_name(), text(language_, TextId::camera_path_file_type),
                    camera_path_extension);
            }
            catch (std::exception const&)
            {
                // If the picker fails, on_finish tries to save the recording in the extension data directory, because written_ is still false.
            }
            if (file_path && !file_path->empty())
                write(*file_path);
            finish();
        }

        // Esc, the ribbon button and the closing of the document finish the command without the save file picker. If writing the
        // recording to the selected file fails, on_finish tries to save the recording to a file in the extension data directory.
        void on_finish() override
        {
            if (!stopped_)
                stop_recording();
            if (recorded_path_ && !written_)
                write(automatic_file_path());
        }

    private:
        void show_banner() const
        {
            auto const duration = minutes_and_seconds(recorder_.recorded_seconds());
            auto const sample_count = recorder_.sample_count();
            host().show_banner(format_text(language_, TextId::recording_banner, duration, sample_count), std::nullopt,
                { lop::BannerButton{ text(language_, TextId::stop_recording), "stop" } });
        }

        // Stops recording samples. If a frame was recorded, creates a CameraPath and assigns it to recorded_path_ and
        // last_recorded_camera_path().
        void stop_recording()
        {
            stopped_ = true;
            if (last_frame_)
                recorded_path_ = recorder_.to_camera_path(last_frame_->frame_time_seconds, last_frame_->camera_pose);
            if (recorded_path_)
                last_recorded_camera_path() = *recorded_path_;
        }

        // Writes the path to file_path and logs the duration, the sample count and the file path; logs the reason of a failure and leaves
        // written_ false.
        void write(std::string const& file_path)
        {
            try
            {
                if (file_path.empty())
                    throw std::runtime_error{ "the data folder of the extension is not available" };
                auto const path = path_from_utf8(file_path);
                if (path.has_parent_path())
                    std::filesystem::create_directories(path.parent_path());
                std::ofstream file{ path, std::ios::binary };
                file << to_camera_path_json(*recorded_path_);
                file.close();
                if (!file)
                    throw std::runtime_error{ "the file could not be written" };
                written_ = true;
                auto const seconds = std::format("{:.1f}", recorded_path_->duration_seconds());
                auto const sample_count = recorded_path_->samples().size();
                host().log(format_text(language_, TextId::recorded_log, seconds, sample_count, file_path));
            }
            catch (std::exception const& exception)
            {
                std::string const reason{ exception.what() };
                host().log(format_text(language_, TextId::write_failed_log, file_path, reason));
            }
        }

        // <extension data>\camera-paths\camera-path-YYYYMMDD-HHMMSS.lopcamerapath; empty if the extension has no data folder.
        [[nodiscard]] std::string automatic_file_path() const
        {
            if (!automatic_save_directory_path_)
                return {};
            return utf8_from_path(path_from_utf8(*automatic_save_directory_path_) / "camera-paths" / path_from_utf8(suggested_file_name()));
        }

        // camera-path-YYYYMMDD-HHMMSS.lopcamerapath, local time.
        [[nodiscard]] std::string suggested_file_name() const { return std::format("camera-path-{}{}", local_timestamp(), camera_path_extension); }

        lop::Language language_;
        CameraPathRecorder recorder_;
        std::optional<lop::ViewFrame> last_frame_;
        std::optional<CameraPath> recorded_path_;
        std::optional<std::string> automatic_save_directory_path_;
        double last_banner_time_seconds_{};
        bool stopped_{};
        bool written_{};
    };

    // Reads a .lopcamerapath file; logs the reason and returns std::nullopt for a file that cannot be read.
    [[nodiscard]] std::optional<CameraPath> read_camera_path_file(lop::Host const& host, lop::Language const language, std::string const& file_path)
    {
        try
        {
            std::ifstream file{ path_from_utf8(file_path), std::ios::binary };
            if (!file)
                throw std::runtime_error{ "the file could not be opened" };
            std::ostringstream content;
            content << file.rdbuf();
            return camera_path_from_json(content.str());
        }
        catch (std::exception const& exception)
        {
            std::string const reason{ exception.what() };
            host.log(format_text(language, TextId::camera_path_read_failed_log, file_path, reason));
            return std::nullopt;
        }
    }

    class PlayCameraPathCommand final : public lop::Command
    {
    public:
        explicit PlayCameraPathCommand(lop::Language const language) : language_{ language } {}

        lop::Task<> on_start() override
        {
            auto path = last_recorded_camera_path();
            if (!path)
                path = co_await open_file();
            if (!path)
            {
                finish();
                co_return;
            }
            co_await play(std::move(*path));
        }

        void on_view_frame(lop::ViewFrame const& frame) override
        {
            if (!playback_ || is_animating_to_first_pose_)
                return;
            // Another component moved the camera after the previous set_camera_pose of this command.
            if (previous_frame_time_seconds_ && !frame.camera_pose_set_by_this_command)
            {
                finish();
                return;
            }
            auto const elapsed = previous_frame_time_seconds_
                ? std::min(frame.frame_time_seconds - *previous_frame_time_seconds_, maximum_playback_step_seconds) : 0.0;
            previous_frame_time_seconds_ = frame.frame_time_seconds;
            auto const pose = playback_->advance(elapsed);
            if (!pose)
            {
                finish();
                return;
            }
            host().set_camera_pose(*pose);
            if (frame.frame_time_seconds - last_banner_time_seconds_ >= banner_refresh_interval_seconds)
            {
                last_banner_time_seconds_ = frame.frame_time_seconds;
                show_banner();
            }
        }

        lop::Task<> on_banner_action(std::string const action) override
        {
            if (!playback_)
                co_return;
            if (action == "pause") playback_->paused = !playback_->paused;
            else if (action == "slower") playback_->decrease_speed();
            else if (action == "faster") playback_->increase_speed();
            else if (action == "loop") playback_->loops = !playback_->loops;
            else if (action == "stop") { finish(); co_return; }
            else if (action == "open")
            {
                // A second animate_camera call would cancel the pending animation, causing the suspended play call to invoke finish().
                if (is_animating_to_first_pose_)
                    co_return;
                if (auto path = co_await open_file())
                    co_await play(std::move(*path));
                co_return;
            }
            show_banner();
        }

        bool on_key_down(lop::Key const key) override
        {
            switch (key)
            {
            case lop::Key::space:
                if (playback_)
                    playback_->paused = !playback_->paused;
                show_banner();
                return true;
            case lop::Key::left:
                if (playback_)
                    playback_->decrease_speed();
                show_banner();
                return true;
            case lop::Key::right:
                if (playback_)
                    playback_->increase_speed();
                show_banner();
                return true;
            case lop::Key::escape:
                finish();
                return true;
            // The navigation keys finish the playback; returning false gives the key to the navigation of lop.
            case lop::Key{ 'W' }: case lop::Key{ 'A' }: case lop::Key{ 'S' }: case lop::Key{ 'D' }:
                finish();
                return false;
            default:
                return false;
            }
        }

        // A mouse-button press or mouse-wheel event finishes playback; returning false lets lop process the event for navigation.
        bool on_mouse_button_down(lop::MouseButton, lop::ScreenPoint, bool) override
        {
            finish();
            return false;
        }

        bool on_mouse_wheel(int, lop::ScreenPoint, bool, bool) override
        {
            finish();
            return false;
        }

    private:
        lop::Task<> play(CameraPath path)
        {
            playback_.emplace(std::move(path));
            previous_frame_time_seconds_.reset();
            is_animating_to_first_pose_ = true;
            show_banner();
            host().set_view_frame_notices(lop::ViewFrameNotices::every_frame);
            // Awaiting animate_camera returns std::nullopt if another camera movement interrupts the animation or the command finishes.
            if (!co_await host().animate_camera(playback_->path().samples().front().pose))
            {
                finish();
                co_return;
            }
            is_animating_to_first_pose_ = false;
        }

        // Asks for a .lopcamerapath file and reads it; logs the reason and returns std::nullopt for a file that cannot be read.
        lop::Task<std::optional<CameraPath>> open_file()
        {
            auto const file_path = co_await host().ask_file({ camera_path_extension });
            if (!file_path)
                co_return std::nullopt;
            co_return read_camera_path_file(host(), language_, *file_path);
        }

        // Shows the progress of the playback, its speed and the six buttons; nothing before the first path is loaded.
        void show_banner() const
        {
            if (!playback_)
                return;
            auto const& path = playback_->path();
            auto const time = minutes_and_seconds(playback_->path_time_seconds());
            auto const duration = minutes_and_seconds(path.duration_seconds());
            auto const speed = std::format("{}", playback_->speed_multiplier());
            auto const message = format_text(language_, playback_->loops ? TextId::playback_banner_looping : TextId::playback_banner, time, duration,
                speed);
            auto const progress = path.duration_seconds() > 0.0 ? std::min(playback_->path_time_seconds() / path.duration_seconds(), 1.0) : 0.0;
            std::vector<lop::BannerButton> const buttons{
                { text(language_, playback_->paused ? TextId::resume : TextId::pause), "pause" },
                { text(language_, TextId::slower), "slower" },
                { text(language_, TextId::faster), "faster" },
                { text(language_, TextId::loop), "loop" },
                { text(language_, TextId::stop_playback), "stop" },
                { text(language_, TextId::open), "open" },
            };
            host().show_banner(message, progress, buttons);
        }

        lop::Language language_;
        std::optional<CameraPathPlayback> playback_;
        std::optional<double> previous_frame_time_seconds_;
        double last_banner_time_seconds_{};
        bool is_animating_to_first_pose_{};
    };
}

std::vector<lop::Declaration> declare_camera_path_commands(lop::Language const language)
{
    return {
        lop::declare<RecordCameraPathCommand>({
            .id = record_command_id,
            .name = text(language, TextId::record_name),
            .new_tab_title = text(language, TextId::utilities_tab),
            .ribbon_section = text(language, TextId::utilities_section),
            .description = text(language, TextId::record_description),
            .icon_png = { std::begin(record_camera_path_icon), std::end(record_camera_path_icon) },
            .requirements = LOP_REQUIRES_OPEN_CLOUD,
        }, language),
        lop::declare<PlayCameraPathCommand>({
            .id = play_command_id,
            .name = text(language, TextId::play_name),
            .new_tab_title = text(language, TextId::utilities_tab),
            .ribbon_section = text(language, TextId::utilities_section),
            .description = text(language, TextId::play_description),
            .icon_png = { std::begin(play_camera_path_icon), std::end(play_camera_path_icon) },
            .requirements = LOP_REQUIRES_OPEN_CLOUD,
        }, language),
    };
}
