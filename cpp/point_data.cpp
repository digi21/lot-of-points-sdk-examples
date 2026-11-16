// Logs the coordinates and attributes of each clicked point until the command finishes.
#include "commands.hpp"
// Regenerate the embedded PNG with:
// tools/png_to_header.ps1 cpp/icons/point_data.png cpp/point_data_icon.h point_data_icon
#include "point_data_icon.h"

#include <format>
#include <iterator>
#include <string>

namespace
{
    class PointData final : public lop::Command
    {
    public:
        explicit PointData(bool const spanish) : spanish_{ spanish } {}

        // Each co_await waits for a click without blocking lop; Esc finishes the command and ask_cloud_point returns std::nullopt.
        lop::Task<> on_start() override
        {
            auto const prompt = spanish_ ? "Pulsa un punto de una nube; Esc para terminar" : "Click a point of a cloud; Esc to finish";
            while (auto const point = co_await host().ask_cloud_point(prompt))
                host().log(describe(*point, host().class_name(point->classification)));
        }

    private:
        [[nodiscard]] std::string describe(lop::Point const& p, std::string const& class_name) const
        {
            auto const classification = class_name.empty() ? std::to_string(p.classification) : std::format("{} {}", p.classification, class_name);
            auto const original_point_index_text = LOP_UNKNOWN_ORIGINAL_POINT_INDEX == p.original_point_index
                ? std::string{ spanish_ ? "desconocido" : "unknown" } : std::to_string(p.original_point_index);

            if (spanish_)
                return std::format("X {:.3f}  Y {:.3f}  Z {:.3f} | nube {} | índice en el archivo original {} | clase {} | intensidad {} | retorno {} de {} | RGB {} {} {}",
                    p.x, p.y, p.z, p.cloud_id, original_point_index_text, classification, p.intensity, p.return_number, p.number_of_returns,
                    p.red, p.green, p.blue);
            return std::format("X {:.3f}  Y {:.3f}  Z {:.3f} | cloud {} | index in the original file {} | class {} | intensity {} | return {} of {} | RGB {} {} {}",
                p.x, p.y, p.z, p.cloud_id, original_point_index_text, classification, p.intensity, p.return_number, p.number_of_returns,
                p.red, p.green, p.blue);
        }

        bool spanish_;
    };
}

lop::Declaration declare_point_data(bool const spanish)
{
    return lop::declare<PointData>({
        .id = "lop.example.point_data",
        .name = spanish ? "Datos del punto" : "Point data",
        .existing_tab_id = "lop.tab.view",
        .ribbon_section = spanish ? "Algoritmos básicos" : "Basic algorithms",
        .description = spanish ? "Escribe en el panel de resultados las coordenadas y los atributos de los puntos que pulses."
                               : "Writes in the Output panel the coordinates and the attributes of the points you click.",
        .icon_png = { std::begin(point_data_icon), std::end(point_data_icon) },
        .requirements = LOP_REQUIRES_VISIBLE_CLOUD,     // ask_cloud_point requires at least one visible open cloud.
    }, spanish);
}
