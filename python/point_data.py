"""point_data.py - an example command in Python: the user clicks points of the clouds, and the command writes in the Output
panel what Lot Of Points knows about each one. Esc ends it. It is cpp/point_data.cpp written in Python; copy this
file and its icon, point_data.png, to %AppData%\\lop\\commands.
"""
import lop

SPANISH = lop.language == "es"


def describe(p, class_name):
    classification = f"{p.classification} {class_name}" if class_name else str(p.classification)
    if p.original_point_index == lop.UNKNOWN_ORIGINAL_POINT_INDEX:
        original_point_index_text = "desconocido" if SPANISH else "unknown"
    else:
        original_point_index_text = str(p.original_point_index)
    if SPANISH:
        return (f"X {p.x:.3f}  Y {p.y:.3f}  Z {p.z:.3f} | nube {p.cloud_id} | "
                f"índice en el archivo original {original_point_index_text} | clase {classification} | intensidad {p.intensity} | "
                f"retorno {p.return_number} de {p.number_of_returns} | RGB {p.red} {p.green} {p.blue}")
    return (f"X {p.x:.3f}  Y {p.y:.3f}  Z {p.z:.3f} | cloud {p.cloud_id} | "
            f"index in the original file {original_point_index_text} | class {classification} | intensity {p.intensity} | "
            f"return {p.return_number} of {p.number_of_returns} | RGB {p.red} {p.green} {p.blue}")


@lop.command("lop.example.python.point_data",
             "Datos del punto (Python)" if SPANISH else "Point data (Python)",
             existing_tab_id="lop.tab.view",
             ribbon_section="Ejemplos en Python" if SPANISH else "Python examples",
             description="Escribe en el panel de resultados las coordenadas y los atributos de los puntos que pulses." if SPANISH
             else "Writes in the Output panel the coordinates and the attributes of the points you click.",
             icon="point_data.png",
             requirements=lop.REQUIRES_VISIBLE_CLOUD)
class PointData(lop.Command):
    # Cada await espera un clic sin bloquear Lot Of Points; Esc finaliza el comando y ask_cloud_point devuelve None.
    async def on_start(self):
        prompt = "Pulsa un punto de una nube; Esc para terminar" if SPANISH else "Click a point of a cloud; Esc to finish"
        while (point := await self.host.ask_cloud_point(prompt)) is not None:
            self.host.log(describe(point, self.host.class_name(point.classification)))
