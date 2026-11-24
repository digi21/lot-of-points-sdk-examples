// Samples of a camera path, its recording, its playback and the .lopcamerapath format.
#pragma once

#include <lop/sdk.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Unit quaternion w + x i + y j + z k.
struct Quaternion
{
    double w{ 1.0 };
    double x{};
    double y{};
    double z{};
};

// Returns the rotation whose -Z axis points along look_direction and whose +Y axis points along the projection of up perpendicular to
// look_direction.
// Requires a nonzero look_direction and an up that is not parallel to it.
[[nodiscard]] Quaternion quaternion_from_look_direction_and_up(lop::Point3 const& look_direction, lop::Point3 const& up);

// Returns vector rotated by the unit quaternion rotation.
[[nodiscard]] lop::Point3 rotated(Quaternion const& rotation, lop::Point3 const& vector);

struct CameraPathSample
{
    double time_seconds{};      // Seconds from the start of the recording.
    lop::CameraPose pose{};
};

// Samples ordered by strictly increasing time_seconds; the first one has time_seconds 0.
class CameraPath
{
public:
    // Throws std::invalid_argument if samples is empty, the first time is not 0, the times do not strictly increase, a number is not finite,
    // look_direction or up has length at most 1e-9, the cross product of their normalized vectors has length below 1e-9, or a field of view
    // is outside (0, 180) degrees.
    explicit CameraPath(std::vector<CameraPathSample> samples);

    [[nodiscard]] std::vector<CameraPathSample> const& samples() const noexcept { return samples_; }
    [[nodiscard]] double duration_seconds() const noexcept { return samples_.back().time_seconds; }

    // Returns the pose at time_seconds: position and field of view by linear interpolation and orientation by spherical linear interpolation
    // (slerp) between the two samples around time_seconds; the first sample's pose for negative time_seconds and the last sample's pose
    // for time_seconds greater than or equal to duration_seconds.
    [[nodiscard]] lop::CameraPose pose_at(double time_seconds) const;

private:
    std::vector<CameraPathSample> samples_;
    std::vector<Quaternion> orientations_;      // One per sample, from quaternion_from_look_direction_and_up.
};

// Collects the samples of a recording: discards the samples closer than minimum_sample_interval_seconds to the previous kept sample and, of
// each run of consecutive samples with the same pose, keeps the first and the last.
class CameraPathRecorder
{
public:
    static constexpr double minimum_sample_interval_seconds = 1.0 / 60.0;

    // clock_seconds: monotonic clock in seconds (lop::ViewFrame::frame_time_seconds); the first call sets the origin of the times.
    void record_pose(double clock_seconds, lop::CameraPose const& pose);

    [[nodiscard]] std::size_t sample_count() const noexcept { return samples_.size(); }
    // Seconds between the first and the last kept sample; 0 without samples.
    [[nodiscard]] double recorded_seconds() const noexcept { return samples_.empty() ? 0.0 : samples_.back().time_seconds; }

    // Builds a CameraPath from a copy of the recorded samples, adding final_pose at clock_seconds without enforcing the minimum interval;
    // returns std::nullopt if no samples were recorded. If the last kept sample is not older than clock_seconds, final_pose replaces its pose.
    [[nodiscard]] std::optional<CameraPath> to_camera_path(double clock_seconds, lop::CameraPose const& final_pose) const;

private:
    std::vector<CameraPathSample> samples_;
    std::optional<double> start_clock_seconds_;
};

// State of a playback: time of the path, speed, pause and loop.
class CameraPathPlayback
{
public:
    // Speed multipliers available; a playback starts at 1.
    static constexpr std::array<double, 6> speed_multipliers{ 0.25, 0.5, 1.0, 2.0, 4.0, 8.0 };

    explicit CameraPathPlayback(CameraPath path) : path_{ std::move(path) } {}

    [[nodiscard]] CameraPath const& path() const noexcept { return path_; }
    // Path time in seconds; may exceed path().duration_seconds() when advance returns std::nullopt.
    [[nodiscard]] double path_time_seconds() const noexcept { return path_time_seconds_; }
    [[nodiscard]] double speed_multiplier() const noexcept { return speed_multipliers[speed_multiplier_index_]; }
    bool paused{};
    bool loops{};       // If true, a time past duration_seconds continues from 0 plus the excess.

    void increase_speed() noexcept;
    void decrease_speed() noexcept;

    // Unless paused, adds max(real_elapsed_seconds, 0.0) times speed_multiplier() to path_time_seconds_. Returns the pose of the path time, or std::nullopt if
    // the time passes duration_seconds with loops false or if duration_seconds is 0.
    [[nodiscard]] std::optional<lop::CameraPose> advance(double real_elapsed_seconds);

private:
    CameraPath path_;
    double path_time_seconds_{};
    std::size_t speed_multiplier_index_{ 2 };
};

// Returns the .lopcamerapath text (JSON, version 1) of path; each double uses the shortest representation that reads back as the same value.
[[nodiscard]] std::string to_camera_path_json(CameraPath const& path);
// Throws std::invalid_argument with the reason for text that does not follow the .lopcamerapath format (version 1).
[[nodiscard]] CameraPath camera_path_from_json(std::string_view text);
