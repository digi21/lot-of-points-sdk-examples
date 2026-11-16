/* C application binary interface (ABI) between lop and the plugin dynamic-link libraries (DLLs) that export lop_register_extension.
 * lop calls create, on_event and destroy of each command on the user interface thread. A command answers each event without waiting:
 * user input and data that need time (a clicked point, the points of a sphere, the open clouds, a file) are requests whose answer arrives
 * later as LOP_EVENT_REQUEST_COMPLETED, and long computations run in background tasks (start_background_task), on a thread of lop,
 * with their state, progress and cancellation in the Background tasks panel.
 * The functions of lop_task_host can wait; the functions of lop_command_host never wait. All strings use UTF-8.
 * lop/sdk.hpp wraps this ABI in a C++20 API with coroutines: it is the API for writing commands. */
#ifndef LOP_SDK_H
#define LOP_SDK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Encodes an SDK version as (major << 16) | minor. lop calls lop_register_extension with LOP_SDK_API_VERSION and then
 * with each earlier version that lop explicitly supports, whose structures are a prefix of the current ones, until a call returns LOP_OK
 * or no supported version remains. lop_register_extension should
 * return LOP_FAILED unless api_version equals the LOP_SDK_API_VERSION of the header that the DLL was compiled with. */
#define LOP_SDK_MAKE_API_VERSION(major, minor) ((uint32_t)(((uint32_t)(major) << 16) | (uint32_t)(minor)))
#define LOP_SDK_API_VERSION_MAJOR(version) ((uint32_t)(version) >> 16)
#define LOP_SDK_API_VERSION_MINOR(version) ((uint32_t)(version) & 0xFFFFu)
#define LOP_SDK_API_VERSION LOP_SDK_MAKE_API_VERSION(0, 7)

#ifdef _WIN32
#define LOP_SDK_EXPORT __declspec(dllexport)
#else
#define LOP_SDK_EXPORT __attribute__((visibility("default")))
#endif

#define LOP_OK          0
#define LOP_CANCELLED   1   /* Indicates cancellation: Esc, the finish of the command, or the Cancel button of a background task. */
#define LOP_FAILED      2

/* Parameters are passed as doubles; integer values must be integral and boolean values must be 0 or 1. create receives 0 for a
 * LOP_PARAMETER_FILE_PATH or LOP_PARAMETER_TEXT parameter; the command reads a LOP_PARAMETER_FILE_PATH value with
 * lop_command_host::parameter_file_path and a LOP_PARAMETER_TEXT value with lop_command_host::parameter_text. */
typedef enum lop_parameter_type
{
    LOP_PARAMETER_REAL = 1,
    LOP_PARAMETER_INTEGER = 2,
    LOP_PARAMETER_BOOLEAN = 3,
    LOP_PARAMETER_FILE_PATH = 4,    /* The path of a file chosen with the file picker of the configuration page; lop ignores default_value,
                                     * minimum, maximum and unit. */
    LOP_PARAMETER_TEXT = 5          /* SDK 0.6. One line of UTF-8 text that the user types in a text box of the configuration page, with at
                                     * most LOP_MAX_PARAMETER_TEXT_BYTES bytes and without control characters (U+0000 to U+001F and U+007F);
                                     * lop ignores default_value, minimum, maximum and unit. */
} lop_parameter_type;

/* Maximum length of the text of a LOP_PARAMETER_TEXT parameter, in bytes of UTF-8, without the terminating null. */
#define LOP_MAX_PARAMETER_TEXT_BYTES 1024u

/* Declares a parameter shown on the configuration page for the command's ribbon section. */
typedef struct lop_parameter
{
    char const* id;             /* The parameter uses a stable ASCII ID, for example max_slope. */
    char const* name;           /* The configuration page displays this name. */
    char const* description;
    lop_parameter_type type;
    double default_value;
    double minimum;
    double maximum;
    char const* unit;           /* Specifies the displayed unit, for example m or deg; an empty string omits the unit. */
    char const* settings_group; /* Names the configuration group, or NULL; groups follow the declaration order of their first parameter. */
    char const* const* file_extensions; /* LOP_PARAMETER_FILE_PATH: the extensions that the file picker shows, each with its period and
                                         * without ';', '*', '?' or white space, for example ".dxf"; ignored for the other types. */
    uint32_t file_extension_count;      /* Zero shows every file and permits NULL file_extensions. */
    char const* default_text;           /* SDK 0.6. LOP_PARAMETER_TEXT: the text until the user changes it, with the limits of
                                         * LOP_PARAMETER_TEXT; NULL is the empty text. Ignored for the other types. */
} lop_parameter;

/* Marks an unavailable original point index, including clouds without stored indices or nodes whose index attributes cannot be read. */
#define LOP_UNKNOWN_ORIGINAL_POINT_INDEX UINT64_MAX

/* Stores a cloud point with world coordinates in the cloud's coordinate system. */
typedef struct lop_point
{
    double x;
    double y;
    double z;
    uint64_t original_point_index;  /* Stores the zero-based position in the original file; original_point_count + k for an added point whose
                                     * zero-based position in addition order is k (see lop_cloud::added_point_count); or LOP_UNKNOWN_ORIGINAL_POINT_INDEX. */
    uint32_t cloud_id;          /* Identifies an open cloud with a nonzero ID; opening or closing other clouds preserves the ID.
     * Closing and reopening this cloud assigns a new ID. */
    uint16_t intensity;         /* Contains intensity reconstructed on the original scale from the stored normalized value and the cloud maximum. */
    uint8_t classification;     /* Includes confirmed classification edits. */
    uint8_t return_number;      /* 1-15; 0 if the file does not have it */
    uint8_t number_of_returns;  /* 1-15; 0 if the file does not have it */
    uint8_t red;                /* 0-255; 0 if the cloud has no colour */
    uint8_t green;
    uint8_t blue;
} lop_point;

/* Stores point_count points in columns using lop_point field semantics; index i identifies the same point in every column.
 * The host owns the memory, which remains valid until release_points of the host that delivered the points, or until that host ends:
 * destroy for a lop_command_host, the return of the background task function for a lop_task_host. */
typedef struct lop_points
{
    uint64_t point_count;
    double const* x;
    double const* y;
    double const* z;
    uint64_t const* original_point_index;
    uint32_t const* cloud_id;
    uint16_t const* intensity;
    uint8_t const* classification;
    uint8_t const* return_number;
    uint8_t const* number_of_returns;
    uint8_t const* red;
    uint8_t const* green;
    uint8_t const* blue;
    int complete;               /* Zero indicates incomplete point or attribute loading, including unreadable nodes, point limits or the two-minute node request deadline.
     * The caller can wait up to ten additional seconds if rendering does not complete the request. */
    void* handle;               /* The host uses this opaque handle to retain and release the point data. */
} lop_points;

/* Identifies stored optional attributes in lop_cloud::extra_fields; process_tiles loads the fields requested by required_extra_fields. */
#define LOP_FIELD_GPS_TIME          (1u << 0)
#define LOP_FIELD_POINT_SOURCE_ID   (1u << 1)

/* Describes an open cloud at the time of the clouds call or request. */
typedef struct lop_cloud
{
    uint32_t cloud_id;              /* Uses the same nonzero cloud identifier as lop_point::cloud_id. */
    char const* name;               /* Contains the cloud name displayed in the Layers panel. */
    char const* original_path;      /* Contains the original file path, or the remote cloud URL without its query string. */
    int visible;                    /* Nonzero indicates that the cloud is visible. */
    int remote;                     /* Nonzero indicates an Azure cloud. */
    int editable;                   /* Nonzero indicates an edit log; changing or adding points also requires has_original_point_indices. */
    int has_original_point_indices; /* Nonzero indicates stored original_point_index values for the cloud's points. */
    uint32_t extra_fields;          /* Contains LOP_FIELD_* bits for attributes stored in the level of detail. */
    double box_min[3];              /* box_min and box_max bound the points in world coordinates. */
    double box_max[3];
    char const* crs_wkt;            /* Contains the WKT (Well-Known Text) of the coordinate reference system that Lot Of Points uses for the cloud:
                                       the cloud's system if it is known and not local; otherwise the document's system if the document has one;
                                       otherwise the cloud's local system or, for a cloud without a system, the unknown local system. Empty if
                                       neither the cloud nor the document provides a WKT and Lot Of Points cannot obtain the WKT of the unknown
                                       local system. */
    char const* horizontal_unit_name;  /* Contains the unit of X and Y, for example "metre", "US survey foot" or "degree"; empty if the
                                          horizontal component is local. */
    double horizontal_metres_per_unit; /* Contains the metres per unit of X and Y; 0 if the unit is angular or the horizontal component is local. */
    char const* vertical_unit_name;    /* Contains the unit of Z; empty if the system has no vertical component or the vertical component is
                                          local. */
    double vertical_metres_per_unit;   /* Contains the metres per unit of Z; 0 if the system has no vertical component or the vertical
                                          component is local. */
    int vertical_axis_points_down;     /* Nonzero indicates a depth: Z values increase in the downward direction. */
    double original_to_world_transform[16]; /* Row-major 4x4 similarity that maps the coordinates of original_path to world coordinates;
                                               identity for a cloud without a .transform.json file. */
    uint64_t original_point_count;  /* SDK 0.6. Contains the number of points of the original file: original_point_index values below it
                                       identify them. 0 if has_original_point_indices is zero. */
    uint64_t added_point_count;     /* SDK 0.6. Contains the number of points that confirmed editing events have added with add_points,
                                       including the added points that a later event deletes; their original_point_index values are
                                       original_point_count, original_point_count + 1, ... in the order of addition, and a LAS file
                                       exported with the edits stores them after the original points, at those positions. */
} lop_cloud;

typedef enum lop_point_order
{
    LOP_ORDER_ORIGINAL_POINT_INDEX = 0, /* Sorts by the zero-based original point index. */
    LOP_ORDER_SOURCE_AND_GPS_TIME = 1   /* Sorts by point source ID, then GPS time; both extra fields must be requested. */
} lop_point_order;

/* Stores core and XY-overlap points in columns with lop_point field semantics; deleted points are excluded.
 * The overlap extends tile_overlap_width_cloud_units beyond the core; index i identifies the same point in every column. */
typedef struct lop_tile
{
    uint64_t point_count;
    double const* x;
    double const* y;
    double const* z;
    uint64_t const* original_point_index;   /* Each element is LOP_UNKNOWN_ORIGINAL_POINT_INDEX if the cloud has no stored original indices. */
    uint16_t const* intensity;
    uint8_t const* classification;          /* Includes confirmed classification edits. */
    uint8_t const* return_number;
    uint8_t const* number_of_returns;
    uint8_t const* red;
    uint8_t const* green;
    uint8_t const* blue;
    double const* gps_time;                 /* NULL unless required_extra_fields includes LOP_FIELD_GPS_TIME. */
    uint16_t const* point_source_id;        /* NULL unless required_extra_fields includes LOP_FIELD_POINT_SOURCE_ID. */
    uint8_t const* in_core;                 /* One marks a core point; the host records result values only for core points. */
    double core_min[3];                     /* core_min and core_max bound the core; Z spans the cloud. */
    double core_max[3];
    uint64_t tile_index;                    /* Uses a zero-based tile index across the call: cloud by cloud, with rows ordered by increasing Y. */
    uint64_t tile_count;
    uint32_t cloud_id;                      /* Identifies the cloud that contains the tile. */
} lop_tile;

/* LAS 1.4 classification flags of lop_added_points::classification_flags. */
#define LOP_CLASSIFICATION_FLAG_SYNTHETIC  (1u << 0)
#define LOP_CLASSIFICATION_FLAG_KEY_POINT  (1u << 1)
#define LOP_CLASSIFICATION_FLAG_WITHHELD   (1u << 2)
#define LOP_CLASSIFICATION_FLAG_OVERLAP    (1u << 3)

/* Stores point_count new points in columns for lop_tile_result::add_points and lop_task_host::add_points; index i identifies the same
 * point in every column. x, y and z are required world coordinates in the cloud's coordinate system and units, like lop_tile::x; every
 * other column may be NULL and then takes the value in brackets. The host copies the columns before add_points returns. */
typedef struct lop_added_points
{
    uint64_t point_count;
    double const* x;
    double const* y;
    double const* z;
    uint8_t const* classification;          /* [0] 0-255; a LAS file of point formats 0-5 cannot store classes above 31. */
    uint8_t const* classification_flags;    /* [LOP_CLASSIFICATION_FLAG_SYNTHETIC] Bits 0-3 as LOP_CLASSIFICATION_FLAG_*; bits 4-7 must be 0. */
    uint16_t const* intensity;              /* [0] On the original scale, like lop_point::intensity. The exported LAS file keeps the value;
                                             * lop_tile, lop_points and the view use the intensity maximum of the cloud, so a value above that
                                             * maximum reads as the maximum. */
    uint8_t const* return_number;           /* [1] 1-15. */
    uint8_t const* number_of_returns;       /* [1] 1-15, >= return_number. */
    uint8_t const* red;                     /* [0] 0-255. */
    uint8_t const* green;                   /* [0] 0-255. */
    uint8_t const* blue;                    /* [0] 0-255. */
    double const* gps_time;                 /* [0.0] */
    uint16_t const* point_source_id;        /* [0] */
} lop_added_points;

/* Receives one result per tile point; classification starts with the tile classes and deleted starts with zeros. */
typedef struct lop_tile_result
{
    uint8_t* classification;
    uint8_t* deleted;           /* Set an element to 1 to delete that point. */

    /* SDK 0.6. Pass add_points_context as the first argument. Queues new points for the cloud of the tile; process_tiles commits them with
     * the classification and deleted results of the call, in the same editing event, so one undo removes both. Callable from any thread
     * until process_tile returns, also concurrently. Keeps the points whose x and y lie in the tile core (core_min inclusive, core_max
     * exclusive) and counts the others in lop_tiles_summary::discarded_added_point_count, so that tiles which compute the same point in
     * their overlap add it once. Discards the queued points of a tile whose process_tile does not return LOP_OK. The tiles of the same
     * process_tiles call do not contain the points that the call adds. Returns LOP_OK, or LOP_FAILED with a logged reason for NULL points
     * or coordinate columns, a non-finite coordinate or gps_time, a return_number or number_of_returns outside 1-15, a return_number above
     * number_of_returns, classification_flags with bits 4-7 set, or a call after process_tile returns; a call after process_tiles returns
     * logs nothing. */
    int (*add_points)(void* add_points_context, lop_added_points const* points);
    void* add_points_context;
} lop_tile_result;

/* Provides callbacks usable from any thread while process_tile runs. */
typedef struct lop_tile_host
{
    void* context;
    void (*report_tile_progress)(void* context, double fraction);   /* Reports the completed fraction of this tile in the inclusive interval [0, 1]. */
    int (*cancelled)(void* context);                                /* If cancelled(context) returns nonzero, process_tile must stop processing and return LOP_CANCELLED. */
    void (*log)(void* context, char const* message);                /* Writes message as a line in the Output panel. */
    uint32_t max_worker_thread_count;   /* Sets the maximum worker threads process_tile should run simultaneously; the value is >= 1 and divides processors among concurrent tiles. */
} lop_tile_host;

/* Processes one tile; result is NULL with LOP_TILES_READ_ONLY.
 * Returns LOP_OK, LOP_CANCELLED or LOP_FAILED; LOP_FAILED stops and fails process_tiles. */
typedef int (*lop_process_tile_function)(void* user_data, lop_tile const* tile, lop_tile_result* result, lop_tile_host const* host);

#define LOP_TILES_CONCURRENT    (1u << 0)   /* Allows simultaneous callbacks on different threads with separate tile and result data. */
#define LOP_TILES_READ_ONLY     (1u << 1)   /* Passes result == NULL and records no edits; clouds need neither an edit log nor original point indices. */

/* Uses this estimate, in bytes per point, when estimated_peak_bytes_per_point is zero. */
#define LOP_DEFAULT_PEAK_BYTES_PER_POINT 1024.0

typedef struct lop_tile_options
{
    double tile_core_side_length_cloud_units;           /* Specifies the square core side in cloud units; the value must be > 0 and <= 10,000,000. */
    double tile_overlap_width_cloud_units;              /* Specifies the XY overlap width in cloud units; the value must be in [0, 10,000,000]. */
    uint32_t required_extra_fields;         /* Requests LOP_FIELD_* attributes; process_tiles fails if a cloud lacks any requested field. */
    lop_point_order point_order;
    uint32_t flags;
    double estimated_peak_bytes_per_point;  /* Estimates peak callback allocation in bytes per tile point; zero uses LOP_DEFAULT_PEAK_BYTES_PER_POINT.
     * Concurrent callbacks wait if estimated allocations plus assembled tile data exceed the available memory budget, unless no callback is running. */
    lop_process_tile_function process_tile;
    void* user_data;                        /* Passes this pointer as the first argument to process_tile. */
} lop_tile_options;

/* Contains change counts only when process_tiles returns LOP_OK. */
typedef struct lop_tiles_summary
{
    uint64_t reclassified_point_count;
    uint64_t deleted_point_count;
    uint32_t changed_cloud_count;               /* Counts the clouds with reclassified, deleted or added points. */
    uint64_t added_point_count;                 /* SDK 0.6. Counts the points that lop_tile_result::add_points has kept. */
    uint64_t discarded_added_point_count;       /* SDK 0.6. Counts proposed points outside the core of the tile that submitted them. */
} lop_tiles_summary;

/* SDK 0.6. read_cloud_points uses this maximum number of points per block when lop_point_reading_options::max_block_point_count is zero. */
#define LOP_DEFAULT_MAX_BLOCK_POINT_COUNT 65536u

/* SDK 0.6. Stores in columns current points of the cloud cloud_id for lop_point_reading_options::process_point_block, with the field
 * semantics of lop_tile: deleted points are excluded and classification includes the confirmed edits; index i identifies the same point in
 * every column. The columns remain valid until process_point_block returns. */
typedef struct lop_point_block
{
    uint64_t point_count;                   /* From 1 to the effective maximum block point count, inclusive; a zero option uses LOP_DEFAULT_MAX_BLOCK_POINT_COUNT. */
    double const* x;                        /* World coordinates in the cloud's coordinate system and units, like lop_tile::x. */
    double const* y;
    double const* z;
    uint64_t const* original_point_index;   /* Like lop_point::original_point_index; LOP_UNKNOWN_ORIGINAL_POINT_INDEX if the cloud has no stored indices. */
    uint16_t const* intensity;              /* The value of the original file when from_level_of_detail is zero; the value reconstructed like
                                             * lop_tile::intensity otherwise. */
    uint8_t const* classification;
    uint8_t const* return_number;
    uint8_t const* number_of_returns;
    uint8_t const* red;                     /* 0-255, like lop_tile::red: the colour of the original file scaled with the colour factor that lop
                                             * computed for the level of detail. */
    uint8_t const* green;
    uint8_t const* blue;
    double const* gps_time;                 /* NULL unless required_extra_fields includes LOP_FIELD_GPS_TIME. */
    uint16_t const* point_source_id;        /* NULL unless required_extra_fields includes LOP_FIELD_POINT_SOURCE_ID. */
    uint32_t cloud_id;
    int from_level_of_detail;               /* Nonzero if lop reads the cloud from its level of detail, zero if from its original file. */
} lop_point_block;

/* SDK 0.6. Receives one block of read_cloud_points on the thread that called read_cloud_points. Returns LOP_OK to receive the next block,
 * LOP_CANCELLED to stop the reading without failing it, or LOP_FAILED to fail it. */
typedef int (*lop_process_point_block_function)(void* user_data, lop_point_block const* block);

/* SDK 0.6. */
typedef struct lop_point_reading_options
{
    uint32_t required_extra_fields;         /* Requests LOP_FIELD_* attributes; read_cloud_points fails if a cloud lacks any requested field
                                             * in lop_cloud::extra_fields. */
    uint32_t max_block_point_count;         /* The points of each block, at most; zero uses LOP_DEFAULT_MAX_BLOCK_POINT_COUNT. */
    lop_process_point_block_function process_point_block;
    void* user_data;                        /* Passes this pointer as the first argument to process_point_block. */
} lop_point_reading_options;

/* SDK 0.6. The summary fields are valid only when read_cloud_points returns LOP_OK. */
typedef struct lop_point_reading_summary
{
    uint64_t read_point_count;              /* Counts the points passed to process_point_block. */
    uint32_t level_of_detail_cloud_count;   /* Counts the clouds read from their level of detail. */
    int stopped_by_process_point_block;     /* Nonzero if process_point_block returned LOP_CANCELLED. */
} lop_point_reading_summary;

/* SDK 0.6. Stores point_count changes of points of one cloud in columns for lop_task_host::commit_point_changes; index i identifies the same
 * change in every column. The host reads the columns before commit_point_changes returns. */
typedef struct lop_point_changes
{
    uint64_t point_count;
    uint64_t const* original_point_index;   /* Required: the point of each change, like lop_point::original_point_index; each value below
                                             * original_point_count + added_point_count of the cloud, without repetitions. */
    uint8_t const* classification;          /* NULL keeps the classes; otherwise the new class of each point that the change does not delete. */
    uint8_t const* deleted;                 /* NULL deletes nothing; otherwise a nonzero element deletes the point and 0 leaves the point's deletion state unchanged. */
} lop_point_changes;

/* SDK 0.6. The summary fields are valid only when commit_point_changes returns LOP_OK. */
typedef struct lop_point_changes_summary
{
    uint64_t reclassified_point_count;      /* Counts the changes that assign a class: with a non-NULL classification, those that do not delete
                                             * their point; lop does not compare the class with the current class of the point. */
    uint64_t deleted_point_count;           /* Counts the changes whose deleted element is nonzero, also for points that are already deleted. */
} lop_point_changes_summary;

/* Identifies the type of a value of a panel or dialog of the DLL (see lop_panel). */
typedef enum lop_ui_value_type
{
    LOP_UI_VALUE_NONE = 0,              /* The key has no value. */
    LOP_UI_VALUE_REAL = 1,              /* real */
    LOP_UI_VALUE_INTEGER = 2,           /* integer */
    LOP_UI_VALUE_BOOLEAN = 3,           /* boolean: 0 or 1 */
    LOP_UI_VALUE_TEXT = 4,              /* text */
    LOP_UI_VALUE_OTHER = 5              /* A value of another type, such as the selected item of a list, written by a binding of the XAML. */
} lop_ui_value_type;

/* Stores a value of Values[key] of a panel or dialog. type identifies the valid value field: real, integer, boolean or text.
 * LOP_UI_VALUE_NONE and LOP_UI_VALUE_OTHER have no value field. */
typedef struct lop_ui_value
{
    uint32_t type;                      /* lop_ui_value_type */
    double real;
    int32_t integer;
    int boolean;
    char const* text;
} lop_ui_value;

/* Associates a field name of a row with its value, which the XAML reads with {Binding [name]}. */
typedef struct lop_ui_field
{
    char const* name;
    lop_ui_value value;                 /* LOP_UI_VALUE_REAL, LOP_UI_VALUE_INTEGER, LOP_UI_VALUE_BOOLEAN or LOP_UI_VALUE_TEXT. */
} lop_ui_field;

/* Stores a row of Lists[key] of a panel or dialog. */
typedef struct lop_ui_row
{
    lop_ui_field const* fields;
    uint32_t field_count;
} lop_ui_row;

/* Formats of export_cloud. */
#define LOP_EXPORT_FORMAT_LAS 0u
#define LOP_EXPORT_FORMAT_LAZ 1u
#define LOP_EXPORT_FORMAT_E57 2u

/* Coordinates of export_cloud. */
#define LOP_EXPORT_WORLD_COORDINATES    0u /* original_to_world_transform applied; the only value for a remote cloud */
#define LOP_EXPORT_ORIGINAL_COORDINATES 1u /* coordinates of original_path; local clouds only, and a local cloud read from its level of detail only if original_to_world_transform is the identity */

typedef struct lop_export_options
{
    uint32_t format;                /* LOP_EXPORT_FORMAT_* */
    uint32_t coordinates;           /* LOP_EXPORT_*_COORDINATES */
    int only_isolated_points;       /* Nonzero writes only the points inside the isolation; zero ignores the isolation. */
} lop_export_options;

typedef struct lop_export_summary
{
    uint64_t written_point_count;       /* Points of the output file, added points included. */
    uint64_t written_added_point_count; /* Points of add_points operations that the file contains. */
    uint64_t deleted_point_count;       /* Points that the edits delete and the file does not contain. */
    int from_level_of_detail;           /* Nonzero if lop read the level of detail (remote cloud, or local cloud whose original file fails the correspondence checks of export_cloud), not the original file. */
} lop_export_summary;

/* Provides the functions of a background task until the task function returns; pass context as the first argument to each function.
 * The functions can be called from the task thread and from threads that the task creates; the functions that load data wait for it. */
typedef struct lop_task_host
{
    void* context;

    /* Writes message as a line in the Output panel. */
    void (*log)(void* context, char const* message);

    /* Replaces the state text that the Background tasks panel shows for the task, for example "Classifying". */
    void (*report_state)(void* context, char const* state);

    /* Shows the completed fraction in the progress bar of the task; values outside [0, 1] are clamped and NaN is ignored.
     * Until the first call, the bar shows indeterminate progress. */
    void (*report_progress)(void* context, double fraction);

    /* Nonzero indicates that the user cancelled the task in the Background tasks panel or when closing lop, or a host failure; the task
     * must poll this function and return LOP_CANCELLED. */
    int (*cancelled)(void* context);

    /* Returns the displayed class name: the user-defined name or the localized ASPRS (American Society for Photogrammetry and Remote Sensing)
     * name; returns an empty string if unnamed. The returned string remains valid until the task function returns. */
    char const* (*class_name)(void* context, uint8_t classification);

    /* Fills clouds with open clouds in Layers panel order and cloud_count with their count; the array and strings remain valid until the next
     * clouds call or the return of the task function. Returns LOP_OK, LOP_CANCELLED on cancellation or host failure, or LOP_FAILED for NULL
     * output pointers. */
    int (*clouds)(void* context, lop_cloud const** clouds, uint32_t* cloud_count);

    /* Loads undeleted points at distance <= radius from center in cloud coordinates, restricted by isolation. Uses still-open
     * cloud_ids[0..cloud_count), including hidden clouds; count zero uses visible clouds and permits NULL cloud_ids. Waits for point loading;
     * returns LOP_OK, LOP_CANCELLED on cancellation or host failure, or LOP_FAILED for NULL output/center, nonfinite coordinates/radius,
     * negative radius, or nonzero cloud_count with NULL cloud_ids. */
    int (*points_in_sphere)(void* context, double const center[3], double radius, uint32_t const* cloud_ids, uint32_t cloud_count,
        lop_points* points);

    /* Releases the point columns and clears the structure; retained column pointers become invalid. */
    void (*release_points)(void* context, lop_points* points);

    /* Partitions cloud_ids[0..cloud_count) into square XY cores aligned to multiples of tile_core_side_length_cloud_units and calls
     * options->process_tile for each core containing points. Shows the fraction of processed tiles in the progress bar of the task; reserves
     * these clouds against closure while permitting other clouds to open or close; unless LOP_TILES_READ_ONLY, fails if another task has
     * reserved one of these clouds for editing. Commits changed core points and the points of lop_tile_result::add_points before returning:
     * as one undoable event or, while start_editing_event has an event open, in that event. While an editing event is open,
     * process_tiles fails for a cloud outside that event's cloud_ids unless LOP_TILES_READ_ONLY. The editing history displays the
     * command name, or the task name for a task started from lop_extension_host; LOP_OK fills a non-NULL summary. Cancellation before commit, invalid options/clouds or callback
     * failure discard provisional edits; recording failure may leave edits committed in some clouds.
     * Returns LOP_CANCELLED on cancellation or host failure, LOP_FAILED with a logged reason on validation, reservation, callback or
     * recording failure. Editing needs an edit log and original indices unless LOP_TILES_READ_ONLY; required extra fields remain
     * mandatory. */
    int (*process_tiles)(void* context, uint32_t const* cloud_ids, uint32_t cloud_count, lop_tile_options const* options,
        lop_tiles_summary* summary);

    /* Loads the GeoPackage tables as Layers panel layers, one per table, as when the user opens the file; does nothing if the file is
     * already loaded. To display the updated contents of a rewritten file, call close_geopackage, then call load_geopackage again. Returns
     * LOP_OK, LOP_CANCELLED on cancellation or host failure, or LOP_FAILED with a logged reason for an empty path or read failure. */
    int (*load_geopackage)(void* context, char const* geopackage_file_path);

    /* Removes from the Layers panel and from the view the layers of the GeoPackage at geopackage_file_path, loaded by load_geopackage or by
     * the user, and stops digitizing on them; does not modify the file and also runs after a cancellation. Returns LOP_OK, also if the file
     * is not loaded, LOP_CANCELLED after a host failure, or LOP_FAILED with a logged reason for an empty path or when lop cannot remove the
     * layers. */
    int (*close_geopackage)(void* context, char const* geopackage_file_path);

    /* Fills directory_path with the folder where this extension keeps its files for the open cloud cloud_id, and creates the folder:
     * "<original file path>.extensions\<extension id>" for a local cloud and "<edits folder of the cloud>\cloud.extensions\<extension id>"
     * for a remote cloud. The extension id is the store extension id, or the file name without extension of a DLL or script outside the
     * store. The string remains valid until the task function returns, including when another thread of the task calls this function again.
     * Returns LOP_OK, LOP_CANCELLED after a host failure, or LOP_FAILED with a logged reason for NULL directory_path, a cloud that is not open
     * or a folder that cannot be created. */
    int (*get_cloud_extension_data_directory_path)(void* context, uint32_t cloud_id, char const** directory_path);

    /* Writes to las_file_path a LAS file of point format 2 with at most one point per cube, read from the level of detail of the local
     * cloud cloud_id down to the nodes whose detail size is <= spacing_cloud_units. The cubes are aligned to the origin of the level of
     * detail: a point at world coordinates p is in the cube with indices floor((p[axis] - origin[axis]) / s), where s is
     * spacing_cloud_units converted to float, and each index must be between -1,048,576 and 1,048,575, inclusive. Each cube keeps the
     * first point read, not the one nearest to its centre. The LAS header stores the origin as the X, Y and Z offsets, with scale
     * factors 0.001. For each relative float coordinate r, the LAS stores the integer q = trunc(r * 1000.0f). The reconstructed
     * relative coordinate 0.001 * q differs from r by less than 0.001 cloud units while the absolute value of r is below 16,777.216
     * cloud units, and by up to 0.001 below 33,554.432, so the stored point can fall in a neighbouring cube. lop_tile::x, y and z minus
     * the LAS offsets give the relative positions that the cubes use. Excludes deleted points and, while there is an isolation, the
     * points outside it; writes the confirmed classes. Also reads the points that editing events confirmed at the moment of the reservation
     * have added to the cloud (see lop_cloud::added_point_count) and thins them in the same cubes as the other points, with
     * edits confirmed before the reservation applied, including edits confirmed after those points were added; the points of undone events are excluded. Reserves the cloud against closure until it returns and shows the fraction of
     * processed points from the selected nodes in the progress bar of the task. Returns LOP_OK, LOP_CANCELLED on cancellation or host
     * failure, or LOP_FAILED with a logged reason for a cloud that is not open, a remote cloud, a spacing that is not a finite number
     * greater than 0 that fits in a float (<= FLT_MAX), a cube index outside that interval, an empty path or a write failure. */
    int (*export_thinned_cloud)(void* context, uint32_t cloud_id, double spacing_cloud_units, char const* las_file_path);

    /* Assign, read and replace values and rows of a panel or dialog like the functions of lop_command_host with the same names. get_ui_value
     * waits for the user interface thread; its text remains valid until the next get_ui_value call or the return of the task function. */
    int (*set_ui_value)(void* context, char const* panel_or_dialog_id, char const* key, lop_ui_value const* value);
    int (*get_ui_value)(void* context, char const* panel_or_dialog_id, char const* key, lop_ui_value* value);
    int (*set_ui_rows)(void* context, char const* panel_or_dialog_id, char const* list_key, lop_ui_row const* rows, uint32_t row_count);

    /* Like lop_command_host::get_extension_data_directory_path, from the task thread or a thread that the task creates. Calls that return
     * LOP_OK set *directory_path to the same pointer for this task; the string remains valid until the task function returns. Returns
     * LOP_CANCELLED after a host failure. */
    int (*get_extension_data_directory_path)(void* context, char const** directory_path);

    /* Fills directory_path with the folder that get_cloud_extension_data_directory_path returns for the open cloud cloud_id if that folder
     * exists, or with NULL if it does not exist, without creating it. The string remains valid until the task function returns, including
     * when another thread of the task calls this function again. Returns LOP_OK in both cases, LOP_CANCELLED after a host failure, or
     * LOP_FAILED with a logged reason for NULL directory_path or a cloud that is not open. */
    int (*find_cloud_extension_data_directory_path)(void* context, uint32_t cloud_id, char const** directory_path);

    /* Assigns name to the class classification in the class catalog of lop, which does not depend on the open document: lop saves the name
     * in its settings, the Classifications panel shows it and class_name returns it. name contains from 1 to LOP_MAX_CLASS_NAME_BYTES bytes
     * of UTF-8, without the terminating null. Waits until the user interface thread saves the name, also after a cancellation. Returns
     * LOP_OK, LOP_CANCELLED after a host failure, or LOP_FAILED with a logged reason for a NULL, empty or longer name, or when lop cannot
     * save the name. */
    int (*set_class_name)(void* context, uint8_t classification, char const* name);

    /* SDK 0.6. Adds points->point_count new points to the open cloud cloud_id outside process_tiles and commits them before returning: lop
     * copies the columns, writes them next to the edit log of the cloud, rebuilds on the calling thread the level of detail of every point
     * added to the cloud, and records an editing event or, while start_editing_event has an event open, adds the points to that event. The
     * new points get the original_point_index values that follow those of the points already added (see lop_cloud::added_point_count).
     * lop keeps every point with finite coordinates, also in hidden clouds and outside box_min and box_max. Calls for the same cloud run
     * one after another. A point_count of 0 returns LOP_OK without changes. Returns LOP_OK; LOP_CANCELLED without changes if the task is
     * cancelled before the commit, or after a host failure; or LOP_FAILED with a logged reason for NULL points or coordinate columns, a
     * non-finite coordinate or gps_time, a return_number or number_of_returns outside 1-15, a return_number above number_of_returns,
     * classification_flags with bits 4-7 set, a cloud that is not open, has no edit log or has no original point indices, a cloud reserved
     * for editing by another task, a cloud that a process_tiles call of this task without LOP_TILES_READ_ONLY is processing, a cloud outside
     * the cloud_ids of the open editing event, or a write failure. */
    int (*add_points)(void* context, uint32_t cloud_id, lop_added_points const* points);

    /* SDK 0.6. Opens an editing event of the task on cloud_ids[0..cloud_count) and reserves those clouds for editing and against closure
     * until end_editing_event or the return of the task function. While the event is open, each process_tiles call without
     * LOP_TILES_READ_ONLY and each add_points and commit_point_changes call still commits its changes before returning, but adds them to this event instead of
     * recording a new one: the editing history shows the event once, and one undo removes every change of the event. The user cannot edit,
     * undo or redo changes of these clouds until the event ends. Returns LOP_OK, LOP_CANCELLED on cancellation or host failure, or
     * LOP_FAILED with a logged reason for a cloud_count of 0, NULL cloud_ids, a repeated cloud, a cloud that is not open, has no edit log or
     * has no original point indices, a cloud reserved for editing by another task or by a process_tiles call of this task, or an editing
     * event already open in this task. */
    int (*start_editing_event)(void* context, uint32_t const* cloud_ids, uint32_t cloud_count);

    /* SDK 0.6. Ends the open editing event of the task and releases the reservation of its clouds; later process_tiles, add_points and commit_point_changes
     * calls record new events. An event without changes leaves no entry in the editing history. lop also ends the open event when the task
     * function returns, also after a cancellation or LOP_FAILED; changes committed before the task function returns remain recorded in that
     * editing event. Returns LOP_OK, also if no event is open; LOP_CANCELLED after a host failure; or LOP_FAILED with a logged reason while another thread of the task runs a
     * process_tiles, add_points, commit_point_changes, read_cloud_points or export_cloud call of the event. */
    int (*end_editing_event)(void* context);

    /* SDK 0.6. Loads the TIFF image at tiff_file_path as an orthophoto of the Layers panel, as when the user opens the file, and waits until
     * lop has read it; does nothing if the file is already loaded, and waits for a read of the same file in progress. The image needs a
     * ModelTransformationTag, or a ModelTiepointTag with a ModelPixelScaleTag, in the world coordinates of the open clouds; without those
     * tags, lop reads the .tfw world file in the same directory and with the same base name as the TIFF (a plane at Z = 0). lop does not read
     * the coordinate reference system of the image. lop decodes the image to 8-bit RGBA with libtiff
     * (TIFFReadRGBAImageOriented), keeps it in memory and draws it while a cloud is open, as a rectangle in the plane of the
     * georeference with depth test, without the pixels whose alpha is below 128; without an alpha channel, the pixels whose red, green and
     * blue equal the GDAL_NODATA value, or 0, are transparent. To display the updated contents of a rewritten file, call close_orthophoto,
     * then call load_orthophoto again. Returns LOP_OK, LOP_CANCELLED on cancellation or host failure, or LOP_FAILED with a logged reason
     * for an empty path, a file that libtiff cannot open or decode, a missing georeference, an invalid world file, more than LOP_MAX_ORTHOPHOTO_PIXELS pixels or
     * a lack of memory. */
    int (*load_orthophoto)(void* context, char const* tiff_file_path);

    /* SDK 0.6. Removes from the Layers panel and from the view the orthophoto of tiff_file_path, loaded by load_orthophoto or by the user;
     * does not modify the file and also runs after a cancellation. Returns LOP_OK, also if the file is not loaded, LOP_CANCELLED after a
     * host failure, or LOP_FAILED with a logged reason for an empty path. */
    int (*close_orthophoto)(void* context, char const* tiff_file_path);

    /* SDK 0.6. Writes to file_path the open cloud cloud_id with its confirmed edits and its added points: without the deleted points,
     * with the confirmed classes and with the points of confirmed add_points operations after the original points. A local cloud is read
     * from its original file if the file passes these correspondence checks: it exists; the cache path computed for it, which contains the
     * file name and the low 32 bits of its modification time, equals the path of the level of detail; and, for a LAS or LAZ file whose level
     * of detail has the record index (.index.atributos), the point count in the file header equals the record count of that index. No point
     * count is compared for E57, PTX, PTS and PCD files, or without .index.atributos. The checks do not detect a replacement by a file with
     * the same name, the same modification time and the same point count. If a check fails, the local cloud is read from its level of
     * detail like a remote cloud, from_level_of_detail is nonzero, and once the export completes lop writes the reason in the Output panel
     * as the first notice. A remote cloud is always read from its level of detail. The level of detail contains every point of the original
     * except exact duplicates, with positions rounded to float relative to the centre of the cloud, colours of 8 bits per channel and a
     * reconstructed intensity; the GPS time and the other attributes are written only if the level of detail stores them (.index.atributos).
     * LAS and LAZ are written with LASzip; E57 with libE57Format. The output stores the WKT of the written coordinates (see the design of
     * lop_cloud::crs_wkt); a local cloud exported from its level of detail with original coordinates stores lop_cloud::crs_wkt, not the WKT of the original file.
     * If the original file is a LAS or LAZ and original_to_world_transform is not the identity, a LAS or LAZ export with world coordinates declares the WKT of the written
     * coordinates or no coordinate reference system, never the GeoTIFF and WKT records (variable length and extended variable length) of the original, which describe the original coordinates.
     * With a WKT of 1 to 65534 bytes the output stores it in a WKT record, and an original earlier than LAS 1.4 becomes a LAS 1.4 with point format 6 (original formats 0 and 1) or 7 (formats 2 and 3);
     * with an empty WKT or one longer than 65534 bytes, the output keeps the original version and point format unless a written point in original format 0-3 has a classification above 31, a return number above 7, or a number of returns above 7;
     * it omits the original GeoTIFF and WKT projection records, and lop writes a notice in the Output panel. An output that is not a LAS 1.4 (an original LAS earlier than 1.4 with point format 6 to 8, or a LAS 1.4 with a header shorter than 375 bytes)
     * also omits those records and gets the notice.
     * Requires, at the moment of the call: for a remote cloud, the export permission of its owner ("exportable": true in the entry of
     * the cloud in the project catalog that Lot Of Points Studio writes, read again over HTTPS); and the lop.exportacion license, or the
     * license of the module of this command together with the format in the export permission of the store manifest of the extension and
     * a cloud in which this task has committed edits or added points.
     * Reserves the cloud until it returns: meanwhile the user cannot close it, edit it, or undo or redo its edits, and another task
     * cannot reserve it for editing. Shows the fraction of written points in the progress bar of the task.
     * Writes to a temporary file next to file_path and renames it when it finishes; after a cancellation or a failure, file_path does not
     * change. Returns LOP_OK and fills a non-NULL summary; LOP_CANCELLED on cancellation or host failure; or LOP_FAILED with a logged
     * reason for NULL options, an unknown format or coordinates value, original coordinates of a remote cloud or of a local cloud read
     * from its level of detail with an original_to_world_transform that is not the identity, a cloud that is not open,
     * a missing permission or license, an empty path, a file_path that designates the original file, the edit log, a file of the
     * confirmed edits or a local level-of-detail file of any cloud open in lop (the comparison follows the filesystem's case-sensitivity rules),
     * coordinates that do not fit in the 32-bit integers of LAS, or a read or write failure. */
    int (*export_cloud)(void* context, uint32_t cloud_id, lop_export_options const* options, char const* file_path,
        lop_export_summary* summary);

    /* SDK 0.6. Sets *allowed to nonzero if export_cloud would accept cloud_id and format now, without reading the cloud, and to zero
     * otherwise; on zero, if reason is not NULL, *reason points to the reason in English, valid until the task
     * function returns. Returns LOP_OK, LOP_CANCELLED after a host failure, or LOP_FAILED for NULL allowed or a cloud that is not open. */
    int (*can_export_cloud)(void* context, uint32_t cloud_id, uint32_t format, int* allowed, char const** reason);

    /* SDK 0.6. Calls options->process_point_block on the calling thread with consecutive blocks of the current points of each open cloud
     * of cloud_ids[0..cloud_count), cloud after cloud in that order: the points of the cloud and the points of its confirmed add_points
     * operations, with the confirmed edits applied, without the deleted points and, while the view isolates points by their original point
     * index, only with the isolated points, like process_tiles. A local cloud whose original file is a LAS file, or a LAZ file with a version-0
     * table of compressed point blocks (chunks) with between 1 and 200,000 points per chunk, inclusive, and whose modification-time-derived
     * cache path and point count match the level of detail, is read from
     * that file: from_level_of_detail is zero, the coordinates are those of the file transformed with original_to_world_transform,
     * original_point_index increases within the cloud, the original points come in the order of the file and the added points follow them.
     * Any other cloud is read from its level of detail: from_level_of_detail is nonzero, each point comes once in an order that the SDK does
     * not fix, exact duplicates of the original are missing and positions are rounded to float relative to the origin of the level of
     * detail; for a local cloud, lop writes in the Output panel why it does not read the original file.
     * Reserves each cloud while it reads it, like export_cloud: meanwhile the user cannot close it, edit it, or undo or redo its edits,
     * and another task cannot reserve it for editing; with an open editing event of this task that contains the cloud, the reading is a
     * call of the event. Reads the edits confirmed when the reservation of each cloud starts, including the changes that process_tiles,
     * add_points and commit_point_changes of this task have committed before. Checks the cancellation of the task before each block and shows the fraction of the
     * points of all the clouds, original_point_count + added_point_count, that lop has read in the progress bar of the task.
     * Returns LOP_OK and fills a non-NULL summary, also when process_point_block stops the reading; LOP_CANCELLED on cancellation of the
     * task or host failure; or LOP_FAILED with a logged reason for NULL options or process_point_block, a nonzero cloud_count with NULL
     * cloud_ids, a repeated cloud, a cloud that is not open, a requested extra field that a cloud lacks, a cloud reserved for editing by
     * another task, a cloud that another call of the open editing event of this task uses, a read failure, or LOP_FAILED from
     * process_point_block. */
    int (*read_cloud_points)(void* context, uint32_t const* cloud_ids, uint32_t cloud_count, lop_point_reading_options const* options,
        lop_point_reading_summary* summary);

    /* SDK 0.6. Applies changes->point_count changes to the points of the open cloud cloud_id identified by their original_point_index and
     * commits them before returning, like the result of a process_tiles call: as one undoable event or, while start_editing_event has an
     * event open, in that event. A change with a nonzero deleted element deletes its point; otherwise, with a non-NULL classification, it
     * assigns the class. lop records the changes as a table by original point index next to the edit log of the cloud, so they also apply to
     * the points of the original file that the level of detail does not contain (exact duplicates), to the added points and to the points outside the isolation of the view. read_cloud_points returns the points with the committed changes when reading the original
     * file; export_cloud writes the points with the committed changes when exporting from the original file. Calls of
     * commit_point_changes and add_points for the same cloud run one after another. With non-NULL changes and changes->original_point_index, a point_count of 0 returns LOP_OK without changes unless the host has already failed.
     * Returns LOP_OK and fills a non-NULL summary; LOP_CANCELLED without changes if the task is cancelled before the commit, or after a host
     * failure; or LOP_FAILED with a logged reason for NULL changes or original_point_index, an original_point_index outside the cloud or
     * repeated, a cloud that is not open, has no edit log or has no original point indices, a cloud reserved for editing or export by another
     * task, a cloud that a process_tiles call without LOP_TILES_READ_ONLY, a read_cloud_points call or an export_cloud call of this task is
     * using, a cloud outside the cloud_ids of the open editing event, or a write failure. */
    int (*commit_point_changes)(void* context, uint32_t cloud_id, lop_point_changes const* changes, lop_point_changes_summary* summary);
} lop_task_host;

/* Maximum length of the name of lop_task_host::set_class_name, in bytes of UTF-8, without the terminating null. */
#define LOP_MAX_CLASS_NAME_BYTES 255u

/* Maximum number of pixels (width x height) of an image of lop_task_host::load_orthophoto; lop keeps 4 bytes per pixel in memory. */
#define LOP_MAX_ORTHOPHOTO_PIXELS 268435456u

/* Runs a background task on a thread of lop with the user_data passed to start_background_task; returns LOP_OK, LOP_CANCELLED or
 * LOP_FAILED. The task should log the reason before returning LOP_FAILED. */
typedef int (*lop_background_task_function)(void* user_data, lop_task_host const* task);

/* Identifies the events that lop delivers to lop_command::on_event, always on the user interface thread. on_event must return 0 for an
 * event type that it does not know: lop can add event types without changing the version of the SDK. */
typedef enum lop_event_type
{
    LOP_EVENT_START = 1,                /* Delivered once, after create, when lop pushes the command onto its command stack. */
    LOP_EVENT_SET_FOCUS = 2,            /* Delivered after START if the command did not finish during START. */
    LOP_EVENT_LOST_FOCUS = 3,           /* Delivered when lop removes the command from its command stack. */
    LOP_EVENT_MOUSE_BUTTON_DOWN = 4,    /* Fills mouse_button, double_click, x and y; returning 1 consumes the press. */
    LOP_EVENT_MOUSE_BUTTON_UP = 5,      /* Fills mouse_button, x and y. */
    LOP_EVENT_MOUSE_MOVE = 6,           /* Fills is_any_mouse_button_pressed, x and y. */
    LOP_EVENT_MOUSE_WHEEL = 7,          /* Fills wheel_delta, control, shift, x and y. */
    LOP_EVENT_KEY_DOWN = 8,             /* Fills key; if on_event returns 0 for Esc, lop finishes the command. */
    LOP_EVENT_REQUEST_PREVIEW = 9,      /* Fills request_id, request_type, has_coordinates and coordinates while a point request with preview waits. */
    LOP_EVENT_REQUEST_COMPLETED = 10,   /* Fills request_id, request_type, result and, with LOP_OK, the field of the request type. */
    LOP_EVENT_FINISH = 11,              /* Delivered once, after LOST_FOCUS and then the completion of every pending request with LOP_CANCELLED;
                                         * lop delivers no other event to the command afterwards. */
    LOP_EVENT_PANEL_ACTION = 12,        /* Fills panel_id and panel_action_name: the user activated a control bound to
                                         * Actions[panel_action_name] of the panel that this command owns (lop_command_panel). Before
                                         * delivering it, lop finishes the commands above this command in the command stack. lop ignores
                                         * the result of on_event. */
    LOP_EVENT_PANEL_VALUE_CHANGED = 13  /* Fills panel_id, panel_value_key and panel_value: the user changed Values[panel_value_key] of the
                                         * panel that this command owns through a binding of the XAML. Not delivered for set_ui_value, for
                                         * the values that the bindings write while lop applies set_ui_value or set_ui_rows, nor for the
                                         * values that the bindings write when lop shows the panel. Before delivering it, lop finishes the
                                         * commands above this command in the command stack. lop ignores the result of on_event. A
                                         * ListView can write -1 to its
                                         * SelectedIndex when lop replaces its rows: the command must accept -1 and indices outside the list. */
} lop_event_type;

typedef enum lop_mouse_button
{
    LOP_MOUSE_BUTTON_LEFT = 1,
    LOP_MOUSE_BUTTON_MIDDLE = 2,
    LOP_MOUSE_BUTTON_RIGHT = 3
} lop_mouse_button;

/* Key codes of LOP_EVENT_KEY_DOWN; letter keys use the codes of the uppercase ASCII letters, from 'A' (65) to 'Z' (90). */
#define LOP_KEY_ESCAPE      1
#define LOP_KEY_ENTER       2
#define LOP_KEY_BACKSPACE   3
#define LOP_KEY_DELETE      4
#define LOP_KEY_SPACE       5
#define LOP_KEY_TAB         6
#define LOP_KEY_LEFT        7
#define LOP_KEY_RIGHT       8
#define LOP_KEY_UP          9
#define LOP_KEY_DOWN        10

/* Identifies the request that a request function of lop_command_host started. */
typedef enum lop_request_type
{
    LOP_REQUEST_POINT = 1,              /* request_point: coordinates */
    LOP_REQUEST_CLOUD_POINT = 2,        /* request_cloud_point: point */
    LOP_REQUEST_GEOMETRY = 3,           /* request_geometry: geometry */
    LOP_REQUEST_POINTS_IN_SPHERE = 4,   /* request_points_in_sphere: points */
    LOP_REQUEST_CLOUDS = 5,             /* request_clouds: clouds and cloud_count */
    LOP_REQUEST_FILE = 6,               /* request_file: file_path */
    LOP_REQUEST_DIALOG = 7,             /* show_dialog: accepted */
    LOP_REQUEST_SAVE_FILE_PATH = 8,     /* request_save_file_path: file_path */
    LOP_REQUEST_ORIGINAL_TO_WORLD_TRANSFORM_CHANGE = 9, /* request_original_to_world_transform_change: accepted */
    LOP_REQUEST_CRS = 10                /* request_crs: crs_wkt */
} lop_request_type;

typedef enum lop_geometry_type
{
    LOP_GEOMETRY_POINT = 1,             /* One vertex. */
    LOP_GEOMETRY_POLYLINE = 2,          /* The vertices of the line; closed joins the last vertex with the first one. */
    LOP_GEOMETRY_TEXT = 3,              /* One vertex, the position of the text; text contains the text. */
    LOP_GEOMETRY_MEASUREMENT = 4        /* The defined vertices of an angle, azimuth or dimension measurement, in drawing order. */
} lop_geometry_type;

/* Describes a vector geometry of the drawing registry (measurements and loaded or drawn vector layers). */
typedef struct lop_geometry
{
    uint32_t type;                      /* lop_geometry_type */
    uint32_t vertex_count;
    double const* xyz;                  /* vertex_count XYZ triples in world coordinates. */
    int closed;
    char const* layer;                  /* The vector layer of the geometry; empty if the geometry has no layer. */
    char const* text;                   /* The content of a LOP_GEOMETRY_TEXT geometry; empty for other types. */
} lop_geometry;

/* Identifies the type of a geometry of lop_command_host::show_temporary_geometries. */
typedef enum lop_temporary_geometry_type
{
    LOP_TEMPORARY_POLYLINE = 1,     /* Two or more vertices; nonzero closed joins the last vertex with the first one when vertex_count > 2. */
    LOP_TEMPORARY_CROSS = 2,        /* One vertex: the centre of a cross whose horizontal and vertical arms each measure
                                     * cross_arm_length_or_text_line_height_pixels physical pixels from end to end at any zoom. */
    LOP_TEMPORARY_TEXT = 3          /* One vertex: one line of text whose line height measures cross_arm_length_or_text_line_height_pixels
                                     * physical pixels at any zoom; the line starts half that number of pixels to the right of the vertex and
                                     * is vertically centred on it. */
} lop_temporary_geometry_type;

/* Describes a geometry of lop_command_host::show_temporary_geometries. */
typedef struct lop_temporary_geometry
{
    uint32_t type;                  /* lop_temporary_geometry_type */
    double const* xyz;              /* vertex_count XYZ triples of finite world coordinates, like lop_point. */
    uint32_t vertex_count;
    int closed;                     /* LOP_TEMPORARY_POLYLINE; ignored for the other types. */
    uint32_t color_argb;            /* 0xAARRGGBB, where AA is the opacity (0xFF is opaque); 0 uses the colour of the temporary line of
                                     * show_temporary_line, opaque red. A nonzero value with AA = 0 is invalid. */
    double cross_arm_length_or_text_line_height_pixels; /* LOP_TEMPORARY_CROSS: the length of each arm; LOP_TEMPORARY_TEXT: the line
                                     * height; in physical pixels, a finite number greater than 0. Ignored for polylines. */
    char const* text;               /* LOP_TEMPORARY_TEXT: a non-empty UTF-8 text; lop draws the characters above U+00FF as '?'. Ignored for
                                     * the other types. */
} lop_temporary_geometry;

/* Maximum number of geometries of a show_temporary_geometries call. */
#define LOP_MAX_TEMPORARY_GEOMETRIES 1024u
/* Maximum sum of the vertex_count values of the geometries of a show_temporary_geometries call. */
#define LOP_MAX_TEMPORARY_GEOMETRY_VERTICES 1048576u

/* Describes an event. Pointer fields remain valid until on_event returns; points belongs to the command (see lop_points). */
typedef struct lop_event
{
    uint32_t size_bytes;                /* sizeof(lop_event) when lop was compiled, in bytes; fields beyond size_bytes are absent. */
    uint32_t type;                      /* lop_event_type */

    uint32_t mouse_button;              /* lop_mouse_button */
    int double_click;                   /* Nonzero for the second press of a double click. */
    int is_any_mouse_button_pressed;    /* Nonzero while a mouse button is pressed during LOP_EVENT_MOUSE_MOVE. */
    double x;                           /* Cursor position in physical pixels of the view, from its top left corner. */
    double y;
    int32_t wheel_delta;                /* Wheel rotation in Windows units: 120 per notch, positive away from the user. */
    int control;                        /* Nonzero if Ctrl is pressed. */
    int shift;                          /* Nonzero if Shift is pressed. */
    uint32_t key;                       /* LOP_KEY_* */

    uint64_t request_id;
    uint32_t request_type;              /* lop_request_type */
    int result;                         /* LOP_OK, LOP_CANCELLED or LOP_FAILED; lop logs the reason of LOP_FAILED in the Output panel. */
    int has_coordinates;                /* Zero in LOP_EVENT_REQUEST_PREVIEW if there is no point under the cursor. */
    double coordinates[3];              /* LOP_REQUEST_POINT and LOP_EVENT_REQUEST_PREVIEW: world coordinates. */
    lop_point point;                    /* LOP_REQUEST_CLOUD_POINT */
    lop_geometry const* geometry;       /* LOP_REQUEST_GEOMETRY */
    lop_points points;                  /* LOP_REQUEST_POINTS_IN_SPHERE; release them with lop_command_host::release_points. */
    lop_cloud const* clouds;            /* LOP_REQUEST_CLOUDS */
    uint32_t cloud_count;
    char const* file_path;              /* LOP_REQUEST_FILE and LOP_REQUEST_SAVE_FILE_PATH: the path of the chosen file. */
    int accepted;                       /* LOP_REQUEST_DIALOG and LOP_REQUEST_ORIGINAL_TO_WORLD_TRANSFORM_CHANGE: nonzero if the user pressed the
                                         * primary button (for the transformation change, if lop has also written the transformation), zero for
                                         * the close button or Esc. */
    char const* crs_wkt;                /* LOP_REQUEST_CRS: the WKT (Well-Known Text) of the chosen coordinate reference system. */
    char const* panel_id;               /* LOP_EVENT_PANEL_ACTION and LOP_EVENT_PANEL_VALUE_CHANGED: the lop_panel::id of the panel. */
    char const* panel_action_name;      /* LOP_EVENT_PANEL_ACTION: the name of Actions[name], for example "identify_selected_point". */
    char const* panel_value_key;        /* LOP_EVENT_PANEL_VALUE_CHANGED: the key of Values, for example "selected_point_index". */
    lop_ui_value panel_value;           /* LOP_EVENT_PANEL_VALUE_CHANGED: the value that the binding assigned to Values[panel_value_key];
                                         * LOP_UI_VALUE_OTHER for a value that is not a double, an integer, a boolean or a text, such as the
                                         * selected item of a list. For LOP_UI_VALUE_TEXT, panel_value.text remains valid until on_event
                                         * returns. */
} lop_event;

/* Provides the functions of a command instance from create until destroy returns; pass context as the first argument to each function.
 * Every function requires the user interface thread and returns without waiting; a call from another thread does nothing and returns the
 * failure value. lop delivers the events produced during an on_event call, such as LOP_EVENT_FINISH after finish, after that call returns:
 * on_event is never called while another on_event call of the same instance runs. */
typedef struct lop_command_host
{
    void* context;

    /* Writes message as a line in the Output panel. */
    void (*log)(void* context, char const* message);

    /* Displays message in the status bar; NULL or an empty string clears it, as does the finish of the command. */
    void (*status)(void* context, char const* message);

    /* Removes the command from the command stack, like Esc. lop delivers LOP_EVENT_LOST_FOCUS, completes every pending request with
     * LOP_CANCELLED and then delivers LOP_EVENT_FINISH; the background tasks of the command keep running. Calls after the first one do nothing. */
    void (*finish)(void* context);

    /* Replaces the temporary polyline with count world-coordinate XYZ vertices; count zero removes it and nonzero closed joins the endpoints
     * when count > 2. lop removes the line when the command finishes. */
    void (*show_temporary_line)(void* context, double const* xyz, uint32_t count, int closed);

    /* Returns the displayed class name: the user-defined name or the localized ASPRS name; returns an empty string if unnamed.
     * The returned string remains valid until destroy returns. */
    char const* (*class_name)(void* context, uint8_t classification);

    /* Replaces the current selection with the points at indices[0..index_count), selecting each point once from unreleased points that a
     * request_points_in_sphere of this command delivered; the selection persists after the command for delete or classify actions. Zero
     * index_count clears the selection and permits NULL points and indices. Returns LOP_OK, LOP_CANCELLED without changing the selection
     * after finish or the closure of a selected cloud, or LOP_FAILED for invalid points/indices or out-of-range indices. */
    int (*select_points)(void* context, lop_points const* points, uint64_t const* indices, uint64_t index_count);

    /* Releases points delivered by LOP_EVENT_REQUEST_COMPLETED and clears the structure; retained column pointers become invalid. */
    void (*release_points)(void* context, lop_points* points);

    /* Shows the task name in the Background tasks panel and calls work(user_data, task host) on a thread of lop. Returns LOP_OK, or
     * LOP_FAILED without calling work after finish, for an empty name or NULL work. lop keeps the command instance until every task of the
     * command returns, so destroy is never called while work runs; user_data must remain valid until work returns. */
    int (*start_background_task)(void* context, char const* name, lop_background_task_function work, void* user_data);

    /* The request functions return a nonzero request_id; lop delivers its answer later in LOP_EVENT_REQUEST_COMPLETED with that request_id.
     * They return 0 without starting the request after finish or for invalid arguments. A command has at most one pending request among
     * request_point, request_cloud_point and request_geometry: starting a second request of these types while one is pending
     * completes the second request with LOP_FAILED. cancel_point_or_geometry_request cancels the pending one without finishing the command. */

    /* Starts a request that displays prompt; lop completes the request when the user clicks a point or confirms one with the snap command:
     * completes with
     * coordinates. With nonzero preview, each mouse move delivers LOP_EVENT_REQUEST_PREVIEW with the point under the cursor. A click
     * without a point under the cursor does not complete the request; Esc that on_event does not consume completes it with LOP_CANCELLED. */
    uint64_t (*request_point)(void* context, char const* prompt, int preview);

    /* Like request_point, and then looks for the undeleted point of a visible cloud nearest to the chosen coordinates, at 0.05 cloud units
     * or less: completes with point. If there is none, lop displays a notice with prompt and keeps the request pending. */
    uint64_t (*request_cloud_point)(void* context, char const* prompt, int preview);

    /* Starts a request that displays prompt; lop completes the request when the user clicks a visible vector geometry: completes with
     * geometry. */
    uint64_t (*request_geometry)(void* context, char const* prompt);

    /* Loads undeleted points at distance <= radius from center in cloud coordinates, restricted by isolation, from the still-open clouds of
     * cloud_ids[0..cloud_count), including hidden clouds, or from the visible clouds if cloud_count is zero: completes with points.
     * Returns 0 for NULL center, nonfinite coordinates/radius, negative radius, or nonzero cloud_count with NULL cloud_ids. */
    uint64_t (*request_points_in_sphere)(void* context, double const center[3], double radius, uint32_t const* cloud_ids, uint32_t cloud_count);

    /* Describes the open clouds in Layers panel order: completes with clouds and cloud_count. */
    uint64_t (*request_clouds)(void* context);

    /* Shows the file picker with the extensions[0..extension_count), each with its period, for example ".gpkg": completes with file_path, or with
     * LOP_CANCELLED if the user closes the picker. Zero extension_count permits every file. */
    uint64_t (*request_file)(void* context, char const* const* extensions, uint32_t extension_count);

    /* The panel and dialog functions use the identifiers that lop_panel and lop_dialog declare. show_panel, set_ui_value, get_ui_value and
     * set_ui_rows return LOP_OK on success or LOP_FAILED on failure: no extension declares the identifier, the license module of the panel or
     * dialog is not enabled, or an argument is invalid. show_dialog returns a request ID, or zero if the request cannot start. lop writes the
     * exceptions of the host in the Output panel. */

    /* Shows the panel: if the panel is not in the layout, lop docks it at its initial side; then lop activates it. For a panel of
     * lop_extension::command_panels, only the instance that owns the panel can call show_panel, which then activates the panel; any other
     * caller receives LOP_FAILED. */
    int (*show_panel)(void* context, char const* panel_id);

    /* Assigns a LOP_UI_VALUE_REAL, LOP_UI_VALUE_INTEGER, LOP_UI_VALUE_BOOLEAN or LOP_UI_VALUE_TEXT value to Values[key] of the panel or
     * dialog; while the dialog is open, to the copy that the user edits. */
    int (*set_ui_value)(void* context, char const* panel_or_dialog_id, char const* key, lop_ui_value const* value);

    /* Fills value with Values[key] of the panel or dialog; while the dialog is open, with the value of the copy that the user edits. The text
     * remains valid until the next get_ui_value call or destroy. */
    int (*get_ui_value)(void* context, char const* panel_or_dialog_id, char const* key, lop_ui_value* value);

    /* Replaces the rows of Lists[list_key] of the panel or dialog with rows[0..row_count); lop copies the rows before returning. */
    int (*set_ui_rows)(void* context, char const* panel_or_dialog_id, char const* list_key, lop_ui_row const* rows, uint32_t row_count);

    /* Starts a request that shows the dialog bound to a copy of its values: completes with accepted. Only if the user presses the primary
     * button does lop copy the edited values to Values of the dialog. Finishing the command closes the dialog. */
    uint64_t (*show_dialog)(void* context, char const* dialog_id);

    /* Returns the UTF-8 path that the configuration page held for the LOP_PARAMETER_FILE_PATH parameter parameter_id when lop created the
     * command, or an empty string if the user has not chosen a file; lop does not check that the file still exists. Returns NULL if the
     * command declares no LOP_PARAMETER_FILE_PATH parameter with that ID. The string remains valid until destroy returns. */
    char const* (*parameter_file_path)(void* context, char const* parameter_id);

    /* Fills directory_path with the folder where this extension keeps the files that do not belong to a cloud,
     * "%LOCALAPPDATA%\lop\extension-data\<extension id>", and creates the folder; the extension id is the one of
     * lop_task_host::get_cloud_extension_data_directory_path. Calls that return LOP_OK set *directory_path to the same
     * pointer for this command instance; the string remains valid until destroy returns.
     * Returns LOP_OK, or LOP_FAILED with a logged reason for NULL directory_path or a folder that cannot be created. */
    int (*get_extension_data_directory_path)(void* context, char const** directory_path);

    /* Starts an animation that moves the camera, with its current view direction, to distance_cloud_units cloud units from point, in world
     * coordinates like lop_point and lop_tile, and returns before the animation ends. Returns LOP_OK, or LOP_FAILED for NULL point, a
     * nonfinite coordinate or a distance that is not a finite number greater than 0. */
    int (*bring_camera)(void* context, double const point[3], double distance_cloud_units);

    /* Starts a request that shows the save file picker with suggested_file_name in the file name box and extension as its only file type,
     * named file_type_name: completes with file_path, the path that the user chose, or with LOP_CANCELLED if the user closes the picker;
     * the command writes the file. suggested_file_name is a file name without folder and without '<', '>', ':', '"', '/',
     * '\', '|', '?', '*' or control characters; NULL or "" leaves the box empty. file_type_name is the name of the file type in the
     * language that lop_register_extension received, for example "Libro de Excel", with at most LOP_MAX_FILE_TYPE_NAME_BYTES bytes of
     * UTF-8 and without control characters; NULL or "" names the type with extension without its period and in uppercase, for example
     * "XLSX". extension has a period followed by at least one character, without ';', '*', '?' or white space, for example ".xlsx".
     * Returns 0 for any other suggested_file_name, file_type_name or extension. */
    uint64_t (*request_save_file_path)(void* context, char const* suggested_file_name, char const* file_type_name, char const* extension);

    /* Starts a request that replaces the original_to_world_transform of the open local cloud cloud_id with the row-major 4x4 similarity
     * original_to_world_transform, which maps the coordinates of original_path to world coordinates, and, if crs_wkt is neither NULL nor
     * empty, the coordinate reference system of the cloud with crs_wkt. lop shows a confirmation dialog that lists the other open local
     * clouds and 3D models with the same transformation as cloud_id, each with an unchecked check box. If the user confirms, lop writes
     * the transformation in "<original_path>.transform.json" of cloud_id and of the checked layers and completes the request with
     * accepted set to nonzero. If lop has written the transformation of at least one cloud, lop finishes the command
     * (LOP_EVENT_LOST_FOCUS and LOP_EVENT_FINISH) right after completing the request. lop reopens the document with the new transformation
     * of each changed cloud once the level of detail of that transformation exists: at once if it already exists, or when a background
     * task creates it. The reopening delivers LOP_CLOUD_CLOSING and LOP_CLOUD_ADDED for every cloud and assigns new cloud IDs. Until then,
     * the cloud stays open with its previous transformation and cloud ID; if the user cancels the background task or the creation fails,
     * lop restores the previous "<original_path>.transform.json". If the user closes the dialog, completes with accepted set to zero.
     * Completes with LOP_FAILED and a logged reason for a closed cloud, a remote cloud, a catalog cloud, a cloud reserved by a background
     * task, a crs_wkt that is not a valid WKT, a crs_wkt whose horizontal component, or whose vertical component when both systems have
     * one, is not equivalent to that of another open cloud with a known non-local system (the rule lop applies when it opens clouds), or a
     * write failure.
     * Returns 0 for NULL original_to_world_transform or a matrix that is not a similarity: 16 finite values, bottom row (0, 0, 0, 1) and
     * positive determinant of the 3x3 linear part. The first row of the linear part has positive squared norm; the squared norms of the
     * three rows differ from the first squared norm by at most 1e-9 times that norm, and the absolute dot products of distinct rows do not
     * exceed the same limit. */
    uint64_t (*request_original_to_world_transform_change)(void* context, uint32_t cloud_id, double const original_to_world_transform[16],
        char const* crs_wkt);

    /* Starts a request that shows the coordinate reference system picker of lop with prompt as its subtitle and initial_crs_wkt
     * preselected (NULL or empty preselects nothing): completes with crs_wkt, or with LOP_CANCELLED if the user closes the picker. */
    uint64_t (*request_crs)(void* context, char const* prompt, char const* initial_crs_wkt);

    /* Like lop_task_host::get_cloud_extension_data_directory_path, on the user interface thread; the string remains valid until the next
     * call to this function or until destroy returns. */
    int (*get_cloud_extension_data_directory_path)(void* context, uint32_t cloud_id, char const** directory_path);

    /* Replaces the temporary geometries of the command with geometries[0..count); count zero removes them and permits NULL geometries. lop
     * copies the geometries before returning, draws them over the clouds and the other geometries of the view, with lines one pixel wide, and
     * removes them when the command finishes. They are independent of the polyline of show_temporary_line. lop ignores the call, keeps the
     * previous geometries and writes the reason in the Output panel for NULL geometries with nonzero count, more than
     * LOP_MAX_TEMPORARY_GEOMETRIES geometries or more than LOP_MAX_TEMPORARY_GEOMETRY_VERTICES vertices in total, and for a geometry with an
     * unknown type, a vertex_count below 2 for a polyline or other than 1 for a cross or a text, NULL xyz, a nonfinite coordinate, a nonzero
     * color_argb with AA = 0, a cross_arm_length_or_text_line_height_pixels that is not a finite number greater than 0 for a cross or a
     * text, or a NULL or empty text for a text. After finish, the call does nothing. */
    void (*show_temporary_geometries)(void* context, lop_temporary_geometry const* geometries, uint32_t count);

    /* Completes the pending request of request_point, request_cloud_point or request_geometry of this command, if there is one, with
     * LOP_CANCELLED, without finishing the command: lop removes the prompt from the status bar at once and delivers LOP_EVENT_REQUEST_COMPLETED
     * after the current on_event returns. A new request of those types can start in the same on_event call. Returns 1 if lop has cancelled a
     * request, or 0 if the command has no such request pending or has finished. */
    int (*cancel_point_or_geometry_request)(void* context);

    /* SDK 0.6. Returns the UTF-8 text that the configuration page held for the LOP_PARAMETER_TEXT parameter parameter_id when lop created
     * the command: the default_text of the declaration until the user changes it, or an empty string. Returns NULL if the command declares
     * no LOP_PARAMETER_TEXT parameter with that ID. The string remains valid until destroy returns. */
    char const* (*parameter_text)(void* context, char const* parameter_id);

    /* SDK 0.7. Asks lop to run the command with the ID command_id after the current on_event call returns: the lop_command::id that a DLL
     * or Python script declares, for example "lop.orientation.orient_cloud", or the ID of a command of lop, for example "lop.view.zoom-extents";
     * lop ignores case. Starting a requested command that cancels previous commands finishes the calling command unless the calling command
     * owns its panel; every command declared by a DLL cancels previous commands. If command_id identifies the calling command, lop finishes the
     * calling command when it processes the queued request. If the license module of the command is not enabled, lop shows the license notice instead of running it. Returns LOP_OK if lop has queued the command,
     * or LOP_FAILED, with the reason in the Output panel, for a NULL or empty command_id, an unknown ID, a command whose requirements do not
     * hold or that requires an open document when no document is open, or after finish. */
    int (*run_command)(void* context, char const* command_id);
} lop_command_host;

/* Maximum length of the file_type_name of lop_command_host::request_save_file_path, in bytes of UTF-8, without the terminating null. */
#define LOP_MAX_FILE_TYPE_NAME_BYTES 128u

typedef struct lop_command lop_command;

/* Creates a command instance when the user launches the command; parameters contains one value per declared parameter, in declaration
 * order, valid until create returns. host remains valid until destroy returns. Returns the instance passed to on_event and destroy, or NULL
 * after logging the reason with host->log; lop then does not push the command. Until LOP_EVENT_START, finish, start_background_task and the
 * request functions fail. */
typedef void* (*lop_command_create_function)(lop_command const* command, double const* parameters, lop_command_host const* host);

/* Handles an event on the user interface thread and returns 1 if it consumes the event, or 0 otherwise and for unknown event types. */
typedef int (*lop_command_event_function)(void* instance, lop_event const* event);

/* Releases the instance on the user interface thread, after LOP_EVENT_FINISH and after every background task of the command returned. */
typedef void (*lop_command_destroy_function)(void* instance);

/* The host enables and runs a command only when every declared LOP_REQUIRES_* condition holds. */
#define LOP_REQUIRES_OPEN_CLOUD     (1u << 0)   /* Requires at least one open cloud. */
#define LOP_REQUIRES_VISIBLE_CLOUD  (1u << 1)   /* Requires at least one visible open cloud, as needed by request_point. */
#define LOP_REQUIRES_EDITABLE_CLOUD (1u << 2)   /* Requires at least one open cloud with an edit log and original point indices, as needed to change points. */

/* Help page: with this command active, pressing F1 in the main window or in a dialog box opens https://www.digi21.net/Ayuda/p/<id> in the
 * browser, where <id> is lop_command::id.
 * With the configuration page of the parameters of the command selected, F1 opens https://www.digi21.net/Ayuda/p/<extension ID>: the ID
 * of the extension in the store, or the file name of the DLL or Python script without its extension outside the store. To publish a page, fork the
 * repository github.com/digi21/docs, add a Markdown page under src/lot-of-points/ that contains the comment <!-- id: <id> -->, and open a
 * pull request against digi21/docs. Until the page is published, digi21.net returns HTTP status
 * code 404 (Not Found) for requests to that address. */
struct lop_command
{
    char const* id;             /* Uses a stable ID unique across DLLs, containing ASCII letters, digits, '.', '_' or '-', for example example.point_data. */
    char const* name;
    /* At most one of existing_tab_id and new_tab_title contains non-whitespace text. A command without tab has no ribbon button: a control
     * bound to Commands[id] of a panel launches it. */
    char const* existing_tab_id;    /* Names a built-in tab: lop.tab.view, lop.tab.draw, lop.tab.edit or lop.tab.classification. */
    char const* new_tab_title;      /* Names a tab shared by matching titles across DLLs; new tabs follow the built-in tabs. */
    char const* ribbon_section;     /* Names a section shared by this DLL's commands with the same tab and section; DLL sections follow built-in sections.
     * Each DLL has separate sections; an empty ribbon_section uses the DLL file name. */
    char const* description;    /* Supplies the button tooltip. */
    unsigned char const* icon_png;  /* Supplies a 32 x 32 pixel PNG, or NULL for the default icon; the host uses its alpha and applies the theme colour.
     * Use black on transparent; 25 % alpha matches the secondary tone of built-in icons. */
    uint32_t icon_png_size;     /* Specifies the PNG byte count, at most 1,048,576 bytes. */
    lop_parameter const* parameters;
    uint32_t parameter_count;
    lop_command_create_function create;
    lop_command_event_function on_event;
    lop_command_destroy_function destroy;
    void* user_data;            /* Stores DLL-owned data available through the command argument to create. */
    uint32_t requirements;      /* Combines LOP_REQUIRES_* flags; zero imposes no cloud requirements. */
    /* Names the licensed module that the command requires, as prefix.name in letters 'a' to 'z', digits '0' to '9', '.' and '-'; NULL or an
     * empty string requires no license. The host shows and runs the command only while the device license enables that module; store
     * extensions name the module of their offer, so the requirement also holds for a dynamic-link library (DLL)
     * copied outside the extension folder. */
    char const* module;
};

typedef enum lop_panel_dock_side
{
    LOP_PANEL_DOCK_LEFT = 0,
    LOP_PANEL_DOCK_RIGHT = 1,
    LOP_PANEL_DOCK_BOTTOM = 2
} lop_panel_dock_side;

/* Declares a dockable panel. lop creates its content with XamlReader.Load from xaml and binds it to a model with Values[key], Lists[key],
 * Commands[id] and Actions[name]. Commands[id], where id is the lop_command::id of a command of any DLL, launches that command, which reads
 * the values with get_ui_value. Actions[name] delivers LOP_EVENT_PANEL_ACTION to the command that owns the panel (lop_command_panel); in a
 * panel, a control bound to Actions[name] is disabled if no lop_command_panel associates the panel with a command or if no command instance
 * currently owns the panel. The XAML has one root element and admits neither x:Class, x:Bind nor event attributes. */
typedef struct lop_panel
{
    char const* id;                     /* A stable ID with the format prefix.name, in ASCII letters, digits, '.', '_' and '-', with at least one '.';
                                         * lop saves the layout of the panel with it. */
    char const* title;
    char const* xaml;
    uint32_t initial_dock_side;         /* lop_panel_dock_side: lop docks the panel there when the panel is not in the saved layout. */
    char const* module;                 /* The license module that the panel requires, with the format of lop_command::module; NULL or empty for none. */
} lop_panel;

/* Declares a dialog with the XAML contract of lop_panel; show_dialog shows it. */
typedef struct lop_dialog
{
    char const* id;                     /* Uses the format of lop_panel::id and differs from the ID of every panel. */
    char const* title;
    char const* xaml;
    char const* primary_button_text;    /* Text of the button that accepts the dialog. */
    char const* close_button_text;      /* Text of the button that closes the dialog without accepting it, like Esc. */
    char const* module;
} lop_dialog;

/* Identifies the cloud notices that lop delivers to lop_extension::on_cloud_event on the user interface thread; on_cloud_event must
 * ignore unknown types. For each extension and cloud_id, lop alternates the two notices, starting with LOP_CLOUD_ADDED. */
typedef enum lop_cloud_event_type
{
    LOP_CLOUD_ADDED = 1,    /* Delivered after lop opens the cloud, and for each open cloud when lop loads or enables the extension
                             * (lop loads every Python script again when the set of enabled scripts changes). */
    LOP_CLOUD_CLOSING = 2   /* Delivered before lop closes the cloud, and before lop disables or reloads the extension,
                             * for each cloud for which the extension received LOP_CLOUD_ADDED; cloud_id is still open until
                             * on_cloud_event returns. Not delivered when lop exits. */
} lop_cloud_event_type;

/* Provides the functions of a cloud notice until on_cloud_event returns; every function requires the user interface thread. */
typedef struct lop_extension_host
{
    void* context;
    void (*log)(void* context, char const* message);
    /* Like lop_command_host::start_background_task; the task has no command, and lop keeps the DLL loaded until work returns. */
    int (*start_background_task)(void* context, char const* name, lop_background_task_function work, void* user_data);
    /* Like the lop_task_host function with the same name; the string remains valid until the next call to
     * get_cloud_extension_data_directory_path during the same on_cloud_event call, or until on_cloud_event returns. */
    int (*get_cloud_extension_data_directory_path)(void* context, uint32_t cloud_id, char const** directory_path);
    /* Starts reading the GeoPackage tables as Layers panel layers on a thread of lop and returns without waiting for the read: LOP_OK if
     * the read starts, is already in progress or the file is already loaded, LOP_FAILED with a logged reason for an empty path. lop writes a
     * read failure in the Output panel. close_geopackage discards a read in progress of the same file. */
    int (*load_geopackage)(void* context, char const* geopackage_file_path);
    /* Like lop_task_host::close_geopackage. */
    int (*close_geopackage)(void* context, char const* geopackage_file_path);
    /* Like lop_command_host::get_extension_data_directory_path; calls that return LOP_OK during the same on_cloud_event call
     * set *directory_path to the same pointer; the string remains valid until on_cloud_event returns. */
    int (*get_extension_data_directory_path)(void* context, char const** directory_path);
    /* Like the lop_task_host function with the same name; the string remains valid until the next call to
     * find_cloud_extension_data_directory_path during the same on_cloud_event call, or until on_cloud_event returns. */
    int (*find_cloud_extension_data_directory_path)(void* context, uint32_t cloud_id, char const** directory_path);
    /* Like the lop_command_host functions with the same names; lop copies the rows before returning. */
    int (*set_ui_value)(void* context, char const* panel_or_dialog_id, char const* key, lop_ui_value const* value);
    int (*set_ui_rows)(void* context, char const* panel_or_dialog_id, char const* list_key, lop_ui_row const* rows, uint32_t row_count);
} lop_extension_host;

/* When lop reopens the document, lop delivers every LOP_CLOUD_CLOSING and then every LOP_CLOUD_ADDED, in Layers panel order; cloud is valid
 * until return. */
typedef void (*lop_cloud_event_function)(void* user_data, uint32_t type, lop_cloud const* cloud, lop_extension_host const* host);

/* Makes the panel panel_id the panel of the command owner_command_id, both declared by the same lop_extension. At application startup, lop neither shows
 * this panel nor restores it from the saved layout. Only the command instance that owns the panel may call show_panel; other callers
 * receive LOP_FAILED. After LOP_EVENT_START of an instance of owner_command_id, if the instance has not finished during START, lop docks the
 * panel at its initial side when the panel is not in the layout and activates it; from then on, until the instance finishes, the controls
 * bound to Actions[name] deliver LOP_EVENT_PANEL_ACTION to that instance, and lop delivers LOP_EVENT_PANEL_VALUE_CHANGED to the owning
 * instance when the user changes Values through XAML bindings. After delivering LOP_EVENT_FINISH to the instance, lop closes the panel;
 * when the user closes the panel, lop finishes the instance. Values and Lists of the panel keep their contents between instances. If
 * another instance of owner_command_id owns the panel when a new instance receives START, lop finishes the new instance after START and
 * writes the reason in the Output panel. */
typedef struct lop_command_panel
{
    char const* owner_command_id;       /* The lop_command::id of a command of the same lop_extension; each command owns at most one panel. */
    char const* panel_id;               /* The lop_panel::id of a panel of the same lop_extension; each panel has at most one owner. */
} lop_command_panel;

/* Contains what a DLL declares: the commands, in the order of their buttons within each section and of the sections within each tab, the
 * panels, the dialogs and the commands that own panels. lop reads the panels, the dialogs and command_panels once, when it starts.
 * lop fills the structure with zeros before calling lop_register_extension; fields added at the end in later versions of the SDK remain
 * zero for a DLL compiled against an SDK header that does not declare those fields. */
typedef struct lop_extension
{
    lop_command const* commands;
    uint32_t command_count;
    lop_panel const* panels;
    uint32_t panel_count;
    lop_dialog const* dialogs;
    uint32_t dialog_count;
    lop_cloud_event_function on_cloud_event;   /* NULL if the extension receives no cloud notices. */
    void* user_data;                           /* First argument of on_cloud_event. */
    lop_command_panel const* command_panels;   /* NULL, or a pointer to command_panel_count elements. */
    uint32_t command_panel_count;
} lop_extension;

/* Each plugin DLL must export lop_register_extension with this signature; the function receives the SDK version and lowercase ISO 639-1
 * language code (for example en or es). Declarations should use that language, with English as fallback; returns LOP_OK or LOP_FAILED for an
 * unsupported version. The DLL must retain the declared arrays and referenced data until unload. The host registers once per DLL load; DLL
 * file names determine alphabetical load order. */
typedef int (*lop_register_extension_function)(uint32_t api_version, char const* language, lop_extension* extension);

#define LOP_REGISTER_EXTENSION_FUNCTION_NAME "lop_register_extension"

#ifdef __cplusplus
}
#endif

#endif
