// Declares the Points around panel and its dialog: ShowPointsPanelCommand, with a ribbon button, shows the panel. The panel buttons launch two
// commands without a ribbon button: CountPointsAroundCommand reads the panel radius and counts the points around a clicked point;
// EditPointsPanelOptionsCommand opens the radius dialog and copies the accepted radius to the panel.
#include "commands.hpp"
// Regenerate the embedded PNG with:
// tools/png_to_header.ps1 cpp/icons/point_data.png cpp/point_data_icon.h point_data_icon
#include "point_data_icon.h"

#include <format>
#include <iterator>
#include <string>
#include <variant>
#include <vector>

namespace
{
    constexpr auto PANEL_ID = "lop.example.points_panel";
    constexpr auto DIALOG_ID = "lop.example.points_panel_options";
    constexpr auto RADIUS = "radius";
    constexpr auto RESULT = "result";
    constexpr auto HISTORY = "history";
    constexpr double DEFAULT_RADIUS = 1.0;

    // Returns the radius in the panel or dialog, or the default radius if the value is not a positive number.
    [[nodiscard]] double radius_in(lop::Host const& host, char const* const panel_or_dialog_id)
    {
        auto const value = host.get_ui_value(panel_or_dialog_id, RADIUS);
        if (auto const* const real = std::get_if<double>(&value); real && *real > 0.0)
            return *real;
        if (auto const* const integer = std::get_if<std::int32_t>(&value); integer && *integer > 0)
            return *integer;
        return DEFAULT_RADIUS;
    }

    // Shows the panel and writes the default radius if the panel has none.
    class ShowPointsPanelCommand final : public lop::Command
    {
    public:
        lop::Task<> on_start() override
        {
            if (std::holds_alternative<std::monostate>(host().get_ui_value(PANEL_ID, RADIUS)))
                host().set_ui_value(PANEL_ID, RADIUS, DEFAULT_RADIUS);
            host().show_panel(PANEL_ID);
            finish();
            co_return;
        }
    };

    // Counts the points of the visible clouds around each clicked point until Esc, writing the result and a history row in the panel.
    class CountPointsAroundCommand final : public lop::Command
    {
    public:
        explicit CountPointsAroundCommand(bool const spanish) : spanish_{ spanish } {}

        lop::Task<> on_start() override
        {
            auto& host = this->host();
            auto const radius = radius_in(host, PANEL_ID);
            auto const prompt = spanish_ ? std::format("Pulsa un punto para contar los puntos cuya distancia al punto pulsado no supere {:g} unidades de la nube; pulsa Esc para terminar", radius)
                                         : std::format("Click a point to count the points whose distance from the clicked point is at most {:g} cloud units; press Esc to finish", radius);
            while (auto const center = co_await host.ask_point(prompt, [&host, radius](std::optional<lop::Point3> const& under_cursor)
                {
                    // The temporary line shows the sphere diameter along X, centered on the point under the cursor.
                    if (under_cursor)
                        host.show_temporary_line({ { under_cursor->x - radius, under_cursor->y, under_cursor->z }, { under_cursor->x + radius, under_cursor->y, under_cursor->z } });
                    else
                        host.show_temporary_line(std::span<lop::Point3 const>{});
                }))
            {
                auto const points = co_await host.points_in_sphere(*center, radius);
                if (!points)
                    co_return;
                auto const text = spanish_ ? std::format("{} puntos a una distancia máxima de {:g} unidades de la nube del punto X {:.3f} Y {:.3f} Z {:.3f}", points->size(), radius, center->x, center->y, center->z)
                                           : std::format("{} points within a distance of {:g} cloud units from point X {:.3f} Y {:.3f} Z {:.3f}", points->size(), radius, center->x, center->y, center->z);
                host.set_ui_value(PANEL_ID, RESULT, text);
                history_.push_back({ { "text", text } });
                host.set_ui_rows(PANEL_ID, HISTORY, history_);
            }
        }

    private:
        bool spanish_;
        // The history keeps the rows of this run of the command.
        std::vector<lop::UiRow> history_;
    };

    // Shows the dialog with the radius of the panel and, if the user accepts it, copies the radius of the dialog to the panel.
    class EditPointsPanelOptionsCommand final : public lop::Command
    {
    public:
        lop::Task<> on_start() override
        {
            auto& host = this->host();
            host.set_ui_value(DIALOG_ID, RADIUS, radius_in(host, PANEL_ID));
            if (co_await host.show_dialog(DIALOG_ID) == true)
                host.set_ui_value(PANEL_ID, RADIUS, radius_in(host, DIALOG_ID));
            finish();
        }
    };

    [[nodiscard]] std::string panel_xaml(bool const spanish)
    {
        return std::format(R"(<StackPanel xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" Padding="8" Spacing="8">
  <NumberBox AutomationProperties.AutomationId="radius" Header="{}" Value="{{Binding Values[radius], Mode=TwoWay}}" Minimum="0.05" Maximum="100" SpinButtonPlacementMode="Inline" />
  <Button AutomationProperties.AutomationId="count" Content="{}" Command="{{Binding Commands[lop.example.count_points_around]}}" />
  <Button AutomationProperties.AutomationId="options" Content="{}" Command="{{Binding Commands[lop.example.edit_points_panel_options]}}" />
  <TextBlock AutomationProperties.AutomationId="result" Text="{{Binding Values[result]}}" TextWrapping="Wrap" />
  <ListView AutomationProperties.AutomationId="history" ItemsSource="{{Binding Lists[history]}}" MinHeight="120">
    <ListView.ItemTemplate><DataTemplate><TextBlock Text="{{Binding [text]}}" /></DataTemplate></ListView.ItemTemplate>
  </ListView>
</StackPanel>)",
            spanish ? "Radio (unidades de la nube)" : "Radius (cloud units)", spanish ? "Contar los puntos alrededor de un punto" : "Count the points around a point",
            spanish ? "Opciones..." : "Options...");
    }

    [[nodiscard]] std::string dialog_xaml(bool const spanish)
    {
        return std::format(R"(<StackPanel xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" Spacing="8" MinWidth="320">
  <NumberBox AutomationProperties.AutomationId="dialogRadius" Header="{}" Value="{{Binding Values[radius], Mode=TwoWay}}" Minimum="0.05" Maximum="100" SpinButtonPlacementMode="Inline" />
</StackPanel>)", spanish ? "Radio (unidades de la nube)" : "Radius (cloud units)");
    }
}

std::vector<lop::Declaration> declare_points_panel_commands(bool const spanish)
{
    auto const section = spanish ? "Utilidades" : "Utilities";
    return {
        lop::declare<ShowPointsPanelCommand>({
            .id = "lop.example.show_points_panel",
            .name = spanish ? "Puntos alrededor" : "Points around",
            .existing_tab_id = "lop.tab.view",
            .ribbon_section = section,
            .description = spanish ? "Muestra el panel que cuenta los puntos alrededor de los puntos que pulses."
                                   : "Shows the panel that counts the points around the points you click.",
            .icon_png = { std::begin(point_data_icon), std::end(point_data_icon) },
        }),
        // The panel buttons launch the two following commands, which have no ribbon tab and therefore no ribbon button.
        lop::declare<CountPointsAroundCommand>({
            .id = "lop.example.count_points_around",
            .name = spanish ? "Contar los puntos alrededor" : "Count the points around",
            .requirements = LOP_REQUIRES_VISIBLE_CLOUD,
        }, spanish),
        lop::declare<EditPointsPanelOptionsCommand>({
            .id = "lop.example.edit_points_panel_options",
            .name = spanish ? "Opciones de Puntos alrededor" : "Points around options",
        }),
    };
}

lop::Panel declare_points_panel(bool const spanish)
{
    return { .id = PANEL_ID, .title = spanish ? "Puntos alrededor" : "Points around", .xaml = panel_xaml(spanish), .initial_dock_side = LOP_PANEL_DOCK_RIGHT };
}

lop::Dialog declare_points_panel_options_dialog(bool const spanish)
{
    return { .id = DIALOG_ID, .title = spanish ? "Opciones de Puntos alrededor" : "Points around options", .xaml = dialog_xaml(spanish),
        .primary_button_text = spanish ? "Aceptar" : "OK", .close_button_text = spanish ? "Cancelar" : "Cancel" };
}
