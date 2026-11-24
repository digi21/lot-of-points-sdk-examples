// Table of the texts of the camera path commands.
#include "camera_path_texts.hpp"

#include <array>
#include <cstddef>

namespace
{
    // Arguments in the order of the columns of camera_path_translations_by_text_id: es, en, ca, gl, eu, fr, it, pt, de; the result follows the order of lop::Language.
    [[nodiscard]] constexpr lop::Translations translations(char const* const es, char const* const en, char const* const ca, char const* const gl,
        char const* const eu, char const* const fr, char const* const it, char const* const pt, char const* const de) noexcept
    {
        return { { ca, de, en, es, eu, fr, gl, it, pt } };
    }

    // One entry per TextId, in the order of the enumeration.
    constexpr std::array camera_path_translations_by_text_id{
        translations("Utilidades", "Utilities", "Utilitats", "Utilidades", "Utilitateak", "Utilitaires", "Utilità", "Utilitários", "Dienstprogramme"),
        translations("Grabar recorrido de la cámara", "Record camera path", "Enregistrar recorregut de la càmera", "Gravar percorrido da cámara",
            "Kameraren ibilbidea grabatu", "Enregistrer le parcours de la caméra", "Registra percorso della telecamera",
            "Gravar percurso da câmara", "Kamerapfad aufzeichnen"),
        translations("Reproducir recorrido de la cámara", "Play camera path", "Reproduir recorregut de la càmera",
            "Reproducir percorrido da cámara", "Kameraren ibilbidea erreproduzitu", "Lire le parcours de la caméra",
            "Riproduci percorso della telecamera", "Reproduzir percurso da câmara", "Kamerapfad abspielen"),
        translations("Parar grabación", "Stop recording", "Aturar l'enregistrament", "Deter a gravación", "Grabazioa gelditu",
            "Arrêter l'enregistrement", "Interrompi registrazione", "Parar gravação", "Aufzeichnung beenden"),
        translations("Pausa", "Pause", "Pausa", "Pausa", "Pausatu", "Pause", "Pausa", "Pausa", "Pause"),
        translations("Continuar", "Resume", "Continua", "Continuar", "Jarraitu", "Reprendre", "Riprendi", "Continuar", "Fortsetzen"),
        translations("Más lento", "Slower", "Més lent", "Máis lento", "Motelago", "Plus lent", "Più lento", "Mais lento", "Langsamer"),
        translations("Más rápido", "Faster", "Més ràpid", "Máis rápido", "Azkarrago", "Plus rapide", "Più veloce", "Mais rápido", "Schneller"),
        translations("Repetir", "Loop", "Repeteix", "Repetir", "Errepikatu", "Boucle", "Ripeti", "Repetir", "Wiederholen"),
        translations("Detener", "Stop", "Atura", "Deter", "Gelditu", "Arrêter", "Interrompi", "Parar", "Beenden"),
        translations("Abrir…", "Open…", "Obre…", "Abrir…", "Ireki…", "Ouvrir…", "Apri…", "Abrir…", "Öffnen…"),
        translations("{0} de {1} · ×{2}", "{0} of {1} · ×{2}", "{0} de {1} · ×{2}", "{0} de {1} · ×{2}", "{0} / {1} · ×{2}",
            "{0} sur {1} · ×{2}", "{0} di {1} · ×{2}", "{0} de {1} · ×{2}", "{0} von {1} · ×{2}"),
        translations("{0} de {1} · ×{2} · en bucle", "{0} of {1} · ×{2} · looping", "{0} de {1} · ×{2} · en bucle", "{0} de {1} · ×{2} · en bucle",
            "{0} / {1} · ×{2} · begizta", "{0} sur {1} · ×{2} · en boucle", "{0} di {1} · ×{2} · in ciclo", "{0} de {1} · ×{2} · em ciclo",
            "{0} von {1} · ×{2} · Schleife"),
        translations("Grabando el recorrido de la cámara: {0} · {1} muestras", "Recording the camera path: {0} · {1} samples",
            "S'està enregistrant el recorregut de la càmera: {0} · {1} mostres", "Gravando o percorrido da cámara: {0} · {1} mostras",
            "Kameraren ibilbidea grabatzen: {0} · {1} lagin", "Enregistrement du parcours de la caméra : {0} · {1} échantillons",
            "Registrazione del percorso della telecamera: {0} · {1} campioni", "A gravar o percurso da câmara: {0} · {1} amostras",
            "Kamerapfad wird aufgezeichnet: {0} · {1} Abtastwerte"),
        translations("Recorrido grabado: {0} s, {1} muestras. Guardado en {2}.", "Path recorded: {0} s, {1} samples. Saved in {2}.",
            "Recorregut enregistrat: {0} s, {1} mostres. Desat a {2}.", "Percorrido gravado: {0} s, {1} mostras. Gardado en {2}.",
            "Ibilbidea grabatuta: {0} s, {1} lagin. Hemen gordeta: {2}.", "Parcours enregistré : {0} s, {1} échantillons. Enregistré dans {2}.",
            "Percorso registrato: {0} s, {1} campioni. Salvato in {2}.", "Percurso gravado: {0} s, {1} amostras. Guardado em {2}.",
            "Pfad aufgezeichnet: {0} s, {1} Abtastwerte. Gespeichert in {2}."),
        translations("No se ha podido escribir {0}: {1}", "Could not write {0}: {1}", "No s'ha pogut escriure {0}: {1}",
            "Non se puido escribir {0}: {1}", "Ezin izan da idatzi {0}: {1}", "Impossible d'écrire {0} : {1}", "Impossibile scrivere {0}: {1}",
            "Não foi possível escrever {0}: {1}", "{0} konnte nicht geschrieben werden: {1}"),
        translations("No se ha podido leer el recorrido de la cámara de {0}: {1}", "Could not read the camera path from {0}: {1}",
            "No s'ha pogut llegir el recorregut de la càmera de {0}: {1}", "Non se puido ler o percorrido da cámara de {0}: {1}",
            "Ezin izan da {0} fitxategiko kameraren ibilbidea irakurri: {1}", "Impossible de lire le parcours de la caméra de {0} : {1}",
            "Impossibile leggere il percorso della telecamera di {0}: {1}", "Não foi possível ler o percurso da câmara de {0}: {1}",
            "Der Kamerapfad aus {0} konnte nicht gelesen werden: {1}"),
        translations("Recorrido de la cámara", "Camera path", "Recorregut de la càmera", "Percorrido da cámara", "Kameraren ibilbidea",
            "Parcours de la caméra", "Percorso della telecamera", "Percurso da câmara", "Kamerapfad"),
        translations("Graba la posición, la orientación y el campo de visión vertical de la cámara durante la navegación.",
            "Records the camera position, orientation and vertical field of view while you navigate.",
            "Enregistra la posició, l'orientació i el camp de visió vertical de la càmera durant la navegació.",
            "Grava a posición, a orientación e o campo de visión vertical da cámara durante a navegación.",
            "Kameraren posizioa, orientazioa eta ikus-eremu bertikala grabatzen ditu nabigatzen duzun bitartean.",
            "Enregistre la position, l'orientation et le champ de vision vertical de la caméra pendant la navigation.",
            "Registra la posizione, l'orientazione e il campo visivo verticale della telecamera durante la navigazione.",
            "Grava a posição, a orientação e o campo de visão vertical da câmara durante a navegação.",
            "Zeichnet Position, Ausrichtung und vertikales Sichtfeld der Kamera während der Navigation auf."),
        translations("Reproduce el último recorrido grabado o un fichero .lopcamerapath; el banner tiene pausa, velocidad y repetición.",
            "Plays the last recorded path or a .lopcamerapath file; the banner has pause, speed and loop.",
            "Reprodueix l'últim recorregut enregistrat o un fitxer .lopcamerapath; el bàner té pausa, velocitat i repetició.",
            "Reproduce o último percorrido gravado ou un ficheiro .lopcamerapath; o báner ten pausa, velocidade e repetición.",
            "Grabatutako azken ibilbidea edo .lopcamerapath fitxategi bat erreproduzitzen du; bannerrak pausa, abiadura eta errepikapena ditu.",
            "Lit le dernier parcours enregistré ou un fichier .lopcamerapath ; la bannière propose pause, vitesse et boucle.",
            "Riproduce l'ultimo percorso registrato o un file .lopcamerapath; il banner ha pausa, velocità e ripetizione.",
            "Reproduz o último percurso gravado ou um ficheiro .lopcamerapath; o banner tem pausa, velocidade e repetição.",
            "Spielt den zuletzt aufgezeichneten Pfad oder eine .lopcamerapath-Datei ab; das Banner bietet Pause, Geschwindigkeit und "
            "Wiederholung."),
    };

    static_assert(camera_path_translations_by_text_id.size() == static_cast<std::size_t>(TextId::play_description) + 1);
}

char const* text(lop::Language const language, TextId const id)
{
    return camera_path_translations_by_text_id[static_cast<std::size_t>(id)].text(language);
}
