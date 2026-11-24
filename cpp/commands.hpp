// Declares the example commands with Spanish text when spanish is true, otherwise English text.
#pragma once

#include <lop/sdk.hpp>

#include <cstdint>
#include <vector>

[[nodiscard]] lop::Declaration declare_information(bool spanish);
[[nodiscard]] lop::Declaration declare_remove_classification(bool spanish);
[[nodiscard]] lop::Declaration declare_height_classifier(bool spanish);
[[nodiscard]] lop::Declaration declare_point_data(bool spanish);
[[nodiscard]] lop::Declaration declare_select_object(bool spanish);

// Declares Record camera path and Play camera path with the texts of language (see camera_path_texts.hpp).
[[nodiscard]] std::vector<lop::Declaration> declare_camera_path_commands(lop::Language language);

// Declares the command that shows the Points around panel and the two commands without ribbon button that the panel launches.
[[nodiscard]] std::vector<lop::Declaration> declare_points_panel_commands(bool spanish);
[[nodiscard]] lop::Panel declare_points_panel(bool spanish);
[[nodiscard]] lop::Dialog declare_points_panel_options_dialog(bool spanish);

// Returns the IDs of the clouds whose editable and has_original_point_indices fields are true, hidden clouds included, and logs each excluded cloud.
[[nodiscard]] std::vector<std::uint32_t> editable_cloud_ids_with_original_point_indices(lop::TaskHost const& task, std::vector<lop::Cloud> const& clouds,
    bool spanish);
