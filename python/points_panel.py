"""points_panel.py - an example panel and dialog in Python: the ribbon button Points around (Python) shows a panel with a
radius; the panel buttons launch two commands without ribbon button: one counts the points of the visible clouds around each
clicked point and writes the result and a history row in the panel; the other opens a dialog with the radius and copies the
accepted radius to the panel. This Python example reproduces the behaviour of cpp/points_panel.cpp; copy
points_panel.py and points_panel.png to %AppData%\\lop\\commands.
"""
import lop

SPANISH = lop.language == "es"

PANEL_ID = "lop.example.python.points_panel"
DIALOG_ID = "lop.example.python.points_panel_options"
RADIUS_VALUE_KEY = "radius"
RESULT_VALUE_KEY = "result"
HISTORY_LIST_KEY = "history"
DEFAULT_RADIUS = 1.0
SECTION = "Ejemplos en Python" if SPANISH else "Python examples"
RADIUS_HEADER = "Radio (unidades de la nube)" if SPANISH else "Radius (cloud units)"


def get_ui_radius(host, panel_or_dialog_id):
    """The radius in the panel or dialog, or the default radius if the value is not a positive number."""
    value = host.get_ui_value(panel_or_dialog_id, RADIUS_VALUE_KEY)
    if isinstance(value, (int, float)) and not isinstance(value, bool) and value > 0:
        return float(value)
    return DEFAULT_RADIUS


@lop.command("lop.example.python.show_points_panel",
             "Puntos alrededor (Python)" if SPANISH else "Points around (Python)",
             existing_tab_id="lop.tab.edit",
             ribbon_section=SECTION,
             description="Muestra el panel que cuenta los puntos alrededor de los puntos que pulses." if SPANISH
             else "Shows the panel that counts the points around the points you click.",
             icon="points_panel.png")
class ShowPointsPanel(lop.Command):
    # Muestra el panel y escribe el radio predeterminado si el panel no tiene ninguno.
    def on_start(self):
        if self.host.get_ui_value(PANEL_ID, RADIUS_VALUE_KEY) is None:
            self.host.set_ui_value(PANEL_ID, RADIUS_VALUE_KEY, DEFAULT_RADIUS)
        self.host.show_panel(PANEL_ID)
        self.finish()


# Los botones del panel lanzan los dos comandos siguientes, que no tienen pestaña y por tanto no tienen botón en el ribbon.
@lop.command("lop.example.python.count_points_around",
             "Contar los puntos alrededor (Python)" if SPANISH else "Count the points around (Python)",
             requirements=lop.REQUIRES_VISIBLE_CLOUD)
class CountPointsAround(lop.Command):
    # Cuenta los puntos de las nubes visibles alrededor de cada punto pulsado hasta Esc; el historial guarda las filas de esta ejecución.
    async def on_start(self):
        host = self.host
        radius = get_ui_radius(host, PANEL_ID)
        prompt = (f"Pulsa un punto para contar los puntos cuya distancia al punto pulsado no supere {radius:g} unidades de la nube; pulsa Esc para terminar"
                  if SPANISH else
                  f"Click a point to count the points whose distance from the clicked point is at most {radius:g} cloud units; press Esc to finish")

        # La línea temporal muestra el diámetro de la esfera a lo largo de X, centrado en el punto bajo el cursor.
        def preview(under_cursor):
            if under_cursor is None:
                host.show_temporary_line([])
            else:
                x, y, z = under_cursor
                host.show_temporary_line([(x - radius, y, z), (x + radius, y, z)])

        history = []
        while (center := await host.ask_point(prompt, preview)) is not None:
            points = await host.points_in_sphere(center, radius)
            if points is None:
                return
            text = (f"{len(points)} puntos a una distancia máxima de {radius:g} unidades de la nube del punto X {center.x:.3f} Y {center.y:.3f} Z {center.z:.3f}"
                    if SPANISH else
                    f"{len(points)} points within a distance of {radius:g} cloud units from point X {center.x:.3f} Y {center.y:.3f} Z {center.z:.3f}")
            host.set_ui_value(PANEL_ID, RESULT_VALUE_KEY, text)
            history.append({"text": text})
            host.set_ui_rows(PANEL_ID, HISTORY_LIST_KEY, history)


@lop.command("lop.example.python.edit_points_panel_options",
             "Opciones de Puntos alrededor (Python)" if SPANISH else "Points around options (Python)")
class EditPointsPanelOptions(lop.Command):
    # Muestra el cuadro con el radio del panel y, si el usuario lo acepta, copia el radio del cuadro al panel.
    async def on_start(self):
        host = self.host
        host.set_ui_value(DIALOG_ID, RADIUS_VALUE_KEY, get_ui_radius(host, PANEL_ID))
        if await host.show_dialog(DIALOG_ID) is True:
            host.set_ui_value(PANEL_ID, RADIUS_VALUE_KEY, get_ui_radius(host, DIALOG_ID))
        self.finish()


lop.panel(PANEL_ID, "Puntos alrededor (Python)" if SPANISH else "Points around (Python)", f"""<StackPanel xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" Padding="8" Spacing="8">
  <NumberBox AutomationProperties.AutomationId="radius" Header="{RADIUS_HEADER}" Value="{{Binding Values[radius], Mode=TwoWay}}" Minimum="0.05" Maximum="100" SpinButtonPlacementMode="Inline" />
  <Button AutomationProperties.AutomationId="count" Content="{"Contar los puntos alrededor de un punto" if SPANISH else "Count the points around a point"}" Command="{{Binding Commands[lop.example.python.count_points_around]}}" />
  <Button AutomationProperties.AutomationId="options" Content="{"Opciones..." if SPANISH else "Options..."}" Command="{{Binding Commands[lop.example.python.edit_points_panel_options]}}" />
  <TextBlock AutomationProperties.AutomationId="result" Text="{{Binding Values[result]}}" TextWrapping="Wrap" />
  <ListView AutomationProperties.AutomationId="history" ItemsSource="{{Binding Lists[history]}}" MinHeight="120">
    <ListView.ItemTemplate><DataTemplate><TextBlock Text="{{Binding [text]}}" /></DataTemplate></ListView.ItemTemplate>
  </ListView>
</StackPanel>""", initial_dock_side=lop.PANEL_DOCK_RIGHT)

lop.dialog(DIALOG_ID, "Opciones de Puntos alrededor (Python)" if SPANISH else "Points around options (Python)", f"""<StackPanel xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" Spacing="8" MinWidth="320">
  <NumberBox AutomationProperties.AutomationId="dialogRadius" Header="{RADIUS_HEADER}" Value="{{Binding Values[radius], Mode=TwoWay}}" Minimum="0.05" Maximum="100" SpinButtonPlacementMode="Inline" />
</StackPanel>""", "Aceptar" if SPANISH else "OK", "Cancelar" if SPANISH else "Cancel")
