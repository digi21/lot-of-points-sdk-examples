// Header-only C++20 API over lop_sdk.h. A command derives from lop::Command: lop calls its event functions on the user interface thread,
// on_start, on_panel_action, on_panel_value_changed and on_banner_action are coroutines that can co_await requests (a point, the points of a
// sphere, the open clouds, a file, a camera animation), and run_in_background runs a computation on a thread of lop as a task of the Background tasks panel. Pass a function
// returning lop::Extension (or only the commands, as std::vector<lop::Declaration>) to LOP_SDK_EXTENSION, which exports lop_register_extension.
#pragma once

#include "../lop_sdk.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <initializer_list>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace lop
{
    using Point = lop_point;

    // World coordinates in the coordinate system of the clouds.
    struct Point3
    {
        double x{};
        double y{};
        double z{};
    };

    // Cursor position in physical pixels of the view, from its top left corner.
    struct ScreenPoint
    {
        double x{};
        double y{};
    };

    enum class MouseButton : std::uint32_t
    {
        left = LOP_MOUSE_BUTTON_LEFT,
        middle = LOP_MOUSE_BUTTON_MIDDLE,
        right = LOP_MOUSE_BUTTON_RIGHT,
    };

    // Keys of on_key_down; the letter keys use the codes of the uppercase ASCII letters: Key{ 'A' } to Key{ 'Z' }.
    enum class Key : std::uint32_t
    {
        escape = LOP_KEY_ESCAPE,
        enter = LOP_KEY_ENTER,
        backspace = LOP_KEY_BACKSPACE,
        delete_key = LOP_KEY_DELETE,
        space = LOP_KEY_SPACE,
        tab = LOP_KEY_TAB,
        left = LOP_KEY_LEFT,
        right = LOP_KEY_RIGHT,
        up = LOP_KEY_UP,
        down = LOP_KEY_DOWN,
    };

    // The cloud notices of Extension::on_cloud_event (see lop_cloud_event_type). For each extension and cloud_id, lop alternates added and
    // closing, starting with added; closing also arrives before lop disables the extension.
    enum class CloudEvent : std::uint32_t { added = LOP_CLOUD_ADDED, closing = LOP_CLOUD_CLOSING };

    // Languages of the lop user interface, in the order of the entries of lop::Translations and of the per-language text tables.
    enum class Language : std::uint8_t { catalan, german, english, spanish, basque, french, galician, italian, portuguese };

    inline constexpr std::size_t language_count = 9;
    static_assert(static_cast<std::size_t>(Language::portuguese) + 1 == language_count);

    // The Language whose lowercase ISO 639-1 code is code ("ca", "de", "en", "es", "eu", "fr", "gl", "it" or "pt"), as
    // lop_register_extension receives it; Language::english for any other code, including an empty one.
    [[nodiscard]] constexpr Language language_from_iso_639_1_code(std::string_view const code) noexcept
    {
        constexpr std::array<std::string_view, language_count> codes{ "ca", "de", "en", "es", "eu", "fr", "gl", "it", "pt" };
        for (std::size_t language_index = 0; language_index < codes.size(); ++language_index)
            if (codes[language_index] == code)
                return static_cast<Language>(language_index);
        return Language::english;
    }

    // One UTF-8 text per Language, in the order of Language. text(language) returns the English entry when the entry of
    // language is null or empty.
    struct Translations
    {
        std::array<char const*, language_count> texts{};

        [[nodiscard]] constexpr char const* text(Language const language) const noexcept
        {
            auto const translated = texts[static_cast<std::size_t>(language)];
            return translated && *translated ? translated : texts[static_cast<std::size_t>(Language::english)];
        }
    };

    class Host;
    class TaskHost;

    // Owns the columns of points delivered by lop and releases them at destruction. Points delivered to a command must be destroyed before
    // the command; points delivered to a background task, before the task returns.
    class Points
    {
    public:
        Points() = default;
        Points(Points&& other) noexcept
            : _release{ std::exchange(other._release, nullptr) }, _context{ std::exchange(other._context, nullptr) },
              _points{ std::exchange(other._points, {}) }, _classification_flags{ std::exchange(other._classification_flags, nullptr) }
        {
        }
        Points& operator=(Points&& other) noexcept
        {
            if (this != &other)
            {
                release();
                _release = std::exchange(other._release, nullptr);
                _context = std::exchange(other._context, nullptr);
                _points = std::exchange(other._points, {});
                _classification_flags = std::exchange(other._classification_flags, nullptr);
            }
            return *this;
        }
        Points(Points const&) = delete;
        Points& operator=(Points const&) = delete;
        ~Points() { release(); }

        [[nodiscard]] std::size_t size() const noexcept { return static_cast<std::size_t>(_points.point_count); }
        [[nodiscard]] bool empty() const noexcept { return 0 == _points.point_count; }
        // Returns false if at least one requested cloud could not be loaded completely.
        [[nodiscard]] bool complete() const noexcept { return 0 != _points.complete; }

        [[nodiscard]] std::span<double const> x() const noexcept { return column(_points.x); }
        [[nodiscard]] std::span<double const> y() const noexcept { return column(_points.y); }
        [[nodiscard]] std::span<double const> z() const noexcept { return column(_points.z); }
        [[nodiscard]] std::span<std::uint64_t const> original_point_index() const noexcept { return column(_points.original_point_index); }
        [[nodiscard]] std::span<std::uint32_t const> cloud_id() const noexcept { return column(_points.cloud_id); }
        [[nodiscard]] std::span<std::uint16_t const> intensity() const noexcept { return column(_points.intensity); }
        [[nodiscard]] std::span<std::uint8_t const> classification() const noexcept { return column(_points.classification); }
        [[nodiscard]] std::span<std::uint8_t const> return_number() const noexcept { return column(_points.return_number); }
        [[nodiscard]] std::span<std::uint8_t const> number_of_returns() const noexcept { return column(_points.number_of_returns); }
        [[nodiscard]] std::span<std::uint8_t const> red() const noexcept { return column(_points.red); }
        [[nodiscard]] std::span<std::uint8_t const> green() const noexcept { return column(_points.green); }
        [[nodiscard]] std::span<std::uint8_t const> blue() const noexcept { return column(_points.blue); }
        // Bits 0-3 as LOP_CLASSIFICATION_FLAG_*; empty if no cloud of the points has LOP_FIELD_CLASSIFICATION_FLAGS.
        [[nodiscard]] std::span<std::uint8_t const> classification_flags() const noexcept { return column(_classification_flags); }

        [[nodiscard]] Point operator[](std::size_t const i) const noexcept
        {
            return { _points.x[i], _points.y[i], _points.z[i], _points.original_point_index[i], _points.cloud_id[i], _points.intensity[i],
                _points.classification[i], _points.return_number[i], _points.number_of_returns[i], _points.red[i], _points.green[i],
                _points.blue[i], _classification_flags ? _classification_flags[i] : std::uint8_t{} };
        }

        // Returns the C structure for select_points; the columns remain owned by this object.
        [[nodiscard]] lop_points const& c_points() const noexcept { return _points; }

        // Assumes responsibility for releasing the point data delivered by lop; release is the release_points function of the host that delivered them, and
        // classification_flags the column of get_points_classification_flags of that host, or nullptr.
        Points(void (*release)(void*, lop_points*), void* const context, lop_points const& points,
            std::uint8_t const* const classification_flags = nullptr) noexcept
            : _release{ release }, _context{ context }, _points{ points }, _classification_flags{ classification_flags }
        {
        }

    private:
        template<typename T>
        [[nodiscard]] std::span<T const> column(T const* const values) const noexcept
        {
            return values ? std::span<T const>{ values, size() } : std::span<T const>{};
        }

        void release() noexcept
        {
            if (_release && _points.handle)
                _release(_context, &_points);
            _release = nullptr;
            _context = nullptr;
            _points = {};
            _classification_flags = nullptr;
        }

        void (*_release)(void*, lop_points*) {};
        void* _context{};
        lop_points _points{};
        std::uint8_t const* _classification_flags{};
    };

    // Copies cloud metadata and strings.
    struct Cloud
    {
        std::uint32_t cloud_id{};
        std::string name;
        std::string original_path;
        bool visible{};
        bool remote{};
        bool editable{};
        bool has_original_point_indices{};
        std::uint32_t extra_fields{};
        std::array<double, 3> box_min{};
        std::array<double, 3> box_max{};
        std::string crs_wkt;
        std::string horizontal_unit_name;
        double horizontal_metres_per_unit{};
        std::string vertical_unit_name;
        double vertical_metres_per_unit{};
        bool vertical_axis_points_down{};
        // Row-major 4x4 similarity that maps the coordinates of original_path to world coordinates; identity for a cloud without a .transform.json file.
        std::array<double, 16> original_to_world_transform{};
        // Number of points of the original file; original_point_index values below it identify them. 0 if has_original_point_indices is false.
        std::uint64_t original_point_count{};
        // Number of points that confirmed editing events have added with add_points, including the added points that a later event deletes;
        // their original_point_index values are original_point_count, original_point_count + 1, ... in the order of addition.
        std::uint64_t added_point_count{};
    };

    // Copies a vector geometry of the drawing registry (see lop_geometry).
    struct Geometry
    {
        lop_geometry_type type{ LOP_GEOMETRY_POINT };
        std::vector<Point3> vertices;
        bool closed{};
        std::string layer;
        std::string text;
    };

    // Describes a geometry of Host::show_temporary_geometries (see lop_temporary_geometry): a polyline, a cross whose horizontal and vertical
    // arms each measure cross_arm_length_or_text_line_height_pixels physical pixels from end to end, or a line of text whose line height
    // measures cross_arm_length_or_text_line_height_pixels physical pixels, with vertices in world coordinates. A color_argb of 0 uses the
    // colour of the temporary line, opaque red.
    struct TemporaryGeometry
    {
        lop_temporary_geometry_type type{ LOP_TEMPORARY_POLYLINE };
        std::vector<Point3> vertices;
        bool closed{};
        std::uint32_t color_argb{};
        double cross_arm_length_or_text_line_height_pixels{};
        std::string text;
    };

    // SDK 0.8. A camera pose in world coordinates, in cloud units, with the world Z axis up (see lop_camera_pose).
    struct CameraPose
    {
        Point3 position{};
        Point3 look_direction{ 0.0, 0.0, -1.0 };
        Point3 up{ 0.0, 1.0, 0.0 };
        double vertical_field_of_view_degrees{ 45.0 };
    };

    // SDK 0.8. Modes of Host::set_view_frame_notices (see LOP_VIEW_FRAME_NOTICES_*).
    enum class ViewFrameNotices : std::uint32_t
    {
        none = LOP_VIEW_FRAME_NOTICES_NONE,
        when_camera_changes = LOP_VIEW_FRAME_NOTICES_WHEN_CAMERA_CHANGES,
        every_frame = LOP_VIEW_FRAME_NOTICES_EVERY_FRAME,
    };

    // SDK 0.8. The fields of a LOP_EVENT_VIEW_FRAME.
    struct ViewFrame
    {
        CameraPose camera_pose;
        double frame_time_seconds{};                // Reading in seconds of a monotonic clock of lop, with an arbitrary origin.
        bool camera_changed{};
        bool camera_pose_set_by_this_command{};
    };

    // SDK 0.8. A button of Host::show_banner: text in the language of the extension, and the action_name that Command::on_banner_action
    // receives, in ASCII letters, digits, '.', '_' and '-'.
    struct BannerButton
    {
        std::string text;
        std::string action_name;
    };

    // Filters the points near a click in ask_cloud_point; true accepts the point.
    using AcceptPoint = std::function<bool(Point const& point, Cloud const& cloud)>;

    // Receives the point under the cursor while a point request waits, or std::nullopt if there is none.
    using PreviewPoint = std::function<void(std::optional<Point3> const& point)>;

    using Tile = lop_tile;
    using TilesSummary = lop_tiles_summary;

    // The new points of TileResult::add_points and TaskHost::add_points, in columns (see lop_added_points).
    using AddedPoints = lop_added_points;

    // TileResult::add_points and TaskHost::add_points throw AddPointsFailed on LOP_FAILED; the Output panel contains the reason.
    class AddPointsFailed : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // TaskHost::start_editing_event throws EditingEventFailed on LOP_FAILED; the Output panel contains the reason.
    class EditingEventFailed : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // The format, the coordinates and the isolation of TaskHost::export_cloud (see lop_export_options).
    enum class ExportFormat : std::uint32_t { las = LOP_EXPORT_FORMAT_LAS, laz = LOP_EXPORT_FORMAT_LAZ, e57 = LOP_EXPORT_FORMAT_E57 };
    enum class ExportCoordinates : std::uint32_t { world = LOP_EXPORT_WORLD_COORDINATES, original = LOP_EXPORT_ORIGINAL_COORDINATES };
    struct ExportOptions { ExportFormat format{ ExportFormat::las }; ExportCoordinates coordinates{ ExportCoordinates::world }; bool only_isolated_points{}; };

    // Wraps lop_tile_result for the tile of ProcessTile: classification and deleted return the result columns of the tile, and add_points
    // queues new points for the cloud of the tile (see lop_tile_result::add_points). The tile must be the one that process_tile received.
    class TileResult
    {
    public:
        explicit TileResult(lop_tile_result& result) noexcept
            : _result{ result }, _add_points{ result.add_points }, _add_points_context{ result.add_points_context } {}

        // Returns one class per tile point; it starts with the tile classes.
        [[nodiscard]] std::span<std::uint8_t> classification(Tile const& tile) const noexcept
        {
            return { _result.classification, static_cast<std::size_t>(tile.point_count) };
        }

        // Returns one flag per tile point; set an element to 1 to delete that point. It starts with zeros.
        [[nodiscard]] std::span<std::uint8_t> deleted(Tile const& tile) const noexcept
        {
            return { _result.deleted, static_cast<std::size_t>(tile.point_count) };
        }

        // Queues the points for the cloud of the tile; callable from any thread until process_tile returns. Throws AddPointsFailed on
        // LOP_FAILED, also when a copy of this TileResult calls it after process_tile returns.
        void add_points(AddedPoints const& points) const
        {
            if (LOP_OK != _add_points(_add_points_context, &points))
                throw AddPointsFailed{ "add_points has failed: the reason is in the Output panel" };
        }

    private:
        lop_tile_result& _result;
        // Copies of lop_tile_result::add_points and add_points_context, which add_points uses; add_points uses these copies without accessing _result after process_tile returns.
        int (*_add_points)(void*, lop_added_points const*){};
        void* _add_points_context{};
    };

    // Provides tile progress, cancellation and logging callbacks usable from any thread until process_tile returns.
    class TileHost
    {
    public:
        explicit TileHost(lop_tile_host const& host) noexcept : _host{ host } {}

        void report_tile_progress(double const fraction) const { _host.report_tile_progress(_host.context, fraction); }
        [[nodiscard]] bool cancelled() const { return 0 != _host.cancelled(_host.context); }

        void log(std::string_view const message) const
        {
            std::string const text{ message };
            _host.log(_host.context, text.c_str());
        }

        [[nodiscard]] std::uint32_t max_worker_thread_count() const noexcept { return _host.max_worker_thread_count; }

    private:
        lop_tile_host const& _host;
    };

    // Receives nullptr result in read-only mode; host.cancelled() on return produces LOP_CANCELLED.
    // Uncaught exceptions are logged and make process_tiles fail.
    using ProcessTile = std::function<void(Tile const& tile, TileResult* result, TileHost const& host)>;

    struct TileOptions
    {
        double tile_core_side_length_cloud_units{};
        double tile_overlap_width_cloud_units{};
        std::uint32_t required_extra_fields{};
        lop_point_order point_order{ LOP_ORDER_ORIGINAL_POINT_INDEX };
        // If true, process_tiles may invoke ProcessTile on several threads at once (LOP_TILES_CONCURRENT).
        // ProcessTile must synchronize modifications to shared state.
        bool concurrent{};
        bool read_only{};
        double estimated_peak_bytes_per_point{};
    };

    // A value of Values[key] of a panel or dialog: std::monostate if the key has no value. get_ui_value returns OtherUiValue for a value of
    // another type, such as the selected item of a list.
    struct OtherUiValue
    {
        [[nodiscard]] bool operator==(OtherUiValue const&) const noexcept = default;
    };
    using UiValue = std::variant<std::monostate, double, std::int32_t, bool, std::string, OtherUiValue>;

    // A row of Lists[key] of a panel or dialog: the name of each field and its double, std::int32_t, bool or std::string value.
    using UiRow = std::vector<std::pair<std::string, UiValue>>;

    // set_ui_value, get_ui_value, set_ui_rows, show_panel and mark_panel_for_attention throw UiFailed on LOP_FAILED; the Output panel contains the reason when the host throws an exception.
    class UiFailed : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // TaskHost::process_tiles throws ProcessTilesFailed on LOP_FAILED; the Output panel contains the reported error.
    class ProcessTilesFailed : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    using PointBlock = lop_point_block;
    using PointReadingSummary = lop_point_reading_summary;

    // The options of TaskHost::read_cloud_points, without the function (see lop_point_reading_options).
    struct PointReadingOptions
    {
        std::uint32_t required_extra_fields{};
        std::uint32_t max_block_point_count{};  // 0 uses LOP_DEFAULT_MAX_BLOCK_POINT_COUNT
    };

    // Receives one block of TaskHost::read_cloud_points on the thread that called read_cloud_points; returns true to receive the next block
    // and false to stop the reading.
    using ProcessPointBlock = std::function<bool(PointBlock const& block)>;

    // TaskHost::read_cloud_points throws ReadCloudPointsFailed on LOP_FAILED when ProcessPointBlock has not thrown; the Output panel contains
    // the reason.
    class ReadCloudPointsFailed : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // The changes of TaskHost::commit_point_changes, in columns (see lop_point_changes), and what the call has committed.
    using PointChanges = lop_point_changes;
    using PointChangesSummary = lop_point_changes_summary;

    // TaskHost::commit_point_changes throws CommitPointChangesFailed on LOP_FAILED; the Output panel contains the reason.
    class CommitPointChangesFailed : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // A request completed with LOP_FAILED; lop has written the reason in the Output panel.
    class RequestFailed : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    namespace detail
    {
        [[nodiscard]] inline Cloud to_cloud(lop_cloud const& cloud)
        {
            return { cloud.cloud_id, cloud.name ? cloud.name : "", cloud.original_path ? cloud.original_path : "", 0 != cloud.visible, 0 != cloud.remote,
                0 != cloud.editable, 0 != cloud.has_original_point_indices, cloud.extra_fields,
                { cloud.box_min[0], cloud.box_min[1], cloud.box_min[2] }, { cloud.box_max[0], cloud.box_max[1], cloud.box_max[2] },
                cloud.crs_wkt ? cloud.crs_wkt : "", cloud.horizontal_unit_name ? cloud.horizontal_unit_name : "", cloud.horizontal_metres_per_unit,
                cloud.vertical_unit_name ? cloud.vertical_unit_name : "", cloud.vertical_metres_per_unit, 0 != cloud.vertical_axis_points_down,
                std::to_array(cloud.original_to_world_transform), cloud.original_point_count, cloud.added_point_count };
        }

        [[nodiscard]] inline std::vector<Cloud> to_clouds(lop_cloud const* const clouds, std::uint32_t const count)
        {
            std::vector<Cloud> copies;
            copies.reserve(count);
            for (std::uint32_t cloud_index = 0; cloud_index < count; ++cloud_index)
                copies.push_back(to_cloud(clouds[cloud_index]));
            return copies;
        }

        [[nodiscard]] inline bool valid_sphere(Point3 const& center, double const radius) noexcept
        {
            return radius >= 0.0 && radius <= std::numeric_limits<double>::max() && center.x - center.x == 0.0 && center.y - center.y == 0.0
                && center.z - center.z == 0.0;
        }

        // Accepts the names of lop_task_host::set_class_name: from 1 to LOP_MAX_CLASS_NAME_BYTES bytes, without null characters.
        [[nodiscard]] inline bool valid_class_name(std::string_view const name) noexcept
        {
            return !name.empty() && name.size() <= LOP_MAX_CLASS_NAME_BYTES && std::string_view::npos == name.find('\0');
        }

        // Accepts the matrices of lop_command_host::request_original_to_world_transform_change: 16 finite values, bottom row exactly
        // (0, 0, 0, 1) and positive determinant of the 3×3 linear part. The first row of the linear part has positive squared norm; the
        // squared norms of the three rows differ from the first squared norm by at most 1e-9 times that norm, and the absolute dot products
        // of distinct rows do not exceed the same limit. It repeats the rule of TransformacionDeNube::EsSemejanza, because this header does
        // not include LasViewShared.
        [[nodiscard]] inline bool is_similarity(std::array<double, 16> const& matrix) noexcept
        {
            if (!std::ranges::all_of(matrix, [](double const value) { return std::isfinite(value); }))
                return false;
            if (matrix[12] != 0.0 || matrix[13] != 0.0 || matrix[14] != 0.0 || matrix[15] != 1.0)
                return false;

            auto const compute_linear_row_dot_product = [&matrix](int const linear_row_index, int const other_linear_row_index)
            {
                return matrix[linear_row_index * 4] * matrix[other_linear_row_index * 4] + matrix[linear_row_index * 4 + 1] * matrix[other_linear_row_index * 4 + 1] + matrix[linear_row_index * 4 + 2] * matrix[other_linear_row_index * 4 + 2];
            };
            auto const first_row_squared_norm = compute_linear_row_dot_product(0, 0);
            if (!(first_row_squared_norm > 0.0))
                return false;

            constexpr auto tolerance = 1e-9;
            for (auto linear_row_index = 0; linear_row_index < 3; ++linear_row_index)
            {
                if (std::abs(compute_linear_row_dot_product(linear_row_index, linear_row_index) - first_row_squared_norm) > tolerance * first_row_squared_norm)
                    return false;
                for (auto other_linear_row_index = linear_row_index + 1; other_linear_row_index < 3; ++other_linear_row_index)
                    if (std::abs(compute_linear_row_dot_product(linear_row_index, other_linear_row_index)) > tolerance * first_row_squared_norm)
                        return false;
            }

            auto const determinant = matrix[0] * (matrix[5] * matrix[10] - matrix[6] * matrix[9]) - matrix[1] * (matrix[4] * matrix[10] - matrix[6] * matrix[8]) + matrix[2] * (matrix[4] * matrix[9] - matrix[5] * matrix[8]);
            return determinant > 0.0;
        }

        // Accepts the geometries of lop_command_host::show_temporary_geometries: at most LOP_MAX_TEMPORARY_GEOMETRIES geometries and
        // LOP_MAX_TEMPORARY_GEOMETRY_VERTICES vertices in total; each geometry has a known type, two or more vertices for a polyline and
        // one for a cross or a text, finite coordinates, a color_argb that is 0 or has a nonzero opacity, a finite
        // cross_arm_length_or_text_line_height_pixels greater than 0 for a cross or a text, and a non-empty text without null characters for
        // a text.
        [[nodiscard]] inline bool valid_temporary_geometries(std::span<TemporaryGeometry const> const geometries) noexcept
        {
            if (geometries.size() > LOP_MAX_TEMPORARY_GEOMETRIES)
                return false;
            std::size_t total_vertex_count{};
            for (auto const& geometry : geometries)
            {
                auto const is_polyline = LOP_TEMPORARY_POLYLINE == geometry.type;
                if (!is_polyline && LOP_TEMPORARY_CROSS != geometry.type && LOP_TEMPORARY_TEXT != geometry.type)
                    return false;
                if (is_polyline ? geometry.vertices.size() < 2 : 1 != geometry.vertices.size())
                    return false;
                total_vertex_count += geometry.vertices.size();
                if (total_vertex_count > LOP_MAX_TEMPORARY_GEOMETRY_VERTICES)
                    return false;
                if (!std::ranges::all_of(geometry.vertices, [](Point3 const& vertex)
                    { return std::isfinite(vertex.x) && std::isfinite(vertex.y) && std::isfinite(vertex.z); }))
                    return false;
                if (0 != geometry.color_argb && 0 == (geometry.color_argb >> 24))
                    return false;
                if (!is_polyline && !(std::isfinite(geometry.cross_arm_length_or_text_line_height_pixels)
                    && geometry.cross_arm_length_or_text_line_height_pixels > 0.0))
                    return false;
                if (LOP_TEMPORARY_TEXT == geometry.type && (geometry.text.empty() || std::string::npos != geometry.text.find('\0')))
                    return false;
            }
            return true;
        }

        // Accepts the suggested_file_name of lop_command_host::request_save_file_path: no folder separator, ':', '<', '>', '"', '|', '?',
        // '*' or control character.
        [[nodiscard]] inline bool valid_suggested_file_name(std::string_view const file_name) noexcept
        {
            return std::ranges::none_of(file_name, [](char const character)
                {
                    return static_cast<unsigned char>(character) < 0x20
                        || std::string_view{ "<>:\"/\\|?*" }.find(character) != std::string_view::npos;
                });
        }

        // Accepts the file_type_name of lop_command_host::request_save_file_path: at most LOP_MAX_FILE_TYPE_NAME_BYTES bytes, without
        // control characters.
        [[nodiscard]] inline bool valid_file_type_name(std::string_view const file_type_name) noexcept
        {
            return file_type_name.size() <= LOP_MAX_FILE_TYPE_NAME_BYTES && std::ranges::none_of(file_type_name, [](char const character)
                {
                    return static_cast<unsigned char>(character) < 0x20 || '\x7F' == character;
                });
        }

        // Accepts the extensions of lop_command_host::request_save_file_path: a period followed by at least one character, without ';',
        // '*', '?' or white space.
        [[nodiscard]] inline bool valid_file_extension(std::string_view const extension) noexcept
        {
            return extension.size() > 1 && '.' == extension.front() && std::ranges::none_of(extension, [](char const character)
                {
                    return ';' == character || '*' == character || '?' == character || ' ' == character
                        || (character >= '\t' && character <= '\r') || '\0' == character;
                });
        }

        // Accepts the poses of lop_command_host::set_camera_pose: finite values, a look_direction of length 1e-9 or more, an up whose
        // component orthogonal to look_direction is not zero and is at least 1e-9 times the length of up, and a field of view greater than 0
        // and less than 180 degrees. Divides each vector by its largest absolute component so that the products do not overflow.
        [[nodiscard]] inline bool valid_camera_pose(CameraPose const& pose) noexcept
        {
            auto const finite = [](Point3 const& vector) { return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z); };
            if (!finite(pose.position) || !finite(pose.look_direction) || !finite(pose.up)
                || !(pose.vertical_field_of_view_degrees > 0.0 && pose.vertical_field_of_view_degrees < 180.0)
                || std::hypot(pose.look_direction.x, pose.look_direction.y, pose.look_direction.z) < 1e-9
                || (0.0 == pose.up.x && 0.0 == pose.up.y && 0.0 == pose.up.z))
                return false;
            auto const divide_by_largest_absolute_component = [](Point3 const& vector)
                {
                    auto const largest_absolute_component = std::max({ std::abs(vector.x), std::abs(vector.y), std::abs(vector.z) });
                    return Point3{ vector.x / largest_absolute_component, vector.y / largest_absolute_component, vector.z / largest_absolute_component };
                };
            auto look = divide_by_largest_absolute_component(pose.look_direction);
            auto const look_length = std::hypot(look.x, look.y, look.z);
            look = { look.x / look_length, look.y / look_length, look.z / look_length };
            auto const up = divide_by_largest_absolute_component(pose.up);
            auto const up_along_look = up.x * look.x + up.y * look.y + up.z * look.z;
            auto const orthogonal_up_length = std::hypot(up.x - up_along_look * look.x, up.y - up_along_look * look.y, up.z - up_along_look * look.z);
            return orthogonal_up_length >= 1e-9 * std::hypot(up.x, up.y, up.z);
        }

        [[nodiscard]] inline lop_camera_pose to_c_camera_pose(CameraPose const& pose) noexcept
        {
            return { { pose.position.x, pose.position.y, pose.position.z }, { pose.look_direction.x, pose.look_direction.y, pose.look_direction.z },
                { pose.up.x, pose.up.y, pose.up.z }, pose.vertical_field_of_view_degrees };
        }

        [[nodiscard]] inline CameraPose from_c_camera_pose(lop_camera_pose const& pose) noexcept
        {
            return { { pose.position[0], pose.position[1], pose.position[2] }, { pose.look_direction[0], pose.look_direction[1], pose.look_direction[2] },
                { pose.up[0], pose.up[1], pose.up[2] }, pose.vertical_field_of_view_degrees };
        }

        // Accepts the arguments of lop_command_host::show_banner: a message of at most LOP_MAX_BANNER_MESSAGE_BYTES bytes, a progress that is
        // not NaN and not greater than 1, at most LOP_MAX_BANNER_BUTTONS buttons, button texts that are not empty and have at most
        // LOP_MAX_BANNER_BUTTON_TEXT_BYTES bytes, action names of ASCII letters, digits, '.', '_' and '-' that are not empty, and no null
        // characters in the message or the texts.
        [[nodiscard]] inline bool valid_banner(std::string_view const message, double const progress, std::span<BannerButton const> const buttons) noexcept
        {
            if (message.size() > LOP_MAX_BANNER_MESSAGE_BYTES || std::string_view::npos != message.find('\0') || std::isnan(progress)
                || progress > 1.0 || buttons.size() > LOP_MAX_BANNER_BUTTONS)
                return false;
            return std::ranges::all_of(buttons, [](BannerButton const& button)
                {
                    return !button.text.empty() && button.text.size() <= LOP_MAX_BANNER_BUTTON_TEXT_BYTES
                        && std::string::npos == button.text.find('\0') && !button.action_name.empty()
                        && std::ranges::all_of(button.action_name, [](char const character)
                            {
                                return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')
                                    || (character >= '0' && character <= '9') || '.' == character || '_' == character || '-' == character;
                            });
                });
        }

        // Converts a double, std::int32_t, bool or std::string value; text points to the string of value. Throws std::invalid_argument for
        // std::monostate and OtherUiValue.
        [[nodiscard]] inline lop_ui_value to_c_ui_value(UiValue const& value)
        {
            lop_ui_value converted{};
            if (auto const* const real = std::get_if<double>(&value))
                converted = { LOP_UI_VALUE_REAL, *real };
            else if (auto const* const integer = std::get_if<std::int32_t>(&value))
                converted = { LOP_UI_VALUE_INTEGER, 0.0, *integer };
            else if (auto const* const boolean = std::get_if<bool>(&value))
                converted = { LOP_UI_VALUE_BOOLEAN, 0.0, 0, *boolean ? 1 : 0 };
            else if (auto const* const text = std::get_if<std::string>(&value))
                converted = { LOP_UI_VALUE_TEXT, 0.0, 0, 0, text->c_str() };
            else
                throw std::invalid_argument{ "a value of a panel or dialog must be a double, std::int32_t, bool or std::string" };
            return converted;
        }

        [[nodiscard]] inline UiValue from_c_ui_value(lop_ui_value const& value)
        {
            switch (value.type)
            {
            case LOP_UI_VALUE_REAL:
                return value.real;
            case LOP_UI_VALUE_INTEGER:
                return value.integer;
            case LOP_UI_VALUE_BOOLEAN:
                return 0 != value.boolean;
            case LOP_UI_VALUE_TEXT:
                return std::string{ value.text ? value.text : "" };
            case LOP_UI_VALUE_OTHER:
                return OtherUiValue{};
            default:
                return std::monostate{};
            }
        }

        // Calls set_ui_rows of a lop_command_host or lop_task_host with C copies of rows that point to the strings of rows.
        template<typename SetUiRows>
        [[nodiscard]] int call_set_ui_rows(SetUiRows const set_ui_rows, void* const context, std::string const& id, std::string const& list_key,
            std::span<UiRow const> const rows)
        {
            std::vector<std::vector<lop_ui_field>> fields;
            std::vector<lop_ui_row> c_rows;
            fields.reserve(rows.size());
            for (auto const& row : rows)
            {
                auto& row_fields = fields.emplace_back();
                for (auto const& [name, value] : row)
                    row_fields.push_back({ name.c_str(), to_c_ui_value(value) });
                c_rows.push_back({ row_fields.empty() ? nullptr : row_fields.data(), static_cast<std::uint32_t>(row_fields.size()) });
            }
            return set_ui_rows(context, id.c_str(), list_key.c_str(), c_rows.empty() ? nullptr : c_rows.data(), static_cast<std::uint32_t>(c_rows.size()));
        }

        // user_data must point to the ProcessTile callback.
        inline int process_tile(void* const user_data, lop_tile const* const tile, lop_tile_result* const result, lop_tile_host const* const host) noexcept
        {
            TileHost const wrapper{ *host };
            try
            {
                std::optional<TileResult> result_wrapper;
                if (result)
                    result_wrapper.emplace(*result);
                (*static_cast<ProcessTile const*>(user_data))(*tile, result_wrapper ? &*result_wrapper : nullptr, wrapper);
                return wrapper.cancelled() ? LOP_CANCELLED : LOP_OK;
            }
            catch (std::exception const& exception)
            {
                host->log(host->context, exception.what());
            }
            catch (...)
            {
                host->log(host->context, "process_tile has thrown an exception that is not a std::exception");
            }
            return LOP_FAILED;
        }

        // The ProcessPointBlock of one read_cloud_points call and the exception that it has thrown, which read_cloud_points rethrows.
        struct PointBlockCall
        {
            ProcessPointBlock const* function{};
            std::exception_ptr exception;
        };

        // user_data must point to the PointBlockCall of the call.
        inline int process_point_block(void* const user_data, lop_point_block const* const block) noexcept
        {
            auto& call = *static_cast<PointBlockCall*>(user_data);
            try
            {
                return (*call.function)(*block) ? LOP_OK : LOP_CANCELLED;
            }
            catch (...)
            {
                call.exception = std::current_exception();
                return LOP_FAILED;
            }
        }
    }

    // The editing event that TaskHost::start_editing_event has opened; end and the destructor end it (see lop_task_host::end_editing_event).
    class EditingEvent
    {
    public:
        EditingEvent(EditingEvent&& other) noexcept : _host{ std::exchange(other._host, nullptr) } {}
        EditingEvent& operator=(EditingEvent&&) = delete;
        ~EditingEvent() { end(); }

        // Ends the event; calls after the first one do nothing.
        void end() noexcept
        {
            if (auto const* const host = std::exchange(_host, nullptr))
                host->end_editing_event(host->context);
        }

    private:
        friend class TaskHost;
        explicit EditingEvent(lop_task_host const& host) noexcept : _host{ &host } {}
        lop_task_host const* _host{};
    };

    // Provides the functions of a background task until the task function returns; the functions can wait.
    class TaskHost
    {
    public:
        explicit TaskHost(lop_task_host const& host) noexcept : _host{ host } {}

        void log(std::string_view const message) const
        {
            std::string const text{ message };
            _host.log(_host.context, text.c_str());
        }

        // Replaces the state that the Background tasks panel shows for the task, for example "Classifying".
        void report_state(std::string_view const state) const
        {
            std::string const text{ state };
            _host.report_state(_host.context, text.c_str());
        }

        // Shows the completed fraction, in [0, 1], including both endpoints, in the progress bar of the task.
        void report_progress(double const fraction) const { _host.report_progress(_host.context, fraction); }

        // Returns true when the task is cancelled or the host has failed; the task function should return soon afterwards.
        [[nodiscard]] bool cancelled() const { return 0 != _host.cancelled(_host.context); }

        // Returns true if clouds, points_in_sphere, process_tiles, add_points, start_editing_event, load_geopackage, close_geopackage,
        // load_orthophoto, close_orthophoto, get_cloud_extension_data_directory_path, export_thinned_cloud, get_extension_data_directory_path,
        // find_cloud_extension_data_directory_path, set_class_name, export_cloud, cannot_export_cloud_reason, read_cloud_points or commit_point_changes has reported LOP_CANCELLED. The task ends with LOP_FAILED after a host failure, or with LOP_CANCELLED when cancellation is reported without
        // a host failure.
        [[nodiscard]] bool cancellation_reported() const noexcept { return _cancellation_reported; }

        // Returns the class name shown in the Classifications panel, or an empty string if the class has no name.
        [[nodiscard]] std::string class_name(std::uint8_t const classification) const
        {
            return _host.class_name(_host.context, classification);
        }

        // Returns copies of open-cloud metadata in Layers panel order, or std::nullopt on cancellation.
        [[nodiscard]] std::optional<std::vector<Cloud>> clouds() const
        {
            lop_cloud const* list{};
            std::uint32_t count{};
            if (LOP_OK != _host.clouds(_host.context, &list, &count))
            {
                _cancellation_reported = true;
                return std::nullopt;
            }
            return detail::to_clouds(list, count);
        }

        // Returns undeleted points at distance <= radius from center, restricted by isolation. Uses still-open clouds in cloud_ids, including
        // hidden clouds; an empty list uses visible clouds. Returns std::nullopt on cancellation.
        // Throws std::invalid_argument for a nonfinite center or radius, or a negative radius.
        [[nodiscard]] std::optional<Points> points_in_sphere(Point3 const& center, double const radius, std::span<std::uint32_t const> const cloud_ids = {}) const
        {
            double const xyz[]{ center.x, center.y, center.z };
            lop_points points{};
            switch (_host.points_in_sphere(_host.context, xyz, radius, cloud_ids.empty() ? nullptr : cloud_ids.data(),
                static_cast<std::uint32_t>(cloud_ids.size()), &points))
            {
            case LOP_OK:
            {
                std::uint8_t const* classification_flags{};
                if (LOP_OK != _host.get_points_classification_flags(_host.context, &points, &classification_flags))
                    classification_flags = nullptr;
                return Points{ _host.release_points, _host.context, points, classification_flags };
            }
            case LOP_CANCELLED:
                _cancellation_reported = true;
                return std::nullopt;
            default:
                throw std::invalid_argument{ "points_in_sphere: the center and the radius must be finite, and the radius cannot be negative" };
            }
        }

        // Calls process_tile for each tile containing points in its core and shows the fraction of processed tiles in the progress bar of the
        // task. On success, commits changed core points and the points of TileResult::add_points in the open editing event of the task.
        // If no editing event is open, creates one named after the command or, for a task started by ExtensionHost::run_in_background, after
        // the task (see lop_task_host::process_tiles).
        // Returns change counts on success, std::nullopt on cancellation, or throws ProcessTilesFailed on failure.
        std::optional<TilesSummary> process_tiles(std::span<std::uint32_t const> const cloud_ids, TileOptions const& options, ProcessTile const& process_tile) const
        {
            lop_tile_options const tile_options{ options.tile_core_side_length_cloud_units, options.tile_overlap_width_cloud_units, options.required_extra_fields,
                options.point_order, (options.concurrent ? LOP_TILES_CONCURRENT : 0u) | (options.read_only ? LOP_TILES_READ_ONLY : 0u),
                options.estimated_peak_bytes_per_point, &detail::process_tile, const_cast<ProcessTile*>(&process_tile) };
            TilesSummary summary{};
            switch (_host.process_tiles(_host.context, cloud_ids.empty() ? nullptr : cloud_ids.data(), static_cast<std::uint32_t>(cloud_ids.size()),
                &tile_options, &summary))
            {
            case LOP_OK:
                return summary;
            case LOP_CANCELLED:
                _cancellation_reported = true;
                return std::nullopt;
            default:
                throw ProcessTilesFailed{ "process_tiles has failed: the reason is in the Output panel" };
            }
        }

        // Calls process_point_block on this thread with consecutive blocks of the current points of each cloud of cloud_ids, cloud after cloud
        // (see lop_task_host::read_cloud_points). Returns the summary, also when process_point_block stops the reading, or std::nullopt on
        // cancellation. Rethrows an exception of process_point_block; throws ReadCloudPointsFailed on another failure.
        [[nodiscard]] std::optional<PointReadingSummary> read_cloud_points(std::span<std::uint32_t const> const cloud_ids,
            PointReadingOptions const& options, ProcessPointBlock const& process_point_block) const
        {
            detail::PointBlockCall call{ &process_point_block };
            lop_point_reading_options const reading_options{ options.required_extra_fields, options.max_block_point_count, &detail::process_point_block, &call };
            PointReadingSummary summary{};
            switch (_host.read_cloud_points(_host.context, cloud_ids.empty() ? nullptr : cloud_ids.data(), static_cast<std::uint32_t>(cloud_ids.size()),
                &reading_options, &summary))
            {
            case LOP_OK:
                return summary;
            case LOP_CANCELLED:
                _cancellation_reported = true;
                return std::nullopt;
            default:
                if (call.exception)
                    std::rethrow_exception(call.exception);
                throw ReadCloudPointsFailed{ "read_cloud_points has failed: the reason is in the Output panel" };
            }
        }

        // Loads GeoPackage tables as layers; returns false on cancellation or read failure, with failures reported in the Output panel. Returns
        // true without reading the file if it is already loaded: to display the updated contents of a rewritten file, call close_geopackage,
        // then call load_geopackage again.
        bool load_geopackage(std::string_view const geopackage_file_path) const
        {
            std::string const text{ geopackage_file_path };
            auto const result = _host.load_geopackage(_host.context, text.c_str());
            if (LOP_CANCELLED == result)
                _cancellation_reported = true;
            return LOP_OK == result;
        }

        // Removes the layers of the GeoPackage from the Layers panel and from the view, also after a cancellation; returns true if the layers
        // are removed or the file is not loaded, and false after a host failure or a failure reported in the Output panel.
        bool close_geopackage(std::string_view const geopackage_file_path) const
        {
            std::string const file_path{ geopackage_file_path };
            auto const result = _host.close_geopackage(_host.context, file_path.c_str());
            if (LOP_CANCELLED == result)
                _cancellation_reported = true;
            return LOP_OK == result;
        }

        // Returns the folder, in UTF-8, where this extension keeps its files for the open cloud cloud_id, after lop creates it; std::nullopt
        // after a host failure or a failure reported in the Output panel.
        [[nodiscard]] std::optional<std::string> get_cloud_extension_data_directory_path(std::uint32_t const cloud_id) const
        {
            char const* directory_path{};
            auto const result = _host.get_cloud_extension_data_directory_path(_host.context, cloud_id, &directory_path);
            if (LOP_CANCELLED == result)
                _cancellation_reported = true;
            if (LOP_OK != result || !directory_path)
                return std::nullopt;
            return std::string{ directory_path };
        }

        // Writes the thinned LAS file (see lop_task_host::export_thinned_cloud); the cubes are aligned to the origin of the level of detail,
        // which the LAS header stores as its offsets. Returns false on cancellation or failure, with failures reported in the Output panel.
        // Throws std::invalid_argument if spacing_cloud_units is non-finite, less than or equal to 0, or greater than
        // std::numeric_limits<float>::max().
        bool export_thinned_cloud(std::uint32_t const cloud_id, double const spacing_cloud_units, std::string_view const las_file_path) const
        {
            if (!std::isfinite(spacing_cloud_units) || spacing_cloud_units <= 0.0 || spacing_cloud_units > static_cast<double>(std::numeric_limits<float>::max()))
                throw std::invalid_argument{ "export_thinned_cloud: spacing_cloud_units must be a finite number greater than 0 that fits in a float" };
            std::string const file_path{ las_file_path };
            auto const result = _host.export_thinned_cloud(_host.context, cloud_id, spacing_cloud_units, file_path.c_str());
            if (LOP_CANCELLED == result)
                _cancellation_reported = true;
            return LOP_OK == result;
        }

        // Assigns value to Values[key] of the panel or dialog (see Host::set_ui_value).
        void set_ui_value(std::string_view const panel_or_dialog_id, std::string_view const key, UiValue const& value) const
        {
            std::string const id{ panel_or_dialog_id };
            std::string const value_key{ key };
            auto const converted = detail::to_c_ui_value(value);
            if (LOP_OK != _host.set_ui_value(_host.context, id.c_str(), value_key.c_str(), &converted))
                throw UiFailed{ "set_ui_value returned LOP_FAILED" };
        }

        // Returns Values[key] of the panel or dialog; waits for the user interface thread.
        [[nodiscard]] UiValue get_ui_value(std::string_view const panel_or_dialog_id, std::string_view const key) const
        {
            std::string const id{ panel_or_dialog_id };
            std::string const value_key{ key };
            lop_ui_value value{};
            if (LOP_OK != _host.get_ui_value(_host.context, id.c_str(), value_key.c_str(), &value))
                throw UiFailed{ "get_ui_value returned LOP_FAILED" };
            return detail::from_c_ui_value(value);
        }

        // Replaces the rows of Lists[list_key] of the panel or dialog.
        void set_ui_rows(std::string_view const panel_or_dialog_id, std::string_view const list_key, std::span<UiRow const> const rows) const
        {
            if (LOP_OK != detail::call_set_ui_rows(_host.set_ui_rows, _host.context, std::string{ panel_or_dialog_id }, std::string{ list_key }, rows))
                throw UiFailed{ "set_ui_rows returned LOP_FAILED" };
        }

        // Marks the tab of the panel panel_id, declared by this extension, so that the user sees that the panel has new content, without
        // opening or activating the panel (see lop_task_host::mark_panel_for_attention). Does nothing visible if the panel is closed or its
        // content is on screen. Throws UiFailed on LOP_FAILED.
        void mark_panel_for_attention(std::string_view const panel_id) const
        {
            std::string const id{ panel_id };
            if (LOP_OK != _host.mark_panel_for_attention(_host.context, id.c_str()))
                throw UiFailed{ "mark_panel_for_attention returned LOP_FAILED" };
        }

        // Returns the folder, in UTF-8, where this extension keeps the files that do not belong to a cloud, after lop creates it; std::nullopt
        // after a host failure or a failure reported in the Output panel.
        [[nodiscard]] std::optional<std::string> get_extension_data_directory_path() const
        {
            char const* directory_path{};
            auto const result = _host.get_extension_data_directory_path(_host.context, &directory_path);
            if (LOP_CANCELLED == result)
                _cancellation_reported = true;
            if (LOP_OK != result || !directory_path)
                return std::nullopt;
            return std::string{ directory_path };
        }

        // Returns the folder of get_cloud_extension_data_directory_path for the open cloud cloud_id if it exists, without creating it;
        // std::nullopt if it does not exist, after a host failure or after a failure reported in the Output panel.
        [[nodiscard]] std::optional<std::string> find_cloud_extension_data_directory_path(std::uint32_t const cloud_id) const
        {
            char const* directory_path{};
            auto const result = _host.find_cloud_extension_data_directory_path(_host.context, cloud_id, &directory_path);
            if (LOP_CANCELLED == result)
                _cancellation_reported = true;
            if (LOP_OK != result || !directory_path)
                return std::nullopt;
            return std::string{ directory_path };
        }

        // Assigns name, in UTF-8, to the class in the class catalog of lop, which the Classifications panel shows and lop saves in its
        // settings (see lop_task_host::set_class_name); waits for the user interface thread. Returns false after a host failure or a
        // failure reported in the Output panel. Throws std::invalid_argument for an empty name, a name longer than LOP_MAX_CLASS_NAME_BYTES
        // bytes or a name with a null character.
        bool set_class_name(std::uint8_t const classification, std::string_view const name) const
        {
            if (!detail::valid_class_name(name))
                throw std::invalid_argument{ "set_class_name: the name must have from 1 to 255 bytes and no null characters" };
            std::string const text{ name };
            auto const result = _host.set_class_name(_host.context, classification, text.c_str());
            if (LOP_CANCELLED == result)
                _cancellation_reported = true;
            return LOP_OK == result;
        }

        // Adds the points to the open cloud cloud_id and commits them before returning (see lop_task_host::add_points). Returns true, or false
        // on cancellation; throws AddPointsFailed on LOP_FAILED.
        bool add_points(std::uint32_t const cloud_id, AddedPoints const& points) const
        {
            switch (_host.add_points(_host.context, cloud_id, &points))
            {
            case LOP_OK:
                return true;
            case LOP_CANCELLED:
                _cancellation_reported = true;
                return false;
            default:
                throw AddPointsFailed{ "add_points has failed: the reason is in the Output panel" };
            }
        }

        // Applies the changes to the points of the open cloud cloud_id and commits them before returning (see
        // lop_task_host::commit_point_changes). Returns the summary, or std::nullopt on cancellation; throws CommitPointChangesFailed on LOP_FAILED.
        [[nodiscard]] std::optional<PointChangesSummary> commit_point_changes(std::uint32_t const cloud_id, PointChanges const& changes) const
        {
            PointChangesSummary summary{};
            switch (_host.commit_point_changes(_host.context, cloud_id, &changes, &summary))
            {
            case LOP_OK:
                return summary;
            case LOP_CANCELLED:
                _cancellation_reported = true;
                return std::nullopt;
            default:
                throw CommitPointChangesFailed{ "commit_point_changes has failed: the reason is in the Output panel" };
            }
        }

        // Opens an editing event on cloud_ids (see lop_task_host::start_editing_event). Returns the event, or std::nullopt on cancellation;
        // throws EditingEventFailed on LOP_FAILED.
        [[nodiscard]] std::optional<EditingEvent> start_editing_event(std::span<std::uint32_t const> const cloud_ids) const
        {
            switch (_host.start_editing_event(_host.context, cloud_ids.empty() ? nullptr : cloud_ids.data(), static_cast<std::uint32_t>(cloud_ids.size())))
            {
            case LOP_OK:
                return EditingEvent{ _host };
            case LOP_CANCELLED:
                _cancellation_reported = true;
                return std::nullopt;
            default:
                throw EditingEventFailed{ "start_editing_event has failed: the reason is in the Output panel" };
            }
        }

        // Ends the open editing event of the task, if there is one (see lop_task_host::end_editing_event).
        void end_editing_event() const { _host.end_editing_event(_host.context); }

        // Loads the georeferenced TIFF as an orthophoto of the Layers panel (see lop_task_host::load_orthophoto); returns false on cancellation
        // or failure, with failures reported in the Output panel. To display a rewritten file, call close_orthophoto, then load_orthophoto.
        bool load_orthophoto(std::string_view const tiff_file_path) const
        {
            std::string const file_path{ tiff_file_path };
            auto const result = _host.load_orthophoto(_host.context, file_path.c_str());
            if (LOP_CANCELLED == result)
                _cancellation_reported = true;
            return LOP_OK == result;
        }

        // Removes the orthophoto of the file from the Layers panel and from the view, also after a cancellation; returns true if it is removed
        // or the file is not loaded, and false after a host failure or a failure reported in the Output panel.
        bool close_orthophoto(std::string_view const tiff_file_path) const
        {
            std::string const file_path{ tiff_file_path };
            auto const result = _host.close_orthophoto(_host.context, file_path.c_str());
            if (LOP_CANCELLED == result)
                _cancellation_reported = true;
            return LOP_OK == result;
        }

        // Writes to file_path the cloud with its confirmed edits and its added points (see lop_task_host::export_cloud).
        // Returns the summary if export_cloud returns LOP_OK; std::nullopt otherwise (lop writes the reason of a failure in the Output panel; a cancellation sets cancellation_reported()).
        [[nodiscard]] std::optional<lop_export_summary> export_cloud(std::uint32_t const cloud_id, std::string const& file_path, ExportOptions const& options = {}) const
        {
            lop_export_options const export_options{ static_cast<std::uint32_t>(options.format), static_cast<std::uint32_t>(options.coordinates),
                options.only_isolated_points ? 1 : 0 };
            lop_export_summary summary{};
            auto const result = _host.export_cloud(_host.context, cloud_id, &export_options, file_path.c_str(), &summary);
            if (LOP_CANCELLED == result)
                _cancellation_reported = true;
            if (LOP_OK != result)
                return std::nullopt;
            return summary;
        }

        // Returns std::nullopt if export_cloud would accept the cloud and the format, or the reason in English otherwise (see
        // lop_task_host::can_export_cloud). After LOP_FAILED or LOP_CANCELLED of can_export_cloud, the reason says so and the Output panel has
        // the cause.
        [[nodiscard]] std::optional<std::string> cannot_export_cloud_reason(std::uint32_t const cloud_id, ExportFormat const format) const
        {
            int allowed{};
            char const* reason{};
            switch (_host.can_export_cloud(_host.context, cloud_id, static_cast<std::uint32_t>(format), &allowed, &reason))
            {
            case LOP_OK:
                if (0 != allowed)
                    return std::nullopt;
                return std::string{ reason ? reason : "export_cloud does not accept the cloud and the format" };
            case LOP_CANCELLED:
                _cancellation_reported = true;
                return std::string{ "can_export_cloud has returned LOP_CANCELLED after a host failure" };
            default:
                return std::string{ "can_export_cloud has failed: the reason is in the Output panel" };
            }
        }

    private:
        lop_task_host const& _host;
        mutable bool _cancellation_reported{};
    };

    // The work of a background task; an uncaught exception is logged and fails the task.
    using BackgroundWork = std::function<void(TaskHost& task)>;

    template<typename T = void>
    class Task;

    namespace detail
    {
        struct TaskPromiseBase
        {
            std::coroutine_handle<> continuation{};
            std::exception_ptr exception;

            std::suspend_always initial_suspend() const noexcept { return {}; }

            struct FinalAwaiter
            {
                bool await_ready() const noexcept { return false; }
                template<typename Promise>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> const handle) const noexcept
                {
                    auto const awaiting_coroutine = handle.promise().continuation;
                    return awaiting_coroutine ? awaiting_coroutine : std::noop_coroutine();
                }
                void await_resume() const noexcept {}
            };

            FinalAwaiter final_suspend() const noexcept { return {}; }
            void unhandled_exception() noexcept { exception = std::current_exception(); }
        };

        template<typename T>
        struct TaskPromise : TaskPromiseBase
        {
            std::optional<T> value;
            Task<T> get_return_object() noexcept;
            template<typename U>
            void return_value(U&& result) { value.emplace(std::forward<U>(result)); }
        };

        template<>
        struct TaskPromise<void> : TaskPromiseBase
        {
            Task<void> get_return_object() noexcept;
            void return_void() const noexcept {}
        };

        struct Instance;
        struct CommandInstance;
    }

    // A coroutine that runs on the user interface thread: it starts when another coroutine awaits it, or when lop starts on_start or a panel
    // or banner event handler of lop::Command, and resumes when lop completes the requests it awaits. Destroying the Task destroys the coroutine and unregisters its awaiters; the host requests remain pending until lop completes them.
    template<typename T>
    class Task
    {
    public:
        using promise_type = detail::TaskPromise<T>;

        Task() noexcept = default;
        explicit Task(std::coroutine_handle<promise_type> const handle) noexcept : _handle{ handle } {}
        Task(Task&& other) noexcept : _handle{ std::exchange(other._handle, {}) } {}
        Task& operator=(Task&& other) noexcept
        {
            if (this != &other)
            {
                if (_handle)
                    _handle.destroy();
                _handle = std::exchange(other._handle, {});
            }
            return *this;
        }
        Task(Task const&) = delete;
        Task& operator=(Task const&) = delete;
        ~Task()
        {
            if (_handle)
                _handle.destroy();
        }

        [[nodiscard]] bool await_ready() const noexcept { return !_handle || _handle.done(); }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> const awaiting) noexcept
        {
            _handle.promise().continuation = awaiting;
            return _handle;
        }

        T await_resume()
        {
            if (!_handle)
                throw std::logic_error{ "lop::Task: the task has no coroutine" };
            auto& promise = _handle.promise();
            if (promise.exception)
                std::rethrow_exception(promise.exception);
            if constexpr (!std::is_void_v<T>)
                return std::move(*promise.value);
        }

    private:
        friend struct detail::CommandInstance;
        std::coroutine_handle<promise_type> _handle{};
    };

    template<typename T>
    Task<T> detail::TaskPromise<T>::get_return_object() noexcept
    {
        return Task<T>{ std::coroutine_handle<TaskPromise<T>>::from_promise(*this) };
    }

    inline Task<void> detail::TaskPromise<void>::get_return_object() noexcept
    {
        return Task<void>{ std::coroutine_handle<TaskPromise<void>>::from_promise(*this) };
    }

    namespace detail
    {
        // A request waiting for LOP_EVENT_REQUEST_COMPLETED; Instance resumes the coroutine that awaits it.
        class PendingRequest
        {
        public:
            virtual ~PendingRequest() = default;
            // Copies the result from the event and resumes the coroutine; the request is no longer registered.
            virtual void complete(lop_event const& event) = 0;
            virtual void preview(lop_event const&) {}
        };

        // Contains what the instance of a command needs: the host, the parameters, the pending requests and the on_start coroutine.
        struct Instance
        {
            lop_command const& command;
            lop_command_host const& host;
            std::vector<double> parameters;
            std::map<std::uint64_t, PendingRequest*> pending;
            std::optional<Task<>> on_start_task;

            // Discards the line if the copy of the text cannot be allocated: the function is called from catch blocks of noexcept functions.
            void log(std::string_view const message) const noexcept
            {
                try
                {
                    std::string const text{ message };
                    host.log(host.context, text.c_str());
                }
                catch (...)
                {
                }
            }
        };

        // Awaits a request of lop_command_host. Start calls the request function and returns its request_id; Convert converts the event to Result.
        template<typename Result>
        class RequestAwaiter : public PendingRequest
        {
        public:
            using Start = std::function<std::uint64_t()>;
            using Convert = std::function<Result(lop_event const&)>;

            RequestAwaiter(Instance& instance, Start start, Convert convert, PreviewPoint preview = {})
                : _instance{ instance }, _start{ std::move(start) }, _convert{ std::move(convert) }, _preview{ std::move(preview) }
            {
            }
            RequestAwaiter(RequestAwaiter const&) = delete;
            RequestAwaiter& operator=(RequestAwaiter const&) = delete;
            ~RequestAwaiter() override
            {
                if (_request_id)
                    _instance.pending.erase(_request_id);
            }

            bool await_ready() const noexcept { return false; }

            // Returns false, and resumes at once as a cancellation, if lop does not start the request.
            bool await_suspend(std::coroutine_handle<> const awaiting)
            {
                _awaiting = awaiting;
                _request_id = _start();
                if (!_request_id)
                    return false;
                _instance.pending[_request_id] = this;
                return true;
            }

            // Returns std::nullopt if the request was cancelled or not started; throws RequestFailed if it failed.
            std::optional<Result> await_resume()
            {
                if (_exception)
                    std::rethrow_exception(_exception);
                return std::move(_result);
            }

            void complete(lop_event const& event) override
            {
                _request_id = 0;
                try
                {
                    if (LOP_OK == event.result)
                        _result.emplace(_convert(event));
                    else if (LOP_FAILED == event.result)
                        throw RequestFailed{ "The request failed; the Output panel contains the reason" };
                }
                catch (...)
                {
                    _exception = std::current_exception();
                }
                _awaiting.resume();
            }

            void preview(lop_event const& event) override
            {
                if (_preview)
                    _preview(event.has_coordinates ? std::optional<Point3>{ Point3{ event.coordinates[0], event.coordinates[1], event.coordinates[2] } } : std::nullopt);
            }

        private:
            Instance& _instance;
            Start _start;
            Convert _convert;
            PreviewPoint _preview;
            std::coroutine_handle<> _awaiting{};
            std::uint64_t _request_id{};
            std::optional<Result> _result;
            std::exception_ptr _exception;
        };

        // user_data must point to a heap-allocated BackgroundWork, which the function deletes.
        inline int run_background_work(void* const user_data, lop_task_host const* const host) noexcept
        {
            std::unique_ptr<BackgroundWork> const work{ static_cast<BackgroundWork*>(user_data) };
            TaskHost task{ *host };
            try
            {
                (*work)(task);
                return task.cancellation_reported() || task.cancelled() ? LOP_CANCELLED : LOP_OK;
            }
            catch (std::exception const& exception)
            {
                host->log(host->context, exception.what());
            }
            catch (...)
            {
                host->log(host->context, "the background task has thrown an exception that is not a std::exception");
            }
            return LOP_FAILED;
        }
    }

    // Provides the functions of lop to a command on the user interface thread, from on_start until the command is destroyed. None of them
    // waits: the ask_*, points_in_sphere and clouds functions return awaitables for co_await in a lop::Task coroutine.
    class Host
    {
    public:
        explicit Host(detail::Instance& instance) noexcept : _instance{ instance }, _host{ instance.host } {}

        void log(std::string_view const message) const { _instance.log(message); }

        // Writes message to the status bar; an empty message clears it.
        void status(std::string_view const message) const
        {
            std::string const text{ message };
            _host.status(_host.context, text.c_str());
        }

        // Removes the command from the command stack; pending requests complete as cancelled and the background tasks keep running.
        void finish() const { _host.finish(_host.context); }

        // Replaces the temporary polyline; an empty vertex list removes it.
        void show_temporary_line(std::span<Point3 const> const vertices, bool const closed = false) const
        {
            std::vector<double> xyz;
            xyz.reserve(3 * vertices.size());
            for (auto const& vertex : vertices)
                xyz.insert(xyz.end(), { vertex.x, vertex.y, vertex.z });
            _host.show_temporary_line(_host.context, xyz.empty() ? nullptr : xyz.data(), static_cast<std::uint32_t>(vertices.size()), closed ? 1 : 0);
        }

        void show_temporary_line(std::initializer_list<Point3> const vertices, bool const closed = false) const
        {
            show_temporary_line(std::span<Point3 const>{ vertices.begin(), vertices.size() }, closed);
        }

        // Returns the class name shown in the Classifications panel, or an empty string if the class has no name.
        [[nodiscard]] std::string class_name(std::uint8_t const classification) const
        {
            return _host.class_name(_host.context, classification);
        }

        // Replaces the selection with the indexed points, preserving it after the command; an empty index list clears it.
        // Returns false after finish or cloud closure; throws std::invalid_argument for invalid indices or moved-from points when indices are nonempty.
        bool select_points(Points const& points, std::span<std::uint64_t const> const indices) const
        {
            switch (_host.select_points(_host.context, indices.empty() ? nullptr : &points.c_points(), indices.empty() ? nullptr : indices.data(),
                indices.size()))
            {
            case LOP_OK:
                return true;
            case LOP_CANCELLED:
                return false;
            default:
                throw std::invalid_argument{ "select_points: every index must be below the number of points" };
            }
        }

        // Shows the task name in the Background tasks panel and runs work on a thread of lop with a TaskHost. The command can call finish
        // afterwards: lop keeps it until the task ends. Throws std::runtime_error if lop does not start the task, for example after finish.
        void run_in_background(std::string_view const name, BackgroundWork work) const
        {
            std::string const text{ name };
            auto heap = std::make_unique<BackgroundWork>(std::move(work));
            if (LOP_OK != _host.start_background_task(_host.context, text.c_str(), &detail::run_background_work, heap.get()))
                throw std::runtime_error{ "run_in_background: lop has not started the task " + text };
            (void)heap.release();
        }

        // Awaits a click with a point under the cursor, or the point confirmed by the snap command; preview receives the point under the cursor.
        // Returns std::nullopt if the command finishes before a point is selected.
        [[nodiscard]] detail::RequestAwaiter<Point3> ask_point(std::string_view const prompt, PreviewPoint preview = {}) const
        {
            auto const text = std::make_shared<std::string>(prompt);
            auto const with_preview = static_cast<bool>(preview);
            return { _instance, [this, text, with_preview] { return _host.request_point(_host.context, text->c_str(), with_preview ? 1 : 0); },
                [](lop_event const& event) { return Point3{ event.coordinates[0], event.coordinates[1], event.coordinates[2] }; }, std::move(preview) };
        }

        // Like ask_point, and returns the undeleted point of a visible cloud nearest to the chosen coordinates, at 0.05 cloud units or less.
        [[nodiscard]] detail::RequestAwaiter<Point> ask_cloud_point(std::string_view const prompt, PreviewPoint preview = {}) const
        {
            auto const text = std::make_shared<std::string>(prompt);
            auto const with_preview = static_cast<bool>(preview);
            return { _instance, [this, text, with_preview] { return _host.request_cloud_point(_host.context, text->c_str(), with_preview ? 1 : 0); },
                [](lop_event const& event) { return event.point; }, std::move(preview) };
        }

        // Asks for points with ask_point until accept accepts one of the undeleted points of any open cloud, hidden ones included, at 0.05
        // cloud units or less from the chosen coordinates, offered by increasing distance. Returns std::nullopt if the command finishes before a point is accepted;
        // propagates the exceptions of accept.
        [[nodiscard]] Task<std::optional<Point>> ask_cloud_point(std::string prompt, AcceptPoint accept) const
        {
            std::string const text{ std::move(prompt) };
            while (true)
            {
                auto const chosen = co_await ask_point(text);
                if (!chosen)
                    co_return std::nullopt;
                auto const open = co_await clouds();
                if (!open)
                    co_return std::nullopt;
                std::vector<std::uint32_t> ids;
                for (auto const& cloud : *open)
                    ids.push_back(cloud.cloud_id);
                if (ids.empty())
                    continue;
                auto const candidates = co_await points_in_sphere(*chosen, 0.05, ids);
                if (!candidates)
                    co_return std::nullopt;
                std::vector<std::pair<double, std::size_t>> by_distance;
                for (std::size_t sphere_point_index = 0; sphere_point_index < candidates->size(); ++sphere_point_index)
                {
                    auto const dx = candidates->x()[sphere_point_index] - chosen->x;
                    auto const dy = candidates->y()[sphere_point_index] - chosen->y;
                    auto const dz = candidates->z()[sphere_point_index] - chosen->z;
                    by_distance.emplace_back(dx * dx + dy * dy + dz * dz, sphere_point_index);
                }
                std::ranges::sort(by_distance);
                for (auto const& [distance, sphere_point_index] : by_distance)
                {
                    auto const point = (*candidates)[sphere_point_index];
                    auto const cloud = std::ranges::find(*open, point.cloud_id, &Cloud::cloud_id);
                    if (cloud != open->end() && accept(point, *cloud))
                        co_return point;
                }
            }
        }

        // Awaits a click on a visible vector geometry.
        [[nodiscard]] detail::RequestAwaiter<Geometry> ask_geometry(std::string_view const prompt) const
        {
            auto const text = std::make_shared<std::string>(prompt);
            return { _instance, [this, text] { return _host.request_geometry(_host.context, text->c_str()); }, [](lop_event const& event)
                {
                    Geometry geometry;
                    if (auto const* const g = event.geometry)
                    {
                        geometry.type = static_cast<lop_geometry_type>(g->type);
                        for (std::uint32_t geometry_vertex_index = 0; geometry_vertex_index < g->vertex_count; ++geometry_vertex_index)
                            geometry.vertices.push_back({ g->xyz[3 * geometry_vertex_index], g->xyz[3 * geometry_vertex_index + 1], g->xyz[3 * geometry_vertex_index + 2] });
                        geometry.closed = 0 != g->closed;
                        geometry.layer = g->layer ? g->layer : "";
                        geometry.text = g->text ? g->text : "";
                    }
                    return geometry;
                } };
        }

        // Loads the undeleted points at distance <= radius from center, restricted by isolation, from the still-open clouds in cloud_ids,
        // including hidden clouds, or from the visible clouds if cloud_ids is empty. Throws std::invalid_argument for a nonfinite center or
        // radius, or a negative radius.
        [[nodiscard]] detail::RequestAwaiter<Points> points_in_sphere(Point3 const& center, double const radius, std::span<std::uint32_t const> const cloud_ids = {}) const
        {
            if (!detail::valid_sphere(center, radius))
                throw std::invalid_argument{ "points_in_sphere: the center and the radius must be finite, and the radius cannot be negative" };
            auto const ids = std::make_shared<std::vector<std::uint32_t>>(cloud_ids.begin(), cloud_ids.end());
            return { _instance, [this, center, radius, ids]
                {
                    double const xyz[]{ center.x, center.y, center.z };
                    return _host.request_points_in_sphere(_host.context, xyz, radius, ids->empty() ? nullptr : ids->data(), static_cast<std::uint32_t>(ids->size()));
                },
                [this](lop_event const& event)
                {
                    std::uint8_t const* classification_flags{};
                    if (LOP_OK != _host.get_points_classification_flags(_host.context, &event.points, &classification_flags))
                        classification_flags = nullptr;
                    return Points{ _host.release_points, _host.context, event.points, classification_flags };
                } };
        }

        [[nodiscard]] detail::RequestAwaiter<Points> points_in_sphere(Point const& center, double const radius, std::span<std::uint32_t const> const cloud_ids = {}) const
        {
            return points_in_sphere(Point3{ center.x, center.y, center.z }, radius, cloud_ids);
        }

        // Describes the open clouds in Layers panel order.
        [[nodiscard]] detail::RequestAwaiter<std::vector<Cloud>> clouds() const
        {
            return { _instance, [this] { return _host.request_clouds(_host.context); },
                [](lop_event const& event) { return detail::to_clouds(event.clouds, event.cloud_count); } };
        }

        // Shows the file picker with the extensions, each with its period (".gpkg"); returns std::nullopt if the user closes the picker.
        [[nodiscard]] detail::RequestAwaiter<std::string> ask_file(std::vector<std::string> extensions = {}) const
        {
            auto const list = std::make_shared<std::vector<std::string>>(std::move(extensions));
            return { _instance, [this, list]
                {
                    std::vector<char const*> texts;
                    for (auto const& extension : *list)
                        texts.push_back(extension.c_str());
                    return _host.request_file(_host.context, texts.empty() ? nullptr : texts.data(), static_cast<std::uint32_t>(texts.size()));
                },
                [](lop_event const& event) { return std::string{ event.file_path ? event.file_path : "" }; } };
        }

        // Shows the panel: if the panel is not in the layout, lop docks it at its initial side; then lop activates it. For a panel with
        // Panel::owner_command_id, only the instance that owns the panel can call show_panel; any other caller receives UiFailed.
        void show_panel(std::string_view const panel_id) const
        {
            std::string const id{ panel_id };
            if (LOP_OK != _host.show_panel(_host.context, id.c_str()))
                throw UiFailed{ "show_panel returned LOP_FAILED" };
        }

        // Asks lop to run the command with the ID command_id after the current on_event call returns (see lop_command_host::run_command). Returns false
        // if lop has not queued the command; the Output panel contains the reason.
        bool run_command(std::string_view const command_id) const
        {
            std::string const id{ command_id };
            return LOP_OK == _host.run_command(_host.context, id.c_str());
        }

        // Marks the tab of the panel panel_id, declared by this extension, so that the user sees that the panel has new content, without
        // opening or activating the panel (see lop_command_host::mark_panel_for_attention). Does nothing visible if the panel is closed or its
        // content is on screen. Throws UiFailed on LOP_FAILED.
        void mark_panel_for_attention(std::string_view const panel_id) const
        {
            std::string const id{ panel_id };
            if (LOP_OK != _host.mark_panel_for_attention(_host.context, id.c_str()))
                throw UiFailed{ "mark_panel_for_attention returned LOP_FAILED" };
        }

        // Assigns a double, std::int32_t, bool or std::string value to Values[key] of the panel or dialog; while the dialog is open, to the copy
        // that the user edits. Throws std::invalid_argument for other values.
        void set_ui_value(std::string_view const panel_or_dialog_id, std::string_view const key, UiValue const& value) const
        {
            std::string const id{ panel_or_dialog_id };
            std::string const value_key{ key };
            auto const converted = detail::to_c_ui_value(value);
            if (LOP_OK != _host.set_ui_value(_host.context, id.c_str(), value_key.c_str(), &converted))
                throw UiFailed{ "set_ui_value returned LOP_FAILED" };
        }

        // Returns Values[key] of the panel or dialog; while the dialog is open, the value of the copy that the user edits.
        [[nodiscard]] UiValue get_ui_value(std::string_view const panel_or_dialog_id, std::string_view const key) const
        {
            std::string const id{ panel_or_dialog_id };
            std::string const value_key{ key };
            lop_ui_value value{};
            if (LOP_OK != _host.get_ui_value(_host.context, id.c_str(), value_key.c_str(), &value))
                throw UiFailed{ "get_ui_value returned LOP_FAILED" };
            return detail::from_c_ui_value(value);
        }

        // Replaces the rows of Lists[list_key] of the panel or dialog; the XAML reads each field with {Binding [name]}.
        void set_ui_rows(std::string_view const panel_or_dialog_id, std::string_view const list_key, std::span<UiRow const> const rows) const
        {
            if (LOP_OK != detail::call_set_ui_rows(_host.set_ui_rows, _host.context, std::string{ panel_or_dialog_id }, std::string{ list_key }, rows))
                throw UiFailed{ "set_ui_rows returned LOP_FAILED" };
        }

        // Shows the dialog bound to a copy of its values; returns true if the user presses the primary button, after lop copies the edited
        // values to Values of the dialog, false if the user closes it, or std::nullopt if the command finishes before the dialog request completes.
        [[nodiscard]] detail::RequestAwaiter<bool> show_dialog(std::string_view const dialog_id) const
        {
            auto const id = std::make_shared<std::string>(dialog_id);
            return { _instance, [this, id] { return _host.show_dialog(_host.context, id->c_str()); },
                [](lop_event const& event) { return 0 != event.accepted; } };
        }

        // Returns the declared parameter value by ID; throws std::out_of_range if the ID is not declared and std::invalid_argument for a
        // LOP_PARAMETER_FILE_PATH or LOP_PARAMETER_TEXT parameter, whose value parameter_file_path or parameter_text returns.
        [[nodiscard]] double parameter_value(std::string_view const id) const
        {
            auto const& command = _instance.command;
            for (std::uint32_t command_parameter_index = 0; command_parameter_index < command.parameter_count; ++command_parameter_index)
                if (id == command.parameters[command_parameter_index].id)
                {
                    if (LOP_PARAMETER_FILE_PATH == command.parameters[command_parameter_index].type)
                        throw std::invalid_argument{ "the parameter " + std::string{ id } + " is a file path: parameter_file_path returns it" };
                    if (LOP_PARAMETER_TEXT == command.parameters[command_parameter_index].type)
                        throw std::invalid_argument{ "the parameter " + std::string{ id } + " is a text: parameter_text returns it" };
                    return _instance.parameters[command_parameter_index];
                }
            throw std::out_of_range{ "the command does not declare the parameter " + std::string{ id } };
        }

        // Returns the path that the configuration page held for the LOP_PARAMETER_FILE_PATH parameter id when lop created the command, or an
        // empty string if the user has not chosen a file. Throws std::out_of_range if the ID is not declared and std::invalid_argument if the
        // parameter is not a LOP_PARAMETER_FILE_PATH parameter.
        [[nodiscard]] std::string parameter_file_path(std::string_view const id) const
        {
            auto const& command = _instance.command;
            for (std::uint32_t command_parameter_index = 0; command_parameter_index < command.parameter_count; ++command_parameter_index)
                if (id == command.parameters[command_parameter_index].id)
                {
                    if (LOP_PARAMETER_FILE_PATH != command.parameters[command_parameter_index].type)
                        throw std::invalid_argument{ "the parameter " + std::string{ id } + " is not a LOP_PARAMETER_FILE_PATH parameter" };
                    auto const* const file_path = _host.parameter_file_path(_host.context, command.parameters[command_parameter_index].id);
                    return file_path ? file_path : "";
                }
            throw std::out_of_range{ "the command does not declare the parameter " + std::string{ id } };
        }

        // Returns the text that the configuration page held for the LOP_PARAMETER_TEXT parameter id when lop created the command. Throws
        // std::out_of_range if the ID is not declared and std::invalid_argument if the parameter is not a LOP_PARAMETER_TEXT parameter.
        [[nodiscard]] std::string parameter_text(std::string_view const id) const
        {
            auto const& command = _instance.command;
            for (std::uint32_t command_parameter_index = 0; command_parameter_index < command.parameter_count; ++command_parameter_index)
                if (id == command.parameters[command_parameter_index].id)
                {
                    if (LOP_PARAMETER_TEXT != command.parameters[command_parameter_index].type)
                        throw std::invalid_argument{ "the parameter " + std::string{ id } + " is not a LOP_PARAMETER_TEXT parameter" };
                    auto const* const text = _host.parameter_text(_host.context, command.parameters[command_parameter_index].id);
                    return text ? text : "";
                }
            throw std::out_of_range{ "the command does not declare the parameter " + std::string{ id } };
        }

        // Returns the folder, in UTF-8, where this extension keeps the files that do not belong to a cloud, after lop creates it; std::nullopt
        // after a failure reported in the Output panel.
        [[nodiscard]] std::optional<std::string> get_extension_data_directory_path() const
        {
            char const* directory_path{};
            if (LOP_OK != _host.get_extension_data_directory_path(_host.context, &directory_path) || !directory_path)
                return std::nullopt;
            return std::string{ directory_path };
        }

        // Starts an animation that moves the camera, with its current view direction, to distance_cloud_units cloud units from point, in
        // world coordinates, and returns before the animation ends. Returns false if lop rejects the call, with the reason of a host
        // failure in the Output panel. Throws std::invalid_argument for a nonfinite coordinate or a distance that is not a finite number
        // greater than 0.
        bool bring_camera(Point3 const& point, double const distance_cloud_units) const
        {
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z) || !std::isfinite(distance_cloud_units)
                || distance_cloud_units <= 0.0)
                throw std::invalid_argument{ "bring_camera: the coordinates must be finite and the distance finite and greater than 0" };
            double const xyz[]{ point.x, point.y, point.z };
            return LOP_OK == _host.bring_camera(_host.context, xyz, distance_cloud_units);
        }

        // Returns a request awaiter. Awaiting it shows the save file picker and yields the selected path, or std::nullopt if the user
        // closes the picker or the command finishes first. The picker uses suggested_file_name, a file name without folder,
        // and extension, with its period (".xlsx"), as its only file type, named file_type_name in the language of the
        // extension ("Libro de Excel"); an empty file_type_name names the type with the extension without its period and in uppercase
        // ("XLSX"). Throws std::invalid_argument for the arguments that lop_command_host::request_save_file_path rejects.
        [[nodiscard]] detail::RequestAwaiter<std::string> ask_save_file_path(std::string_view const suggested_file_name,
            std::string_view const file_type_name, std::string_view const extension) const
        {
            if (!detail::valid_suggested_file_name(suggested_file_name) || !detail::valid_file_type_name(file_type_name)
                || !detail::valid_file_extension(extension))
                throw std::invalid_argument{ "ask_save_file_path: the suggested file name contains a character that a file name does not "
                    "admit, the file type name has control characters or more than 128 bytes, or the extension is not a period followed by "
                    "characters other than ';', '*', '?' and white space" };
            auto const texts = std::make_shared<std::array<std::string, 3>>(
                std::array<std::string, 3>{ std::string{ suggested_file_name }, std::string{ file_type_name }, std::string{ extension } });
            return { _instance,
                [this, texts]
                {
                    return _host.request_save_file_path(_host.context, (*texts)[0].c_str(), (*texts)[1].c_str(), (*texts)[2].c_str());
                },
                [](lop_event const& event) { return std::string{ event.file_path ? event.file_path : "" }; } };
        }

        // Returns a request awaiter. Awaiting it asks lop to replace the transformation of the open local cloud cloud_id with
        // original_to_world_transform and, if crs_wkt is not empty, its coordinate reference system (see
        // lop_command_host::request_original_to_world_transform_change). Yields true if the user confirms and lop has written the
        // .transform.json file, false if the user closes the confirmation dialog, or std::nullopt if the command finishes first. After
        // true, the command finishes; lop reopens the document when the level of detail of the new transformation exists, and then every
        // cloud receives a new ID. Throws std::invalid_argument if the matrix is not a similarity with the rule of
        // lop_command_host::request_original_to_world_transform_change.
        [[nodiscard]] detail::RequestAwaiter<bool> ask_original_to_world_transform_change(std::uint32_t const cloud_id,
            std::array<double, 16> const& original_to_world_transform, std::string_view const crs_wkt = {}) const
        {
            if (!detail::is_similarity(original_to_world_transform))
                throw std::invalid_argument{ "ask_original_to_world_transform_change: the matrix is not a similarity" };
            auto const matrix = std::make_shared<std::array<double, 16>>(original_to_world_transform);
            auto const wkt = std::make_shared<std::string>(crs_wkt);
            return { _instance,
                [this, cloud_id, matrix, wkt]
                {
                    return _host.request_original_to_world_transform_change(_host.context, cloud_id, matrix->data(), wkt->c_str());
                },
                [](lop_event const& event) { return 0 != event.accepted; } };
        }

        // Returns a request awaiter. Awaiting it shows the coordinate reference system picker of lop with prompt as its subtitle and the
        // coordinate reference system represented by initial_crs_wkt preselected, and yields the Well-Known Text (WKT) representation of the
        // coordinate reference system that the user chooses, or std::nullopt if the user closes the picker or the command finishes first.
        [[nodiscard]] detail::RequestAwaiter<std::string> ask_crs(std::string_view const prompt, std::string_view const initial_crs_wkt = {}) const
        {
            auto const texts = std::make_shared<std::array<std::string, 2>>(
                std::array<std::string, 2>{ std::string{ prompt }, std::string{ initial_crs_wkt } });
            return { _instance, [this, texts] { return _host.request_crs(_host.context, (*texts)[0].c_str(), (*texts)[1].c_str()); },
                [](lop_event const& event) { return std::string{ event.crs_wkt ? event.crs_wkt : "" }; } };
        }

        // Returns the folder of lop_task_host::get_cloud_extension_data_directory_path for the open cloud cloud_id after lop creates it;
        // std::nullopt after a failure reported in the Output panel.
        [[nodiscard]] std::optional<std::string> get_cloud_extension_data_directory_path(std::uint32_t const cloud_id) const
        {
            char const* directory_path{};
            if (LOP_OK != _host.get_cloud_extension_data_directory_path(_host.context, cloud_id, &directory_path) || !directory_path)
                return std::nullopt;
            return std::string{ directory_path };
        }

        // Replaces the temporary geometries of the command; an empty list removes them. lop draws them over the clouds and removes them
        // when the command finishes; they do not change the polyline of show_temporary_line. Throws std::invalid_argument, without calling
        // lop, for the geometries that lop_command_host::show_temporary_geometries rejects and for a text with null characters.
        void show_temporary_geometries(std::span<TemporaryGeometry const> const geometries) const
        {
            if (!detail::valid_temporary_geometries(geometries))
                throw std::invalid_argument{ "show_temporary_geometries: a geometry has an unknown type, a wrong number of vertices, a "
                    "nonfinite coordinate, a nonzero colour with zero opacity, a nonfinite or nonpositive "
                    "cross_arm_length_or_text_line_height_pixels for LOP_TEMPORARY_CROSS or LOP_TEMPORARY_TEXT, an empty text or a text with "
                    "null characters, or there are more than LOP_MAX_TEMPORARY_GEOMETRIES geometries or LOP_MAX_TEMPORARY_GEOMETRY_VERTICES "
                    "vertices" };
            std::vector<double> xyz;
            for (auto const& geometry : geometries)
                for (auto const& vertex : geometry.vertices)
                    xyz.insert(xyz.end(), { vertex.x, vertex.y, vertex.z });
            std::vector<lop_temporary_geometry> command_host_geometries;
            command_host_geometries.reserve(geometries.size());
            std::size_t first_coordinate_index{};
            for (auto const& geometry : geometries)
            {
                command_host_geometries.push_back({ static_cast<std::uint32_t>(geometry.type), xyz.data() + first_coordinate_index,
                    static_cast<std::uint32_t>(geometry.vertices.size()), geometry.closed ? 1 : 0, geometry.color_argb,
                    geometry.cross_arm_length_or_text_line_height_pixels, geometry.text.c_str() });
                first_coordinate_index += 3 * geometry.vertices.size();
            }
            _host.show_temporary_geometries(_host.context, command_host_geometries.empty() ? nullptr : command_host_geometries.data(),
                static_cast<std::uint32_t>(command_host_geometries.size()));
        }

        void show_temporary_geometries(std::initializer_list<TemporaryGeometry> const geometries) const
        {
            show_temporary_geometries(std::span<TemporaryGeometry const>{ geometries.begin(), geometries.size() });
        }

        // Cancels the pending ask_point, ask_cloud_point or ask_geometry request of the command without finishing the command: the coroutine
        // that awaits it resumes with std::nullopt after the current on_event call returns; another point, cloud-point or geometry request can
        // start in the same on_event call. Returns true if a request was pending.
        bool cancel_point_or_geometry_request() const { return 0 != _host.cancel_point_or_geometry_request(_host.context); }

        // SDK 0.8. Returns the pose that lop last sent to the viewer, with the current vertical field of view, or std::nullopt when no
        // document is open.
        [[nodiscard]] std::optional<CameraPose> get_camera_pose() const
        {
            lop_camera_pose pose{};
            if (LOP_OK != _host.get_camera_pose(_host.context, &pose))
                return std::nullopt;
            return detail::from_c_camera_pose(pose);
        }

        // SDK 0.8. Places the camera at pose without animation (see lop_command_host::set_camera_pose). Returns false if lop rejects the call
        // because no document is open or the command has finished. Throws std::invalid_argument, without calling lop, for a nonfinite value,
        // a look_direction shorter than 1e-9, an up whose component orthogonal to look_direction is zero or shorter than 1e-9 times the length
        // of up, or a field of view outside (0, 180) degrees.
        bool set_camera_pose(CameraPose const& pose) const
        {
            if (!detail::valid_camera_pose(pose))
                throw std::invalid_argument{ "set_camera_pose: the values must be finite, look_direction at least 1e-9 long, up with a nonzero "
                    "component orthogonal to look_direction of at least 1e-9 times the length of up, and the field of view greater than 0 and "
                    "less than 180 degrees" };
            auto const converted = detail::to_c_camera_pose(pose);
            return LOP_OK == _host.set_camera_pose(_host.context, &converted);
        }

        // SDK 0.8. Returns a request awaiter. Awaiting it animates the camera to pose and yields true when the camera has pose, or std::nullopt
        // if another movement interrupts the animation or the command finishes first. Throws std::invalid_argument like set_camera_pose.
        [[nodiscard]] detail::RequestAwaiter<bool> animate_camera(CameraPose const& pose) const
        {
            if (!detail::valid_camera_pose(pose))
                throw std::invalid_argument{ "animate_camera: the values must be finite, look_direction at least 1e-9 long, up with a nonzero "
                    "component orthogonal to look_direction of at least 1e-9 times the length of up, and the field of view greater than 0 and "
                    "less than 180 degrees" };
            auto const converted = detail::to_c_camera_pose(pose);
            return { _instance, [this, converted] { return _host.request_camera_animation(_host.context, &converted); },
                [](lop_event const&) { return true; } };
        }

        // SDK 0.8. Assigns the mode of Command::on_view_frame; ViewFrameNotices::none stops the notices. Throws std::runtime_error if lop
        // rejects the call, for example after finish.
        void set_view_frame_notices(ViewFrameNotices const mode) const
        {
            if (LOP_OK != _host.set_view_frame_notices(_host.context, static_cast<std::uint32_t>(mode)))
                throw std::runtime_error{ "set_view_frame_notices returned LOP_FAILED" };
        }

        // SDK 0.8. Shows the banner of this command (see lop_command_host::show_banner); std::nullopt as progress shows no bar, and a press of a
        // button calls Command::on_banner_action with its action_name. Throws std::invalid_argument, without calling lop, for the values that
        // lop_command_host::show_banner rejects and for null characters in the texts, and UiFailed if lop returns LOP_FAILED, for example after finish.
        void show_banner(std::string_view const message, std::optional<double> const progress, std::span<BannerButton const> const buttons) const
        {
            auto const progress_for_host = progress.value_or(-1.0);
            if (!detail::valid_banner(message, progress_for_host, buttons))
                throw std::invalid_argument{ "show_banner: the message has more than LOP_MAX_BANNER_MESSAGE_BYTES bytes, the progress is NaN or "
                    "greater than 1, there are more than LOP_MAX_BANNER_BUTTONS buttons, a button text is empty or has more than "
                    "LOP_MAX_BANNER_BUTTON_TEXT_BYTES bytes, an action name is empty or has characters other than ASCII letters, digits, '.', '_' "
                    "and '-', or a text has null characters" };
            std::string const text{ message };
            std::vector<lop_banner_button> command_host_buttons;
            command_host_buttons.reserve(buttons.size());
            for (auto const& button : buttons)
                command_host_buttons.push_back({ button.text.c_str(), button.action_name.c_str() });
            if (LOP_OK != _host.show_banner(_host.context, text.c_str(), progress_for_host,
                command_host_buttons.empty() ? nullptr : command_host_buttons.data(), static_cast<std::uint32_t>(command_host_buttons.size())))
                throw UiFailed{ "show_banner returned LOP_FAILED" };
        }

        void show_banner(std::string_view const message, std::optional<double> const progress, std::initializer_list<BannerButton> const buttons) const
        {
            show_banner(message, progress, std::span<BannerButton const>{ buttons.begin(), buttons.size() });
        }

        // SDK 0.8. Removes the visible banner of this command and discards any banner content stored for this command.
        void hide_banner() const { _host.hide_banner(_host.context); }

        // SDK 0.8. With kept == true, a command launched with cancellation of previous commands leaves this command in the command stack, below
        // the new command. With kept == false (the initial value), such a launch finishes this command unless it owns its panel (see
        // lop_command_host::set_kept_below_new_commands). Throws std::runtime_error after finish.
        void set_kept_below_new_commands(bool const kept) const
        {
            if (LOP_OK != _host.set_kept_below_new_commands(_host.context, kept ? 1 : 0))
                throw std::runtime_error{ "set_kept_below_new_commands returned LOP_FAILED" };
        }

    private:
        detail::Instance& _instance;
        lop_command_host const& _host;
    };

    // Provides the functions of a cloud notice until the notice function returns; every function requires the user interface thread.
    // After Extension::on_cloud_event returns, a kept copy behaves as follows: log does nothing, run_in_background throws std::runtime_error,
    // get_cloud_extension_data_directory_path, get_extension_data_directory_path and find_cloud_extension_data_directory_path return
    // std::nullopt, load_geopackage and close_geopackage return false, and set_ui_value, set_ui_rows and mark_panel_for_attention throw UiFailed.
    class ExtensionHost
    {
    public:
        explicit ExtensionHost(lop_extension_host const& host) noexcept : _host{ host } {}

        void log(std::string_view const message) const
        {
            std::string const text{ message };
            _host.log(_host.context, text.c_str());
        }

        // Shows the task name in the Background tasks panel and runs work on a thread of lop with a TaskHost, like Host::run_in_background;
        // lop keeps the DLL loaded until work returns. Throws std::runtime_error if lop does not start the task, for example after
        // Extension::on_cloud_event returns.
        void run_in_background(std::string_view const name, BackgroundWork work) const
        {
            std::string const text{ name };
            auto heap = std::make_unique<BackgroundWork>(std::move(work));
            if (LOP_OK != _host.start_background_task(_host.context, text.c_str(), &detail::run_background_work, heap.get()))
                throw std::runtime_error{ "run_in_background: lop has not started the task " + text };
            (void)heap.release();
        }

        // Returns the folder, in UTF-8, where this extension keeps its files for the open cloud cloud_id, after lop creates it; std::nullopt
        // after a failure reported in the Output panel or after Extension::on_cloud_event returns.
        [[nodiscard]] std::optional<std::string> get_cloud_extension_data_directory_path(std::uint32_t const cloud_id) const
        {
            char const* directory_path{};
            if (LOP_OK != _host.get_cloud_extension_data_directory_path(_host.context, cloud_id, &directory_path) || !directory_path)
                return std::nullopt;
            return std::string{ directory_path };
        }

        // Starts reading the GeoPackage tables as layers on a thread of lop and returns true if the read starts, is already in progress or the
        // file is already loaded; the result does not indicate that the layers are already in the Layers panel. lop writes a read failure in
        // the Output panel. Returns false for an empty path or after Extension::on_cloud_event returns.
        bool load_geopackage(std::string_view const geopackage_file_path) const
        {
            std::string const file_path{ geopackage_file_path };
            return LOP_OK == _host.load_geopackage(_host.context, file_path.c_str());
        }

        // Removes the layers of the GeoPackage from the Layers panel and from the view (see TaskHost::close_geopackage); returns false after a
        // failure reported in the Output panel or after Extension::on_cloud_event returns.
        bool close_geopackage(std::string_view const geopackage_file_path) const
        {
            std::string const file_path{ geopackage_file_path };
            return LOP_OK == _host.close_geopackage(_host.context, file_path.c_str());
        }

        // Returns the folder, in UTF-8, where this extension keeps the files that do not belong to a cloud, after lop creates it; std::nullopt
        // after a failure reported in the Output panel or after Extension::on_cloud_event returns.
        [[nodiscard]] std::optional<std::string> get_extension_data_directory_path() const
        {
            char const* directory_path{};
            if (LOP_OK != _host.get_extension_data_directory_path(_host.context, &directory_path) || !directory_path)
                return std::nullopt;
            return std::string{ directory_path };
        }

        // Returns the folder of get_cloud_extension_data_directory_path for the open cloud cloud_id if it exists, without creating it;
        // std::nullopt if it does not exist, after a failure reported in the Output panel or after Extension::on_cloud_event returns.
        [[nodiscard]] std::optional<std::string> find_cloud_extension_data_directory_path(std::uint32_t const cloud_id) const
        {
            char const* directory_path{};
            if (LOP_OK != _host.find_cloud_extension_data_directory_path(_host.context, cloud_id, &directory_path) || !directory_path)
                return std::nullopt;
            return std::string{ directory_path };
        }

        // Assigns value to Values[key] of the panel or dialog (see Host::set_ui_value); throws UiFailed on LOP_FAILED.
        void set_ui_value(std::string_view const panel_or_dialog_id, std::string_view const key, UiValue const& value) const
        {
            std::string const id{ panel_or_dialog_id };
            std::string const value_key{ key };
            auto const value_for_host = detail::to_c_ui_value(value);
            if (LOP_OK != _host.set_ui_value(_host.context, id.c_str(), value_key.c_str(), &value_for_host))
                throw UiFailed{ "set_ui_value returned LOP_FAILED" };
        }

        // Replaces the rows of Lists[list_key] of the panel or dialog (see Host::set_ui_rows); throws UiFailed on LOP_FAILED.
        void set_ui_rows(std::string_view const panel_or_dialog_id, std::string_view const list_key, std::span<UiRow const> const rows) const
        {
            if (LOP_OK != detail::call_set_ui_rows(_host.set_ui_rows, _host.context, std::string{ panel_or_dialog_id }, std::string{ list_key }, rows))
                throw UiFailed{ "set_ui_rows returned LOP_FAILED" };
        }

        // Marks the tab of the panel panel_id, declared by this extension (see Host::mark_panel_for_attention); throws UiFailed on LOP_FAILED.
        void mark_panel_for_attention(std::string_view const panel_id) const
        {
            std::string const id{ panel_id };
            if (LOP_OK != _host.mark_panel_for_attention(_host.context, id.c_str()))
                throw UiFailed{ "mark_panel_for_attention returned LOP_FAILED" };
        }

    private:
        lop_extension_host _host;
    };

    // Base class of the commands. lop calls the event functions on the user interface thread; the functions that return bool return true
    // to consume the event. host() is available from on_start until the destruction of the command.
    class Command
    {
    public:
        virtual ~Command() = default;

        // Starts once, when lop pushes the command; it can co_await requests and keep running while the other events arrive.
        // An uncaught exception is logged and finishes the command.
        virtual Task<> on_start() { co_return; }
        virtual void on_set_focus() {}
        virtual void on_lost_focus() {}
        virtual bool on_mouse_button_down(MouseButton, ScreenPoint, bool /*double_click*/) { return false; }
        virtual bool on_mouse_button_up(MouseButton, ScreenPoint) { return false; }
        virtual bool on_mouse_move(ScreenPoint, bool /*is_any_mouse_button_pressed*/) { return false; }
        // delta uses Windows units: 120 per notch, positive away from the user.
        virtual bool on_mouse_wheel(int /*delta*/, ScreenPoint, bool /*control*/, bool /*shift*/) { return false; }
        // If the command does not consume Esc, lop finishes it.
        virtual bool on_key_down(Key) { return false; }
        // Called when the user activates a control bound to Actions[action] of the panel of this command (lop_command_panel); a coroutine
        // that can co_await requests while lop delivers other events, including other panel events (see Host::cancel_point_or_geometry_request).
        // An uncaught exception is logged in the Output panel and shown in the status bar; the command keeps running.
        virtual Task<> on_panel_action(std::string /*panel_id*/, std::string /*action*/) { co_return; }
        // Called when the user changes Values[key] of the panel of this command through a binding of the XAML. The coroutine can await
        // requests while lop delivers other events; uncaught exceptions are logged in the Output panel and shown in the status bar without
        // finishing the command.
        virtual Task<> on_panel_value_changed(std::string /*panel_id*/, std::string /*key*/, UiValue /*value*/) { co_return; }
        // SDK 0.8. Called for each LOP_EVENT_VIEW_FRAME (see Host::set_view_frame_notices). An uncaught exception is logged and finishes the
        // command, like the other event handlers that are not coroutines.
        virtual void on_view_frame(ViewFrame const& /*frame*/) {}
        // SDK 0.8. Called when the user presses a button of the banner of this command (see Host::show_banner); a coroutine like
        // on_panel_action. An uncaught exception is logged in the Output panel and shown in the status bar; the command keeps running.
        virtual Task<> on_banner_action(std::string /*action*/) { co_return; }
        // The last event: lop has removed the command from its stack and has completed its pending requests as cancelled.
        virtual void on_finish() {}

    protected:
        [[nodiscard]] Host& host() const
        {
            if (!_host)
                throw std::logic_error{ "lop::Command::host: the host is not available in the constructor" };
            return *_host;
        }

        void finish() const { host().finish(); }

    private:
        friend struct detail::CommandInstance;
        Host* _host{};
    };

    struct Parameter
    {
        std::string id;
        std::string name;
        std::string description;
        lop_parameter_type type{ LOP_PARAMETER_REAL };
        double default_value{};
        double minimum{};
        double maximum{};
        std::string unit;
        std::string settings_group;
        std::vector<std::string> file_extensions;   // LOP_PARAMETER_FILE_PATH: the extensions of the file picker, each with its period (".dxf").
        std::string default_text;   // LOP_PARAMETER_TEXT: the text until the user changes it; at most LOP_MAX_PARAMETER_TEXT_BYTES bytes.
    };

    // Describes a command; declare supplies the factory in create.
    struct Declaration
    {
        std::string id;                 // lop_command::id; F1 opens the help page with this ID (see the help page note above lop_command).
        std::string name;
        std::string existing_tab_id;    // At most one of existing_tab_id and new_tab_title contains text; with neither, the command has no button.
        std::string new_tab_title;
        std::string ribbon_section;
        std::string description;
        std::vector<unsigned char> icon_png;
        std::vector<Parameter> parameters;
        std::uint32_t requirements{};
        std::string module;                 // Identifier of the licensed module required by the command (see lop_command::module); an empty value means the command requires no module license.
        std::function<std::unique_ptr<Command>()> create;
    };

    // Describes a dockable panel (see lop_panel); in the XAML, Commands[id] launches the command whose Declaration::id is id and Actions[name]
    // delivers an action to the command that owns the panel.
    struct Panel
    {
        std::string id;
        std::string title;
        std::string xaml;
        lop_panel_dock_side initial_dock_side{ LOP_PANEL_DOCK_RIGHT };
        std::string module;
        // The Declaration::id of the command that owns the panel (lop_command_panel); empty for a panel that any command shows with show_panel.
        std::string owner_command_id;
    };

    // Describes a dialog (see lop_dialog).
    struct Dialog
    {
        std::string id;
        std::string title;
        std::string xaml;
        std::string primary_button_text;
        std::string close_button_text;
        std::string module;
    };

    // Contains what the DLL declares; LOP_SDK_EXTENSION also accepts a function that returns only the commands.
    struct Extension
    {
        std::vector<Declaration> commands;
        std::vector<Panel> panels;
        std::vector<Dialog> dialogs;
        // Receives the cloud notices on the user interface thread; empty if the extension receives none. An exception goes to the Output panel.
        std::function<void(CloudEvent event, Cloud const& cloud, ExtensionHost const& host)> on_cloud_event;
    };

    // Captures copies of arguments and constructs a T instance each time the user launches the command.
    template<typename T, typename... Arguments>
    [[nodiscard]] Declaration declare(Declaration declaration, Arguments... arguments)
    {
        declaration.create = [arguments...] { return std::make_unique<T>(arguments...); };
        return declaration;
    }

    namespace detail
    {
        // Owns the command object of an instance and dispatches the events of lop to it.
        struct CommandInstance : Instance
        {
            CommandInstance(lop_command const& command, lop_command_host const& host, std::vector<double> parameters)
                : Instance{ command, host, std::move(parameters), {}, {} }, wrapper{ *this }
            {
            }

            Host wrapper;
            std::unique_ptr<Command> object;
            // Stores the coroutines returned by on_panel_action, on_panel_value_changed and on_banner_action until remove_finished_event_tasks
            // removes completed coroutines or destroy clears the list.
            std::list<Task<>> event_tasks;

            void adopt(std::unique_ptr<Command> command_to_adopt)
            {
                object = std::move(command_to_adopt);
                object->_host = &wrapper;
            }

            // Writes the message of exception in the Output panel and in the status bar; message_prefix begins the message for an exception
            // that does not derive from std::exception. Discards the message if its text cannot be allocated.
            void log_and_show_status(std::exception_ptr const& exception, std::string_view const message_prefix) const noexcept
            {
                std::string message;
                try
                {
                    try
                    {
                        std::rethrow_exception(exception);
                    }
                    catch (std::exception const& caught_exception)
                    {
                        message = caught_exception.what();
                    }
                    catch (...)
                    {
                        message = std::string{ message_prefix } +" has thrown an exception that is not a std::exception";
                    }
                }
                catch (...)
                {
                    return;
                }
                log(message);
                host.status(host.context, message.c_str());
            }

            // Starts the coroutine and removes it at once if it finishes without suspending.
            void start_event_task(Task<> task)
            {
                auto& event_task = event_tasks.emplace_back(std::move(task));
                if (event_task._handle)
                    event_task._handle.resume();
                remove_finished_event_tasks();
            }

            // Logs the exception of each finished coroutine in the Output panel and the status bar; the command keeps running.
            void remove_finished_event_tasks() noexcept
            {
                for (auto task = event_tasks.begin(); task != event_tasks.end();)
                {
                    if (task->_handle && !task->_handle.done())
                    {
                        ++task;
                        continue;
                    }
                    if (task->_handle)
                        if (auto const exception = task->_handle.promise().exception)
                            log_and_show_status(exception, "a panel or banner event handler");
                    task = event_tasks.erase(task);
                }
            }

            // Logs the exception of a finished on_start and finishes the command.
            void handle_on_start_completion() noexcept
            {
                if (!on_start_task || !on_start_task->_handle || !on_start_task->_handle.done())
                    return;
                auto const exception = on_start_task->_handle.promise().exception;
                on_start_task.reset();
                if (!exception)
                    return;
                try
                {
                    std::rethrow_exception(exception);
                }
                catch (std::exception const& e)
                {
                    log(e.what());
                }
                catch (...)
                {
                    log("on_start has thrown an exception that is not a std::exception");
                }
                host.finish(host.context);
            }

            int dispatch(lop_event const& event)
            {
                ScreenPoint const position{ event.x, event.y };
                switch (event.type)
                {
                case LOP_EVENT_START:
                    on_start_task.emplace(object->on_start());
                    if (on_start_task->_handle)
                        on_start_task->_handle.resume();
                    handle_on_start_completion();
                    return 1;
                case LOP_EVENT_SET_FOCUS:
                    object->on_set_focus();
                    return 1;
                case LOP_EVENT_LOST_FOCUS:
                    object->on_lost_focus();
                    return 1;
                case LOP_EVENT_MOUSE_BUTTON_DOWN:
                    return object->on_mouse_button_down(static_cast<MouseButton>(event.mouse_button), position, 0 != event.double_click) ? 1 : 0;
                case LOP_EVENT_MOUSE_BUTTON_UP:
                    return object->on_mouse_button_up(static_cast<MouseButton>(event.mouse_button), position) ? 1 : 0;
                case LOP_EVENT_MOUSE_MOVE:
                    return object->on_mouse_move(position, 0 != event.is_any_mouse_button_pressed) ? 1 : 0;
                case LOP_EVENT_MOUSE_WHEEL:
                    return object->on_mouse_wheel(event.wheel_delta, position, 0 != event.control, 0 != event.shift) ? 1 : 0;
                case LOP_EVENT_KEY_DOWN:
                    return object->on_key_down(static_cast<Key>(event.key)) ? 1 : 0;
                case LOP_EVENT_REQUEST_PREVIEW:
                    if (auto const request = pending.find(event.request_id); request != pending.end())
                        request->second->preview(event);
                    return 1;
                case LOP_EVENT_REQUEST_COMPLETED:
                    if (auto const request = pending.find(event.request_id); request != pending.end())
                    {
                        auto* const awaiter = request->second;
                        pending.erase(request);
                        awaiter->complete(event);
                        handle_on_start_completion();
                        remove_finished_event_tasks();
                    }
                    else if (LOP_REQUEST_POINTS_IN_SPHERE == event.request_type && LOP_OK == event.result)
                    {
                        // Nobody awaits these points any longer.
                        auto points = event.points;
                        host.release_points(host.context, &points);
                    }
                    return 1;
                case LOP_EVENT_FINISH:
                    object->on_finish();
                    return 1;
                // A lop that delivers a lop_event without the panel fields does not deliver these types; the check protects the reads.
                case LOP_EVENT_PANEL_ACTION:
                    if (event.size_bytes < offsetof(lop_event, panel_action_name) + sizeof(event.panel_action_name))
                        return 0;
                    start_event_task(object->on_panel_action(event.panel_id ? event.panel_id : "",
                        event.panel_action_name ? event.panel_action_name : ""));
                    return 1;
                case LOP_EVENT_PANEL_VALUE_CHANGED:
                    if (event.size_bytes < offsetof(lop_event, panel_value) + sizeof(event.panel_value))
                        return 0;
                    start_event_task(object->on_panel_value_changed(event.panel_id ? event.panel_id : "",
                        event.panel_value_key ? event.panel_value_key : "", from_c_ui_value(event.panel_value)));
                    return 1;
                // A lop that delivers a lop_event without the fields of the SDK 0.8 does not deliver these types; the check protects the reads.
                case LOP_EVENT_VIEW_FRAME:
                    if (event.size_bytes < offsetof(lop_event, camera_pose_set_by_this_command) + sizeof(event.camera_pose_set_by_this_command)
                        || !event.camera_pose)
                        return 0;
                    object->on_view_frame({ from_c_camera_pose(*event.camera_pose), event.frame_time_seconds, 0 != event.camera_changed,
                        0 != event.camera_pose_set_by_this_command });
                    return 1;
                case LOP_EVENT_BANNER_ACTION:
                    if (event.size_bytes < offsetof(lop_event, banner_action_name) + sizeof(event.banner_action_name))
                        return 0;
                    start_event_task(object->on_banner_action(event.banner_action_name ? event.banner_action_name : ""));
                    return 1;
                default:
                    return 0;
                }
            }
        };

        inline void* create(lop_command const* const command, double const* const parameters, lop_command_host const* const host) noexcept
        {
            try
            {
                auto const& declaration = *static_cast<Declaration const*>(command->user_data);
                if (!declaration.create)
                    throw std::logic_error{ "the command " + declaration.id + " is not declared with lop::declare" };
                auto instance = std::make_unique<CommandInstance>(*command, *host,
                    std::vector<double>(parameters, parameters ? parameters + command->parameter_count : parameters));
                instance->adopt(declaration.create());
                return instance.release();
            }
            catch (std::exception const& exception)
            {
                host->log(host->context, exception.what());
            }
            catch (...)
            {
                host->log(host->context, "the command constructor has thrown an exception that is not a std::exception");
            }
            return nullptr;
        }

        inline int on_event(void* const instance, lop_event const* const event) noexcept
        {
            auto& command = *static_cast<CommandInstance*>(instance);
            try
            {
                return command.dispatch(*event);
            }
            catch (std::exception const& exception)
            {
                command.log(exception.what());
            }
            catch (...)
            {
                command.log("the command has thrown an exception that is not a std::exception");
            }
            command.host.finish(command.host.context);
            return 0;
        }

        // Destroys the coroutines of the panel and banner events and on_start, including the suspended ones, before the command object they use.
        inline void destroy(void* const instance) noexcept
        {
            std::unique_ptr<CommandInstance> command{ static_cast<CommandInstance*>(instance) };
            command->event_tasks.clear();
            command->on_start_task.reset();
            command->object.reset();
        }

        // user_data must point to the Extension whose on_cloud_event is not empty; ignores unknown notice types and logs the exceptions.
        inline void on_cloud_event(void* const user_data, std::uint32_t const type, lop_cloud const* const cloud, lop_extension_host const* const host) noexcept
        {
            if (LOP_CLOUD_ADDED != type && LOP_CLOUD_CLOSING != type)
                return;
            try
            {
                static_cast<Extension const*>(user_data)->on_cloud_event(static_cast<CloudEvent>(type), to_cloud(*cloud), ExtensionHost{ *host });
            }
            catch (std::exception const& exception)
            {
                host->log(host->context, exception.what());
            }
            catch (...)
            {
                host->log(host->context, "on_cloud_event has thrown an exception that is not a std::exception");
            }
        }

        // Owns declarations and the C arrays whose pointers remain valid while the DLL is loaded.
        struct Registered
        {
            Extension extension;
            std::vector<std::vector<lop_parameter>> parameters;
            // Pointers to the strings of Parameter::file_extensions, per command and parameter; moving a vector keeps its elements in place.
            std::vector<std::vector<std::vector<char const*>>> file_extensions;
            std::vector<lop_command> commands;
            std::vector<lop_panel> panels;
            std::vector<lop_dialog> dialogs;
            // One element per Panel with a non-empty owner_command_id, in the order of Extension::panels.
            std::vector<lop_command_panel> command_panels;
        };

        [[nodiscard]] inline char const* null_if_empty(std::string const& text) noexcept
        {
            return text.empty() ? nullptr : text.c_str();
        }

        // Retains each registration until DLL unload, including registrations for different languages.
        [[nodiscard]] inline std::list<Registered>& registrations()
        {
            static std::list<Registered> instances;
            return instances;
        }

        template<typename Declare>
        int register_extension(std::uint32_t const api_version, char const* const language, lop_extension* const extension, Declare&& declare) noexcept
        {
            if (LOP_SDK_API_VERSION != api_version || !extension)
                return LOP_FAILED;

            auto& all = registrations();
            auto added = false;
            try
            {
                // Constructs the registration in its final list node because C declarations retain pointers to its contents.
                auto& r = all.emplace_back();
                added = true;
                auto declared = declare(std::string_view{ language ? language : "" });
                if constexpr (std::is_same_v<std::decay_t<decltype(declared)>, Extension>)
                    r.extension = std::move(declared);
                else
                    r.extension.commands = std::move(declared);
                r.parameters.reserve(r.extension.commands.size());
                r.file_extensions.reserve(r.extension.commands.size());
                r.commands.reserve(r.extension.commands.size());
                for (auto& declaration : r.extension.commands)
                {
                    auto& parameters = r.parameters.emplace_back();
                    auto& file_extensions_of_command = r.file_extensions.emplace_back();
                    for (auto const& p : declaration.parameters)
                    {
                        auto& file_extensions = file_extensions_of_command.emplace_back();
                        for (auto const& file_extension : p.file_extensions)
                            file_extensions.push_back(file_extension.c_str());
                        parameters.push_back({ p.id.c_str(), p.name.c_str(), p.description.c_str(), p.type, p.default_value, p.minimum, p.maximum,
                            p.unit.c_str(), p.settings_group.empty() ? nullptr : p.settings_group.c_str(),
                            file_extensions.empty() ? nullptr : file_extensions.data(), static_cast<std::uint32_t>(file_extensions.size()),
                            LOP_PARAMETER_TEXT == p.type ? p.default_text.c_str() : nullptr });
                    }

                    r.commands.push_back({ declaration.id.c_str(), declaration.name.c_str(), declaration.existing_tab_id.c_str(), declaration.new_tab_title.c_str(),
                        declaration.ribbon_section.c_str(),
                        declaration.description.c_str(), declaration.icon_png.empty() ? nullptr : declaration.icon_png.data(),
                        static_cast<std::uint32_t>(declaration.icon_png.size()), parameters.empty() ? nullptr : parameters.data(),
                        static_cast<std::uint32_t>(parameters.size()), &create, &on_event, &destroy, &declaration, declaration.requirements,
                        declaration.module.empty() ? nullptr : declaration.module.c_str() });
                }
                for (auto const& panel : r.extension.panels)
                {
                    r.panels.push_back({ panel.id.c_str(), panel.title.c_str(), panel.xaml.c_str(), static_cast<std::uint32_t>(panel.initial_dock_side), null_if_empty(panel.module) });
                    if (!panel.owner_command_id.empty())
                        r.command_panels.push_back({ panel.owner_command_id.c_str(), panel.id.c_str() });
                }
                for (auto const& dialog : r.extension.dialogs)
                    r.dialogs.push_back({ dialog.id.c_str(), dialog.title.c_str(), dialog.xaml.c_str(), dialog.primary_button_text.c_str(),
                        dialog.close_button_text.c_str(), null_if_empty(dialog.module) });
                // detail::on_cloud_event receives the Extension of this registration as user_data.
                auto const has_cloud_event_function = static_cast<bool>(r.extension.on_cloud_event);
                *extension = { r.commands.data(), static_cast<std::uint32_t>(r.commands.size()), r.panels.empty() ? nullptr : r.panels.data(),
                    static_cast<std::uint32_t>(r.panels.size()), r.dialogs.empty() ? nullptr : r.dialogs.data(), static_cast<std::uint32_t>(r.dialogs.size()),
                    has_cloud_event_function ? &on_cloud_event : nullptr, has_cloud_event_function ? &r.extension : nullptr,
                    r.command_panels.empty() ? nullptr : r.command_panels.data(), static_cast<std::uint32_t>(r.command_panels.size()) };
                return LOP_OK;
            }
            catch (...)
            {
                if (added)
                    all.pop_back();
                return LOP_FAILED;
            }
        }
    }
}

// Exports lop_register_extension using function(std::string_view language), which returns lop::Extension or std::vector<lop::Declaration>.
// language is the lowercase ISO 639-1 code of the lop interface language; lop::language_from_iso_639_1_code converts it to lop::Language
// (english for a code outside ca, de, en, es, eu, fr, gl, it and pt), and lop::Translations::text returns the English entry when the
// entry of that language is null or empty.
#define LOP_SDK_EXTENSION(function)                                                                                         \
    extern "C" LOP_SDK_EXPORT int lop_register_extension(std::uint32_t const api_version, char const* const language,      \
        lop_extension* const extension)                                                                                     \
    {                                                                                                                       \
        return ::lop::detail::register_extension(api_version, language, extension, function);                              \
    }
