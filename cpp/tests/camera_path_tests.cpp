// Tests of the camera path model; the exit code is the number of failed checks. Usage: camera_path_tests [pose_at|orientation|recorder|playback|json]
#include "../camera_path.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <format>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>

namespace
{
    int failure_count{};

    void check(bool const condition, std::string_view const description, int const line)
    {
        if (!condition)
        {
            ++failure_count;
            std::printf("FAILED line %d: %.*s\n", line, static_cast<int>(description.size()), description.data());
        }
    }

#define CHECK(condition) check((condition), #condition, __LINE__)

    template<typename Function>
    [[nodiscard]] bool throws_invalid_argument(Function&& function)
    {
        try
        {
            function();
        }
        catch (std::invalid_argument const&)
        {
            return true;
        }
        return false;
    }

    [[nodiscard]] lop::Point3 look_direction_from(double const pitch_degrees, double const yaw_degrees)
    {
        auto const pitch = pitch_degrees * std::numbers::pi / 180.0;
        auto const yaw = yaw_degrees * std::numbers::pi / 180.0;
        return { std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw) };
    }

    [[nodiscard]] lop::Point3 up_from(double const pitch_degrees, double const yaw_degrees)
    {
        auto const pitch = pitch_degrees * std::numbers::pi / 180.0;
        auto const yaw = yaw_degrees * std::numbers::pi / 180.0;
        return { -std::sin(pitch) * std::sin(yaw), std::cos(pitch), std::sin(pitch) * std::cos(yaw) };
    }

    [[nodiscard]] lop::CameraPose pose_from(double const x, double const pitch_degrees, double const yaw_degrees, double const field_of_view_degrees)
    {
        return { { x, 2.0 * x, 3.0 }, look_direction_from(pitch_degrees, yaw_degrees), up_from(pitch_degrees, yaw_degrees), field_of_view_degrees };
    }

    [[nodiscard]] double distance(lop::Point3 const& a, lop::Point3 const& b) { return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z); }

    [[nodiscard]] bool same_bits(double const a, double const b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }

    [[nodiscard]] bool same_bits(lop::Point3 const& a, lop::Point3 const& b) { return same_bits(a.x, b.x) && same_bits(a.y, b.y) && same_bits(a.z, b.z); }

    [[nodiscard]] bool same_bits(lop::CameraPose const& a, lop::CameraPose const& b)
    {
        return same_bits(a.position, b.position) && same_bits(a.look_direction, b.look_direction) && same_bits(a.up, b.up)
            && same_bits(a.vertical_field_of_view_degrees, b.vertical_field_of_view_degrees);
    }

    [[nodiscard]] CameraPath sample_path()
    {
        return CameraPath{ { { 0.0, pose_from(10.0, 0.0, 0.0, 40.0) }, { 1.0, pose_from(20.0, 30.0, 90.0, 50.0) },
            { 2.5, pose_from(-5.0, -20.0, 200.0, 60.0) } } };
    }

    // 1. pose_at returns the exact sample at its time and the mean of positions and field of view at the midpoint.
    void test_pose_at()
    {
        auto const path = sample_path();
        for (auto const& sample : path.samples())
            CHECK(same_bits(path.pose_at(sample.time_seconds), sample.pose));

        for (std::size_t sample_index = 0; sample_index + 1 < path.samples().size(); ++sample_index)
        {
            auto const& a = path.samples()[sample_index];
            auto const& b = path.samples()[sample_index + 1];
            auto const middle = path.pose_at((a.time_seconds + b.time_seconds) / 2.0);
            CHECK(distance(middle.position, { (a.pose.position.x + b.pose.position.x) / 2.0, (a.pose.position.y + b.pose.position.y) / 2.0,
                (a.pose.position.z + b.pose.position.z) / 2.0 }) < 1e-12);
            CHECK(std::abs(middle.vertical_field_of_view_degrees - (a.pose.vertical_field_of_view_degrees + b.pose.vertical_field_of_view_degrees) / 2.0)
                < 1e-12);
            CHECK(std::abs(std::hypot(middle.look_direction.x, middle.look_direction.y, middle.look_direction.z) - 1.0) < 1e-12);
        }
        CHECK(same_bits(path.pose_at(-1.0), path.samples().front().pose));
        CHECK(same_bits(path.pose_at(100.0), path.samples().back().pose));
        CHECK(path.duration_seconds() == 2.5);
    }

    // 2. The quaternion of a look direction and an up reproduces both vectors.
    void test_orientation()
    {
        double maximum_distance = 0.0;
        for (double pitch = 0.0; pitch <= 180.0; pitch += 15.0)
            for (double yaw = -720.0; yaw <= 720.0; yaw += 15.0)
            {
                auto const look = look_direction_from(pitch, yaw);
                auto const up = up_from(pitch, yaw);
                auto const orientation = quaternion_from_look_direction_and_up(look, up);
                maximum_distance = std::max({ maximum_distance, distance(rotated(orientation, { 0.0, 0.0, -1.0 }), look), distance(rotated(orientation, { 0.0, 1.0, 0.0 }), up) });
            }
        CHECK(maximum_distance < 1e-12);
        std::printf("maximum distance between expected and rotated unit vectors (dimensionless): %g\n", maximum_distance);

        auto const identity = quaternion_from_look_direction_and_up({ 0.0, 0.0, -1.0 }, { 0.0, 1.0, 0.0 });
        CHECK(identity.w == 1.0 && identity.x == 0.0 && identity.y == 0.0 && identity.z == 0.0);
    }

    // 3. The recorder tests check compression of identical consecutive poses, at most 61 stored samples for frames spanning one second at 144 Hz,
    // and preservation of the final pose.
    void test_recorder()
    {
        auto const pose_a = pose_from(1.0, 0.0, 0.0, 45.0);
        auto const pose_b = pose_from(2.0, 10.0, 20.0, 45.0);

        CameraPathRecorder equal_poses;
        for (int frame = 0; frame < 10; ++frame)
            equal_poses.append(100.0 + 0.1 * frame, pose_a);
        CHECK(2 == equal_poses.sample_count());
        auto const equal_path = equal_poses.to_camera_path(100.0 + 0.1 * 9, pose_a);
        CHECK(equal_path && 2 == equal_path->samples().size());
        CHECK(equal_path && equal_path->samples().front().time_seconds == 0.0);
        CHECK(equal_path && std::abs(equal_path->samples().back().time_seconds - 0.9) < 1e-12);

        // A pause of 3 s between two movements is a pause of 3 s, not a slow movement.
        CameraPathRecorder pause;
        pause.append(0.0, pose_a);
        for (int frame = 1; frame <= 180; ++frame)
            pause.append(frame / 60.0, pose_a);
        pause.append(3.1, pose_b);
        auto const pause_path = pause.to_camera_path(3.1, pose_b);
        CHECK(pause_path && 3 == pause_path->samples().size());
        CHECK(pause_path && distance(pause_path->pose_at(1.5).position, pose_a.position) < 1e-12);
        CHECK(pause_path && distance(pause_path->pose_at(1.5).look_direction, pose_a.look_direction) < 1e-12);
        CHECK(pause_path && distance(pause_path->pose_at(1.5).up, pose_a.up) < 1e-12);

        // 144 Hz frames with a different pose in each frame.
        CameraPathRecorder recorder_at_144_frames_per_second;
        for (int frame = 0; frame <= 144; ++frame)
            recorder_at_144_frames_per_second.append(frame / 144.0, pose_from(frame, 0.0, 0.0, 45.0));
        CHECK(recorder_at_144_frames_per_second.sample_count() <= 61);
        CHECK(recorder_at_144_frames_per_second.sample_count() > 1);

        // The final pose is kept although it is less than 1/60 s after the last kept sample.
        CameraPathRecorder close_final;
        close_final.append(100.0, pose_a);
        close_final.append(100.5, pose_a);
        close_final.append(101.0, pose_b);
        auto const final_pose = pose_from(3.0, 0.0, 45.0, 45.0);
        auto const close_path = close_final.to_camera_path(101.005, final_pose);
        CHECK(close_path && 4 == close_path->samples().size());
        CHECK(close_path && same_bits(close_path->samples().back().pose, final_pose));
        CHECK(close_path && std::abs(close_path->samples().back().time_seconds - 1.005) < 1e-9);

        CHECK(!CameraPathRecorder{}.to_camera_path(1.0, pose_a));
    }

    // 4. The playback advances with the speed, stops on pause, ends or loops past the duration, and ends at once for a duration of 0.
    void test_playback()
    {
        auto const path = CameraPath{ { { 0.0, pose_from(0.0, 0.0, 0.0, 45.0) }, { 2.0, pose_from(2.0, 0.0, 0.0, 45.0) } } };

        CameraPathPlayback playback{ path };
        CHECK(playback.speed_multiplier() == 1.0);
        playback.increase_speed();
        CHECK(playback.speed_multiplier() == 2.0);
        auto pose = playback.advance(0.25);
        CHECK(pose && std::abs(playback.path_time_seconds() - 0.5) < 1e-12);
        CHECK(pose && std::abs(pose->position.x - 0.5) < 1e-12);

        playback.paused = true;
        pose = playback.advance(1.0);
        CHECK(pose && std::abs(playback.path_time_seconds() - 0.5) < 1e-12);
        playback.paused = false;

        pose = playback.advance(0.75);       // 0.5 + 1.5 = 2.0: the end is not past the duration.
        CHECK(pose && std::abs(playback.path_time_seconds() - 2.0) < 1e-12);
        CHECK(!playback.advance(0.01));

        CameraPathPlayback looping{ path };
        looping.loops = true;
        CHECK(looping.advance(1.5).has_value());
        pose = looping.advance(1.0);         // 2.5: continues from 0 plus 0.5.
        CHECK(pose && std::abs(looping.path_time_seconds() - 0.5) < 1e-12);

        for (int step = 0; step < 10; ++step)
            looping.increase_speed();
        CHECK(looping.speed_multiplier() == 8.0);
        for (int step = 0; step < 10; ++step)
            looping.decrease_speed();
        CHECK(looping.speed_multiplier() == 0.25);

        CameraPathPlayback single_sample{ CameraPath{ { { 0.0, pose_from(0.0, 0.0, 0.0, 45.0) } } } };
        CHECK(!single_sample.advance(1.0));
        single_sample.loops = true;
        CHECK(!single_sample.advance(1.0));
    }

    [[nodiscard]] std::string json_with(std::string_view const version, std::string_view const samples)
    {
        return std::format(R"({{"Version":{},"Samples":{}}})", version, samples);
    }

    [[nodiscard]] std::string json_sample(std::string_view const time_seconds_json, std::string_view const vertical_field_of_view_degrees_json = "45.0")
    {
        return std::format(R"({{"TimeSeconds":{},"Position":[1,2,3],"LookDirection":[0,0,-1],"Up":[0,1,0],"VerticalFieldOfViewDegrees":{}}})",
            time_seconds_json, vertical_field_of_view_degrees_json);
    }

    // 5. The JSON text reads back as the same samples, bit by bit, and the invalid texts are rejected.
    void test_json()
    {
        CameraPath const path{ { { 0.0, { { 0.1, -0.0, 443210.123456789 }, { 1.0 / 3.0, 2.0 / 3.0, -1e-300 }, { 0.0, 1.0, 0.0 }, 45.000000000001 } },
            { 0.016666666666666666, { { 1e300, 4712345.12, 1234.57 }, { -0.1234, 0.9877, -0.0988 }, { 0.0123, -0.0988, 0.9951 }, 179.99999999999997 } },
            { 12.3, { { 5.0e-324, 1.7976931348623157e308, 3.0 }, { 0.5, 0.5, 0.70710678118654757 }, { 0.0, 1.0, 0.0 }, 1e-300 } } } };
        auto const text = to_camera_path_json(path);
        auto const parsed_path = camera_path_from_json(text);
        CHECK(parsed_path.samples().size() == path.samples().size());
        for (std::size_t sample_index = 0; sample_index < std::min(parsed_path.samples().size(), path.samples().size()); ++sample_index)
        {
            CHECK(same_bits(parsed_path.samples()[sample_index].time_seconds, path.samples()[sample_index].time_seconds));
            CHECK(same_bits(parsed_path.samples()[sample_index].pose, path.samples()[sample_index].pose));
        }
        CHECK(to_camera_path_json(parsed_path) == text);

        // A text with a byte order mark and spaces is accepted.
        CHECK(camera_path_from_json("\xEF\xBB\xBF " + json_with("1", "[ " + json_sample("0") + " ]")).samples().size() == 1);

        CHECK(throws_invalid_argument([] { (void)camera_path_from_json(json_with("2", "[" + json_sample("0") + "]")); }));
        CHECK(throws_invalid_argument([] { (void)camera_path_from_json(json_with("1", "[]")); }));
        CHECK(throws_invalid_argument([] { (void)camera_path_from_json(json_with("1", "[" + json_sample("0") + "," + json_sample("0") + "]")); }));
        CHECK(throws_invalid_argument([] { (void)camera_path_from_json(json_with("1", "[" + json_sample("1") + "]")); }));
        CHECK(throws_invalid_argument([] { (void)camera_path_from_json(json_with("1", "[" + json_sample("0") + "," + json_sample("null") + "]")); }));
        CHECK(throws_invalid_argument([] { (void)camera_path_from_json(json_with("1", "[" + json_sample("NaN") + "]")); }));
        CHECK(throws_invalid_argument([] { (void)camera_path_from_json(json_with("1", "[" + json_sample("0", "180") + "]")); }));
        CHECK(throws_invalid_argument([] { (void)camera_path_from_json(json_with("1", "[" + json_sample("0", "0") + "]")); }));
        CHECK(throws_invalid_argument([] { (void)camera_path_from_json("not json"); }));
        CHECK(throws_invalid_argument([] { (void)camera_path_from_json("[]"); }));
        CHECK(throws_invalid_argument([] { (void)camera_path_from_json(R"({"Version":1})"); }));
        CHECK(throws_invalid_argument([] { (void)camera_path_from_json(json_with("1", R"([{"TimeSeconds":0}])")); }));

        // The constructor rejects the values that JSON cannot carry and the vectors that lop rejects.
        auto const nan = std::numeric_limits<double>::quiet_NaN();
        lop::CameraPose const valid{ { 0.0, 0.0, 0.0 }, { 0.0, 0.0, -1.0 }, { 0.0, 1.0, 0.0 }, 45.0 };
        auto const copy_valid_pose_and_apply_change = [&valid](auto const change)
        {
            auto pose = valid;
            change(pose);
            return pose;
        };
        CHECK(throws_invalid_argument([&] { CameraPath{ { { 0.0, copy_valid_pose_and_apply_change([&](lop::CameraPose& pose) { pose.position.x = nan; }) } } }; }));
        CHECK(throws_invalid_argument([&] { CameraPath{ { { nan, valid } } }; }));
        CHECK(throws_invalid_argument([&] { CameraPath{ { { 0.0, copy_valid_pose_and_apply_change([](lop::CameraPose& pose) { pose.look_direction = { 0.0, 0.0, 1e-10 }; }) } } }; }));
        CHECK(throws_invalid_argument([&] { CameraPath{ { { 0.0, copy_valid_pose_and_apply_change([](lop::CameraPose& pose) { pose.up = { 0.0, 0.0, 5.0 }; }) } } }; }));
        CHECK(throws_invalid_argument([&] { CameraPath{ { { 0.0, copy_valid_pose_and_apply_change([](lop::CameraPose& pose) { pose.vertical_field_of_view_degrees = 180.0; }) } } }; }));
        CHECK(throws_invalid_argument([&] { CameraPath{ std::vector<CameraPathSample>{} }; }));
    }
}

int main(int const argument_count, char** const arguments)
{
    std::string_view const selected = argument_count > 1 ? arguments[1] : "";
    auto const run = [selected](std::string_view const name, void (*test)())
    {
        if (selected.empty() || selected == name)
            test();
    };
    run("pose_at", test_pose_at);
    run("orientation", test_orientation);
    run("recorder", test_recorder);
    run("playback", test_playback);
    run("json", test_json);
    std::printf("%d failed checks\n", failure_count);
    return failure_count;
}
