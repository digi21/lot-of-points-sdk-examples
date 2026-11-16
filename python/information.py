"""information.py - an example command in Python: reads every open cloud without modifying its points, in a background task,
and writes in the Output panel the point counts by class and return, the coordinate and intensity statistics, the points with a
colour other than black and the area covered. It is cpp/information.cpp written in Python, with numpy and with the same
output; copy this file and its icon, information.png, to %AppData%\\lop\\commands.
"""
import numpy as np

import lop

SPANISH = lop.language == "es"

# Limits the core grid to 2^34 cells.
MAXIMUM_CELL_COUNT = 1 << 34


class CloudSummary:
    """The statistics of the core points of one cloud."""

    def __init__(self):
        self.point_count = 0
        self.minimum_coordinates = [0.0, 0.0, 0.0]
        self.maximum_coordinates = [0.0, 0.0, 0.0]
        self.sum_z = 0.0
        self.minimum_intensity = 0
        self.maximum_intensity = 0
        self.sum_intensity = 0
        self.non_black_rgb_point_count = 0
        self.point_counts_by_return = np.zeros(16, dtype=np.uint64)
        self.point_counts_by_class = np.zeros(256, dtype=np.uint64)
        self.area = 0.0
        """The sum of the areas of the occupied cells, in square cloud units."""


def format_digits(text, spanish):
    """Groups the digits of text, a number with a period as decimal separator, with the separators of the language, and
    removes the sign of a negative zero."""
    separator, point = (".", ",") if spanish else (",", ".")
    sign = ""
    if text.startswith("-"):
        text = text[1:]
        if text.strip("0."):
            sign = "-"
    integer, _, decimals = text.partition(".")
    groups = []
    while len(integer) > 3:
        groups.insert(0, integer[-3:])
        integer = integer[:-3]
    groups.insert(0, integer)
    out = sign + separator.join(groups)
    return out + point + decimals if decimals else out


def format_count(value, spanish):
    return format_digits(str(int(value)), spanish)


def format_real(value, decimals, spanish):
    return format_digits(f"{value:.{decimals}f}", spanish)


def format_with_up_to_three_decimals(value, spanish):
    digits = f"{value:.3f}".rstrip("0").rstrip(".")
    return format_digits(digits, spanish)


def format_percentage(part, whole, spanish):
    return format_real(100.0 * part / whole if whole else 0.0, 2, spanish) + (" %" if spanish else "%")


def cell_widths(extent, cell_side_length, cell_indices):
    """The width of each cell within extent: the last cell is clipped, and a cell outside extent has width 0."""
    left = extent - cell_indices.astype(np.float64) * cell_side_length
    return np.where(left < cell_side_length, np.maximum(left, 0.0), cell_side_length)


def accumulate_tile(summaries_by_cloud_id, cell_side_length, tile, spanish):
    summary = summaries_by_cloud_id.get(tile.cloud_id)
    if summary is None:
        raise RuntimeError("Información: una tesela de una nube que no se ha pedido" if spanish
                           else "Information: a tile of a cloud that was not asked for")
    width = tile.core_max[0] - tile.core_min[0]
    height = tile.core_max[1] - tile.core_min[1]
    column_count = int(np.ceil(width / cell_side_length))
    row_count = int(np.ceil(height / cell_side_length))
    if column_count == 0 or row_count == 0 or column_count > MAXIMUM_CELL_COUNT // row_count:
        raise RuntimeError("Información: la cuadrícula del núcleo debe contener entre 1 y 17.179.869.184 celdas, ambos extremos incluidos."
                           if spanish else "Information: the core grid must contain between 1 and 17,179,869,184 cells, both included.")

    core = tile.in_core != 0
    if not core.any():
        return
    x, y, z = tile.x[core], tile.y[core], tile.z[core]
    intensity = tile.intensity[core].astype(np.int64)
    if summary.point_count == 0:
        summary.minimum_coordinates = [float(x.min()), float(y.min()), float(z.min())]
        summary.maximum_coordinates = [float(x.max()), float(y.max()), float(z.max())]
        summary.minimum_intensity = int(intensity.min())
        summary.maximum_intensity = int(intensity.max())
    else:
        summary.minimum_coordinates = [min(previous, float(axis.min())) for previous, axis in zip(summary.minimum_coordinates, (x, y, z))]
        summary.maximum_coordinates = [max(previous, float(axis.max())) for previous, axis in zip(summary.maximum_coordinates, (x, y, z))]
        summary.minimum_intensity = min(summary.minimum_intensity, int(intensity.min()))
        summary.maximum_intensity = max(summary.maximum_intensity, int(intensity.max()))
    summary.point_count += int(core.sum())
    summary.sum_z += float(z.sum())
    summary.sum_intensity += int(intensity.sum())
    summary.non_black_rgb_point_count += int(((tile.red[core] != 0) | (tile.green[core] != 0) | (tile.blue[core] != 0)).sum())
    summary.point_counts_by_return += np.bincount(tile.return_number[core] & 0x0F, minlength=16).astype(np.uint64)
    summary.point_counts_by_class += np.bincount(tile.classification[core], minlength=256).astype(np.uint64)

    # Rounding near the core maximum can produce an index beyond the final cell.
    columns = np.minimum(((x - tile.core_min[0]) / cell_side_length).astype(np.uint64), column_count - 1)
    rows = np.minimum(((y - tile.core_min[1]) / cell_side_length).astype(np.uint64), row_count - 1)
    occupied = np.unique(rows * np.uint64(column_count) + columns)
    occupied_columns = occupied % np.uint64(column_count)
    occupied_rows = occupied // np.uint64(column_count)
    summary.area += float((cell_widths(width, cell_side_length, occupied_columns) * cell_widths(height, cell_side_length, occupied_rows)).sum())


def write_summary(task, clouds, summaries, cell_side_length, run_cloud_index, spanish):
    summary = summaries[run_cloud_index]
    cloud = clouds[run_cloud_index]
    point_count = summary.point_count
    log = task.log
    not_available = "no disponible" if spanish else "not available"
    log(f"Nube {format_count(run_cloud_index + 1, spanish)} de {format_count(len(clouds), spanish)}: {cloud.name}" if spanish
        else f"Cloud {format_count(run_cloud_index + 1, spanish)} of {format_count(len(clouds), spanish)}: {cloud.name}")
    log(f"  Ruta: {cloud.original_path}" if spanish else f"  Path: {cloud.original_path}")
    depth = (" (profundidad)" if spanish else " (depth)") if cloud.vertical_axis_points_down else ""
    log(f"  Unidad de X e Y: {cloud.horizontal_unit_name or not_available}; unidad de Z: {cloud.vertical_unit_name or not_available}{depth}" if spanish
        else f"  X and Y unit: {cloud.horizontal_unit_name or not_available}; Z unit: {cloud.vertical_unit_name or not_available}{depth}")
    log(f"  Puntos: {format_count(point_count, spanish)}" if spanish else f"  Points: {format_count(point_count, spanish)}")
    if point_count == 0:
        return

    for axis_index, axis in enumerate(("X", "Y")):
        minimum = format_real(summary.minimum_coordinates[axis_index], 3, spanish)
        maximum = format_real(summary.maximum_coordinates[axis_index], 3, spanish)
        log(f"  {axis}: mínimo {minimum}; máximo {maximum}" if spanish else f"  {axis}: minimum {minimum}; maximum {maximum}")
    minimum, maximum, mean = (format_real(value, 3, spanish) for value in (summary.minimum_coordinates[2], summary.maximum_coordinates[2], summary.sum_z / point_count))
    log(f"  Z: mínimo {minimum}; máximo {maximum}; media {mean}" if spanish else f"  Z: minimum {minimum}; maximum {maximum}; mean {mean}")
    minimum, maximum = format_count(summary.minimum_intensity, spanish), format_count(summary.maximum_intensity, spanish)
    mean = format_real(summary.sum_intensity / point_count, 1, spanish)
    log(f"  Intensidad: mínima {minimum}; máxima {maximum}; media {mean}" if spanish else f"  Intensity: minimum {minimum}; maximum {maximum}; mean {mean}")
    count = format_count(summary.non_black_rgb_point_count, spanish)
    percentage = format_percentage(summary.non_black_rgb_point_count, point_count, spanish)
    log(f"  Puntos con RGB distinto de 0 0 0: {count} ({percentage})" if spanish else f"  Points with RGB other than 0 0 0: {count} ({percentage})")

    for value in range(16):
        return_count = int(summary.point_counts_by_return[value])
        if not return_count:
            continue
        count, percentage = format_count(return_count, spanish), format_percentage(return_count, point_count, spanish)
        if spanish:
            log(f"  Retorno {value} (sin dato): {count} ({percentage})" if value == 0 else f"  Retorno {value}: {count} ({percentage})")
        else:
            log(f"  Return {value} (no data): {count} ({percentage})" if value == 0 else f"  Return {value}: {count} ({percentage})")

    # Lengths use cloud coordinate units; areas use square cloud units.
    area = format_real(summary.area, 2, spanish)
    side = format_with_up_to_three_decimals(cell_side_length, spanish)
    density = format_real(point_count / summary.area if summary.area > 0.0 else 0.0, 2, spanish)
    log(f"  Superficie ocupada: {area} unidades² de la nube en celdas de lado {side} en unidades de la nube; densidad media: {density} puntos por unidad²"
        if spanish else
        f"  Area covered: {area} square cloud units in cells with a side of {side} in cloud units; mean density: {density} points per square cloud unit")

    for value in range(256):
        class_count = int(summary.point_counts_by_class[value])
        if not class_count:
            continue
        count, percentage = format_count(class_count, spanish), format_percentage(class_count, point_count, spanish)
        name = task.class_name(value)
        label = (f"Clase {value} - {name}" if spanish else f"Class {value} - {name}") if name else (f"Clase {value}" if spanish else f"Class {value}")
        log(f"  {label}: {count} ({percentage})")


def log_cloud_statistics(task, cell_side_length, spanish):
    clouds = task.clouds()
    if clouds is None:
        return
    if not clouds:
        task.log("Información: no hay ninguna nube abierta." if spanish else "Information: there is no open cloud.")
        return

    # Processes every open cloud, including hidden clouds; read-only processing requires neither edit logs nor original point indices.
    summaries = [CloudSummary() for _ in clouds]
    summaries_by_cloud_id = {cloud.cloud_id: summary for cloud, summary in zip(clouds, summaries)}
    task.report_state("Leyendo las nubes" if spanish else "Reading the clouds")
    # Serial tile execution permits updates to the shared summaries without a lock; point statistics require no overlap.
    if task.process_tiles([cloud.cloud_id for cloud in clouds], lop.TileOptions(1000.0, read_only=True),
                          lambda tile, result, host: accumulate_tile(summaries_by_cloud_id, cell_side_length, tile, spanish)) is None:
        return

    task.report_state("Escribiendo el resumen" if spanish else "Writing the summary")
    for run_cloud_index in range(len(clouds)):
        write_summary(task, clouds, summaries, cell_side_length, run_cloud_index, spanish)


@lop.command("lop.example.python.information",
             "Información (Python)" if SPANISH else "Information (Python)",
             existing_tab_id="lop.tab.view",
             ribbon_section="Ejemplos en Python" if SPANISH else "Python examples",
             description="Escribe en el panel de salida los recuentos de puntos por clase y estadísticas de cada nube abierta." if SPANISH
             else "Writes to the Output panel the point counts by class and statistics of each open cloud.",
             icon="information.png",
             parameters=[
                 lop.Parameter("cell_size", "Lado de la celda de la superficie" if SPANISH else "Area cell side", 1.0, 0.1, 100.0,
                               unit="unidades de la nube" if SPANISH else "cloud units",
                               description="Lado de las celdas con las que se mide la superficie que ocupan los puntos." if SPANISH
                               else "Side length of the cells used to calculate the area covered by the points."),
             ],
             requirements=lop.REQUIRES_OPEN_CLOUD)
class Information(lop.Command):
    def on_start(self):
        cell_side_length = self.host.parameter_value("cell_size")
        self.host.run_in_background("Información" if SPANISH else "Information", lambda task: log_cloud_statistics(task, cell_side_length, SPANISH))
        self.finish()
