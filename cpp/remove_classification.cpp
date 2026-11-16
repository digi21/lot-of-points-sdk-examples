// Assigns class 1 (unclassified) to the points of editable clouds with original point indices, in a background task, and commits the changes
// as one undoable event.
#include "commands.hpp"
// Regenerate the embedded PNG with:
// tools/png_to_header.ps1 cpp/icons/remove_classification.png cpp/remove_classification_icon.h remove_classification_icon
#include "remove_classification_icon.h"

#include <format>
#include <iterator>

namespace
{
    void assign_unclassified_class(lop::TaskHost& task, bool const spanish)
    {
        auto const clouds = task.clouds();
        if (!clouds)
            return;
        auto const cloud_ids = editable_cloud_ids_with_original_point_indices(task, *clouds, spanish);
        if (cloud_ids.empty())
        {
            task.log(spanish ? "Quitar clasificación: no hay ninguna nube que se pueda clasificar." : "Remove classification: there is no cloud to classify.");
            return;
        }

        task.report_state(spanish ? "Quitando la clasificación" : "Removing the classification");
        // Classification depends on each point alone: the tile side does not affect the result and no overlap is needed.
        // process_tile allocates no memory; a positive estimate avoids the fallback used for zero.
        auto const summary = task.process_tiles(cloud_ids, { .tile_core_side_length_cloud_units = 1000.0, .concurrent = true, .estimated_peak_bytes_per_point = 1.0 },
            [](lop::Tile const& tile, lop::TileResult* const result, lop::TileHost const& host)
            {
                auto const classification = result->classification(tile);
                for (std::uint64_t i = 0; i < tile.point_count; ++i)
                {
                    classification[i] = 1;
                    if (0 == (i & 0xFFFF))
                    {
                        if (host.cancelled())
                            return;
                        host.report_tile_progress(static_cast<double>(i) / static_cast<double>(tile.point_count));
                    }
                }
            });
        if (!summary)
            return;
        task.log(spanish ? std::format("Quitar clasificación: {} puntos pasan a la clase 1 en {} nubes.", summary->reclassified_point_count, summary->changed_cloud_count)
                         : std::format("Remove classification: {} points go to class 1 in {} clouds.", summary->reclassified_point_count, summary->changed_cloud_count));
    }

    class RemoveClassificationCommand final : public lop::Command
    {
    public:
        explicit RemoveClassificationCommand(bool const spanish) : spanish_{ spanish } {}

        lop::Task<> on_start() override
        {
            auto const spanish = spanish_;
            host().run_in_background(spanish ? "Quitar clasificación" : "Remove classification", [spanish](lop::TaskHost& task) { assign_unclassified_class(task, spanish); });
            finish();
            co_return;
        }

    private:
        bool spanish_;
    };
}

lop::Declaration declare_remove_classification(bool const spanish)
{
    return lop::declare<RemoveClassificationCommand>({
        .id = "lop.example.remove_classification",
        .name = spanish ? "Quitar clasificación" : "Remove classification",
        .existing_tab_id = "lop.tab.classification",
        .ribbon_section = spanish ? "Algoritmos básicos" : "Basic algorithms",
        .description = spanish ? "Asigna la clase 1 (sin clasificar) a los puntos de las nubes editables que conservan los índices del archivo original."
                               : "Assigns class 1 (unclassified) to the points of the editable clouds that keep the indices of the original file.",
        .icon_png = { std::begin(remove_classification_icon), std::end(remove_classification_icon) },
        .requirements = LOP_REQUIRES_EDITABLE_CLOUD,
    }, spanish);
}
