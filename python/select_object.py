"""select_object.py - an example command in Python: the user clicks a point of an object of a cloud, and the command
selects the points of the object, which the user then deletes or classifies from the Actions tab. Esc ends it.

The object is the points of the class of the clicked point that are joined to it: the command groups the points of that
class in cubic cells of side cell_side_length, and two points are joined if their cells touch (26 neighbours). Points of
another class never join, however close they are. select_object.py does what cpp/select_object.cpp does, with
numpy and with the same result; copy this file and its icon, select_object.png, to %AppData%\\lop\\commands.
"""
import numpy as np

import lop

SPANISH = lop.language == "es"

# Encodes each relative cell coordinate in 17 bits after adding CELL_OFFSET.
# The maximum radius is 200 cloud units and the minimum cell side is 0.01 cloud units, so each axis spans at most 40,000 cells.
CELL_BASE = 1 << 17
CELL_OFFSET = 1 << 16
NEIGHBOURS = np.array([(di * CELL_BASE + dj) * CELL_BASE + dk for di in (-1, 0, 1) for dj in (-1, 0, 1) for dk in (-1, 0, 1)],
                      dtype=np.int64)


def cell_keys(i, j, k):
    return ((i + CELL_OFFSET) * CELL_BASE + (j + CELL_OFFSET)) * CELL_BASE + (k + CELL_OFFSET)


def find_object(points, center, classification, cell_side_length, radius):
    """The indices of the points of the class joined to the cell of center (None if that cell has none), and whether the
    object may go on outside the sphere."""
    candidates = np.flatnonzero(points.classification == classification)
    cells = [np.floor((axis[candidates] - c) / cell_side_length).astype(np.int64)
             for axis, c in ((points.x, center.x), (points.y, center.y), (points.z, center.z))]
    keys = cell_keys(*cells)
    occupied = np.unique(keys)
    seed = np.searchsorted(occupied, cell_keys(0, 0, 0))
    if seed == len(occupied) or occupied[seed] != cell_keys(0, 0, 0):
        return None, False

    # Traverses occupied cells by breadth-first search; each iteration adds unvisited occupied neighbours of the previous group.
    joined = np.zeros(len(occupied), dtype=bool)
    joined[seed] = True
    frontier = np.array([seed])
    while len(frontier):
        neighbours = (occupied[frontier][:, None] + NEIGHBOURS[None, :]).ravel()
        found = np.minimum(np.searchsorted(occupied, neighbours), len(occupied) - 1)
        found = np.unique(found[occupied[found] == neighbours])
        frontier = found[~joined[found]]
        joined[frontier] = True

    indices = candidates[joined[np.searchsorted(occupied, keys)]]

    # Points in touching cells can be up to 2 * sqrt(3) * cell_side_length apart.
    # Uses four cell sides as the boundary margin when deciding whether to enlarge the search sphere.
    inner = max(0.0, radius - 4.0 * cell_side_length)
    distance2 = (points.x[indices] - center.x) ** 2 + (points.y[indices] - center.y) ** 2 + (points.z[indices] - center.z) ** 2
    return indices, bool(np.any(distance2 > inner * inner))


@lop.command("lop.example.python.select_object",
             "Seleccionar objeto (Python)" if SPANISH else "Select object (Python)",
             existing_tab_id="lop.tab.edit",
             ribbon_section="Ejemplos en Python" if SPANISH else "Python examples",
             description="Selecciona los puntos del objeto que pulses: los de su clase unidos a él. Después, bórralos o clasifícalos desde la pestaña Acciones." if SPANISH
             else "Selects the points of the object you click: those of its class joined to it. Then delete or classify them from the Actions tab.",
             icon="select_object.png",
             parameters=[
                 lop.Parameter("gap", "Lado de la celda" if SPANISH else "Cell side", 0.2, 0.01, 5.0, unit="unidades de la nube" if SPANISH else "cloud units",
                               description="El lado de las celdas en las que se agrupan los puntos: los de la misma clase en celdas que se tocan son del mismo objeto." if SPANISH
                               else "The side of the cells in which the points are grouped: those of the same class in touching cells belong to the same object."),
                 lop.Parameter("max_size", "Radio máximo de búsqueda" if SPANISH else "Maximum search radius", 10.0, 1.0, 200.0, unit="unidades de la nube" if SPANISH else "cloud units",
                               description="Hasta qué distancia del punto pulsado se busca el objeto. Un objeto que llegue más lejos no se selecciona." if SPANISH
                               else "How far from the clicked point the object is looked for. An object that reaches farther is not selected."),
             ],
             requirements=lop.REQUIRES_VISIBLE_CLOUD)
class SelectObject(lop.Command):
    # Cada await espera sin bloquear Lot Of Points: el clic y después los puntos de cada esfera.
    async def on_start(self):
        host = self.host
        cell_side_length = host.parameter_value("gap")
        max_search_radius = host.parameter_value("max_size")
        # Requires a search radius greater than four cell sides so the inner sphere has a positive radius.
        if 4.0 * cell_side_length >= max_search_radius:
            host.log(f"El lado de la celda ({cell_side_length:g} en unidades de la nube) tiene que ser menor que la cuarta parte del radio máximo de búsqueda ({max_search_radius:g} en unidades de la nube)." if SPANISH
                     else f"The cell side ({cell_side_length:g} in cloud units) must be less than a quarter of the maximum search radius ({max_search_radius:g} in cloud units).")
            self.finish()
            return
        prompt = "Pulsa un punto del objeto; Esc para terminar" if SPANISH else "Click a point of the object; Esc to finish"
        while (point := await host.ask_cloud_point(prompt)) is not None:
            classification = point.classification
            class_name = host.class_name(classification)
            name = f"{classification} {class_name}" if class_name else str(classification)

            radius = min(max(2.0, 10.0 * cell_side_length), max_search_radius)
            while True:
                host.status(f"Buscando el objeto dentro de un radio de {radius:g} (en unidades de la nube) alrededor del punto..." if SPANISH
                            else f"Looking for the object within a radius of {radius:g} (in cloud units) around the point...")
                points = await host.points_in_sphere(point, radius)
                if points is None:
                    return

                indices, reaches_border = find_object(points, point, classification, cell_side_length, radius)
                incomplete = "" if points.complete else (" (faltan puntos: alguna nube no se ha cargado entera)" if SPANISH
                                                         else " (points missing: some cloud has not been loaded entirely)")
                if indices is None:
                    host.log(f"No hay puntos de la clase {name} en el punto pulsado{incomplete}." if SPANISH
                             else f"There are no points of class {name} at the clicked point{incomplete}.")
                    break
                if not reaches_border:
                    if host.select_points(points, indices):
                        host.log(f"Seleccionados {len(indices)} puntos de la clase {name}{incomplete}." if SPANISH
                                 else f"Selected {len(indices)} points of class {name}{incomplete}.")
                    else:
                        host.log("Se ha cerrado una nube del objeto mientras se buscaba: púlsalo otra vez." if SPANISH
                                 else "A cloud of the object was closed while the object was looked for: click it again.")
                    break
                if radius >= max_search_radius:
                    host.log(f"El objeto de la clase {name} puede pasar del radio máximo de búsqueda ({max_search_radius:g} en unidades de la nube): no se selecciona." if SPANISH
                             else f"The object of class {name} may extend beyond the maximum search radius ({max_search_radius:g} in cloud units): it is not selected.")
                    break
                radius = min(2.0 * radius, max_search_radius)
