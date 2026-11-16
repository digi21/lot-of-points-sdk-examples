// Registers the example commands, panel and dialog of lop_examples.dll.
#include "commands.hpp"

#include <format>
#include <string_view>
#include <vector>

namespace
{
    // Uses Spanish for es and English for other languages; declaration order determines button order within each section.
    [[nodiscard]] lop::Extension example_extension(std::string_view const language)
    {
        auto const spanish = language == "es";
        lop::Extension extension{ { declare_information(spanish), declare_remove_classification(spanish), declare_height_classifier(spanish),
            declare_point_data(spanish), declare_select_object(spanish) } };
        for (auto& declaration : declare_points_panel_commands(spanish))
            extension.commands.push_back(std::move(declaration));
        extension.panels.push_back(declare_points_panel(spanish));
        extension.dialogs.push_back(declare_points_panel_options_dialog(spanish));
        return extension;
    }
}

std::vector<std::uint32_t> editable_cloud_ids_with_original_point_indices(lop::TaskHost const& task, std::vector<lop::Cloud> const& clouds, bool const spanish)
{
    std::vector<std::uint32_t> ids;
    for (auto const& cloud : clouds)
    {
        if (cloud.editable && cloud.has_original_point_indices)
        {
            ids.push_back(cloud.cloud_id);
            continue;
        }
        auto const reason = !cloud.editable ? (spanish ? "no tiene registro de ediciones" : "it has no edit log")
                                            : (spanish ? "su nivel de detalle no guarda la posición de cada punto en el original"
                                                       : "its level of detail does not keep the position of each point in the original");
        task.log(spanish ? std::format("{}: excluida del procesamiento porque {}.", cloud.name, reason) : std::format("{}: excluded from processing because {}.", cloud.name, reason));
    }
    return ids;
}

LOP_SDK_EXTENSION(example_extension)
