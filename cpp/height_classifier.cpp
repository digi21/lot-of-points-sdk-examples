// Assigns ground_class when height above the cell minimum is <= max_height; otherwise assigns other_class. Runs in a background task over
// a square XY grid relative to the tile core minimum and commits changes as one undoable event.
#include "commands.hpp"
// Regenerate the embedded PNG with:
// tools/png_to_header.ps1 cpp/icons/height_classifier.png cpp/height_classifier_icon.h height_classifier_icon
#include "height_classifier_icon.h"

#include <cmath>
#include <format>
#include <iterator>
#include <limits>
#include <vector>

namespace
{
    // Tile callbacks share these parameters without modifying them.
    struct HeightClassificationParameters
    {
        double cell_side_length{};
        double max_height_above_cell_lowest_point{};
        std::uint8_t ground_class{};
        std::uint8_t other_class{};
    };

    void process_tile(HeightClassificationParameters const& values, lop::Tile const& tile, lop::TileResult const& result, lop::TileHost const& host)
    {
        if (0 == tile.point_count)
            return;
        auto const cell = values.cell_side_length;
        auto const origin_x = tile.core_min[0];
        auto const origin_y = tile.core_min[1];

        // Computes cell indices relative to the core minimum, including points in the overlap.
        double min_column_index{}, min_row_index{}, max_column_index{}, max_row_index{};
        for (std::uint64_t tile_point_index = 0; tile_point_index < tile.point_count; ++tile_point_index)
        {
            auto const column_index = std::floor((tile.x[tile_point_index] - origin_x) / cell);
            auto const row_index = std::floor((tile.y[tile_point_index] - origin_y) / cell);
            if (0 == tile_point_index || column_index < min_column_index) min_column_index = column_index;
            if (0 == tile_point_index || row_index < min_row_index) min_row_index = row_index;
            if (0 == tile_point_index || column_index > max_column_index) max_column_index = column_index;
            if (0 == tile_point_index || row_index > max_row_index) max_row_index = row_index;
        }

        auto const column_count = static_cast<std::size_t>(max_column_index - min_column_index) + 1;
        auto const row_count = static_cast<std::size_t>(max_row_index - min_row_index) + 1;
        auto const tile_grid_cell_index_for_point = [&](std::uint64_t const tile_point_index)
            {
                auto const tile_grid_column_index = static_cast<std::size_t>(std::floor((tile.x[tile_point_index] - origin_x) / cell) - min_column_index);
                auto const tile_grid_row_index = static_cast<std::size_t>(std::floor((tile.y[tile_point_index] - origin_y) / cell) - min_row_index);
                return tile_grid_row_index * column_count + tile_grid_column_index;
            };
        std::vector<double> minimum_elevations_by_cell(column_count * row_count, std::numeric_limits<double>::infinity());
        for (std::uint64_t tile_point_index = 0; tile_point_index < tile.point_count; ++tile_point_index)
        {
            auto& z = minimum_elevations_by_cell[tile_grid_cell_index_for_point(tile_point_index)];
            if (tile.z[tile_point_index] < z)
                z = tile.z[tile_point_index];
        }

        auto const result_classification = result.classification(tile);
        for (std::uint64_t tile_point_index = 0; tile_point_index < tile.point_count; ++tile_point_index)
        {
            auto const height = tile.z[tile_point_index] - minimum_elevations_by_cell[tile_grid_cell_index_for_point(tile_point_index)];
            result_classification[tile_point_index] = height <= values.max_height_above_cell_lowest_point ? values.ground_class : values.other_class;

            if (0 == (tile_point_index & 0xFFFF))
            {
                if (host.cancelled())
                    return;
                host.report_tile_progress(static_cast<double>(tile_point_index) / static_cast<double>(tile.point_count));
            }
        }
    }

    void classify_points_by_height(lop::TaskHost& task, HeightClassificationParameters const& values, bool const spanish)
    {
        auto const clouds = task.clouds();
        if (!clouds)
            return;
        auto const cloud_ids = editable_cloud_ids_with_original_point_indices(task, *clouds, spanish);
        if (cloud_ids.empty())
        {
            task.log(spanish ? "Clasificar por altura: no hay ninguna nube que se pueda clasificar." : "Classify by height: there is no cloud to classify.");
            return;
        }

        task.report_state(spanish ? "Clasificando" : "Classifying");
        // Each tile callback modifies its own result and allocated grid; shared parameters are read-only.
        // Estimates 8 bytes per point; the allocated grid uses 8 bytes per cell, including empty cells.
        auto const summary = task.process_tiles(cloud_ids, { .tile_core_side_length_cloud_units = 50.0, .tile_overlap_width_cloud_units = 10.0,
            .concurrent = true, .estimated_peak_bytes_per_point = 8.0 },
            [&values](lop::Tile const& tile, lop::TileResult* const result, lop::TileHost const& host) { process_tile(values, tile, *result, host); });
        if (!summary)
            return;
        task.log(spanish ? std::format("Clasificar por altura: {} puntos cambian de clase en {} nubes.", summary->reclassified_point_count, summary->changed_cloud_count)
                         : std::format("Classify by height: {} points change class in {} clouds.", summary->reclassified_point_count, summary->changed_cloud_count));
    }

    class HeightClassifierCommand final : public lop::Command
    {
    public:
        explicit HeightClassifierCommand(bool const spanish) : spanish_{ spanish } {}

        lop::Task<> on_start() override
        {
            HeightClassificationParameters const values{ host().parameter_value("cell_size"), host().parameter_value("max_height"),
                static_cast<std::uint8_t>(host().parameter_value("ground_class")), static_cast<std::uint8_t>(host().parameter_value("other_class")) };
            auto const spanish = spanish_;
            host().run_in_background(spanish ? "Clasificar por altura" : "Classify by height",
                [values, spanish](lop::TaskHost& task) { classify_points_by_height(task, values, spanish); });
            finish();
            co_return;
        }

    private:
        bool spanish_;
    };
}

lop::Declaration declare_height_classifier(bool const spanish)
{
    auto const unit = spanish ? "unidades de la nube" : "cloud units";
    // The English and Spanish declarations keep the same parameter IDs, order, types, ranges, defaults and units.
    return lop::declare<HeightClassifierCommand>({
        .id = "lop.example.height_classifier",
        .name = spanish ? "Clasificar por altura" : "Classify by height",
        .existing_tab_id = "lop.tab.classification",
        .ribbon_section = spanish ? "Algoritmos básicos" : "Basic algorithms",
        .description = spanish ? "Asigna la clase de suelo a los puntos que no superan la altura máxima sobre el mínimo de su celda y la otra clase a los que la superan."
                               : "Assigns the ground class to the points that do not exceed the maximum height above the minimum of their cell and the other class to the points that exceed it.",
        .icon_png = { std::begin(height_classifier_icon), std::end(height_classifier_icon) },
        .parameters = {
            { .id = "cell_size", .name = spanish ? "Lado de la celda" : "Cell side",
              .description = spanish ? "Lado de las celdas en las que se busca el punto más bajo." : "Side of the cells in which the lowest point is searched.",
              .default_value = 2.0, .minimum = 0.1, .maximum = 100.0, .unit = unit },
            { .id = "max_height", .name = spanish ? "Altura máxima sobre el punto más bajo de la celda" : "Maximum height above the lowest point of the cell",
              .description = spanish ? "Asigna la clase de suelo si la altura del punto sobre el mínimo de su celda es menor o igual que este valor."
                                     : "Assigns the ground class if the height of the point above the minimum of its cell is less than or equal to this value.",
              .default_value = 0.5, .minimum = 0.0, .maximum = 100.0, .unit = unit },
            { .id = "ground_class", .name = spanish ? "Clase de suelo" : "Ground class", .type = LOP_PARAMETER_INTEGER,
              .default_value = 2.0, .minimum = 0.0, .maximum = 255.0 },
            { .id = "other_class", .name = spanish ? "Clase para los puntos que superan la altura máxima" : "Class for points above the maximum height", .type = LOP_PARAMETER_INTEGER,
              .default_value = 1.0, .minimum = 0.0, .maximum = 255.0 },
        },
        .requirements = LOP_REQUIRES_EDITABLE_CLOUD,
    }, spanish);
}
