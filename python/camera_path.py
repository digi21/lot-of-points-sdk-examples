"""camera_path.py - the camera path model of the Record camera path and Play camera path commands: samples, validation,
interpolation, recording, playback and the .lopcamerapath format. It is cpp/camera_path.cpp written in Python and does not
import lop, so it runs and is tested outside Lot Of Points; the commands are in camera_path_commands.py.
"""
import bisect
import json
import math
from typing import NamedTuple

FORMAT_VERSION = 1
MINIMUM_VECTOR_LENGTH = 1e-9
# The minimum time between two regular samples of a recording, in seconds; the final sample may be closer to the preceding one.
MINIMUM_SAMPLE_INTERVAL_SECONDS = 1.0 / 60.0
# Speed multipliers available in a playback; a playback starts at 1.
SPEED_MULTIPLIERS = (0.25, 0.5, 1.0, 2.0, 4.0, 8.0)


class Quaternion(NamedTuple):
    """Unit quaternion w + x i + y j + z k."""
    w: float = 1.0
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0


class CameraPathPose(NamedTuple):
    """The camera pose of a sample, in world coordinates: position, look_direction and up are tuples of three floats and
    vertical_field_of_view_degrees is in degrees (the fields of lop.CameraPose)."""
    position: tuple
    look_direction: tuple
    up: tuple
    vertical_field_of_view_degrees: float


class CameraPathSample(NamedTuple):
    time_seconds: float     # Seconds from the start of the recording.
    pose: CameraPathPose


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _length(vector):
    return math.hypot(vector[0], vector[1], vector[2])


def _scaled(vector, factor):
    return (vector[0] * factor, vector[1] * factor, vector[2] * factor)


def _is_finite(values):
    return all(math.isfinite(value) for value in values)


def _normalized(q):
    norm = math.sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3])
    return Quaternion(q[0] / norm, q[1] / norm, q[2] / norm, q[3] / norm)


def _slerp(a, b, fraction):
    """Uses normalized linear interpolation when the absolute quaternion dot product is at least 0.9995; otherwise, uses spherical
    linear interpolation along the shortest arc."""
    cosine = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z
    if cosine < 0.0:
        b = Quaternion(-b.w, -b.x, -b.y, -b.z)
        cosine = -cosine
    weight_a = 1.0 - fraction
    weight_b = fraction
    if cosine < 0.9995:
        angle = math.acos(cosine)
        sine = math.sin(angle)
        weight_a = math.sin((1.0 - fraction) * angle) / sine
        weight_b = math.sin(fraction * angle) / sine
    return _normalized((weight_a * a.w + weight_b * b.w, weight_a * a.x + weight_b * b.x, weight_a * a.y + weight_b * b.y,
                        weight_a * a.z + weight_b * b.z))


def quaternion_from_look_direction_and_up(look_direction, up):
    """Returns the rotation whose -Z axis points along look_direction and whose +Y axis points along the projection of up perpendicular to look_direction.
    Requires a nonzero look_direction and an up that is not parallel to it."""
    back = _scaled(look_direction, -1.0 / _length(look_direction))
    right = _cross(up, back)
    right = _scaled(right, 1.0 / _length(right))
    orthogonal_up = _cross(back, right)

    # Rotation matrix with columns right, orthogonal_up and back; conversion to a quaternion with Shepperd's method.
    m00, m01, m02 = right[0], orthogonal_up[0], back[0]
    m10, m11, m12 = right[1], orthogonal_up[1], back[1]
    m20, m21, m22 = right[2], orthogonal_up[2], back[2]
    trace = m00 + m11 + m22
    if trace > 0.0:
        s = math.sqrt(trace + 1.0) * 2.0
        return _normalized((s / 4.0, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s))
    if m00 > m11 and m00 > m22:
        s = math.sqrt(1.0 + m00 - m11 - m22) * 2.0
        return _normalized(((m21 - m12) / s, s / 4.0, (m01 + m10) / s, (m02 + m20) / s))
    if m11 > m22:
        s = math.sqrt(1.0 + m11 - m00 - m22) * 2.0
        return _normalized(((m02 - m20) / s, (m01 + m10) / s, s / 4.0, (m12 + m21) / s))
    s = math.sqrt(1.0 + m22 - m00 - m11) * 2.0
    return _normalized(((m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, s / 4.0))


def rotated(rotation, vector):
    """Returns vector rotated by the unit quaternion rotation: v + 2w (u x v) + 2 u x (u x v), with u the vector part."""
    u = (rotation.x, rotation.y, rotation.z)
    u_cross_v = _cross(u, vector)
    u_cross_u_cross_v = _cross(u, u_cross_v)
    return tuple(vector[axis_index] + 2.0 * (rotation.w * u_cross_v[axis_index] + u_cross_u_cross_v[axis_index]) for axis_index in range(3))


def _same_pose(a, b):
    return (tuple(a.position) == tuple(b.position) and tuple(a.look_direction) == tuple(b.look_direction) and tuple(a.up) == tuple(b.up)
            and a.vertical_field_of_view_degrees == b.vertical_field_of_view_degrees)


def _validate_sample(sample, sample_index):
    """Raises ValueError for a sample that a camera path does not accept, without the checks that involve other samples."""
    def fail(reason):
        raise ValueError(f"sample {sample_index}: {reason}")

    pose = sample.pose
    if (not math.isfinite(sample.time_seconds) or not _is_finite(pose.position) or not _is_finite(pose.look_direction)
            or not _is_finite(pose.up) or not math.isfinite(pose.vertical_field_of_view_degrees)):
        fail("a number is not finite")
    if not 0.0 < pose.vertical_field_of_view_degrees < 180.0:
        fail("VerticalFieldOfViewDegrees is not greater than 0 and less than 180")
    look_length = _length(pose.look_direction)
    up_length = _length(pose.up)
    if not look_length > MINIMUM_VECTOR_LENGTH:
        fail("LookDirection must have a length greater than 1e-9")
    if not up_length > MINIMUM_VECTOR_LENGTH:
        fail("Up must have a length greater than 1e-9")
    if _length(_cross(_scaled(pose.look_direction, 1.0 / look_length), _scaled(pose.up, 1.0 / up_length))) < MINIMUM_VECTOR_LENGTH:
        fail("The cross product of normalized LookDirection and Up must have a length of at least 1e-9")


class CameraPath:
    """Samples ordered by strictly increasing time_seconds; the first one has time_seconds 0."""

    def __init__(self, samples):
        """Raises ValueError if samples is empty, the first time is not 0, the times do not strictly increase, a number is not
        finite, look_direction or up has length at most 1e-9, the cross product of their normalized vectors has length below 1e-9, or a
        field of view is outside (0, 180) degrees."""
        self._samples = [CameraPathSample(float(sample.time_seconds), sample.pose) for sample in samples]
        if not self._samples:
            raise ValueError("the path has no samples")
        for sample_index, sample in enumerate(self._samples):
            _validate_sample(sample, sample_index)
            if sample_index == 0 and sample.time_seconds != 0.0:
                raise ValueError("sample 0: TimeSeconds is not 0")
            if sample_index > 0 and not sample.time_seconds > self._samples[sample_index - 1].time_seconds:
                raise ValueError(f"sample {sample_index}: TimeSeconds does not increase")
        self._times = [sample.time_seconds for sample in self._samples]
        # One orientation per sample, from quaternion_from_look_direction_and_up.
        self._orientations = [quaternion_from_look_direction_and_up(sample.pose.look_direction, sample.pose.up)
                              for sample in self._samples]

    @property
    def samples(self):
        return self._samples

    @property
    def duration_seconds(self):
        return self._times[-1]

    def pose_at(self, time_seconds):
        """Returns the pose at time_seconds: position and field of view by linear interpolation and orientation by spherical linear
        interpolation (slerp) between the two samples around time_seconds; the first sample's pose for negative time_seconds and the last sample's pose for time_seconds greater than or equal to duration_seconds."""
        next_sample_index = bisect.bisect_right(self._times, time_seconds)      # The first sample whose time is greater than time_seconds.
        if next_sample_index == 0:
            return self._samples[0].pose
        if next_sample_index == len(self._samples):
            return self._samples[-1].pose
        before = self._samples[next_sample_index - 1]
        if before.time_seconds == time_seconds:
            return before.pose
        after = self._samples[next_sample_index]
        fraction = (time_seconds - before.time_seconds) / (after.time_seconds - before.time_seconds)
        orientation = _slerp(self._orientations[next_sample_index - 1], self._orientations[next_sample_index], fraction)

        def lerp(start, end):
            return start + (end - start) * fraction

        return CameraPathPose(
            tuple(lerp(before.pose.position[axis_index], after.pose.position[axis_index]) for axis_index in range(3)),
            rotated(orientation, (0.0, 0.0, -1.0)),
            rotated(orientation, (0.0, 1.0, 0.0)),
            lerp(before.pose.vertical_field_of_view_degrees, after.pose.vertical_field_of_view_degrees))


class CameraPathRecorder:
    """Collects the samples of a recording: discards the samples closer than MINIMUM_SAMPLE_INTERVAL_SECONDS to the previous kept
    sample and, of each run of consecutive samples with the same pose, keeps the first and the last."""

    def __init__(self):
        self._samples = []
        self._start_clock_seconds = None

    @property
    def sample_count(self):
        return len(self._samples)

    @property
    def recorded_seconds(self):
        """Seconds between the first and the last kept sample; 0 without samples."""
        return self._samples[-1].time_seconds if self._samples else 0.0

    def record_pose(self, clock_seconds, pose):
        """clock_seconds is the monotonic clock in seconds (ViewFrame.frame_time_seconds); the first call sets the origin of the times."""
        if self._start_clock_seconds is None:
            self._start_clock_seconds = clock_seconds
        sample = CameraPathSample(clock_seconds - self._start_clock_seconds, pose)

        if not self._samples:
            self._samples.append(sample)
            return
        if sample.time_seconds - self._samples[-1].time_seconds < MINIMUM_SAMPLE_INTERVAL_SECONDS:
            return

        # With the last two samples equal to the new pose, the last one is replaced: the run keeps its first sample and the most recent one.
        if len(self._samples) >= 2 and _same_pose(self._samples[-1].pose, pose) and _same_pose(self._samples[-2].pose, pose):
            self._samples[-1] = sample
            return
        self._samples.append(sample)

    def to_camera_path(self, clock_seconds, final_pose):
        """Builds a CameraPath from a copy of the recorded samples, adding final_pose at clock_seconds without enforcing the minimum
        interval; returns None if no samples were recorded. If the last kept sample is not older than clock_seconds, final_pose replaces
        its pose."""
        if not self._samples:
            return None
        samples = list(self._samples)
        final_sample = CameraPathSample(clock_seconds - self._start_clock_seconds, final_pose)
        if final_sample.time_seconds <= samples[-1].time_seconds:
            samples[-1] = CameraPathSample(samples[-1].time_seconds, final_pose)
        elif len(samples) >= 2 and _same_pose(samples[-1].pose, final_pose) and _same_pose(samples[-2].pose, final_pose):
            samples[-1] = final_sample
        else:
            samples.append(final_sample)
        return CameraPath(samples)


class CameraPathPlayback:
    """State of a playback: time of the path, speed, pause and loop."""

    def __init__(self, path):
        self._path = path
        self._path_time_seconds = 0.0
        self._speed_multiplier_index = 2
        self.paused = False
        self.loops = False      # If True, a time past duration_seconds continues from 0 plus the excess.

    @property
    def path(self):
        return self._path

    @property
    def path_time_seconds(self):
        """Path time in seconds; may exceed path.duration_seconds when advance returns None."""
        return self._path_time_seconds

    @property
    def speed_multiplier(self):
        return SPEED_MULTIPLIERS[self._speed_multiplier_index]

    def increase_speed(self):
        if self._speed_multiplier_index + 1 < len(SPEED_MULTIPLIERS):
            self._speed_multiplier_index += 1

    def decrease_speed(self):
        if self._speed_multiplier_index > 0:
            self._speed_multiplier_index -= 1

    def advance(self, real_elapsed_seconds):
        """Unless paused, adds max(real_elapsed_seconds, 0.0) times speed_multiplier to the path time. Returns the pose of the path time, or None
        if the time passes duration_seconds with loops False or if duration_seconds is 0."""
        duration = self._path.duration_seconds
        if not duration > 0.0:
            return None
        if not self.paused:
            self._path_time_seconds += max(real_elapsed_seconds, 0.0) * self.speed_multiplier
        if self._path_time_seconds > duration:
            if not self.loops:
                return None
            self._path_time_seconds = math.fmod(self._path_time_seconds, duration)
        return self._path.pose_at(self._path_time_seconds)


def to_camera_path_json(path):
    """Returns the .lopcamerapath text (JSON, version 1) of path; each float uses the shortest representation that reads back as the
    same value."""
    samples = [{"TimeSeconds": sample.time_seconds, "Position": list(sample.pose.position),
                "LookDirection": list(sample.pose.look_direction), "Up": list(sample.pose.up),
                "VerticalFieldOfViewDegrees": sample.pose.vertical_field_of_view_degrees} for sample in path.samples]
    return json.dumps({"Version": FORMAT_VERSION, "Samples": samples}, separators=(",", ":"), allow_nan=False)


def _is_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _member(item, key, sample_index):
    if key not in item:
        raise ValueError(f"sample {sample_index}: {key} is missing")
    return item[key]


def _read_number(item, key, sample_index):
    value = _member(item, key, sample_index)
    if not _is_number(value):
        raise ValueError(f"sample {sample_index}: {key} is not a number")
    try:
        return float(value)
    except OverflowError:
        raise ValueError(f"sample {sample_index}: {key} is out of range") from None


def _read_vector(item, key, sample_index):
    value = _member(item, key, sample_index)
    if not isinstance(value, list) or len(value) != 3 or not all(_is_number(number) for number in value):
        raise ValueError(f"sample {sample_index}: {key} is not an array of 3 numbers")
    try:
        return tuple(float(number) for number in value)
    except OverflowError:
        raise ValueError(f"sample {sample_index}: {key} is out of range") from None


def _reject_constant(name):
    raise ValueError(f"{name} is not valid JSON")


def camera_path_from_json(text):
    """Returns the CameraPath of text (str or UTF-8 bytes). Raises ValueError with the reason for a text that does not follow the
    .lopcamerapath format (version 1)."""
    if isinstance(text, (bytes, bytearray)):
        try:
            text = bytes(text).decode("utf-8")
        except UnicodeDecodeError as exception:
            raise ValueError(f"the text is not valid UTF-8: {exception}") from None
    # Editors can add a UTF-8 byte order mark that JSON does not admit.
    text = text.removeprefix("﻿")
    try:
        root = json.loads(text, parse_constant=_reject_constant)
    except (ValueError, RecursionError) as exception:
        raise ValueError(f"the text is not valid JSON: {exception}") from None
    if not isinstance(root, dict):
        raise ValueError("the JSON root is not an object")

    version = root.get("Version")
    if not isinstance(version, int) or isinstance(version, bool):
        raise ValueError("Version is missing or is not an integer")
    if version != FORMAT_VERSION:
        raise ValueError(f"Version {version} is not supported; this program reads version {FORMAT_VERSION}")

    samples_json = root.get("Samples")
    if not isinstance(samples_json, list):
        raise ValueError("Samples is missing or is not an array")

    samples = []
    for sample_index, item in enumerate(samples_json):
        if not isinstance(item, dict):
            raise ValueError(f"sample {sample_index}: it is not an object")
        samples.append(CameraPathSample(
            _read_number(item, "TimeSeconds", sample_index),
            CameraPathPose(_read_vector(item, "Position", sample_index), _read_vector(item, "LookDirection", sample_index),
                           _read_vector(item, "Up", sample_index), _read_number(item, "VerticalFieldOfViewDegrees", sample_index))))
    return CameraPath(samples)
