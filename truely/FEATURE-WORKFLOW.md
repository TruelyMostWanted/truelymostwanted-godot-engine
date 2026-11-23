# Feature-Workflow der Truely-Engine

Wie ein Feature in diese Engine-Fork kommt: vom Issue über Branch, Build und Test bis zum Merge.
Gilt für jedes Feature- und Refactoring-Issue. Branches und Remotes stehen in [`README.md`](README.md).

> Dieses Dokument soll später zu einem Claude-Skill werden. Es ist deshalb in festen Schritten und Prüflisten geschrieben.

---

## 1. Grundregeln

Die Regeln für Code und Issues stehen in [`CONVENTIONS.md`](CONVENTIONS.md). Kurz:
- Neue Systeme sind **keine Nodes**, sondern C++-Singletons (Server) und `Resource`s (Daten), gebunden in `_bind_methods()`.
- C# bekommt die API über den generierten Glue: `TtsServer` (statisch) und `TtsServerInstance`.
- Jedes Feature hat Klassenreferenz, Wiki-Seite und Checks in GDScript und C# (Mono-Build).
- Issues auf Englisch, nach Vorlage, mit „Feasibility to check“ und den fünf Abnahmepunkten.

---

## 2. Ablauf pro Issue

```
Issue (Englisch, Vorlage)
  └─► Feature-Branch von truely-godot-engine
        ├─► Implementieren + beliebig oft committen
        ├─► Mono-Build (Editor + Glue + Assemblies)
        ├─► Testprojekt: Ordner res://<feature>/ mit GDScript- und C#-Test
        ├─► Push (nur WIP oder fertig)  ──►  GitHub Actions abwarten
        └─► grün? ──► Squash-Merge nach truely-godot-engine  (Feature-Branch bleibt bestehen!)
```

### 2.1 Feature-Branch

```powershell
git switch truely-godot-engine
git pull
git switch -c 03-feature-path-prefix-server
```

- **Ein Branch pro Issue**, abgezweigt von `truely-godot-engine`.
- **Name:** `<issue-nummer>-feature-<kurzname>`, die Nummer mindestens zweistellig: Issue #3 → `03-feature-path-prefix-server`, Issue #1 → `01-feature-resource-paths-retrieval`, Issue #123 → `123-feature-…`. Für Refactorings und Bugs `…-refactoring-…` bzw. `…-bug-…`.
- Beiträge an Godot selbst (Upstream-PRs) zweigen dagegen von `master` ab. Siehe [`README.md`](README.md).

### 2.2 Committen

- **Beliebig oft** in den Feature-Branch committen.
- Der pre-commit-Hook (clang-format, codespell, doc-status, …) muss durchlaufen. Korrigiert er Dateien, diese neu hinzufügen und noch einmal committen.

### 2.3 Pushen

- **Nur bei einem WIP-Stand oder wenn das Feature fertig ist.** Nicht nach jedem Commit.
- Jeder Push startet die volle CI mit allen Plattformen. Reine Änderungen an Vorlagen oder Doku dürfen `[skip ci]` in der Commit-Message tragen.

```powershell
git push -u origin 03-feature-path-prefix-server
```

### 2.4 CI abwarten

```powershell
gh run watch                      # den laufenden Lauf verfolgen
gh run list --branch 03-feature-path-prefix-server -L 3
```

Erst weitermachen, wenn der Lauf **grün** ist.

**Richtung:** Features gehen nach `truely-godot-engine`, nie umgekehrt. `truely-godot-engine` wird **nicht** in einen fertigen Feature-Branch gemergt, um die CI grün zu bekommen. Hat die Basis einen CI-Fix bekommen, den der Branch noch nicht hat: PR öffnen. Die CI des PR (`pull_request`) läuft auf dem Merge-Ergebnis aus Feature und aktueller Basis und ist dann das Tor für den Squash-Merge. Ist er rot, den Fehler im Feature-Branch beheben, committen, pushen und wieder abwarten.

### 2.5 Mergen

Bei grüner CI: **Squash-Merge** nach `truely-godot-engine`, also ein Commit pro Feature.

```powershell
gh pr create --base truely-godot-engine --head 03-feature-path-prefix-server --fill
gh pr merge --squash              # OHNE --delete-branch
```

- **Den Feature-Branch niemals löschen**, weder auf GitHub noch lokal. Er bleibt immer bestehen.
- In den Repo-Einstellungen ist „Automatically delete head branches“ (`delete_branch_on_merge`) **aus** und bleibt aus.
- Beim PR das *base repository* prüfen: `TruelyMostWanted/truelymostwanted-godot-engine`, nicht `godotengine/godot`.

> **Weiterarbeit auf einem gemergten Feature-Branch:** Nach dem Squash-Merge kennt `truely-godot-engine` die einzelnen Commits nicht.
> Zuerst `git merge truely-godot-engine` im Feature-Branch ausführen, dann weiterarbeiten. Sonst kommen alte Änderungen doppelt oder es gibt Konflikte.

---

## 3. Mono-Build

Ein Feature wird **immer mit Mono (C#)** gebaut, damit die C#-Bindings entstehen und getestet werden können.

```powershell
# 1. Editor mit Mono bauen
./truely/build.ps1 module_mono_enabled=yes

# 2. C#-Glue aus der gebauten Engine erzeugen
./bin/godot.windows.editor.x86_64.mono.exe --headless --generate-mono-glue modules/mono/glue

# 3. C#-Assemblies bauen und als lokale NuGet-Pakete ablegen
python modules/mono/build_scripts/build_assemblies.py --godot-output-dir=./bin --godot-platform=windows --push-nupkgs-local P:/Projects/GitHub/nuget-local
```

- Der Glue (Schritt 2) muss **nach jeder Änderung an gebundenen Klassen** neu erzeugt werden. Sonst fehlen die neuen Klassen in C#.
- Das Testprojekt bezieht `Godot.NET.Sdk` aus dem lokalen NuGet-Ordner (siehe 4.3), damit es die neuen Klassen kennt.
- Der Dateiname in Schritt 2 hängt von Plattform und Build ab. Den tatsächlichen Namen in `bin/` nachsehen.

---

## 4. Testprojekt

### 4.1 Repository

Alle Feature-Tests der Custom-Engine liegen im öffentlichen Repository
**[godot-custom-build-project](https://github.com/TruelyMostWanted/godot-custom-build-project)**.

```powershell
# lokal abrufen (einmalig)
gh repo clone TruelyMostWanted/godot-custom-build-project P:/Projects/GitHub/godot-custom-build-project
```

### 4.2 Aufbau

Wie unsere üblichen Godot-Projekte, aber **mit einem Ordner pro Feature** direkt unter `res://`. Der Ordner heißt wie der Feature-Branch der Engine.

```
godot-custom-build-project/
├── AGENTS.md / CLAUDE.md / README.md
├── documentation/
├── external-projects/
├── godot-exports/                       Export-Ziel (nicht versioniert)
└── godot-project/                       res://
    ├── project.godot
    ├── GodotCustomBuildProject.csproj   Godot.NET.Sdk aus dem lokalen NuGet-Ordner
    ├── nuget.config                     Godot-Pakete nur aus ../../nuget-local
    ├── checks/                          FeatureCheck (GDScript) + FeatureCheckCs (C#), new-feature.ps1, run-checks.ps1
    ├── _feature-template/               Vorlage für neue Feature-Ordner
    └── 03-feature-path-prefix-server/
        ├── scripts/gd/                  Checks in GDScript
        ├── scripts/cs/                  Checks in C#
        ├── scenes/                      check_gd.tscn, check_cs.tscn und weitere Testszenen
        ├── resources/                   Testdaten (.tres, .json, .ini, .csv, …)
        └── packed-scenes/               wiederverwendbare Szenen des Features
```

Neuen Feature-Ordner anlegen:

```powershell
./godot-project/checks/new-feature.ps1 -Name 03-feature-path-prefix-server
```

### 4.3 C#-Anbindung an den Custom-Build

- `nuget.config` holt `Godot.NET.Sdk`, `GodotSharp`, `GodotSharpEditor` und `Godot.SourceGenerators` **nur** aus dem lokalen Ordner
  `P:/Projects/GitHub/nuget-local` (relativ: `../../nuget-local`), alles andere von nuget.org.
- Die `.csproj` nutzt `Godot.NET.Sdk/4.8.0-beta`. Die Version folgt `version.py` der Engine. Mit `-Status truely` in `build.ps1`
  wird daraus `4.8.0-truely`; dann die `.csproj` anpassen.
- `build_assemblies.py --push-nupkgs-local` leert den NuGet-Cache für diese Pakete selbst (`ClearNuGetLocalCache=true`).
  Ein neuer Engine-Build mit derselben Version wird also übernommen.

### 4.4 Tests

- **Jedes Feature wird in GDScript und in C# getestet.** Beide Checks prüfen dasselbe.
- GDScript: `scripts/gd/check.gd` erbt von `FeatureCheck`, überschreibt `_run_checks()` und ruft `expect()` auf.
- C#: `scripts/cs/<Name>Check.cs` erbt von `FeatureCheckCs`, überschreibt `RunChecks()` und ruft `Expect()` auf.
- Jede Check-Szene läuft headless und endet mit **Exit-Code 0 (bestanden) oder 1 (fehlgeschlagen)**.

```powershell
./godot-project/checks/run-checks.ps1 -Godot P:/Projects/GitHub/truelymostwanted-godot-engine/bin/godot.windows.editor.x86_64.mono.exe -Feature 03-feature-path-prefix-server
```

- `run-checks.ps1` baut vorher das C#-Projekt (`dotnet build`), importiert das Projekt und gibt eine Tabelle aus.
- Die Ergebnisse (bestanden, fehlgeschlagen, Auffälligkeiten) kommen als Kommentar ins Issue der Engine.

---

## 5. Wiki

Die Features werden im **[Wiki der Engine](https://github.com/TruelyMostWanted/truelymostwanted-godot-engine/wiki)** dokumentiert, auf Englisch.
Die Seiten folgen dem Aufbau der **offiziellen Godot-Docs**. Es gibt zwei Arten von Seiten, die Vorlagen liegen im Wiki als `Template-Class-Reference` und `Template-Guide`.

### 5.1 Klassenreferenz: eine Seite pro Klasse

Vorbild: [ResourceLoader](https://docs.godotengine.org/en/stable/classes/class_resourceloader.html). Seitenname `Class-<Klasse>`, zum Beispiel `Class-TtsServer`.
Inhalt wie `doc/classes/<Klasse>.xml`, in dieser Reihenfolge:

| Abschnitt | Inhalt |
|---|---|
| Kopf | `# TtsServer`, darunter **Inherits:** `Object` (bei Bedarf **Inherited By:**) und die Kurzbeschreibung in einem Satz |
| Description | ausführliche Beschreibung, bei Bedarf mit Beispielen |
| Tutorials | Links zu den Guide-Seiten des Features |
| Properties | Tabelle: Typ, Name, Default |
| Methods | Tabelle: Rückgabetyp, Signatur |
| Signals | Signal mit Parametern und Beschreibung |
| Enumerations | `enum` mit Werten und Beschreibung |
| Constants | Konstanten mit Wert und Beschreibung |
| Property Descriptions | pro Property: Typ, Name, Default, Setter/Getter, Beschreibung |
| Method Descriptions | pro Methode: Signatur, Beschreibung |

Codebeispiele zeigen immer **GDScript, C# und GDExtension (C++, godot-cpp)** nacheinander. Die Godot-Docs nutzen dafür Reiter; im Wiki stehen sie als drei Codeblöcke mit den Überschriften **GDScript**, **C#** und **C++ (GDExtension)**.

### 5.2 Guide: eine Seite pro Feature

Vorbild: [Custom resource format loaders](https://docs.godotengine.org/en/4.4/contributing/development/core_and_modules/custom_resource_format_loaders.html). Seitenname wie der Feature-Branch, zum Beispiel `03-feature-path-prefix-server`.

| Abschnitt | Inhalt |
|---|---|
| Titel + Introduction | Was das Feature tut und welches Problem es löst |
| References | Links zu den Klassenreferenz-Seiten, zum Issue und zum Feature-Ordner in godot-custom-build-project |
| What is not covered | Was das Feature bewusst nicht kann (aus „Out of scope“) |
| Schritte | eine Überschrift pro Schritt der Benutzung (z. B. „Registering a prefix“, „Loading a resource“), jeweils mit GDScript, C# und C++ (GDExtension) |
| Limitations | was nicht geht, Plattform-Unterschiede (aus „Feasibility to check“) |

- **Wiki-Seiten sind immer Pflicht**, für jedes umgesetzte Issue, auch für Refactorings ohne neue API (Guide-Seite und Klassenreferenz jeder betroffenen Klasse). Entscheidungen aus Concept-Issues stehen auf der Seite `Design-Decisions`.
- **Inhaltsverzeichnis:** Jede Seite hat unter dem Titelblock ein nummeriertes Inhaltsverzeichnis (1, 1.1, 1.2, …) mit Sprungmarken zu den Überschriften. Es wird erzeugt, nicht von Hand geschrieben: `python truely/wiki-toc.py <wiki-ordner>` vor jedem Push des Wikis (ersetzt ein vorhandenes Verzeichnis zwischen `<!-- toc -->` und `<!-- /toc -->`).
- Die Seite `Home` listet alle Features mit Status, `_Sidebar` verlinkt Guides und Klassenreferenzen.
- Das Wiki ist ein eigenes Git-Repository: `https://github.com/TruelyMostWanted/truelymostwanted-godot-engine.wiki.git`.

---

## 6. Abnahme (Definition of Done)

Jedes Feature- und Refactoring-Issue endet mit diesen fünf Punkten:

- [ ] C++ data structures and API created
- [ ] C#/GDScript bindings created and registered
- [ ] Documentation written (class reference in `doc/classes/` and wiki page: GDScript, C#, GDExtension)
- [ ] Test build on the current platform done (no errors or warnings)
- [ ] Tested in the test project (`res://<feature-branch>/`) with a Mono build, in GDScript and in C#

Danach: Push → CI grün → Squash-Merge → Feature-Branch bleibt bestehen.

---

## 7. Später: Claude-Skill

Geplanter Skill, zum Beispiel `truely-engine-feature`, mit diesen Schritten:
1. Issue lesen, Feature-Branch anlegen.
2. Implementieren nach Abschnitt 1, beliebig oft committen.
3. Mono-Build nach Abschnitt 3.
4. Feature-Ordner im Testprojekt nach Abschnitt 4 anlegen, GDScript- und C#-Checks schreiben und ausführen.
5. Wiki-Seite nach Abschnitt 5 schreiben.
6. Abnahmepunkte aus Abschnitt 6 abhaken.
7. Pushen, CI abwarten, Squash-Merge, Branch behalten.
