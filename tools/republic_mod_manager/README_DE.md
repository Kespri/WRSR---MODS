# Republic Mod Manager – Anleitung

[English](README_EN.md) | **Deutsch**

Republic Mod Manager (kurz RMM) ist ein Fenster für alle deine TesmioLoader-Plugins. Du siehst, welche Plugins du hast, schaltest sie ein oder aus, stellst ihre Werte mit Beschreibung und Bereichsprüfung ein und startest das Spiel. Der TesmioLoader selbst bleibt unverändert; RMM schreibt nur Einstellungsdateien.

---

## 📋 Inhaltsverzeichnis

1. [Voraussetzungen](#-voraussetzungen)
2. [Das Fenster](#-das-fenster)
3. [Der Schalter „Plugin aktiv“](#-der-schalter-plugin-aktiv)
4. [Speichern und Starten](#-speichern-und-starten)
5. [Die Karte „Hinweise“](#-die-karte-hinweise)
6. [Wie ein Plugin ins Spiel kommt](#-wie-ein-plugin-ins-spiel-kommt)
7. [Inhaltspakete](#-inhaltspakete-ressourcen-vorkommen-gebäude)
8. [Dateien nur lokal](#-dateien-nur-lokal)
9. [Vor dem Spielstart](#-vor-dem-spielstart)
10. [Spielstände](#-spielstände)
11. [Das Protokollfenster](#-das-protokollfenster)
12. [Das Einstellungsfenster](#-das-einstellungsfenster)
13. [Profile und Wiederherstellung](#-profile-und-wiederherstellung)
14. [Listen-Editoren: Resources, Needs, Deposits und Paket-Editoren](#-listen-editoren)
15. [Erzeugte Gebäude (Reiter „SML-Gebäude“)](#️-erzeugte-gebäude-reiter-sml-gebäude)
16. [Spielversion und rmm.ini](#-spielversion-und-rmmini)
17. [Wo deine Dateien liegen](#-wo-deine-dateien-liegen)
18. [Wenn etwas nicht klappt](#-wenn-etwas-nicht-klappt)
19. [Für Plugin-Autoren](#-für-plugin-autoren)

---

## 🚀 Voraussetzungen

- *Workers & Resources: Soviet Republic* 1.1.1.9
- TesmioLoader von MaxLegend im Spielordner (`tesmioloader\build` mit `tesmiolauncher.exe` und `tesmioloader.ini`). Das RMM-Paket bringt eine korrigierte Fassung der `tesmioloader.dll` mit (b0.3.6-rmm.1, gleiche API) und setzt sie beim Installieren über die vorhandene; die alte landet in `tesmioloader\rmm_install_backup`
- Windows 10 oder 11

RMM liegt als `rmm.exe` im Ordner `tesmioloader\build`. Starte es von dort oder über die Desktop-Verknüpfung. Es findet den Loader, das Spiel und den Steam-Workshop-Ordner von selbst. Nur ein RMM-Fenster ist gleichzeitig offen; ein zweiter Start holt das vorhandene nach vorn.

---

## 🖥️ Das Fenster

**Links die Liste.** Jede Zeile ist ein Plugin: abonnierte Workshop-Pakete und alles, was als DLL in `tesmioloader\build\plugins` liegt. Das Symbol zeigt die Herkunft: Steam-Symbol für Workshop-Pakete, TesmioLauncher-Symbol für Plugins im plugins-Ordner, Zahnrad für alles andere. Ein grüner Punkt heißt: Dieses Plugin läuft beim nächsten Spielstart. Ein gelbes „Update“ heißt: Das Workshop-Paket ist neuer als das, was du zuletzt gespeichert hast. Ein bernsteinfarbener Punkt heißt: Hier hast du etwas geändert und noch nicht gespeichert.

**Unter dem Suchfeld vier Filter:** „Alle“, „Aktiv“, „Probleme“ und „Updates“. Sie zeigen genau die Einträge, auf die das zutrifft — steht ein Plugin nicht unter „Probleme“, hat es auch keins. Nur ein Eintrag mit ungespeicherten Änderungen bleibt in jedem Filter stehen; der bernsteinfarbene Punkt sagt dir, warum. Die offene Seite bleibt geöffnet, auch wenn ihr Eintrag gerade herausgefiltert ist; „Alle“ holt ihn zurück.

Rechts neben dem Suchfeld sitzt der Aktualisieren-Knopf: Er liest die Paketordner neu, wenn du zwischendurch etwas abonniert oder kopiert hast.

**Oben der Kopf.** Name, Version und Beschreibung des Plugins, rechts der Schalter „Plugin aktiv“ und die Sprache der Oberfläche. RMM spricht Deutsch und Englisch; „Automatisch“ nimmt Deutsch, wenn Windows auf Deutsch läuft, sonst Englisch.

**Darunter die Reiter.** Die Reiter kommen vom Plugin. Ein Plugin ohne eigene Einstellungsseite bekommt einen Reiter „Allgemein“, den RMM aus den Kommentaren der INI-Datei baut.

**Die Statuszeile** unter den Reitern sagt, wie das Plugin geladen wird (Workshop Bridge, Soviet Mod Loader oder TesmioLoader), wo seine Dateien liegen (Workshop-Paket, lokal in plugins\ oder plugins\) und wann der Loader es zuletzt geladen hat. Fahre mit der Maus über einen Punkt, dann erklärt ein Hinweis, was er bedeutet. Tritt ein Plugin zur Seite, weil Soviet Mod Loader seine Fähigkeit selbst mitbringt (Deposits Plus, Resources Plus, Needs Plus), leuchtet kein Ladeweg, sondern ein oranger Punkt "Pausiert", und rechts steht "Beim letzten Spielstart zur Seite getreten".

**Die Karten** tragen die Einstellungen. Jede Zeile hat einen Titel, einen kurzen Text, was die Einstellung im Spiel verändert, und rechts das Feld:

- Schalter für EIN und AUS.
- Zahlenfelder mit Plus und Minus. Fahre mit der Maus über das Feld, dann siehst du den erlaubten Bereich. Ein Dezimalkomma darfst du tippen, gespeichert wird mit Punkt.
- Auswahllisten, Textfelder und mehrzeilige Felder mit Zähler.
- Ein Zurücksetzen-Knopf erscheint nur, wenn dein Wert vom Standard des Plugins abweicht.

Rechtsklick auf einen Text kopiert ihn in die Zwischenablage, praktisch für Fehlerberichte.

**Die Fußzeile** zeigt den Zustand: grün „Gespeichert“, bernstein „Ungespeicherte Änderungen“, rot „Konfiguration ungültig“ mit dem Grund. Daneben die Knöpfe Zurücksetzen, Speichern und Speichern + Starten. Speichern ist nur anklickbar, wenn es etwas Gültiges zu speichern gibt. Hast du mehrere Plugins geändert, nennt die Fußzeile sie alle.

Das Fenster merkt sich Größe, Sprache, gewähltes Plugin und Reiter. Es braucht mindestens 1560 mal 760 Punkte; bei schmalen Fenstern rutschen die Beschriftungen über die Felder.

---

## 🔛 Der Schalter „Plugin aktiv“

Der Schalter im Kopf entscheidet, ob das Plugin beim nächsten Spielstart läuft. Was er genau schreibt, hängt davon ab, wer die DLL lädt (siehe Statuszeile):

- **Workshop Bridge:** Der Schalter trägt das Paket in die Liste der Bridge ein (`user_config\workshop_bridge.ini`) oder streicht es dort. Ein Paket, das du nie eingeschaltet hast, lädt die Bridge nicht.
- **TesmioLoader:** Der Schalter ist das Kästchen des TesmioLaunchers, also der Eintrag `[plugins] <name>` in `tesmioloader.ini`. Beim Einschalten setzt RMM zusätzlich das Feld `enabled` in der INI des Plugins auf 1, falls es eines gibt, damit die DLL nicht sofort wieder ablehnt.
- **Soviet Mod Loader:** SML lädt jedes abonnierte Paket selbst. Der Schalter setzt dann nur das Feld `enabled` in der INI. Hat die INI kein solches Feld, fehlt der Schalter, und ein Hinweis sagt, dass nur das Abbestellen das Paket abschaltet.

Der Schalter zeigt „an“ nur, wenn alles zusammenpasst, genau wie der grüne Punkt in der Liste. Inhaltspakete ohne DLL (Ressourcen, Vorkommen, Gebäude) haben statt dessen den Schalter „Im Spiel bereitstellen“.

---

## 💾 Speichern und Starten

**Speichern** schreibt deine Änderungen:

- Deine persönlichen Werte landen in `tesmioloader\build\user_config\<name>.ini`. Die INI des Pakets bleibt, wie sie ist. Ein Update des Pakets überschreibt deine Werte deshalb nie.
- Bei Plugins, die nur als DLL im plugins-Ordner liegen, sichert RMM beim ersten Speichern die vorgefundene INI als Original unter `user_config\.autoload\<name>.upstream.ini` und schreibt dann die fertige INI mit deinen Werten nach `plugins\`. „Original wiederherstellen“ in der Fußzeile holt die gesicherte Datei byteweise zurück.
- Vor jedem Speichern legt RMM einen Wiederherstellungspunkt an (siehe [Profile und Wiederherstellung](#-profile-und-wiederherstellung)).
- Muss eine neue DLL nach `plugins\` (nur beim Ladeweg TesmioLoader), zeigt RMM vorher Paket, Quelle, Ziel und die Prüfsumme der Datei und fragt nach.

Speichern geht nur, wenn Spiel und TesmioLauncher beendet sind und alle Werte im erlaubten Bereich liegen. Nur das Öffnen, Suchen und Wechseln von Plugins oder Reitern schreibt nie eine Datei.

**Mehrere Plugins auf einmal:** Du musst nicht nach jedem Plugin speichern. Wechsle einfach zum nächsten Eintrag, deine Änderungen bleiben im Speicher, und der Eintrag bekommt einen bernsteinfarbenen Punkt in der Liste. Die Fußzeile zählt alle ungespeicherten Plugins auf. Speichern oder Speichern + Starten schreibt sie dann alle auf einmal, eins nach dem anderen. Stimmt in einem Plugin ein Wert nicht, bleibt Speichern gesperrt, und die Fußzeile sagt, in welchem. Zurücksetzen gilt immer nur für das Plugin, das gerade offen ist. Beim Schließen von RMM fragt es noch einmal nach: alle speichern, alle verwerfen oder abbrechen.

**Speichern + Starten** speichert genauso und startet dann das Spiel über `tesmiolauncher.exe` ohne dessen Fenster. RMM schließt sich dabei, wenn du es nicht ausdrücklich offen lässt (siehe unten). Willst du das Launcher-Fenster sehen, setze in `rmm.ini` unter `[settings]` den Wert `tesmiolauncher_window = 1`.

Nach dem Start schaut RMM dem Spiel noch etwa 15 Sekunden zu und schließt sich erst dann. Beendet sich das Spiel **sofort** wieder — in den ersten fünf Sekunden, bevor es überhaupt geladen hat —, sagt RMM dir das und nennt den häufigsten Grund: einen Steam-Client, der zwar läuft, gerade aber kein Spiel annimmt, das ausserhalb von Steam startet; dann hilft nur, Steam ganz zu beenden und neu zu starten. Machst du das Spiel selbst wieder zu, und sei es gleich nach dem Ladebildschirm, bleibt RMM still und schließt sich einfach. Willst du das alte Verhalten (RMM schließt sofort), setze in `rmm.ini` unter `[settings]` den Wert `launch_watch_seconds = 0`.

**Soll RMM gar nicht zugehen,** setze im Einstellungsfenster den Schalter „Fenster offen lassen“ (oder `keep_open = 1` in der `rmm.ini`). Dann startet das Spiel wie immer, und RMM bleibt stehen, bis du es selbst schliesst — praktisch, wenn du beim Laden noch etwas nachsehen willst. Die Fußzeile sagt dir dann, dass das Spiel läuft. Die Warnung bei einem sofortigen Abgang kommt weiterhin.

Vor dem Start prüft RMM außerdem, ob alle Ressourcen, auf die eingeschaltete Plugins verweisen, im Plugin Resources vorhanden sind. Fehlt eine, nennt die Meldung das Plugin, und der Start wartet.

---

## 💡 Die Karte „Hinweise“

Auf dem ersten Reiter jedes Plugins steht oben eine Karte „Hinweise“. Dort erscheint alles, was du wissen solltest, bevor du speicherst:

- **Blau:** Informationen, etwa ein Hinweis des Plugin-Autors, ein anstehendes Update mit alter und neuer Version, oder dass eine Abhängigkeit aktiv ist.
- **Gelb:** Warnungen, etwa eine Abhängigkeit, die zwar da, aber nicht eingeschaltet ist. Schalte sie ein, sonst lässt sich das Plugin nicht speichern.
- **Rot:** Fehler, etwa eine fehlende Abhängigkeit, eine doppelt vorhandene DLL oder ein Workshop-Ordner, der bei RMM und der Workshop Bridge nicht derselbe ist. Ein rotes Feld blockiert das Speichern.

Bei Paketen, die es erlauben, liegt hier auch der Schalter „Dateien nur lokal“.

---

## 🛤️ Wie ein Plugin ins Spiel kommt

Der TesmioLoader allein lädt nur DLLs aus `tesmioloader\build\plugins`. Für Workshop-Pakete gibt es drei Wege, und RMM erkennt von selbst, welcher bei dir gilt:

1. **Workshop Bridge** (bei RMM dabei): Das Plugin `workshop_bridge` lädt die DLLs der eingeschalteten Pakete direkt aus dem Steam-Workshop-Ordner. Steam hält sie aktuell, nichts wird kopiert. RMM stellt nur die INI bereit.
2. **Soviet Mod Loader:** Ist SML installiert und eingeschaltet, lädt er alle abonnierten Pakete. RMM schreibt dann nur INI-Dateien und nie eine DLL. Liegt von einem Paket noch eine DLL in `plugins\`, weigert sich RMM zu speichern, weil das Plugin sonst doppelt laden würde. Die Bridge hält sich unter SML zurück.
3. **TesmioLoader klassisch:** Ohne Bridge und SML kopiert RMM DLL und INI nach `plugins\` und schaltet das Plugin in `tesmioloader.ini` ein.

**Resources, Deposits, Needs und Buildings gehören unter SML dem Mod Loader.** Diese vier Fähigkeiten sind in SML eingebaut, es gibt darum keine eigene `plugins\resources.dll` und so weiter, und ihre `plugins\*.ini` schreibt SML bei jedem Spielstart neu (die erste Zeile der Datei sagt das). Bearbeiten kannst du sie trotzdem: SML merkt sich seine Grundfassung unter `tesmioloader\build\soviet_mod_loader\base`, liest sie bei jedem Start und schreibt sie nie zurück — genau dorthin setzen die Editoren deine Einträge. Ein blauer Hinweis über den Karten sagt es, und in der Statuszeile steht „Grundfassung von Soviet Mod Loader". Solange SML noch nie gelaufen ist, gibt es diese Grundfassung nicht — dann steht auf der Seite, dass ein Spielstart sie anlegt.

**Was ein Paket mitbringt, steht mit Schloss in der Liste.** Damit du siehst, was wirklich im Spiel ist, zeigt der Editor nicht nur deine Grundfassung, sondern jeden Eintrag der fertig zusammengeführten Datei — ein Eintrag aus einem Paket also auch, mit Schloss und dem Namen des Pakets darunter. Ansehen kannst du ihn immer. Ausblenden nie: Zusammenführen kann eine Zeile ersetzen, aber keine zurücknehmen.

**Ändern geht trotzdem — über die Schicht „RMM Overlay".** Deine Grundfassung ist die schwächste Schicht, ein Paket gewinnt gegen sie. Änderst du einen Paketeintrag, legt RMM ihn darum in ein eigenes kleines Mod in deinem Workshop-Ordner (`rmm_overlay`, nur für Ressourcen und Bedürfnisse). SML liest es mit hoher Priorität nach allen Paketen, deine Zeile gilt also. Zwei Dinge gehören dazu: SML schreibt dem überstimmten Paket in seinem Bestätigungsfenster ein „conflict" ins Protokoll — das Paket bleibt aktiv, es ist nur der Hinweis, dass jemand anderes den Schlüssel beansprucht. Und nimmst du die Änderung zurück, verschwindet die Schicht wieder von selbst. „Neu anfangen" räumt sie ebenfalls weg.

**Zwischen SML und Bridge wechseln.** Damit beide Ladewege dieselbe Welt ergeben, hält RMM drei Dinge gleich: Die Reihenfolge der Ressourcen folgt SMLs Katalog, damit Spielstände ihre Ressourcen am gewohnten Platz finden. Vorkommen aus Inhaltspaketen bekommen die Typnummer, die SML ihnen gegeben hat. Und die eigene Ressourcenliste steht in beiden Dateien: Speicherst du unter der Bridge in Resources Plus, landen deine eigenen Einträge (ohne die der Pakete) auch in SMLs Grundfassung; speicherst du unter SML in Resources, landet die Grundfassung samt bereitgestellten Paketen auch in `plugins\resources_plus.ini`. Gebäude regelt Buildings Plus selbst: Es übernimmt SMLs Nummern aus dessen Katalog, beide Wege schreiben denselben Ordner. Beim Wechsel wird jeder Paketordner einmal vom anderen Generator neu geschrieben, das ist normal.

**Ein Plugin, das SML ersetzt, leuchtet orange.** Manche Pakete machen dasselbe wie eine der vier eingebauten Fähigkeiten — Deposits Plus zum Beispiel. Läuft SML, treten sie beim Spielstart von selbst zur Seite. Der Punkt in der Liste ist dann nicht grau (das hiesse „du hast es ausgeschaltet"), sondern orange. Auf der Seite selbst steht auf **jedem** Reiter eine gelbe Warnung, und alles darunter ist grau und gesperrt — sonst stellst du in Ruhe etwas ein, das gerade niemand liest. Willst du trotzdem etwas für später vorbereiten, gibt der Knopf „Trotzdem bearbeiten" die Seite für diese Sitzung frei.

Die Workshop Bridge erscheint selbst als Plugin in der Liste. Ihre Karte hat den Schalter „Bridge aktiv“, den Workshop-Ordner (normalerweise „auto“ = der Steam-Workshop-Ordner deines Spiels), die Regel, welche Pakete geladen werden, und einen Knopf „Jetzt aufräumen“, der Einträge von Paketen entfernt, die du nicht mehr hast. Die Paketliste selbst pflegst du nie von Hand; das macht der Schalter „Plugin aktiv“ der Pakete.

---

## 📦 Inhaltspakete (Ressourcen, Vorkommen, Gebäude)

Manche Workshop-Pakete bringen keine DLL mit, sondern nur Inhalte: neue Ressourcen, Vorkommen, Bedürfnisse oder Gebäude als INI-Abschnitte, dazu Dateien wie Symbole und Modelle. Solche Pakete erkennst du in der Liste daran, dass ihre Seite keine Einstellungen hat, sondern nur einen Schalter „Im Spiel bereitstellen“.

Einschalten und Speichern macht drei Dinge:

- Die Einträge wandern in die INIs der zuständigen Plugins: Ressourcen zu **Resources**, Vorkommen zu **Deposits Plus**, Bedürfnisse zu **Needs**, Gebäude zu **Buildings Plus**.
- Die mitgelieferten Dateien landen unter `tesmioloader\vfs`, wo das Spiel sie statt seiner eigenen liest.
- Bei Vorkommen vergibt RMM die Typnummer selbst, damit sie sich nie mit einem anderen Vorkommen beisst. Eine einmal vergebene Nummer bleibt.

In den Editoren erscheinen die Einträge als Originale mit Schloss: du kannst ihre Werte für dich überschreiben, aber den Eintrag selbst nicht löschen.

**Kollisionen gewinnst du.** Bringt ein Paket eine Kennung mit, die es in deinem Spiel schon gibt — deine eigene Ressource, ein Eintrag eines anderen Pakets oder einer aus der ausgelieferten INI des Plugins —, dann wird das Paket **gar nicht** bereitgestellt: keine Einträge, keine Dateien im vfs. Auf der Paketseite steht eine rote Meldung mit der Kennung, die im Weg ist. Nimm sie erst weg — einen eigenen Eintrag im RMM, einen Originaleintrag direkt in `plugins\<plugin>.ini` — und schalte das Paket danach wieder ein. Dein Bestand wird nie stillschweigend ersetzt, und eine Datei, die vor dem Paket im vfs lag, wird ohnehin weder überschrieben noch später gelöscht. Läuft Soviet Mod Loader, entfällt diese Meldung: dort gibt es keinen Schalter zum Bereitstellen, SML führt das Paket selbst, und der Eintrag „im Weg" ist meist genau der, den er daraus gebaut hat.

Ausschalten und Speichern nimmt Einträge und Dateien wieder heraus. Deine eigenen Einträge und deine Überschreibungen bleiben. Ändert sich das Paket im Workshop, erscheint in der Liste die gelbe Marke „Update“; einmal speichern übernimmt den neuen Stand.

Ist ein Plugin nicht eingerichtet, sagt die Paketseite das (zum Beispiel „Für Gebäude ist kein passendes Plugin eingerichtet“) und überspringt diesen Teil. Unter Soviet Mod Loader ist der Schalter gar nicht erst da: SML führt solche Pakete beim Spielstart selbst zusammen, abonnieren reicht. Die Paketseite sagt das mit einem blauen Hinweis und zeigt weiterhin, was drin ist.

---

## 📁 Dateien nur lokal

Manche Pakete erlauben, ihre Dateien komplett in den plugins-Ordner zu kopieren, etwa weil sie Texturen mitbringen. Dann steht in der Karte „Hinweise“ der Schalter „Dateien nur lokal“. Einschalten und Speichern zeigt zuerst jede Datei, die kopiert wird, samt Prüfsumme der DLL; nach „OK“ kopiert RMM DLL, INI und den Zubehörordner nach `plugins\`. Die Bridge überspringt das Paket danach von selbst, und der Schalter „Plugin aktiv“ arbeitet über `tesmioloader.ini`.

Ausschalten und Speichern entfernt genau diese Dateien wieder, nach einer Rückfrage mit Liste. Fremde Dateien in `plugins\` fasst RMM nie an. Steam-Updates wirken bei lokalen Dateien erst, wenn du erneut speicherst; das gelbe „Update“ in der Liste erinnert dich daran.

---

## ✅ Vor dem Spielstart

Das Klemmbrett-Symbol in der Seitenleiste öffnet eine Seite, die alles zusammenfasst, was der nächste Spielstart tun wird:

- **Überblick:** wie viele Plugins geladen werden, wie viele ausgeschaltet sind, wie viele Pakete ein ungespeichertes Update haben, wann das Spiel zuletzt lief und wie viele Spielstände gefunden wurden.
- **Was im Weg steht:** fehlende Abhängigkeiten, abgewiesene Pakete, wartende Updates — und Abhängigkeiten, die **zu spät** geladen werden. Ein Klick auf eine Zeile schliesst das Fenster und zeigt den betroffenen Eintrag.
- **Gebäudezeilen ohne Ressource:** eine `$STORAGE`-, `$PRODUCTION`- oder `$CONSUMPTION`-Zeile in Vanilla Buildings oder Buildings Plus, die eine Ressource nennt, die kein Plugin liefert. Damit stürzt das Spiel beim Einlesen der Gebäude ab; RMM nennt Plugin, Abschnitt und Ressource hier und fragt bei „Speichern + Starten" noch einmal nach.
- **Ladereihenfolge:** die Liste in der Reihenfolge, in der der Loader arbeitet. Zuerst alles aus `plugins\` in der Reihenfolge der `tesmioloader.ini`, danach die Workshop-Pakete über die Bridge in der Reihenfolge ihrer Ordnernamen. Das ist wichtig, wenn ein Plugin einen Dienst eines anderen braucht: Den gibt es erst, wenn der Anbieter geladen ist.

---

## 💾 Spielstände

RMM liest die Datei `tesmioloader.save.ini`, die der Loader neben jeden Spielstand legt. Darin steht, welche Plugins geladen waren und welche Ressourcen und Vorkommen die Welt kennt. Geschrieben wird dort nie etwas.

Daraus werden zwei Dinge:

- Auf der Seite eines Plugins oder Inhaltspakets steht eine Zeile **„Wird von … Spielständen benutzt"** mit den Namen.
- Beim **Ausschalten** fragt RMM nach und nennt genau diese Spielstände. „Nein" lässt den Schalter, wo er war.

---

## 📜 Das Protokollfenster

Das Protokoll-Symbol in der Seitenleiste öffnet ein Fenster mit drei Arten von Quellen: dem Journal dieser RMM-Sitzung, `tesmioloader.log` aus dem Loader-Ordner und jedem Plugin-Protokoll `tesmioloader.<plugin>.log`, egal ob es im Unterordner `logs\` oder direkt im Loader-Ordner liegt. Die Dateien lassen sich auch lesen, während das Spiel läuft; „Aktualisieren“ liest neu, „Ordner öffnen“ zeigt den Loader-Ordner im Explorer.

„Suchen“ filtert nach Text, „Absender“ nach dem ersten Wort einer Zeile (`plugin`, `bridge`, `hook`, ein Plugin-Name), „Nur Probleme und Warnungen“ blendet alles Unauffällige aus. Fehlerzeilen stehen rot, Warnungen gelb. Die Fußzeile nennt Zeilen, Treffer und die letzte Abschlusszeile des Loaders.

---


## ⚙️ Das Einstellungsfenster

Das Schieberegler-Symbol in der Seitenleiste öffnet die Einstellungen von RMM selbst — nicht die des gewählten Plugins:

- **Fenster des TesmioLaunchers zeigen:** AUS startet das Spiel sofort, EIN zeigt erst den Launcher.
- **Nach dem Start zusehen:** wie viele Sekunden RMM prüft, ob das Spiel oben bleibt. 0 schliesst sofort.
- **Fenster offen lassen:** EIN lässt RMM nach dem Start stehen; du schliesst es selbst.
- **Sprache** der Oberfläche, dasselbe wie der Knopf `DE` daneben.
- **Vor unbekannter Spielversion warnen:** EIN meldet, wenn dein Spiel nicht die Version ist, für die die Plugins gebaut wurden.
- **Meldungen des Spiels:** ein Schalter mit vier Stellungen, wie viel das Spiel in `tesmioloader.log` schreiben darf. Von links nach rechts: aus, nur Fehler, Fehler und Warnungen, alles. Unter dem Schalter steht, was die gewählte Stellung bedeutet.
- **Wiederholungen zeigen:** wie oft dieselbe Meldung ausgeschrieben wird, bevor sie nur noch gezählt wird. Die Summe steht am Ende des Protokolls. 0 schreibt jede Wiederholung aus.
- **Angaben für einen Fehlerbericht:** legt Versionen, Ordner, gefundene Plugins und den Zustand von Steam in die Zwischenablage.

Die zwei Werte der Karte **Protokoll** gehören dem Loader, nicht RMM: sie landen in `[tesmioloader]` der `tesmioloader.ini` und gelten ab dem nächsten Spielstart. RMM ändert dort nur diese beiden Zeilen, Kommentare und alle anderen Schlüssel bleiben stehen. Die Karte erscheint nur, wenn die Datei da ist.

Die übrigen Werte gelten sofort und werden beim Schliessen gespeichert — und zwar dort, wo RMM sich alles merkt, nicht in der `rmm.ini`. Weicht dein Wert von dem ab, was in der `rmm.ini` steht, sagt dir eine kleine Zeile unter dem Feld, was dort vorgegeben ist.

**Neu anfangen.** Ganz unten im Einstellungsfenster stehen zwei Knöpfe, die sauber getrennt sind:

- **RMM-Daten löschen** (gelb) räumt nur weg, was RMM sich selbst merkt: Profile, Wiederherstellungspunkte und den gemerkten Fensterzustand. Deine Plugins, ihre Einstellungen und deine Spielstände bleiben unberührt.
- **Alles zurücknehmen** (rot) nimmt zusätzlich alles zurück, was RMM je in den Loader-Ordner geschrieben hat: deine Überschreibungen, lokale Kopien in `plugins\`, Dateien im `vfs`, die Einträge in `tesmioloader.ini` und in der Liste der Workshop Bridge; die gesicherten Original-INIs schreibt RMM zurück.

Vor dem roten Knopf zeigt RMM erst, was verloren geht — **einschliesslich der Namen deiner Spielstände**, die auf die betroffenen Pakete bauen. Danach musst du das Wort `LÖSCHEN` eintippen, bevor der letzte Knopf überhaupt anklickbar wird. Der Fokus liegt überall auf Abbrechen, damit die Eingabetaste nichts kaputtmacht.

Vorher legt RMM eine Sicherung aller INI-Dateien unter `tesmioloader\rmm_reset_backup\<Zeitstempel>` an. Nicht angefasst werden der Spielordner mit deinen Spielständen, deine Abos im Workshop, der TesmioLoader selbst und alles, was RMM nie geschrieben hat. Willst du auch das Programm loswerden, nimm `Uninstall-RMM.bat` aus dem Workshop-Paket.

---
## 🗂️ Profile und Wiederherstellung

Das Archiv-Symbol in der Seitenleiste öffnet das Fenster „Profile und Wiederherstellung“ mit zwei Reitern.

**Profile** sichern den kompletten Einstellungsstand unter einem Namen: `tesmioloader.ini`, alle INIs in `plugins\` und `user_config\` und die gesicherten Originale. DLLs gehören nie dazu. „Aktuellen Stand sichern…“ fragt Name und Notiz ab. Rechts siehst du je Datei, ob sie zum gespeicherten Stand passt. „Profil anwenden“ schreibt alles zurück; INIs von Plugins, die das Profil nicht kennt, bleiben stehen. Profile liegen unter `user_config\.autoload\profiles\`.

**Wiederherstellungspunkte** entstehen bei jedem Speichern: der Stand aller berührten Dateien von vor dem Speichern, je Plugin genau einer. „Auf diesen Punkt zurücksetzen“ schreibt diese Dateien zurück, auch eine damals kopierte DLL; der jetzige Stand wird dabei zum neuen Punkt, du kannst also wieder zurück. Punkte, deren Dateien fehlen, werden nur angezeigt.

Beide Aktionen brauchen ein geschlossenes Spiel und einen geschlossenen TesmioLauncher.

---

## 📝 Listen-Editoren

Einige Plugins verwalten Listen statt einzelner Werte. RMM zeigt sie als Liste links und Felder rechts. Das gilt für die drei Plugins, die mit dem TesmioLoader kommen, und für Workshop-Pakete, die so eine Seite mitbringen (etwa Deposits Plus, Research Expansion, Technical Service Storage, Vanilla Buildings, UI Layout Fixes).

**Gemeinsam für alle:**

- Einträge aus dem Plugin oder Paket tragen ein Schloss. Du kannst sie ändern, aber nicht löschen; über den roten Knopf lassen sie sich ausblenden und unter „Ausgeblendete Originaleinträge“ wieder anzeigen.
- Eigene Einträge legst du mit „+“ an. Der Dialog fragt nur das Nötige ab und schlägt vor, was er kann; alles Weitere stellst du danach rechts ein. Eigene Einträge lassen sich löschen.
- Kennungen, die etwas im Spiel benennen müssen (eine Ressource, eine Forschung, eine Gebäudedatei, eine Text-ID), prüft RMM vor dem Speichern. Stimmt eine nicht, wird die Fußzeile rot und nennt Eintrag und Feld.
- Eine gelbe Warnung erinnert dich, wenn eine Änderung den Spielstand betrifft, etwa neue Ressourcen oder Vorkommen, die ein laufendes Spiel dann braucht.

**Resources:** Liste aller Ressourcen des Plugins. **Resources Plus** hat dieselbe Seite für `plugins\resources_plus.ini`, die Datei des fehlerbereinigten Forks; der Installer des RMM-Pakets schaltet das Original aus und Resources Plus ein, beide Einträge bleiben in der Liste. Unter SML leuchtet Resources Plus orange, es pausiert dort. Vorbildressource, Transportklasse und Materialfamilie sind Auswahllisten mit den Namen aus dem Spiel. Es gibt keinen Aus-Schalter, weil das Plugin keinen sicheren kennt. **Needs Plus** ist der gleiche Fork für needs: dieselbe Seite wie Needs, für `plugins\needs_plus.ini`, vom Installer eingeschaltet, unter SML orange.

**Needs:** Liste der Bedürfnisse mit Spender, Faktor, Ladenkategorie, Wahrscheinlichkeit und Unzufriedenheit. Höchstens acht Einträge. Der Reiter „Allgemein“ trägt die Schalter des Plugins.

**Deposits:** Reiter „Allgemein“ mit den Plugin-Schaltern und Reiter „Vorkommen“ mit einem Eintrag je Vorkommen: Kennung, Typnummer, Karte, Platz auf der Karte, Symbol und mehr. Die Typnummer schlägt der Dialog als nächste freie vor; eine doppelte wird abgewiesen.

**Buildings Plus:** Reiter „Allgemein“ mit den Plugin-Schaltern, Reiter „Gebäude“ mit deinen eigenen Erklärungen — und Reiter „SML-Gebäude“, siehe unten.

Der Reiter „Gebäude“ ist eine Liste mit einer Zeile je Gebäude: Schalter, Name, Nummer, Objektordner und Spender, rechts der Zustand und die Knöpfe **Ändern…**, **Öffnen** und der Papierkorb. Der Zustand sagt, was beim nächsten Spielstart passiert — ob das Gebäude im Spiel ist, erst noch geschrieben wird, neu gebaut wird, oder ob etwas im Weg steht: ein Spender, den es auf deinem Rechner nicht gibt, oder eine Nummer, die schon einem fremden Ordner gehört. Beides bricht sonst erst beim Spielstart ab und steht nur im Protokoll.

Hinter „Ändern…“ liegen die Angaben (Nummer, Spender, Objektname, Name im Spiel, Beschreibung, Lebensdauer) und ein zweiter Reiter für die Zeilen. Dort steht links die `building.ini` des Spenders — **durchgestrichen, was deine Erklärung wegwirft**. Denn eine einzige eigene `$STORAGE`-Zeile wirft *alle* Lager des Spenders weg, und eine `$PRODUCTION`-Zeile das ganze Rezept samt Verbrauch. Was du behalten willst, holst du mit einem Knopf zurück: **„Diese N Zeilen übernehmen“**. Die Ansicht lässt sich auf **Ergebnis** umschalten — dann siehst du die Datei, die beim nächsten Start wirklich entsteht, mit der Nummer jedes Lagers für `$RESOURCE_VISUALIZATION` und einer Warnung, wenn Wasser oder Abwasser vor einem angezeigten Lager steht.

**Paket-Editoren** bringen ihre eigenen Reiter, Listen und Hilfen mit. Manche haben Knöpfe, die Anleitungen des Pakets öffnen, Auswahlfenster für Gebäude, Forschungen oder Spieltexte, Bildvorschauen oder einen Reiter für Übersetzungen.

---

## 🏗️ Erzeugte Gebäude (Reiter „SML-Gebäude“)

Ein Gebäude aus einem Inhaltspaket wird beim Spielstart als vollständiges Workshop-Objekt nach `media_soviet\workshop_wip\<Nummer>\` geschrieben — von Buildings Plus oder, wenn Soviet Mod Loader läuft, von dessen eigenem Gebäude-Teil. Das sieht aus wie ein Abo, ist aber deine eigene Datei auf deiner Platte.

Der Reiter zeigt je Ordner Name, Nummer, Objektordner und Herkunft. Zwei Dinge meldet er von sich aus:

- **Besitzer fehlt.** Steht in der `workshopconfig.ini` eine `$OWNER_ID 0`, meldet das Spiel bei **jedem** Laden eines Spielstands, dass Workshop-Objekte fehlen. Soviet Mod Loader schreibt diese Null in jedes Gebäude, das es erzeugt. Der Knopf „Besitzer eintragen“ setzt deine Steam-ID ein — geändert wird genau diese eine Zahl, jedes andere Byte der Datei bleibt, wie es war.
- **Verwaist.** Gibt es zu einem Ordner kein Paket mehr, das ihn erklärt, **beendet Soviet Mod Loader das Spiel beim Start** mit einer Fehlerbox ohne Reparaturangebot. RMM sagt dir das vorher und nennt die Ordner. Lösche sie, sobald kein Spielstand die Gebäude mehr nutzt.

Ist Buildings Plus eingeschaltet, trägt es die fehlende Nummer beim Spielstart ohnehin selbst nach — der Knopf ist für den Fall, dass du es aus hast. Verändert wird sonst nichts: der Stempel `tesmioloader.stamp` bleibt unangetastet, denn ein Ordner ohne ihn bringt Soviet Mod Loader ebenfalls zum Abbruch.

### Ein erzeugtes Gebäude ändern

„Ändern…" öffnet die `building.ini` des Gebäudes. Links stehen die Zeilen, die der Generator schreibt, rechts **deine Änderungen**: eine Zeile ersetzen, eine Zeile entfernen, eine Zeile hinzufügen. Ein `*` markiert links jede Zeile, an der schon etwas von dir hängt.

Klickst du eine Zeile an, kommt der **ganze Block** ins Textfeld — also ein Token mit den Zeilen darunter, die ihm gehören (`$RESOURCE_VISUALIZATION` samt `position`, `rotation`, `scale`). Das ist nicht nur bequem: eine Zeile wie `rotation 0.0` steht in so einer Datei oft dreimal, und RMM ändert nie auf gut Glück die falsche. Als ganzer Block ist sie eindeutig, und damit lässt sie sich überhaupt erst ändern.

Wichtig ist, **was** gespeichert wird: nicht die geänderte Datei, sondern deine Änderungen. Das klingt nach Haarspalterei, entscheidet aber den Ernstfall — wenn das Paket ein Update bekommt, schreibt der Generator die Datei neu, und RMM spielt deine Änderungen in die **neue** Fassung ein. Was der Autor in der Zwischenzeit verbessert hat, bleibt also erhalten; eine gespeicherte Kopie hätte es weggeworfen. Passt eine Zeile nach dem Update nicht mehr (sie ist weg oder steht jetzt mehrfach da), sagt RMM das und lässt sie aus, statt still nichts zu tun.

Das Nachziehen passiert, sobald du den Reiter öffnest — also bevor du das Spiel startest. Eine blaue Zeile sagt dir, dass es passiert ist.

Hast du die Datei **von Hand** geändert, außerhalb von RMM, merkt RMM das (die Datei ist nicht mehr die, die es geschrieben hat) und fasst sie nicht an. Deine gespeicherten Änderungen liegen dann brach, bis du im Fenster auf „Übernehmen" gehst.

„Alles zurücksetzen" im Fenster wirft deine Änderungen weg und stellt die Fassung des Generators wieder her. Dasselbe macht „Alles zurücknehmen" im Einstellungsfenster für alle Gebäude auf einmal.

---

## 🎮 Spielversion und rmm.ini

Beim Start liest RMM die Kennung von `SOVIET64.exe`. Gehört sie zu keiner Spielversion, für die die Plugins gemacht sind, erscheint eine Warnung, und „Speichern + Starten“ fragt vor dem Start nach. Bekannt ist 1.1.1.9. Eine neue Kennung trägst du in `settings_schemas\game_versions.ini` unter `[supported]` ein.

`rmm.ini` neben `rmm.exe` hat wenige Schalter, alle mit Erklärung in der Datei:

| Schlüssel | Bedeutung |
|---|---|
| `[paths] workshop_root` | Ordner mit den Paketen. Fehlt die Zeile (oder steht ein `;` davor), nimmt RMM den Steam-Workshop-Ordner deines Spiels. Ein leerer Wert ist nicht erlaubt. |
| `[settings] language` | `auto` (Deutsch bei deutschem Windows, sonst Englisch), `de` oder `en`. |
| `[settings] version_check` | 0 schaltet die Warnung zur Spielversion ab. |
| `[settings] tesmiolauncher_window` | 1 zeigt das Fenster des TesmioLaunchers bei „Speichern + Starten“. |
| `[settings] launch_watch_seconds` | Wie lange RMM nach „Speichern + Starten“ zusieht, ob das Spiel oben bleibt. Standard 15, 0 schaltet es ab. |
| `[settings] keep_open` | 1 lässt das RMM-Fenster nach „Speichern + Starten“ offen stehen; du schliesst es selbst. Standard 0. |

Was du im Fenster einstellst, hat Vorrang vor der Datei.
**Ohne Fenster speichern.** Für Wartung von der Kommandozeile aus:

```bash
rmm.exe --save --build "<Spiel>\tesmioloader\build" --package "<Ordner des Plugins oder Pakets>"
```

Das ist derselbe Weg wie der Knopf Speichern, mit denselben Prüfungen: Spiel und TesmioLauncher müssen zu sein, alle Werte gültig. Jede Rückfrage, die das Fenster stellen würde, wird abgelehnt statt beantwortet; eine DLL wandert also nie ohne dich nach `plugins\`. Die Antwort ist eine Zeile, die mit PASS oder FAIL beginnt.

Den Kopfschalter legt `--activate` um, bevor gespeichert wird:

```bash
rmm.exe --activate on --save --build "<Spiel>\tesmioloader\build" --package "<Ordner>"
```

`on` schaltet ein, `off` aus. Das ist derselbe Klick wie im Fenster, nur ohne Maus: bei einem Inhaltspaket heißt das "Im Spiel bereitstellen", bei einem Plugin "Plugin aktiv". Geschrieben wird erst durch `--save`; ohne `--save` weist RMM die Option ab.

**Steam-Anmeldung prüfen.** Weil das Spiel hier direkt und nicht über Steam startet, braucht es einen angemeldeten Steam-Client. Ob einer da ist, sagt dir:

```bash
rmm.exe --steam-check
```

Die Antwort ist eine Zeile mit PASS oder FAIL und dazu das, was RMM in der Steam-Registrierung gefunden hat. Dieselbe Prüfung läuft vor „Speichern + Starten“; warnen tut sie nur, wenn Steam keinen angemeldeten Spieler einträgt oder gar kein Client läuft. Kann RMM nicht nachsehen, sagt es nichts und startet.

---

## 📦 Wo deine Dateien liegen

```
SovietRepublic\tesmioloader\build\
├── rmm.exe, rmm.ini                    RMM und seine Grundeinstellungen
├── settings_schemas\                   Einstellungsseiten für die Loader-Plugins
├── plugins\                            DLLs und wirksame INIs der Plugins
│   ├── workshop_bridge.dll, .ini       die Workshop Bridge
│   └── buildings_plus.dll, .ini        Buildings Plus (neue Gebäude aus einer Erklärung)
├── user_config\
│   ├── <name>.ini                      deine persönlichen Werte je Plugin
│   ├── workshop_bridge.ini             Paketliste der Bridge
│   └── .autoload\
│       ├── <name>.upstream.ini         gesicherte Originale
│       ├── backups\<paket>\previous\   Wiederherstellungspunkte
│       └── profiles\                   deine Profile
└── logs\                               Protokolle der Plugins
```

Fenstergröße, Sprache und Auswahl merkt sich RMM unter `%LOCALAPPDATA%\RepublicModManager`.

---

## 🛠️ Wenn etwas nicht klappt

| Was du siehst | Was dahintersteckt | Was hilft |
|---|---|---|
| Rotes Feld „Abhängigkeit fehlt“ | Ein Paket, das dieses Plugin braucht, ist nicht abonniert oder nicht eingeschaltet | Paket abonnieren und mit „Plugin aktiv“ einschalten |
| Rotes Feld „Doppelte DLL“ | Die DLL liegt in `plugins\` und wird zusätzlich von SML oder der Bridge geladen | Die Kopie in `plugins\` entfernen oder „Dateien nur lokal“ nutzen |
| Rotes Feld zum Workshop-Ordner | RMM und Bridge lesen verschiedene Ordner | In der Karte der Workshop Bridge den Ordner auf „auto“ stellen |
| „Beim letzten Spielstart nicht geladen“ | Das Plugin war beim letzten Start aus, oder das Spiel lief seit dem Einschalten nicht | Speichern und das Spiel einmal starten |
| Speichern geht nicht | Spiel oder TesmioLauncher läuft, oder ein Wert liegt außerhalb des Bereichs | Programme beenden, roten Hinweis in der Fußzeile lesen |
| Ein Wert steht nach dem Speichern wieder anders | Das Plugin las eine andere INI als erwartet | Statuszeile prüfen: Wo liegen die Dateien? Protokollfenster öffnen |

Für einen Fehlerbericht: Rechtsklick auf die Meldung, „Text kopieren“, dazu das Protokollfenster mit „Nur Probleme und Warnungen“.

---

## 🧩 Für Plugin-Autoren

Wie ein Workshop-Paket aufgebaut sein muss, damit RMM es zeigt, und wie eine Einstellungsseite mit Reitern, Karten, Feldern, Listen und Übersetzungen beschrieben wird, steht in `SCHEMA_DE.md` (englisch: `SCHEMA_EN.md`).

---

**Genosse, Achtung:** Dieses Programm wurde mit Hilfe einer künstlichen Intelligenz geschrieben. Die Fünfjahrespläne dazu hat trotzdem ein Mensch aufgestellt, getestet und beim Abstürzen des Spiels geflucht. Wer keine KI im Code möchte, bleibt einfach beim Grundspiel. Kein Hass, keine Umerziehung.

**GNU GPL v3.** Quelltext: https://github.com/Kespri/WRSR-MODS/tree/main/tools/republic_mod_manager
