// Selects same-class points connected through cubic cells with 26-neighbour adjacency.
// Doubles the search radius when a component point is farther than max(0, radius - 4 * cell_side_length) from the clicked point, without exceeding max_search_radius.
#include "commands.hpp"
// Regenerate the embedded PNG with:
// tools/png_to_header.ps1 cpp/icons/select_object.png cpp/select_object_icon.h select_object_icon
#include "select_object_icon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <format>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace
{
    // Encodes each relative cell coordinate in 17 bits after adding CELL_OFFSET.
    // The maximum radius is 200 cloud units and the minimum cell side is 0.01 cloud units, so each axis spans at most 40,000 cells.
    constexpr std::int64_t CELL_BASE{ 1 << 17 };
    constexpr std::int64_t CELL_OFFSET{ 1 << 16 };

    [[nodiscard]] std::int64_t cell_key(std::int64_t const i, std::int64_t const j, std::int64_t const k) noexcept
    {
        return ((i + CELL_OFFSET) * CELL_BASE + (j + CELL_OFFSET)) * CELL_BASE + (k + CELL_OFFSET);
    }

    struct Object
    {
        std::vector<std::uint64_t> indices;  // Indices refer to the input points and are sorted in ascending order.
        bool reaches_border{};               // True if a selected point's distance from center exceeds max(0, radius - 4 * cell_side_length).
        bool found{};                        // True if the clicked cell contains points of the requested class.
    };

    // Returns the same-class component connected to the cell containing center.
    [[nodiscard]] Object find_object(lop::Points const& points, lop::Point const& center, std::uint8_t const classification,
        double const cell_side_length, double const radius)
    {
        auto const x = points.x();
        auto const y = points.y();
        auto const z = points.z();
        auto const classes = points.classification();

        std::vector<std::pair<std::int64_t, std::uint64_t>> by_cell;
        for (std::size_t i = 0; i < points.size(); ++i)
            if (classes[i] == classification)
                by_cell.emplace_back(cell_key(static_cast<std::int64_t>(std::floor((x[i] - center.x) / cell_side_length)),
                    static_cast<std::int64_t>(std::floor((y[i] - center.y) / cell_side_length)),
                    static_cast<std::int64_t>(std::floor((z[i] - center.z) / cell_side_length))), i);
        std::ranges::sort(by_cell);

        std::vector<std::int64_t> cells;
        for (auto const& [key, index] : by_cell)
            if (cells.empty() || cells.back() != key)
                cells.push_back(key);

        auto const find_cell = [&cells](std::int64_t const key) -> std::ptrdiff_t
            {
                auto const found = std::ranges::lower_bound(cells, key);
                return found != cells.end() && *found == key ? found - cells.begin() : -1;
            };

        Object object;
        auto const seed = find_cell(cell_key(0, 0, 0));
        if (seed < 0)
            return object;
        object.found = true;

        // Traverses occupied cells by breadth-first search using 26-neighbour adjacency.
        std::vector<char> joined(cells.size());
        std::deque<std::ptrdiff_t> pending{ seed };
        joined[static_cast<std::size_t>(seed)] = 1;
        while (!pending.empty())
        {
            auto const cell = cells[static_cast<std::size_t>(pending.front())];
            pending.pop_front();
            for (std::int64_t di = -1; di <= 1; ++di)
                for (std::int64_t dj = -1; dj <= 1; ++dj)
                    for (std::int64_t dk = -1; dk <= 1; ++dk)
                    {
                        auto const neighbour = find_cell(cell + (di * CELL_BASE + dj) * CELL_BASE + dk);
                        if (neighbour >= 0 && !joined[static_cast<std::size_t>(neighbour)])
                        {
                            joined[static_cast<std::size_t>(neighbour)] = 1;
                            pending.push_back(neighbour);
                        }
                    }
        }

        // Points in touching cells can be up to 2 * sqrt(3) * cell_side_length apart.
        // Uses four cell sides as the boundary margin when deciding whether to enlarge the search sphere.
        auto const inner = std::max(0.0, radius - 4.0 * cell_side_length);
        for (auto const& [key, index] : by_cell)
            if (joined[static_cast<std::size_t>(find_cell(key))])
            {
                object.indices.push_back(index);
                auto const dx = x[index] - center.x;
                auto const dy = y[index] - center.y;
                auto const dz = z[index] - center.z;
                if (dx * dx + dy * dy + dz * dz > inner * inner)
                    object.reaches_border = true;
            }
        std::ranges::sort(object.indices);
        return object;
    }

    class SelectObject final : public lop::Command
    {
    public:
        explicit SelectObject(bool const spanish) : spanish_{ spanish } {}

        // Each co_await waits without blocking lop: the click, then the points of each sphere.
        lop::Task<> on_start() override
        {
            auto& host = this->host();
            auto const cell_side_length = host.parameter_value("gap");
            auto const max_search_radius = host.parameter_value("max_size");
            // Requires a search radius greater than four cell sides so the inner sphere has a positive radius.
            if (4.0 * cell_side_length >= max_search_radius)
            {
                host.log(spanish_ ? std::format("El lado de la celda ({:g} en unidades de la nube) tiene que ser menor que la cuarta parte del radio máximo de búsqueda ({:g} en unidades de la nube).", cell_side_length, max_search_radius)
                                  : std::format("The cell side ({:g} in cloud units) must be less than a quarter of the maximum search radius ({:g} in cloud units).", cell_side_length, max_search_radius));
                finish();
                co_return;
            }
            auto const prompt = spanish_ ? "Pulsa un punto del objeto; Esc para terminar" : "Click a point of the object; Esc to finish";
            while (auto const point = co_await host.ask_cloud_point(prompt))
            {
                auto const classification = point->classification;
                auto const class_name = host.class_name(classification);
                auto const name = class_name.empty() ? std::to_string(classification) : std::format("{} {}", classification, class_name);

                for (auto radius = std::min(std::max(2.0, 10.0 * cell_side_length), max_search_radius);; radius = std::min(2.0 * radius, max_search_radius))
                {
                    host.status(spanish_ ? std::format("Buscando el objeto dentro de un radio de {:g} (en unidades de la nube) alrededor del punto...", radius)
                                         : std::format("Looking for the object within a radius of {:g} (in cloud units) around the point...", radius));
                    auto const points = co_await host.points_in_sphere(*point, radius);
                    if (!points)
                        co_return;

                    auto const object = find_object(*points, *point, classification, cell_side_length, radius);
                    auto const incomplete = points->complete() ? ""
                        : (spanish_ ? " (faltan puntos: alguna nube no se ha cargado entera)" : " (points missing: some cloud has not been loaded entirely)");
                    if (!object.found)
                    {
                        host.log(spanish_ ? std::format("No hay puntos de la clase {} en el punto pulsado{}.", name, incomplete)
                                          : std::format("There are no points of class {} at the clicked point{}.", name, incomplete));
                        break;
                    }
                    if (!object.reaches_border)
                    {
                        if (host.select_points(*points, object.indices))
                            host.log(spanish_ ? std::format("Seleccionados {} puntos de la clase {}{}.", object.indices.size(), name, incomplete)
                                              : std::format("Selected {} points of class {}{}.", object.indices.size(), name, incomplete));
                        else
                            host.log(spanish_ ? "Se ha cerrado una nube del objeto mientras se buscaba: púlsalo otra vez."
                                              : "A cloud of the object was closed while the object was looked for: click it again.");
                        break;
                    }
                    if (radius >= max_search_radius)
                    {
                        host.log(spanish_ ? std::format("El objeto de la clase {} puede pasar del radio máximo de búsqueda ({:g} en unidades de la nube): no se selecciona.", name, max_search_radius)
                                          : std::format("The object of class {} may extend beyond the maximum search radius ({:g} in cloud units): it is not selected.", name, max_search_radius));
                        break;
                    }
                }
            }
        }

    private:
        bool spanish_;
    };
}

lop::Declaration declare_select_object(bool const spanish)
{
    return lop::declare<SelectObject>({
        .id = "lop.example.select_object",
        .name = spanish ? "Seleccionar objeto" : "Select object",
        .existing_tab_id = "lop.tab.edit",
        .ribbon_section = spanish ? "Algoritmos básicos" : "Basic algorithms",
        .description = spanish ? "Selecciona los puntos del objeto que pulses: los de su clase unidos a él. Después, bórralos o clasifícalos desde la pestaña Acciones."
                               : "Selects the points of the object you click: those of its class joined to it. Then delete or classify them from the Actions tab.",
        .icon_png = { std::begin(select_object_icon), std::end(select_object_icon) },
        .parameters = {
            { .id = "gap", .name = spanish ? "Lado de la celda" : "Cell side",
              .description = spanish ? "El lado de las celdas en las que se agrupan los puntos: los de la misma clase en celdas que se tocan son del mismo objeto."
                                     : "The side of the cells in which the points are grouped: those of the same class in touching cells belong to the same object.",
              .default_value = 0.2, .minimum = 0.01, .maximum = 5.0, .unit = spanish ? "unidades de la nube" : "cloud units" },
            { .id = "max_size", .name = spanish ? "Radio máximo de búsqueda" : "Maximum search radius",
              .description = spanish ? "Hasta qué distancia del punto pulsado se busca el objeto. Un objeto que llegue más lejos no se selecciona."
                                     : "How far from the clicked point the object is looked for. An object that reaches farther is not selected.",
              .default_value = 10.0, .minimum = 1.0, .maximum = 200.0, .unit = spanish ? "unidades de la nube" : "cloud units" },
        },
        .requirements = LOP_REQUIRES_VISIBLE_CLOUD,     // ask_cloud_point requires at least one visible open cloud.
    }, spanish);
}
