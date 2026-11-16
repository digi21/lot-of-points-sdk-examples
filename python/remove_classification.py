"""remove_classification.py - an example command in Python: assigns class 1 (unclassified) to the points of the editable
clouds that keep the indices of the original file, in a background task, and records the changes as one event that the
user can undo. It is cpp/remove_classification.cpp written in Python; copy this file and its icon,
remove_classification.png, to %AppData%\\lop\\commands.
"""
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


def assign_unclassified_class(tile, result, host):
    result.classification[:] = 1


def remove_classification(task, spanish):
    clouds = task.clouds()
    if clouds is None:
        return
    cloud_ids = editable_cloud_ids_with_original_point_indices(task, clouds, spanish)
    if not cloud_ids:
        task.log("Quitar clasificación: no hay ninguna nube que se pueda clasificar." if spanish else "Remove classification: there is no cloud to classify.")
        return

    task.report_state("Quitando la clasificación" if spanish else "Removing the classification")
    # Classification depends on each point alone: the tile side does not affect the result and no overlap is needed.
    # process_tile allocates one byte per point for the result column.
    summary = task.process_tiles(cloud_ids, lop.TileOptions(1000.0, concurrent=True, estimated_peak_bytes_per_point=1.0), assign_unclassified_class)
    if summary is None:
        return
    task.log(f"Quitar clasificación: {summary.reclassified_point_count} puntos pasan a la clase 1 en {summary.changed_cloud_count} nubes." if spanish
             else f"Remove classification: {summary.reclassified_point_count} points go to class 1 in {summary.changed_cloud_count} clouds.")


@lop.command("lop.example.python.remove_classification",
             "Quitar clasificación (Python)" if SPANISH else "Remove classification (Python)",
             existing_tab_id="lop.tab.classification",
             ribbon_section="Ejemplos en Python" if SPANISH else "Python examples",
             description="Asigna la clase 1 (sin clasificar) a los puntos de las nubes editables que conservan los índices del archivo original." if SPANISH
             else "Assigns class 1 (unclassified) to the points of the editable clouds that keep the indices of the original file.",
             icon="remove_classification.png",
             requirements=lop.REQUIRES_EDITABLE_CLOUD)
class RemoveClassification(lop.Command):
    def on_start(self):
        self.host.run_in_background("Quitar clasificación" if SPANISH else "Remove classification", lambda task: remove_classification(task, SPANISH))
        self.finish()
