"""Checks that the Python and the C++ code of the camera path read each other's .lopcamerapath files with the same samples, bit by bit.

Usage: python -I camera_path_round_trip.py <round_trip_helper executable>

The helper is built from cpp/camera_path.cpp and has two modes: "write <file>" writes, with the C++ code, the path that
generated_samples() generates here, and "read <file>" reads a file with the C++ code and prints the bits of each number. The exit
code is the number of failed checks.
"""
import os
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from camera_path import CameraPath, CameraPathPose, CameraPathSample, camera_path_from_json, to_camera_path_json  # noqa: E402

failure_count = 0
MASK_64_BITS = (1 << 64) - 1


def check(condition, description):
    global failure_count
    if not condition:
        failure_count += 1
        print(f"FAILED: {description}")
    else:
        print(f"ok: {description}")


def generated_samples():
    """Must match generated_samples() of round_trip_helper.cpp: three samples with extreme values and 1000 pseudo-random samples."""
    state = 12345

    def next_pseudorandom_fraction():
        nonlocal state
        state = (state * 6364136223846793005 + 1442695040888963407) & MASK_64_BITS
        return (state >> 11) * (1.0 / 9007199254740992.0)

    samples = [
        CameraPathSample(0.0, CameraPathPose((0.1, -0.0, 443210.123456789), (1.0 / 3.0, 2.0 / 3.0, -1e-300), (0.0, 1.0, 0.0), 45.000000000001)),
        CameraPathSample(0.016666666666666666, CameraPathPose((1e300, 4712345.12, 1234.57), (-0.1234, 0.9877, -0.0988),
                                                              (0.0123, -0.0988, 0.9951), 179.99999999999997)),
        CameraPathSample(12.3, CameraPathPose((5.0e-324, 1.7976931348623157e308, 3.0), (0.5, 0.5, 0.70710678118654757), (0.0, 1.0, 0.0),
                                              1e-300)),
    ]
    time_seconds = 12.3
    for _ in range(1000):
        time_seconds += 0.001 + next_pseudorandom_fraction()
        x = next_pseudorandom_fraction() * 1e6
        y = next_pseudorandom_fraction() * 1e7
        z = next_pseudorandom_fraction() * 3000.0
        lx = next_pseudorandom_fraction() - 0.5
        ly = next_pseudorandom_fraction() - 0.5
        lz = -1.0 - next_pseudorandom_fraction()
        vertical_field_of_view_degrees = 1.0 + next_pseudorandom_fraction() * 170.0
        samples.append(CameraPathSample(time_seconds, CameraPathPose((x, y, z), (lx, ly, lz), (0.0, 0.0, 1.0),
                                                                     vertical_field_of_view_degrees)))
    return samples


def hex_bits(value):
    return struct.pack(">d", value).hex()


def bit_lines(samples):
    """The lines that "read" of the helper prints for samples: 10 numbers, then the field of view."""
    lines = []
    for sample in samples:
        pose = sample.pose
        lines.append(" ".join(hex_bits(value) for value in (sample.time_seconds, *pose.position, *pose.look_direction, *pose.up)))
        lines.append(hex_bits(pose.vertical_field_of_view_degrees))
    return lines


def main():
    helper_executable_path = sys.argv[1]
    expected_samples = generated_samples()
    expected_lines = bit_lines(expected_samples)
    with tempfile.TemporaryDirectory() as temporary_directory_path:
        # C++ writes, Python reads.
        cpp_written_file_path = os.path.join(temporary_directory_path, "written-by-cpp.lopcamerapath")
        subprocess.run([helper_executable_path, "write", cpp_written_file_path], check=True)
        with open(cpp_written_file_path, "rb") as file:
            cpp_bytes = file.read()
        camera_path_read_from_cpp_file = camera_path_from_json(cpp_bytes)
        check(len(camera_path_read_from_cpp_file.samples) == len(expected_samples),
              f"Python reads {len(camera_path_read_from_cpp_file.samples)} samples written by C++")
        check(bit_lines(camera_path_read_from_cpp_file.samples) == expected_lines,
              "the samples that Python reads from the C++ file match expected_samples bit for bit")

        # Python writes, C++ reads.
        python_written_file_path = os.path.join(temporary_directory_path, "written-by-python.lopcamerapath")
        python_text = to_camera_path_json(CameraPath(expected_samples))
        with open(python_written_file_path, "wb") as file:
            file.write(python_text.encode("utf-8"))
        result = subprocess.run([helper_executable_path, "read", python_written_file_path], check=True, capture_output=True, text=True)
        check(result.stdout.split() == " ".join(expected_lines).split(),
              "the samples that C++ reads from the Python file match expected_samples bit for bit")

        # The C++ code reads the file that Python writes after reading the file of C++.
        python_rewritten_file_path = os.path.join(temporary_directory_path, "rewritten-by-python.lopcamerapath")
        with open(python_rewritten_file_path, "wb") as file:
            file.write(to_camera_path_json(camera_path_read_from_cpp_file).encode("utf-8"))
        result = subprocess.run([helper_executable_path, "read", python_rewritten_file_path], check=True, capture_output=True, text=True)
        check(result.stdout.split() == " ".join(expected_lines).split(),
              "the samples that C++ reads from the file rewritten by Python match expected_samples bit for bit")

        # Information only: the texts of both files.
        print(f"info: the C++ file has {len(cpp_bytes)} bytes and the Python file {len(python_text.encode('utf-8'))} bytes; "
              f"the texts are {'equal' if cpp_bytes.decode('utf-8') == python_text else 'different'}")
    print(f"{failure_count} failed checks")
    return failure_count


if __name__ == "__main__":
    sys.exit(main())
