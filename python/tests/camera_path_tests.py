"""Tests of the camera path model (python/camera_path.py), the Python version of cpp/tests/camera_path_tests.cpp; the exit code is the
number of failed checks. They do not need Lot Of Points. Usage: python -I camera_path_tests.py [pose_at|orientation|recorder|playback|json]
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from camera_path import (CameraPath, CameraPathPlayback, CameraPathPose, CameraPathRecorder, CameraPathSample,  # noqa: E402
                         camera_path_from_json, quaternion_from_look_direction_and_up, rotated, to_camera_path_json)

failure_count = 0


def check(condition, description):
    global failure_count
    if not condition:
        failure_count += 1
        line = sys._getframe(1).f_lineno
        print(f"FAILED line {line}: {description}")


def throws_value_error(function):
    try:
        function()
    except ValueError:
        return True
    return False


def look_direction_from(pitch_degrees, yaw_degrees):
    pitch = math.radians(pitch_degrees)
    yaw = math.radians(yaw_degrees)
    return (math.cos(pitch) * math.sin(yaw), math.sin(pitch), -math.cos(pitch) * math.cos(yaw))


def up_from(pitch_degrees, yaw_degrees):
    pitch = math.radians(pitch_degrees)
    yaw = math.radians(yaw_degrees)
    return (-math.sin(pitch) * math.sin(yaw), math.cos(pitch), math.sin(pitch) * math.cos(yaw))


def pose_from(x, pitch_degrees, yaw_degrees, vertical_field_of_view_degrees):
    return CameraPathPose((x, 2.0 * x, 3.0), look_direction_from(pitch_degrees, yaw_degrees), up_from(pitch_degrees, yaw_degrees),
                          vertical_field_of_view_degrees)


def distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2])


def bits(value):
    return struct.pack("<d", value)


def same_bits(a, b):
    """Compares two CameraPathPose, or two floats, bit by bit."""
    if isinstance(a, CameraPathPose):
        return (all(bits(u) == bits(v) for u, v in zip(a.position, b.position))
                and all(bits(u) == bits(v) for u, v in zip(a.look_direction, b.look_direction))
                and all(bits(u) == bits(v) for u, v in zip(a.up, b.up))
                and bits(a.vertical_field_of_view_degrees) == bits(b.vertical_field_of_view_degrees))
    return bits(a) == bits(b)


def sample_path():
    return CameraPath([CameraPathSample(0.0, pose_from(10.0, 0.0, 0.0, 40.0)), CameraPathSample(1.0, pose_from(20.0, 30.0, 90.0, 50.0)),
                       CameraPathSample(2.5, pose_from(-5.0, -20.0, 200.0, 60.0))])


def test_pose_at():
    """1. pose_at returns each sample's exact pose at that sample's time_seconds and the mean of positions and field of view at the midpoint."""
    path = sample_path()
    for sample in path.samples:
        check(same_bits(path.pose_at(sample.time_seconds), sample.pose), "pose_at at the time of a sample")

    for a, b in zip(path.samples, path.samples[1:]):
        middle = path.pose_at((a.time_seconds + b.time_seconds) / 2.0)
        mean_position = tuple((a.pose.position[axis_index] + b.pose.position[axis_index]) / 2.0 for axis_index in range(3))
        check(distance(middle.position, mean_position) < 1e-12, "the midpoint position is the mean")
        check(abs(middle.vertical_field_of_view_degrees
                  - (a.pose.vertical_field_of_view_degrees + b.pose.vertical_field_of_view_degrees) / 2.0) < 1e-12,
              "the midpoint field of view is the mean")
        check(abs(math.hypot(*middle.look_direction) - 1.0) < 1e-12, "the midpoint look direction is a unit vector")
    check(same_bits(path.pose_at(-1.0), path.samples[0].pose), "a negative time gives the first sample")
    check(same_bits(path.pose_at(100.0), path.samples[-1].pose), "a time past the duration gives the last sample")
    check(path.duration_seconds == 2.5, "duration_seconds")


def test_orientation():
    """2. The quaternion of a look direction and an up reproduces both vectors."""
    maximum_distance = 0.0
    for pitch_degrees in range(0, 181, 15):
        for yaw_degrees in range(-720, 721, 15):
            look = look_direction_from(pitch_degrees, yaw_degrees)
            up = up_from(pitch_degrees, yaw_degrees)
            orientation = quaternion_from_look_direction_and_up(look, up)
            maximum_distance = max(maximum_distance, distance(rotated(orientation, (0.0, 0.0, -1.0)), look),
                                   distance(rotated(orientation, (0.0, 1.0, 0.0)), up))
    check(maximum_distance < 1e-12, "maximum distance between expected and rotated unit vectors below 1e-12")
    print(f"maximum distance between expected and rotated unit vectors (dimensionless): {maximum_distance:g}")

    identity = quaternion_from_look_direction_and_up((0.0, 0.0, -1.0), (0.0, 1.0, 0.0))
    check(identity.w == 1.0 and identity.x == 0.0 and identity.y == 0.0 and identity.z == 0.0, "identity quaternion")


def test_recorder():
    """3. The recorder tests check compression of identical consecutive poses, at most 61 stored samples for frames spanning one second
    at 144 Hz, and preservation of the final pose."""
    pose_a = pose_from(1.0, 0.0, 0.0, 45.0)
    pose_b = pose_from(2.0, 10.0, 20.0, 45.0)

    identical_pose_recorder = CameraPathRecorder()
    for frame in range(10):
        identical_pose_recorder.record_pose(100.0 + 0.1 * frame, pose_a)
    check(identical_pose_recorder.sample_count == 2, "10 equal poses leave 2 samples")
    equal_path = identical_pose_recorder.to_camera_path(100.0 + 0.1 * 9, pose_a)
    check(equal_path is not None and len(equal_path.samples) == 2, "the path of equal poses has 2 samples")
    check(equal_path is not None and equal_path.samples[0].time_seconds == 0.0, "the first time is 0")
    check(equal_path is not None and abs(equal_path.samples[-1].time_seconds - 0.9) < 1e-12, "the last time is 0.9")

    # Checks that the pose at 1.5 s matches pose_a after recording repeated pose_a samples from 0 to 3 s.
    pause = CameraPathRecorder()
    pause.record_pose(0.0, pose_a)
    for frame in range(1, 181):
        pause.record_pose(frame / 60.0, pose_a)
    pause.record_pose(3.1, pose_b)
    pause_path = pause.to_camera_path(3.1, pose_b)
    check(pause_path is not None and len(pause_path.samples) == 3, "the pause leaves 3 samples")
    if pause_path is not None:
        middle = pause_path.pose_at(1.5)
        check(distance(middle.position, pose_a.position) < 1e-12, "the position stays in the pause")
        check(distance(middle.look_direction, pose_a.look_direction) < 1e-12, "the look direction stays in the pause")
        check(distance(middle.up, pose_a.up) < 1e-12, "the up stays in the pause")

    # 144 Hz frames with a different pose in each frame.
    recorder_at_144_frames_per_second = CameraPathRecorder()
    for frame in range(145):
        recorder_at_144_frames_per_second.record_pose(frame / 144.0, pose_from(float(frame), 0.0, 0.0, 45.0))
    check(recorder_at_144_frames_per_second.sample_count <= 61, "at most 61 samples for frames spanning one second")
    check(recorder_at_144_frames_per_second.sample_count > 1, "more than one sample")

    # The final pose is kept although it is less than 1/60 s after the last kept sample.
    short_final_interval_recorder = CameraPathRecorder()
    short_final_interval_recorder.record_pose(100.0, pose_a)
    short_final_interval_recorder.record_pose(100.5, pose_a)
    short_final_interval_recorder.record_pose(101.0, pose_b)
    final_pose = pose_from(3.0, 0.0, 45.0, 45.0)
    short_final_interval_path = short_final_interval_recorder.to_camera_path(101.005, final_pose)
    check(short_final_interval_path is not None and len(short_final_interval_path.samples) == 4, "the close final pose adds a sample")
    check(short_final_interval_path is not None and same_bits(short_final_interval_path.samples[-1].pose, final_pose),
          "the last sample is the final pose")
    check(short_final_interval_path is not None and abs(short_final_interval_path.samples[-1].time_seconds - 1.005) < 1e-9,
          "the last time is 1.005")

    check(CameraPathRecorder().to_camera_path(1.0, pose_a) is None, "no samples, no path")


def test_playback():
    """4. advance multiplies the elapsed time by speed_multiplier, preserves path_time_seconds while paused, wraps path_time_seconds past the
    duration when loops is True, and returns None past the duration when loops is False or when the duration is 0 s."""
    path = CameraPath([CameraPathSample(0.0, pose_from(0.0, 0.0, 0.0, 45.0)), CameraPathSample(2.0, pose_from(2.0, 0.0, 0.0, 45.0))])

    playback = CameraPathPlayback(path)
    check(playback.speed_multiplier == 1.0, "the initial speed is 1")
    playback.increase_speed()
    check(playback.speed_multiplier == 2.0, "the speed is 2")
    pose = playback.advance(0.25)
    check(pose is not None and abs(playback.path_time_seconds - 0.5) < 1e-12, "multiplier 2 advances twice")
    check(pose is not None and abs(pose.position[0] - 0.5) < 1e-12, "the pose of the path time")

    playback.paused = True
    pose = playback.advance(1.0)
    check(pose is not None and abs(playback.path_time_seconds - 0.5) < 1e-12, "a pause does not advance")
    playback.paused = False

    pose = playback.advance(0.75)       # 0.5 + 1.5 = 2.0: the end is not past the duration.
    check(pose is not None and abs(playback.path_time_seconds - 2.0) < 1e-12, "the end is not past the duration")
    check(playback.advance(0.01) is None, "past the duration without loops")

    looping = CameraPathPlayback(path)
    looping.loops = True
    check(looping.advance(1.5) is not None, "loop, first advance")
    pose = looping.advance(1.0)         # 2.5: continues from 0 plus 0.5.
    check(pose is not None and abs(looping.path_time_seconds - 0.5) < 1e-12, "loop continues from 0 plus the excess")

    for _ in range(10):
        looping.increase_speed()
    check(looping.speed_multiplier == 8.0, "the maximum speed is 8")
    for _ in range(10):
        looping.decrease_speed()
    check(looping.speed_multiplier == 0.25, "the minimum speed is 0.25")

    single_sample_playback = CameraPathPlayback(CameraPath([CameraPathSample(0.0, pose_from(0.0, 0.0, 0.0, 45.0))]))
    check(single_sample_playback.advance(1.0) is None, "a duration of 0 ends at once")
    single_sample_playback.loops = True
    check(single_sample_playback.advance(1.0) is None, "a duration of 0 ends at once with loops")


def json_with(version, samples):
    return f'{{"Version":{version},"Samples":{samples}}}'


def json_sample(time_seconds_json, vertical_field_of_view_degrees_json="45.0"):
    return (f'{{"TimeSeconds":{time_seconds_json},"Position":[1,2,3],"LookDirection":[0,0,-1],"Up":[0,1,0],'
            f'"VerticalFieldOfViewDegrees":{vertical_field_of_view_degrees_json}}}')


def test_json():
    """5. camera_path_from_json(to_camera_path_json(path)) preserves every sample number bit for bit; camera_path_from_json raises
    ValueError for the invalid texts tested below, and the CameraPath constructor raises ValueError for the invalid values tested after them."""
    path = CameraPath([
        CameraPathSample(0.0, CameraPathPose((0.1, -0.0, 443210.123456789), (1.0 / 3.0, 2.0 / 3.0, -1e-300), (0.0, 1.0, 0.0), 45.000000000001)),
        CameraPathSample(0.016666666666666666, CameraPathPose((1e300, 4712345.12, 1234.57), (-0.1234, 0.9877, -0.0988),
                                                              (0.0123, -0.0988, 0.9951), 179.99999999999997)),
        CameraPathSample(12.3, CameraPathPose((5.0e-324, 1.7976931348623157e308, 3.0), (0.5, 0.5, 0.70710678118654757), (0.0, 1.0, 0.0),
                                              1e-300))])
    text = to_camera_path_json(path)
    parsed_path = camera_path_from_json(text)
    check(len(parsed_path.samples) == len(path.samples), "the same number of samples")
    for original, copy in zip(path.samples, parsed_path.samples):
        check(same_bits(copy.time_seconds, original.time_seconds), "the same time, bit by bit")
        check(same_bits(copy.pose, original.pose), "the same pose, bit by bit")
    check(to_camera_path_json(parsed_path) == text, "the text of the copy is the same text")

    # A text with a byte order mark and spaces is accepted, as str and as bytes.
    check(len(camera_path_from_json("﻿ " + json_with("1", "[ " + json_sample("0") + " ]")).samples) == 1, "BOM in a str")
    check(len(camera_path_from_json(b"\xef\xbb\xbf " + json_with("1", "[ " + json_sample("0") + " ]").encode("utf-8")).samples) == 1,
          "BOM in bytes")

    def invalid(text_to_read):
        return throws_value_error(lambda: camera_path_from_json(text_to_read))

    check(invalid(json_with("2", "[" + json_sample("0") + "]")), "Version 2")
    check(invalid(json_with("1", "[]")), "an empty list")
    check(invalid(json_with("1", "[" + json_sample("0") + "," + json_sample("0") + "]")), "a repeated time")
    check(invalid(json_with("1", "[" + json_sample("1") + "]")), "the first time is not 0")
    check(invalid(json_with("1", "[" + json_sample("0") + "," + json_sample("null") + "]")), "a null time")
    check(invalid(json_with("1", "[" + json_sample("NaN") + "]")), "NaN")
    check(invalid(json_with("1", "[" + json_sample("0", "180") + "]")), "a field of view of 180")
    check(invalid(json_with("1", "[" + json_sample("0", "0") + "]")), "a field of view of 0")
    check(invalid(json_with("1.0", "[" + json_sample("0") + "]")), "Version 1.0 is not an integer")
    check(invalid(json_with("true", "[" + json_sample("0") + "]")), "Version true")
    check(invalid(json_with("1", "[" + json_sample("0", "1e999") + "]")), "a number out of range")
    check(invalid(json_with("1", "[" + json_sample("0", "true") + "]")), "a boolean is not a number")
    check(invalid("not json"), "not json")
    check(invalid("[]"), "the root is an array")
    check(invalid('{"Version":1}'), "no Samples")
    check(invalid(json_with("1", '[{"TimeSeconds":0}]')), "a sample without members")
    check(invalid(b"\xff\xfe"), "bytes that are not UTF-8")

    # The constructor rejects the values that JSON cannot carry and the vectors that lop rejects.
    nan = float("nan")
    valid = CameraPathPose((0.0, 0.0, 0.0), (0.0, 0.0, -1.0), (0.0, 1.0, 0.0), 45.0)
    check(throws_value_error(lambda: CameraPath([CameraPathSample(0.0, valid._replace(position=(nan, 0.0, 0.0)))])), "NaN position")
    check(throws_value_error(lambda: CameraPath([CameraPathSample(nan, valid)])), "NaN time")
    check(throws_value_error(lambda: CameraPath([CameraPathSample(0.0, valid._replace(look_direction=(0.0, 0.0, 1e-10)))])),
          "a short look direction")
    check(throws_value_error(lambda: CameraPath([CameraPathSample(0.0, valid._replace(up=(0.0, 0.0, 5.0)))])), "up parallel to look")
    check(throws_value_error(lambda: CameraPath([CameraPathSample(0.0, valid._replace(vertical_field_of_view_degrees=180.0))])), "FOV 180")
    check(throws_value_error(lambda: CameraPath([])), "an empty path")


def main():
    selected = sys.argv[1] if len(sys.argv) > 1 else ""
    for name, test in (("pose_at", test_pose_at), ("orientation", test_orientation), ("recorder", test_recorder),
                       ("playback", test_playback), ("json", test_json)):
        if not selected or selected == name:
            test()
    print(f"{failure_count} failed checks")
    return failure_count


if __name__ == "__main__":
    sys.exit(main())
