"""camera_path_commands.py - the Record camera path and Play camera path commands in Python. Recording samples camera poses,
enforces a minimum interval between regular samples and retains only the first and last sample of consecutive identical poses; the
playback assigns the pose of the path in each frame. They are cpp/camera_path_commands.cpp written in Python; copy
this file, camera_path.py, camera_path_texts.py and the two icons, record_camera_path.png and play_camera_path.png, to
%AppData%\\lop\\commands.
"""
import datetime
import os

import lop

import camera_path
from camera_path import CameraPathPlayback, CameraPathPose, CameraPathRecorder
from camera_path_texts import texts_for

TEXTS = texts_for(lop.language)

CAMERA_PATH_EXTENSION = ".lopcamerapath"
# Longest real interval that one frame advances the playback, in seconds; rendering stops while the window is minimized.
MAXIMUM_PLAYBACK_STEP_SECONDS = 0.1
BANNER_REFRESH_INTERVAL_SECONDS = 0.25

# The last camera path recorded in this process. Reloading the scripts after the enabled extensions change clears _last_recorded_camera_path.
_last_recorded_camera_path = None


def pose_from_lop(pose):
    """Returns the CameraPathPose of a lop.CameraPose."""
    return CameraPathPose(tuple(pose.position), tuple(pose.look_direction), tuple(pose.up), pose.vertical_field_of_view_degrees)


def pose_to_lop(pose):
    """Returns the lop.CameraPose of a CameraPathPose."""
    return lop.CameraPose(pose.position, pose.look_direction, pose.up, pose.vertical_field_of_view_degrees)


def format_duration_minutes_seconds(seconds):
    """Returns "mm:ss" for a time in seconds, truncated; the minutes have as many digits as needed."""
    whole_seconds = int(max(seconds, 0.0))
    return f"{whole_seconds // 60:02}:{whole_seconds % 60:02}"


def local_timestamp():
    """Returns the local time as YYYYMMDD-HHMMSS."""
    return datetime.datetime.now().strftime("%Y%m%d-%H%M%S")


def suggested_file_name():
    """Returns camera-path-YYYYMMDD-HHMMSS.lopcamerapath, local time."""
    return f"camera-path-{local_timestamp()}{CAMERA_PATH_EXTENSION}"


@lop.command("lop.example.python.record_camera_path", TEXTS["record_name"] + " (Python)",
             existing_tab_id="lop.tab.view",
             ribbon_section="Ejemplos en Python" if lop.language == "es" else "Python examples",
             description=TEXTS["record_description"],
             icon="record_camera_path.png",
             requirements=lop.REQUIRES_OPEN_CLOUD)
class RecordCameraPathCommand(lop.Command):
    def __init__(self):
        self._recorder = CameraPathRecorder()
        self._last_frame = None
        self._recorded_path = None
        self._automatic_save_directory_path = None
        self._last_banner_time_seconds = 0.0
        self._stopped = False
        self._written = False

    def on_start(self):
        # Stores the extension data directory path during on_start for the automatic save in on_finish.
        self._automatic_save_directory_path = self.host.get_extension_data_directory_path()
        self.host.set_kept_below_new_commands(True)
        self.host.set_view_frame_notices(lop.VIEW_FRAME_NOTICES_EVERY_FRAME)
        self._show_banner()

    def on_view_frame(self, frame):
        if self._stopped:
            return
        self._recorder.record_pose(frame.frame_time_seconds, pose_from_lop(frame.camera_pose))
        self._last_frame = frame
        if frame.frame_time_seconds - self._last_banner_time_seconds >= BANNER_REFRESH_INTERVAL_SECONDS:
            self._last_banner_time_seconds = frame.frame_time_seconds
            self._show_banner()

    async def on_banner_action(self, action_name):
        if action_name != "stop" or self._stopped:
            return
        self._stop_recording()
        if self._recorded_path is None:
            self.finish()
            return
        file_path = None
        try:
            file_path = await self.host.ask_save_file_path(suggested_file_name(), TEXTS["camera_path_file_type"], CAMERA_PATH_EXTENSION)
        except Exception:
            pass    # The picker failed: on_finish writes the recording in the data folder of the extension.
        if file_path:
            self._write_camera_path_file(file_path)
        self.finish()

    def on_finish(self):
        # Esc, the ribbon button and the closing of the document finish the command without the save file picker. If writing the
        # recording to the selected file fails, on_finish writes the recording to a file in the extension data directory.
        if not self._stopped:
            self._stop_recording()
        if self._recorded_path is not None and not self._written:
            self._write_camera_path_file(self._automatic_file_path())

    def _show_banner(self):
        message = TEXTS["recording_banner"].format(format_duration_minutes_seconds(self._recorder.recorded_seconds), self._recorder.sample_count)
        self.host.show_banner(message, None, [lop.BannerButton(TEXTS["stop_recording"], "stop")])

    def _stop_recording(self):
        """Stops recording samples. If a frame was recorded, creates a CameraPath and assigns it to _recorded_path and
        _last_recorded_camera_path."""
        global _last_recorded_camera_path
        self._stopped = True
        if self._last_frame is not None:
            self._recorded_path = self._recorder.to_camera_path(self._last_frame.frame_time_seconds,
                                                                pose_from_lop(self._last_frame.camera_pose))
        if self._recorded_path is not None:
            _last_recorded_camera_path = self._recorded_path

    def _write_camera_path_file(self, file_path):
        """Writes the path to file_path and logs the duration, the sample count and the file path; logs the reason of a failure and
        leaves _written False."""
        try:
            if not file_path:
                raise RuntimeError("the data folder of the extension is not available")
            directory_path = os.path.dirname(file_path)
            if directory_path:
                os.makedirs(directory_path, exist_ok=True)
            with open(file_path, "wb") as file:
                file.write(camera_path.to_camera_path_json(self._recorded_path).encode("utf-8"))
            self._written = True
            self.host.log(TEXTS["recorded_log"].format(f"{self._recorded_path.duration_seconds:.1f}", len(self._recorded_path.samples),
                                                       file_path))
        except Exception as exception:
            self.host.log(TEXTS["write_failed_log"].format(file_path, exception))

    def _automatic_file_path(self):
        """<extension data>\\camera-paths\\camera-path-YYYYMMDD-HHMMSS.lopcamerapath; empty if the extension has no data folder."""
        if not self._automatic_save_directory_path:
            return ""
        return os.path.join(self._automatic_save_directory_path, "camera-paths", suggested_file_name())


@lop.command("lop.example.python.play_camera_path", TEXTS["play_name"] + " (Python)",
             existing_tab_id="lop.tab.view",
             ribbon_section="Ejemplos en Python" if lop.language == "es" else "Python examples",
             description=TEXTS["play_description"],
             icon="play_camera_path.png",
             requirements=lop.REQUIRES_OPEN_CLOUD)
class PlayCameraPathCommand(lop.Command):
    def __init__(self):
        self._playback = None
        self._previous_frame_time_seconds = None
        self._last_banner_time_seconds = 0.0
        self._is_animating_to_first_pose = False

    async def on_start(self):
        path = _last_recorded_camera_path or await self._open_file()
        if path is None:
            self.finish()
            return
        await self._play(path)

    def on_view_frame(self, frame):
        if self._playback is None or self._is_animating_to_first_pose:
            return
        # Another component moved the camera after the previous set_camera_pose of this command.
        if self._previous_frame_time_seconds is not None and not frame.camera_pose_set_by_this_command:
            self.finish()
            return
        playback_step_seconds = 0.0 if self._previous_frame_time_seconds is None else min(
            frame.frame_time_seconds - self._previous_frame_time_seconds, MAXIMUM_PLAYBACK_STEP_SECONDS)
        self._previous_frame_time_seconds = frame.frame_time_seconds
        pose = self._playback.advance(playback_step_seconds)
        if pose is None:
            self.finish()
            return
        self.host.set_camera_pose(pose_to_lop(pose))
        if frame.frame_time_seconds - self._last_banner_time_seconds >= BANNER_REFRESH_INTERVAL_SECONDS:
            self._last_banner_time_seconds = frame.frame_time_seconds
            self._show_banner()

    async def on_banner_action(self, action_name):
        if self._playback is None:
            return
        if action_name == "pause":
            self._playback.paused = not self._playback.paused
        elif action_name == "slower":
            self._playback.decrease_speed()
        elif action_name == "faster":
            self._playback.increase_speed()
        elif action_name == "loop":
            self._playback.loops = not self._playback.loops
        elif action_name == "stop":
            self.finish()
            return
        elif action_name == "open":
            # A second animate_camera call would cancel the pending animation, causing the suspended _play call to invoke finish().
            if self._is_animating_to_first_pose:
                return
            path = await self._open_file()
            if path is not None:
                await self._play(path)
            return
        self._show_banner()

    def on_key_down(self, key):
        if key == lop.KEY_SPACE:
            if self._playback is not None:
                self._playback.paused = not self._playback.paused
            self._show_banner()
            return True
        if key == lop.KEY_LEFT:
            if self._playback is not None:
                self._playback.decrease_speed()
            self._show_banner()
            return True
        if key == lop.KEY_RIGHT:
            if self._playback is not None:
                self._playback.increase_speed()
            self._show_banner()
            return True
        if key == lop.KEY_ESCAPE:
            self.finish()
            return True
        # The navigation keys finish the playback; returning False gives the key to the navigation of lop.
        if key in (ord("W"), ord("A"), ord("S"), ord("D")):
            self.finish()
            return False
        return False

    # A mouse-button press or mouse-wheel event finishes playback; returning False lets lop process the event for navigation.
    def on_mouse_button_down(self, button, position, double_click):
        self.finish()
        return False

    def on_mouse_wheel(self, delta, position, control, shift):
        self.finish()
        return False

    async def _play(self, path):
        self._playback = CameraPathPlayback(path)
        self._previous_frame_time_seconds = None
        self._is_animating_to_first_pose = True
        self._show_banner()
        self.host.set_view_frame_notices(lop.VIEW_FRAME_NOTICES_EVERY_FRAME)
        # Awaiting animate_camera returns None if another camera movement interrupts the animation or the command finishes.
        if not await self.host.animate_camera(pose_to_lop(path.samples[0].pose)):
            self.finish()
            return
        self._is_animating_to_first_pose = False

    async def _open_file(self):
        """Asks for a .lopcamerapath file and reads it; logs the reason and returns None for a file that cannot be read."""
        file_path = await self.host.ask_file([CAMERA_PATH_EXTENSION])
        if file_path is None:
            return None
        try:
            with open(file_path, "rb") as file:
                return camera_path.camera_path_from_json(file.read())
        except Exception as exception:
            self.host.log(TEXTS["camera_path_read_failed_log"].format(file_path, exception))
            return None

    def _show_banner(self):
        """Shows the progress of the playback, its speed and the six buttons; nothing before the first path is loaded."""
        playback = self._playback
        if playback is None:
            return
        duration_seconds = playback.path.duration_seconds
        message = TEXTS["playback_banner_looping" if playback.loops else "playback_banner"].format(
            format_duration_minutes_seconds(playback.path_time_seconds), format_duration_minutes_seconds(duration_seconds),
            f"{playback.speed_multiplier:g}")
        progress = min(playback.path_time_seconds / duration_seconds, 1.0) if duration_seconds > 0.0 else 0.0
        self.host.show_banner(message, progress, [
            lop.BannerButton(TEXTS["resume" if playback.paused else "pause"], "pause"),
            lop.BannerButton(TEXTS["slower"], "slower"),
            lop.BannerButton(TEXTS["faster"], "faster"),
            lop.BannerButton(TEXTS["loop"], "loop"),
            lop.BannerButton(TEXTS["stop_playback"], "stop"),
            lop.BannerButton(TEXTS["open"], "open"),
        ])
