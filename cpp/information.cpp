// Reads every open cloud without edits, in a background task, and logs point counts by class and return, coordinate and intensity statistics,
// non-black RGB counts and occupied area.
#include "commands.hpp"
// Regenerate the embedded PNG with:
// tools/png_to_header.ps1 cpp/icons/information.png cpp/information_icon.h information_icon
#include "information_icon.h"

#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    // Limits the core bitmap to 2^34 cells: 2^34 bits, or 2 GiB.
    constexpr std::uint64_t MAXIMUM_CELL_COUNT{ 1ull << 34 };

    // Accumulates statistics for core points in one cloud.
    struct CloudSummary
    {
        std::uint64_t point_count{};
        std::array<double, 3> minimum{};
        std::array<double, 3> maximum{};
        double sum_z{};
        std::uint16_t minimum_intensity{};
        std::uint16_t maximum_intensity{};
        std::uint64_t sum_intensity{};
        std::uint64_t non_black_rgb_point_count{};
        std::array<std::uint64_t, 16> point_counts_by_return{};
        std::array<std::uint64_t, 256> point_counts_by_class{};
        double area{};                  // Sums occupied cell areas in square cloud units.
    };

    // process_tiles runs serially without TileOptions::concurrent; tile callbacks can update this shared state without a lock.
    struct CloudStatisticsState
    {
        std::vector<lop::Cloud> clouds;
        std::vector<CloudSummary> summaries;    // Contains one summary per cloud in the order of clouds.
        double cell_side_length{};
        bool spanish{};
    };

    // Formats a line like printf.
    [[nodiscard]] std::string format(char const* const pattern, ...)
    {
        va_list arguments;
        va_start(arguments, pattern);
        va_list copy;
        va_copy(copy, arguments);
        auto const length = std::vsnprintf(nullptr, 0, pattern, copy);
        va_end(copy);
        std::string line(length > 0 ? static_cast<std::size_t>(length) : 0, '\0');
        if (length > 0)
            std::vsnprintf(line.data(), line.size() + 1, pattern, arguments);
        va_end(arguments);
        return line;
    }

    // Formats digits with a period as the decimal separator using Spanish or English digit grouping and decimal separators, and removes the
    // sign from negative zero.
    [[nodiscard]] std::string format_digits(std::string const& text, bool const spanish)
    {
        auto const separator = spanish ? '.' : ',';
        auto const point = spanish ? ',' : '.';
        std::string out;
        auto const* digits = text.c_str();
        if ('-' == *digits)
        {
            ++digits;
            if ('\0' != digits[std::strspn(digits, "0.")])
                out += '-';
        }
        auto const integer = std::strcspn(digits, ".");
        for (std::size_t digit_index = 0; digit_index < integer; ++digit_index)
        {
            if (digit_index > 0 && 0 == (integer - digit_index) % 3)
                out += separator;
            out += digits[digit_index];
        }
        if ('.' == digits[integer])
        {
            out += point;
            out += digits + integer + 1;
        }
        return out;
    }

    [[nodiscard]] std::string format_count(std::uint64_t const value, bool const spanish)
    {
        return format_digits(format("%llu", static_cast<unsigned long long>(value)), spanish);
    }

    [[nodiscard]] std::string format_real(double const value, int const decimals, bool const spanish)
    {
        return format_digits(format("%.*f", decimals, value), spanish);
    }

    // Formats at most three decimal places and removes trailing decimal zeros.
    [[nodiscard]] std::string format_with_up_to_three_decimals(double const value, bool const spanish)
    {
        auto digits = format("%.3f", value);
        while ('0' == digits.back())
            digits.pop_back();
        if ('.' == digits.back())
            digits.pop_back();
        return format_digits(digits, spanish);
    }

    // Formats part / whole as a percentage with two decimals and language-specific separators; whole == 0 produces zero.
    [[nodiscard]] std::string format_percentage(std::uint64_t const part, std::uint64_t const whole, bool const spanish)
    {
        return format_real(whole ? 100.0 * static_cast<double>(part) / static_cast<double>(whole) : 0.0, 2, spanish) + (spanish ? " %" : "%");
    }

    // Returns the cell width within extent, clipping the final cell and returning zero outside the extent.
    [[nodiscard]] double cell_width(double const extent, double const cell, std::uint64_t const extent_cell_index)
    {
        auto const left = extent - static_cast<double>(extent_cell_index) * cell;
        return left < cell ? (left > 0.0 ? left : 0.0) : cell;
    }

    void process_tile(CloudStatisticsState& state, lop::Tile const& tile, lop::TileHost const& host)
    {
        auto const cell = state.cell_side_length;
        auto const width = tile.core_max[0] - tile.core_min[0];
        auto const height = tile.core_max[1] - tile.core_min[1];
        auto const column_count = static_cast<std::uint64_t>(std::ceil(width / cell));
        auto const row_count = static_cast<std::uint64_t>(std::ceil(height / cell));

        CloudSummary* summary{};
        for (std::size_t run_cloud_index = 0; run_cloud_index < state.clouds.size(); ++run_cloud_index)
            if (state.clouds[run_cloud_index].cloud_id == tile.cloud_id)
                summary = &state.summaries[run_cloud_index];
        if (!summary)
            throw std::runtime_error{ state.spanish ? "Información: una tesela de una nube que no se ha pedido" : "Information: a tile of a cloud that was not asked for" };
        if (0 == column_count || 0 == row_count || column_count > MAXIMUM_CELL_COUNT / row_count)
            throw std::runtime_error{ state.spanish ? "Información: la cuadrícula del núcleo debe contener entre 1 y 17.179.869.184 celdas, ambos extremos incluidos."
                                                    : "Information: the core grid must contain between 1 and 17,179,869,184 cells, both included." };

        // Allocates one bit per core cell; cells terminate at the core boundary and are not shared between tiles.
        std::vector<unsigned char> occupied(static_cast<std::size_t>((column_count * row_count + 7) / 8));
        for (std::uint64_t tile_point_index = 0; tile_point_index < tile.point_count; ++tile_point_index)
        {
            if (tile.in_core[tile_point_index])
            {
                auto const intensity = tile.intensity[tile_point_index];
                auto column = static_cast<std::uint64_t>((tile.x[tile_point_index] - tile.core_min[0]) / cell);
                auto row = static_cast<std::uint64_t>((tile.y[tile_point_index] - tile.core_min[1]) / cell);
                std::array const point{ tile.x[tile_point_index], tile.y[tile_point_index], tile.z[tile_point_index] };
                if (0 == summary->point_count)
                {
                    summary->minimum = summary->maximum = point;
                    summary->minimum_intensity = summary->maximum_intensity = intensity;
                }
                for (std::size_t coordinate_axis_index = 0; coordinate_axis_index < 3; ++coordinate_axis_index)
                {
                    if (point[coordinate_axis_index] < summary->minimum[coordinate_axis_index]) summary->minimum[coordinate_axis_index] = point[coordinate_axis_index];
                    if (point[coordinate_axis_index] > summary->maximum[coordinate_axis_index]) summary->maximum[coordinate_axis_index] = point[coordinate_axis_index];
                }
                if (intensity < summary->minimum_intensity) summary->minimum_intensity = intensity;
                if (intensity > summary->maximum_intensity) summary->maximum_intensity = intensity;

                ++summary->point_count;
                summary->sum_z += point[2];
                summary->sum_intensity += intensity;
                if (tile.red[tile_point_index] || tile.green[tile_point_index] || tile.blue[tile_point_index])
                    ++summary->non_black_rgb_point_count;
                ++summary->point_counts_by_return[tile.return_number[tile_point_index] & 0x0F];
                ++summary->point_counts_by_class[tile.classification[tile_point_index]];

                // Rounding near the core maximum can produce an index beyond the final cell.
                if (column >= column_count) column = column_count - 1;
                if (row >= row_count) row = row_count - 1;
                auto const bit = row * column_count + column;
                if (!(occupied[bit >> 3] & (1u << (bit & 7))))
                {
                    occupied[bit >> 3] |= static_cast<unsigned char>(1u << (bit & 7));
                    summary->area += cell_width(width, cell, column) * cell_width(height, cell, row);
                }
            }

            if (0 == (tile_point_index & 0xFFFF))
            {
                if (host.cancelled())
                    return;
                host.report_tile_progress(static_cast<double>(tile_point_index) / static_cast<double>(tile.point_count));
            }
        }
    }

    void write_summary(CloudStatisticsState const& state, std::size_t const run_cloud_index, lop::TaskHost const& task)
    {
        static char const* const axes[] = { "X", "Y" };
        auto const spanish = state.spanish;
        auto const& summary = state.summaries[run_cloud_index];
        auto const& cloud = state.clouds[run_cloud_index];
        auto const points = summary.point_count;
        auto const log_line = [&task](std::string const& line) { task.log(line); };

        log_line(format(spanish ? "Nube %s de %s: %s" : "Cloud %s of %s: %s", format_count(run_cloud_index + 1, spanish).c_str(),
            format_count(state.clouds.size(), spanish).c_str(), cloud.name.c_str()));
        log_line(format(spanish ? "  Ruta: %s" : "  Path: %s", cloud.original_path.c_str()));
        log_line(format(spanish ? "  Unidad de X e Y: %s; unidad de Z: %s%s" : "  X and Y unit: %s; Z unit: %s%s",
            !cloud.horizontal_unit_name.empty() ? cloud.horizontal_unit_name.c_str() : (spanish ? "no disponible" : "not available"),
            !cloud.vertical_unit_name.empty() ? cloud.vertical_unit_name.c_str() : (spanish ? "no disponible" : "not available"),
            cloud.vertical_axis_points_down ? (spanish ? " (profundidad)" : " (depth)") : ""));
        log_line(format(spanish ? "  Puntos: %s" : "  Points: %s", format_count(points, spanish).c_str()));
        if (0 == points)
            return;

        for (std::size_t coordinate_axis_index = 0; coordinate_axis_index < 2; ++coordinate_axis_index)
            log_line(format(spanish ? "  %s: mínimo %s; máximo %s" : "  %s: minimum %s; maximum %s", axes[coordinate_axis_index],
                format_real(summary.minimum[coordinate_axis_index], 3, spanish).c_str(), format_real(summary.maximum[coordinate_axis_index], 3, spanish).c_str()));
        log_line(format(spanish ? "  Z: mínimo %s; máximo %s; media %s" : "  Z: minimum %s; maximum %s; mean %s",
            format_real(summary.minimum[2], 3, spanish).c_str(), format_real(summary.maximum[2], 3, spanish).c_str(),
            format_real(summary.sum_z / static_cast<double>(points), 3, spanish).c_str()));
        log_line(format(spanish ? "  Intensidad: mínima %s; máxima %s; media %s" : "  Intensity: minimum %s; maximum %s; mean %s",
            format_count(summary.minimum_intensity, spanish).c_str(), format_count(summary.maximum_intensity, spanish).c_str(),
            format_real(static_cast<double>(summary.sum_intensity) / static_cast<double>(points), 1, spanish).c_str()));
        log_line(format(spanish ? "  Puntos con RGB distinto de 0 0 0: %s (%s)" : "  Points with RGB other than 0 0 0: %s (%s)",
            format_count(summary.non_black_rgb_point_count, spanish).c_str(), format_percentage(summary.non_black_rgb_point_count, points, spanish).c_str()));

        for (unsigned value = 0; value < 16; ++value)
            if (summary.point_counts_by_return[value])
                log_line(format(spanish ? (0 == value ? "  Retorno %u (sin dato): %s (%s)" : "  Retorno %u: %s (%s)")
                                   : (0 == value ? "  Return %u (no data): %s (%s)" : "  Return %u: %s (%s)"),
                    value, format_count(summary.point_counts_by_return[value], spanish).c_str(), format_percentage(summary.point_counts_by_return[value], points, spanish).c_str()));

        // Lengths use cloud coordinate units; areas use square cloud units.
        log_line(format(spanish ? "  Superficie ocupada: %s unidades² de la nube en celdas de lado %s en unidades de la nube; densidad media: %s puntos por unidad²"
                           : "  Area covered: %s square cloud units in cells with a side of %s in cloud units; mean density: %s points per square cloud unit",
            format_real(summary.area, 2, spanish).c_str(), format_with_up_to_three_decimals(state.cell_side_length, spanish).c_str(),
            format_real(summary.area > 0.0 ? static_cast<double>(points) / summary.area : 0.0, 2, spanish).c_str()));

        for (unsigned value = 0; value < 256; ++value)
        {
            if (!summary.point_counts_by_class[value])
                continue;
            auto const count = format_count(summary.point_counts_by_class[value], spanish);
            auto const percentage = format_percentage(summary.point_counts_by_class[value], points, spanish);
            auto const name = task.class_name(static_cast<std::uint8_t>(value));
            if (!name.empty())
                log_line(format(spanish ? "  Clase %u - %s: %s (%s)" : "  Class %u - %s: %s (%s)", value, name.c_str(), count.c_str(), percentage.c_str()));
            else
                log_line(format(spanish ? "  Clase %u: %s (%s)" : "  Class %u: %s (%s)", value, count.c_str(), percentage.c_str()));
        }
    }

    void log_cloud_statistics(lop::TaskHost& task, double const cell_side_length, bool const spanish)
    {
        CloudStatisticsState state;
        state.spanish = spanish;
        state.cell_side_length = cell_side_length;
        auto clouds = task.clouds();
        if (!clouds)
            return;
        state.clouds = std::move(*clouds);
        if (state.clouds.empty())
        {
            task.log(spanish ? "Información: no hay ninguna nube abierta." : "Information: there is no open cloud.");
            return;
        }

        // Processes every open cloud, including hidden clouds; read-only processing requires neither edit logs nor original point indices.
        state.summaries.resize(state.clouds.size());
        std::vector<std::uint32_t> cloud_ids;
        for (auto const& cloud : state.clouds)
            cloud_ids.push_back(cloud.cloud_id);

        task.report_state(spanish ? "Leyendo las nubes" : "Reading the clouds");
        // Serial tile execution permits updates to the shared summaries without a lock; point statistics require no overlap.
        if (!task.process_tiles(cloud_ids, { .tile_core_side_length_cloud_units = 1000.0, .read_only = true },
            [&state](lop::Tile const& tile, lop::TileResult*, lop::TileHost const& host) { process_tile(state, tile, host); }))
            return;

        task.report_state(spanish ? "Escribiendo el resumen" : "Writing the summary");
        for (std::size_t run_cloud_index = 0; run_cloud_index < state.clouds.size(); ++run_cloud_index)
            write_summary(state, run_cloud_index, task);
    }

    class LogCloudStatisticsCommand final : public lop::Command
    {
    public:
        explicit LogCloudStatisticsCommand(bool const spanish) : spanish_{ spanish } {}

        lop::Task<> on_start() override
        {
            auto const cell_side_length = host().parameter_value("cell_size");
            auto const spanish = spanish_;
            host().run_in_background(spanish ? "Información" : "Information",
                [cell_side_length, spanish](lop::TaskHost& task) { log_cloud_statistics(task, cell_side_length, spanish); });
            finish();
            co_return;
        }

    private:
        bool spanish_;
    };
}

lop::Declaration declare_information(bool const spanish)
{
    return lop::declare<LogCloudStatisticsCommand>({
        .id = "lop.example.information",
        .name = spanish ? "Información" : "Information",
        .new_tab_title = spanish ? "Utilidades" : "Utilities",
        .ribbon_section = spanish ? "Utilidades" : "Utilities",
        .description = spanish ? "Escribe en el panel de salida los recuentos de puntos por clase y estadísticas de cada nube abierta."
                               : "Writes to the Output panel the point counts by class and statistics of each open cloud.",
        .icon_png = { std::begin(information_icon), std::end(information_icon) },
        .parameters = {
            { .id = "cell_size", .name = spanish ? "Lado de la celda de la superficie" : "Area cell side",
              .description = spanish ? "Lado de las celdas con las que se mide la superficie que ocupan los puntos."
                                     : "Side length of the cells used to calculate the area covered by the points.",
              .default_value = 1.0, .minimum = 0.1, .maximum = 100.0, .unit = spanish ? "unidades de la nube" : "cloud units" },
        },
        .requirements = LOP_REQUIRES_OPEN_CLOUD,
    }, spanish);
}
