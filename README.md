# Installationsanleitung

Diese Dokumentation beschreibt die Einrichtung der Entwicklungsumgebung für das Projekt.

## 1. Basiskomponenten

Installiere die folgenden Standard-Werkzeuge, sofern noch nicht vorhanden:

1. **Git:** [git-scm.com](https://git-scm.com/downloads) (Wähle während der Installation "Use Git from the Windows Command Prompt").
2. **GitHub Desktop:** [desktop.github.com/download/](https://desktop.github.com/download/) (Empfohlen für den einfachen und grafischen Git-Workflow).
3. **Python:** Installiere eine Python Distribution deiner Wahl.

---

## 2. Python-Umgebung & Thonny IDE Setup

Um Paketkonflikte zu vermeiden und eine saubere Entwicklungsumgebung für die Programmierung des Sensors zu gewährleisten, nutzen wir eine isolierte Umgebung innerhalb von Miniforge.

1. Öffne den **Miniforge Prompt** über das Windows-Startmenü.
2. Erstelle ein neues Environment mit Python und Pip:
   ```bash
   conda create -n thonny_env python=3.12 pip -y
   ```
3. Aktiviere das Environment:
   ```bash
   conda activate thonny_env
   
   ```
4. Installiere Thonny innerhalb dieses Environments:
   ```bash
   python -m pip install thonny
   
   ```
5. **Starten der IDE:** Um Thonny zu nutzen, öffne den Miniforge Prompt und gib folgende Befehle ein:
   ```bash
   conda activate thonny_env
   thonny
   
   ```

---

## 3. Git-Ersteinrichtung & Konfiguration

Damit Git deine Änderungen korrekt zuordnen kann, müssen Name und E-Mail hinterlegt werden. Bei Nutzung von GitHub Desktop erfolgt dies meist automatisch bei der Anmeldung. Alternativ öffne ein Terminal (z. B. in VS Code oder den Miniforge Prompt) und führe aus:

```bash
git config --global user.name "Dein Vor- und Nachname"
git config --global user.email "deine.email@beispiel.de"
```

* **VS Code Extensions:** Die "Git Base" Extension ist standardmäßig aktiv. Für eine bessere Übersicht sind "GitHub Pull Requests" & "GitLens" empfehlenswert.
* **Authentifizierung:** Beim ersten Hochladen öffnet sich in der Regel automatisch ein Browserfenster zur Anmeldung ("Sign in with GitHub").

---

## 4. Projekt-Setup

Verknüpfe dein lokales Arbeitsverzeichnis mit dem Projekt-Repository.

**Option A: Über GitHub Desktop (Empfohlen)**
1. Gehe in GitHub Desktop auf `File` > `Clone repository...`.
2. Wähle den Tab `URL` und füge den Link ein: `https://github.com/TheSecretJas/BriefkastenSensor`
3. Wähle deinen lokalen Zielordner aus und klicke auf `Clone`.
4. Öffne den resultierenden Ordner anschließend in VS Code (`Datei` > `Ordner öffnen...`).

**Option B: Über das Terminal (Alternativ)**
1. Erstelle einen lokalen Ordner für deine Projekte.
2. Klone das **Briefkastensensor** Repository:
   ```bash
   git clone [https://github.com/TheSecretJas/BriefkastenSensor](https://github.com/TheSecretJas/BriefkastenSensor)
   
   ```
3. Öffne den Ordner in gewünschter SW, um Änderungen vorzunehmen:
   * `Datei` > `Ordner öffnen...` > Wähle den Ordner `BriefkastenSensor` aus.


---

## 5. Git-Workflow

Da die Arbeit mit der Konsole im Entwicklungs-Alltag schnell unübersichtlich wird, ist **GitHub Desktop** die empfohlene Alternative.

### 5.1 Workflow mit GitHub Desktop (Empfohlen)

1. **Stand synchronisieren:** Klicke oben in der Leiste auf **"Fetch origin"** (und anschließend auf **"Pull origin"**, falls neue Änderungen vorhanden sind), um vor Arbeitsbeginn den aktuellen Stand vom Server zu laden.
2. **Feature-Branch erstellen:** Klicke oben auf **"Current Branch"** -> **"New Branch"**. Gib einen passenden Namen ein (z. B. `feature/name-der-anpassung`) und erstelle den Branch basierend auf `main`.
3. **Änderungen committen:** 
   * Prüfe in der linken Spalte die geänderten Dateien und wähle nur die aus, die wirklich Teil deiner Änderung sind.
   * Gib unten links unter **"Summary"** eine kurze, prägnante Beschreibung der Änderung ein.
   * Klicke auf den blauen Button **"Commit to [Branch-Name]"**.
4. **Branch aktuell halten:** Bei längerer Bearbeitungsdauer im oberen Menü auf **"Branch"** -> **"Merge into current branch..."** gehen, `main` auswählen und bestätigen, um Konflikte frühzeitig zu lösen.
5. **Push & Pull Request:** Klicke oben auf **"Publish branch"** (oder **"Push origin"**). Danach erscheint ein Button **"Create Pull Request"**, der dich direkt zu GitHub weiterleitet, um deine Änderungen einzureichen.

### 5.2 Workflow über die Konsole (Alternativ)

1. **Stand synchronisieren:**
   ```bash
   git pull origin main
   ```
2. **Feature-Branch erstellen:**
   ```bash
   git switch -c feature/name-der-anpassung
   ```
3. **Änderungen committen:** (Vorab den Status prüfen)
   ```bash
   git status
   git add .
   git commit -m "Kurze, prägnante Beschreibung der Änderung"
   ```
4. **Branch aktuell halten:**
   ```bash
   git merge main
   ```
5. **Push & Pull Request:**
   ```bash
   git push origin feature/name-der-anpassung
   
   ```
