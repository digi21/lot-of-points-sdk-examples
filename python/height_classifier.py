"""height_classifier.py - an example command in Python: assigns the ground class to the points whose height above the lowest
point of their cell does not exceed the maximum height, and the other class to the rest, in a background task over a square
XY grid relative to the minimum of the core of each tile, and records the changes as one event that the user can undo. It
is cpp/height_classifier.cpp written in Python, with numpy and with the same result; copy this file and its icon,
height_classifier.png, to %AppData%\\lop\\commands.
"""
import numpy as np

import lop

SPANISH = lop.language == "es"


def editable_cloud_ids_with_original_point_indices(task, clouds, spanish):
    """The ids of the clouds whose editable and has_original_point_indices are True, hidden clouds included; logs each
    excluded cloud."""
    ids = []
    for cloud in clouds:
        if cloud.editable and cloud.has_original_point_indices:
            ids.append(cloud.cloud_id)
            continue
        if not cloud.editable:
            reason = "no tiene registro de ediciones" if spanish else "it has no edit log"
        else:
            reason = ("su nivel de detalle no guarda la posición de cada punto en el original" if spanish
                      else "its level of detail does not keep the position of each point in the original")
        task.log(f"{cloud.name}: excluida del procesamiento porque {reason}." if spanish else f"{cloud.name}: excluded from processing because {reason}.")
    return ids


def classify_tile(cell_side_length, max_height_above_cell_lowest_point, ground_class, other_class, tile, result):
    if len(tile) == 0:
        return
    # Computes cell indices relative to the core minimum, including points in the overlap.
    columns = np.floor((tile.x - tile.core_min[0]) / cell_side_length)
    rows = np.floor((tile.y - tile.core_min[1]) / cell_side_length)
    columns = (columns - columns.min()).astype(np.int64)
    rows = (rows - rows.min()).astype(np.int64)
    cells = rows * (int(columns.max()) + 1) + columns
    minimum_elevations_by_cell = np.full(int(cells.max()) + 1, np.inf)
    np.minimum.at(minimum_elevations_by_cell, cells, tile.z)
    height = tile.z - minimum_elevations_by_cell[cells]
    result.classification = np.where(height <= max_height_above_cell_lowest_point, ground_class, other_class).astype(np.uint8)


def classify_points_by_height(task, cell_side_length, max_height_above_cell_lowest_point, ground_class, other_class, spanish):
    clouds = task.clouds()
    if clouds is None:
        return
    cloud_ids = editable_cloud_ids_with_original_point_indices(task, clouds, spanish)
    if not cloud_ids:
        task.log("Clasificar por altura: no hay ninguna nube que se pueda clasificar." if spanish else "Classify by height: there is no cloud to classify.")
        return

    task.report_state("Clasificando" if spanish else "Classifying")
    # Each tile callback modifies its own result and grid; the parameters are read-only.
    # Estimates 8 bytes per point; the grid uses 8 bytes per cell, including empty cells.
    summary = task.process_tiles(cloud_ids, lop.TileOptions(50.0, 10.0, concurrent=True, estimated_peak_bytes_per_point=8.0),
                                 lambda tile, result, host: classify_tile(cell_side_length, max_height_above_cell_lowest_point, ground_class, other_class, tile, result))
    if summary is None:
        return
    task.log(f"Clasificar por altura: {summary.reclassified_point_count} puntos cambian de clase en {summary.changed_cloud_count} nubes." if spanish
             else f"Classify by height: {summary.reclassified_point_count} points change class in {summary.changed_cloud_count} clouds.")


UNIT = "unidades de la nube" if SPANISH else "cloud units"


# The English and Spanish declarations keep the same parameter IDs, order, types, ranges, defaults and units.
@lop.command("lop.example.python.height_classifier",
             "Clasificar por altura (Python)" if SPANISH else "Classify by height (Python)",
             existing_tab_id="lop.tab.classification",
             ribbon_section="Ejemplos en Python" if SPANISH else "Python examples",
             description="Asigna la clase de suelo a los puntos que no superan la altura máxima sobre el mínimo de su celda y la otra clase a los que la superan." if SPANISH
             else "Assigns the ground class to the points that do not exceed the maximum height above the minimum of their cell and the other class to the points that exceed it.",
             icon="height_classifier.png",
             parameters=[
                 lop.Parameter("cell_size", "Lado de la celda" if SPANISH else "Cell side", 2.0, 0.1, 100.0, unit=UNIT,
                               description="Lado de las celdas en las que se busca el punto más bajo." if SPANISH
                               else "Side of the cells in which the lowest point is searched."),
                 lop.Parameter("max_height", "Altura máxima sobre el punto más bajo de la celda" if SPANISH else "Maximum height above the lowest point of the cell",
                               0.5, 0.0, 100.0, unit=UNIT,
                               description="Asigna la clase de suelo si la altura del punto sobre el mínimo de su celda es menor o igual que este valor." if SPANISH
                               else "Assigns the ground class if the height of the point above the minimum of its cell is less than or equal to this value."),
                 lop.Parameter("ground_class", "Clase de suelo" if SPANISH else "Ground class", 2, 0, 255, type="integer"),
                 lop.Parameter("other_class", "Clase para los puntos que superan la altura máxima" if SPANISH else "Class for points above the maximum height",
                               1, 0, 255, type="integer"),
             ],
             requirements=lop.REQUIRES_EDITABLE_CLOUD)
class HeightClassifier(lop.Command):
    def on_start(self):
        host = self.host
        values = (host.parameter_value("cell_size"), host.parameter_value("max_height"), int(host.parameter_value("ground_class")),
                  int(host.parameter_value("other_class")))
        host.run_in_background("Clasificar por altura" if SPANISH else "Classify by height",
                               lambda task: classify_points_by_height(task, *values, SPANISH))
        self.finish()
