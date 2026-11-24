// Implements the camera path model: validation, interpolation, recording, playback and the .lopcamerapath format.
#include "camera_path.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

namespace
{
    using lop::Point3;
    using Json = nlohmann::ordered_json;

    constexpr int camera_path_format_version = 1;
    constexpr double minimum_vector_length = 1e-9;

    [[nodiscard]] Point3 cross(Point3 const& a, Point3 const& b) noexcept
    {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }

    [[nodiscard]] double length(Point3 const& vector) noexcept { return std::hypot(vector.x, vector.y, vector.z); }

    [[nodiscard]] Point3 scaled(Point3 const& vector, double const factor) noexcept
    {
        return { vector.x * factor, vector.y * factor, vector.z * factor };
    }

    [[nodiscard]] bool is_finite(Point3 const& vector) noexcept
    {
        return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
    }

    [[nodiscard]] Quaternion normalized(Quaternion const& q) noexcept
    {
        auto const norm = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
        return { q.w / norm, q.x / norm, q.y / norm, q.z / norm };
    }

    // Uses normalized linear interpolation when the absolute quaternion dot product is at least 0.9995; otherwise, uses spherical linear interpolation along the shortest arc.
    [[nodiscard]] Quaternion slerp(Quaternion const& a, Quaternion b, double const fraction) noexcept
    {
        auto cosine = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
        if (cosine < 0.0)
        {
            b = { -b.w, -b.x, -b.y, -b.z };
            cosine = -cosine;
        }
        double weight_a = 1.0 - fraction;
        double weight_b = fraction;
        if (cosine < 0.9995)
        {
            auto const angle = std::acos(cosine);
            auto const sine = std::sin(angle);
            weight_a = std::sin((1.0 - fraction) * angle) / sine;
            weight_b = std::sin(fraction * angle) / sine;
        }
        return normalized({ weight_a * a.w + weight_b * b.w, weight_a * a.x + weight_b * b.x, weight_a * a.y + weight_b * b.y,
            weight_a * a.z + weight_b * b.z });
    }

    [[nodiscard]] bool same_pose(lop::CameraPose const& a, lop::CameraPose const& b) noexcept
    {
        auto const same_vector = [](Point3 const& u, Point3 const& v) { return u.x == v.x && u.y == v.y && u.z == v.z; };
        return same_vector(a.position, b.position) && same_vector(a.look_direction, b.look_direction) && same_vector(a.up, b.up)
            && a.vertical_field_of_view_degrees == b.vertical_field_of_view_degrees;
    }

    // Throws std::invalid_argument for a sample that a camera path does not accept, without the checks that involve other samples.
    void validate_sample(CameraPathSample const& sample, std::size_t const sample_index)
    {
        auto const fail = [sample_index](char const* const reason) { throw std::invalid_argument{ std::format("sample {}: {}", sample_index, reason) }; };
        auto const& pose = sample.pose;
        if (!std::isfinite(sample.time_seconds) || !is_finite(pose.position) || !is_finite(pose.look_direction) || !is_finite(pose.up)
            || !std::isfinite(pose.vertical_field_of_view_degrees))
            fail("a number is not finite");
        if (!(pose.vertical_field_of_view_degrees > 0.0 && pose.vertical_field_of_view_degrees < 180.0))
            fail("VerticalFieldOfViewDegrees is not greater than 0 and less than 180");
        auto const look_length = length(pose.look_direction);
        auto const up_length = length(pose.up);
        if (!(look_length > minimum_vector_length))
            fail("LookDirection must have a length greater than 1e-9");
        if (!(up_length > minimum_vector_length))
            fail("Up must have a length greater than 1e-9");
        if (length(cross(scaled(pose.look_direction, 1.0 / look_length), scaled(pose.up, 1.0 / up_length))) < minimum_vector_length)
            fail("The cross product of normalized LookDirection and Up must have a length of at least 1e-9");
    }

    [[nodiscard]] Json const& member(Json const& object, char const* const key, std::size_t const sample_index)
    {
        auto const found = object.find(key);
        if (found == object.end())
            throw std::invalid_argument{ std::format("sample {}: {} is missing", sample_index, key) };
        return *found;
    }

    [[nodiscard]] double read_number(Json const& object, char const* const key, std::size_t const sample_index)
    {
        auto const& value = member(object, key, sample_index);
        if (!value.is_number())
            throw std::invalid_argument{ std::format("sample {}: {} is not a number", sample_index, key) };
        return value.get<double>();
    }

    [[nodiscard]] Point3 read_vector(Json const& object, char const* const key, std::size_t const sample_index)
    {
        auto const& value = member(object, key, sample_index);
        if (!value.is_array() || 3 != value.size() || !std::ranges::all_of(value, [](Json const& item) { return item.is_number(); }))
            throw std::invalid_argument{ std::format("sample {}: {} is not an array of 3 numbers", sample_index, key) };
        return { value[0].get<double>(), value[1].get<double>(), value[2].get<double>() };
    }

    [[nodiscard]] Json vector_to_json(Point3 const& vector) { return Json::array({ vector.x, vector.y, vector.z }); }
}

Quaternion quaternion_from_look_direction_and_up(lop::Point3 const& look_direction, lop::Point3 const& up)
{
    auto const back = scaled(look_direction, -1.0 / length(look_direction));
    auto right = cross(up, back);
    right = scaled(right, 1.0 / length(right));
    auto const orthogonal_up = cross(back, right);

    // Rotation matrix with columns right, orthogonal_up and back; conversion to a quaternion with Shepperd's method.
    double const m00 = right.x, m01 = orthogonal_up.x, m02 = back.x;
    double const m10 = right.y, m11 = orthogonal_up.y, m12 = back.y;
    double const m20 = right.z, m21 = orthogonal_up.z, m22 = back.z;
    if (auto const trace = m00 + m11 + m22; trace > 0.0)
    {
        auto const s = std::sqrt(trace + 1.0) * 2.0;
        return normalized({ s / 4.0, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s });
    }
    if (m00 > m11 && m00 > m22)
    {
        auto const s = std::sqrt(1.0 + m00 - m11 - m22) * 2.0;
        return normalized({ (m21 - m12) / s, s / 4.0, (m01 + m10) / s, (m02 + m20) / s });
    }
    if (m11 > m22)
    {
        auto const s = std::sqrt(1.0 + m11 - m00 - m22) * 2.0;
        return normalized({ (m02 - m20) / s, (m01 + m10) / s, s / 4.0, (m12 + m21) / s });
    }
    auto const s = std::sqrt(1.0 + m22 - m00 - m11) * 2.0;
    return normalized({ (m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, s / 4.0 });
}

lop::Point3 rotated(Quaternion const& rotation, lop::Point3 const& vector)
{
    // v + 2w (u x v) + 2 u x (u x v), with u the vector part of the quaternion.
    Point3 const u{ rotation.x, rotation.y, rotation.z };
    auto const u_cross_v = cross(u, vector);
    auto const u_cross_u_cross_v = cross(u, u_cross_v);
    return { vector.x + 2.0 * (rotation.w * u_cross_v.x + u_cross_u_cross_v.x), vector.y + 2.0 * (rotation.w * u_cross_v.y + u_cross_u_cross_v.y),
        vector.z + 2.0 * (rotation.w * u_cross_v.z + u_cross_u_cross_v.z) };
}

CameraPath::CameraPath(std::vector<CameraPathSample> samples) : samples_{ std::move(samples) }
{
    if (samples_.empty())
        throw std::invalid_argument{ "the path has no samples" };
    for (std::size_t sample_index = 0; sample_index < samples_.size(); ++sample_index)
    {
        validate_sample(samples_[sample_index], sample_index);
        if (0 == sample_index && 0.0 != samples_[0].time_seconds)
            throw std::invalid_argument{ "sample 0: TimeSeconds is not 0" };
        if (sample_index > 0 && !(samples_[sample_index].time_seconds > samples_[sample_index - 1].time_seconds))
            throw std::invalid_argument{ std::format("sample {}: TimeSeconds does not increase", sample_index) };
    }
    orientations_.reserve(samples_.size());
    for (auto const& sample : samples_)
        orientations_.push_back(quaternion_from_look_direction_and_up(sample.pose.look_direction, sample.pose.up));
}

lop::CameraPose CameraPath::pose_at(double const time_seconds) const
{
    // The first sample whose time is greater than time_seconds.
    auto const next = std::ranges::upper_bound(samples_, time_seconds, {}, &CameraPathSample::time_seconds);
    if (next == samples_.begin())
        return samples_.front().pose;
    if (next == samples_.end())
        return samples_.back().pose;
    auto const& before = *(next - 1);
    if (before.time_seconds == time_seconds)
        return before.pose;

    auto const& after = *next;
    auto const fraction = (time_seconds - before.time_seconds) / (after.time_seconds - before.time_seconds);
    auto const next_sample_index = static_cast<std::size_t>(next - samples_.begin());
    auto const orientation = slerp(orientations_[next_sample_index - 1], orientations_[next_sample_index], fraction);
    auto const lerp = [fraction](double const from, double const to) { return from + (to - from) * fraction; };
    return { { lerp(before.pose.position.x, after.pose.position.x), lerp(before.pose.position.y, after.pose.position.y),
                 lerp(before.pose.position.z, after.pose.position.z) },
        rotated(orientation, { 0.0, 0.0, -1.0 }), rotated(orientation, { 0.0, 1.0, 0.0 }),
        lerp(before.pose.vertical_field_of_view_degrees, after.pose.vertical_field_of_view_degrees) };
}

void CameraPathRecorder::append(double const clock_seconds, lop::CameraPose const& pose)
{
    if (!start_clock_seconds_)
        start_clock_seconds_ = clock_seconds;
    CameraPathSample const sample{ clock_seconds - *start_clock_seconds_, pose };

    if (samples_.empty())
    {
        samples_.push_back(sample);
        return;
    }
    if (sample.time_seconds - samples_.back().time_seconds < minimum_sample_interval_seconds)
        return;

    // With the last two samples equal to the new pose, the last one is replaced: the run keeps its first sample and the most recent one.
    if (samples_.size() >= 2 && same_pose(samples_.back().pose, pose) && same_pose(samples_[samples_.size() - 2].pose, pose))
    {
        samples_.back() = sample;
        return;
    }
    samples_.push_back(sample);
}

std::optional<CameraPath> CameraPathRecorder::to_camera_path(double const clock_seconds, lop::CameraPose const& final_pose) const
{
    if (samples_.empty())
        return std::nullopt;
    auto samples = samples_;
    CameraPathSample const final_sample{ clock_seconds - *start_clock_seconds_, final_pose };
    if (final_sample.time_seconds <= samples.back().time_seconds)
        samples.back().pose = final_pose;
    else if (samples.size() >= 2 && same_pose(samples.back().pose, final_pose) && same_pose(samples[samples.size() - 2].pose, final_pose))
        samples.back() = final_sample;
    else
        samples.push_back(final_sample);
    return CameraPath{ std::move(samples) };
}

void CameraPathPlayback::increase_speed() noexcept
{
    if (speed_multiplier_index_ + 1 < speed_multipliers.size())
        ++speed_multiplier_index_;
}

void CameraPathPlayback::decrease_speed() noexcept
{
    if (speed_multiplier_index_ > 0)
        --speed_multiplier_index_;
}

std::optional<lop::CameraPose> CameraPathPlayback::advance(double const real_elapsed_seconds)
{
    auto const duration = path_.duration_seconds();
    if (!(duration > 0.0))
        return std::nullopt;
    if (!paused)
        path_time_seconds_ += std::max(real_elapsed_seconds, 0.0) * speed_multiplier();
    if (path_time_seconds_ > duration)
    {
        if (!loops)
            return std::nullopt;
        path_time_seconds_ = std::fmod(path_time_seconds_, duration);
    }
    return path_.pose_at(path_time_seconds_);
}

std::string to_camera_path_json(CameraPath const& path)
{
    auto samples = Json::array();
    for (auto const& sample : path.samples())
    {
        Json item;
        item["TimeSeconds"] = sample.time_seconds;
        item["Position"] = vector_to_json(sample.pose.position);
        item["LookDirection"] = vector_to_json(sample.pose.look_direction);
        item["Up"] = vector_to_json(sample.pose.up);
        item["VerticalFieldOfViewDegrees"] = sample.pose.vertical_field_of_view_degrees;
        samples.push_back(std::move(item));
    }
    Json root;
    root["Version"] = camera_path_format_version;
    root["Samples"] = std::move(samples);
    return root.dump();
}

CameraPath camera_path_from_json(std::string_view text)
{
    // Editors can add a UTF-8 byte order mark that JSON does not admit.
    if (text.starts_with("\xEF\xBB\xBF"))
        text.remove_prefix(3);

    Json root;
    try
    {
        root = Json::parse(text);
    }
    catch (Json::exception const& exception)
    {
        throw std::invalid_argument{ std::string{ "the text is not valid JSON: " } + exception.what() };
    }
    if (!root.is_object())
        throw std::invalid_argument{ "the JSON root is not an object" };

    auto const version = root.find("Version");
    if (version == root.end() || !version->is_number_integer())
        throw std::invalid_argument{ "Version is missing or is not an integer" };
    if (version->get<long long>() != camera_path_format_version)
        throw std::invalid_argument{ std::format("Version {} is not supported; this program reads version {}", version->dump(),
            camera_path_format_version) };

    auto const samples_json = root.find("Samples");
    if (samples_json == root.end() || !samples_json->is_array())
        throw std::invalid_argument{ "Samples is missing or is not an array" };

    std::vector<CameraPathSample> samples;
    samples.reserve(samples_json->size());
    for (std::size_t sample_index = 0; sample_index < samples_json->size(); ++sample_index)
    {
        auto const& item = (*samples_json)[sample_index];
        if (!item.is_object())
            throw std::invalid_argument{ std::format("sample {}: it is not an object", sample_index) };
        samples.push_back({ read_number(item, "TimeSeconds", sample_index),
            { read_vector(item, "Position", sample_index), read_vector(item, "LookDirection", sample_index), read_vector(item, "Up", sample_index),
                read_number(item, "VerticalFieldOfViewDegrees", sample_index) } });
    }
    return CameraPath{ std::move(samples) };
}
