# ============================================================
# Datei:   load_env.py
# Projekt: Briefkastensensor (C++ Portierung von MicroPython)
# Autor:   Jonas Thern
# Hinweis: Kommentare und Formatierung wurden mit Unterstuetzung
#          von kuenstlicher Intelligenz (Claude, Anthropic) erstellt.
# ============================================================
# PlatformIO Pre-Build-Skript: liest die .env Datei im Projektstamm
# und uebergibt die Werte als Compiler-Defines an die
# Provisionierungs-Firmware. Ersetzt das bisherige setup_NVS.py.

Import("env")
import os

ENV_FILE = os.path.join(env["PROJECT_DIR"], ".env")

# Zuordnung: .env Schluessel -> C++ Define
KEY_MAP = {
    "AES_MAIN":   "PROV_AES_MAIN",
    "WIFI_SSID":  "PROV_WIFI_SSID",
    "WIFI_PASS":  "PROV_WIFI_PASS",
    "SMTP_USER":  "PROV_SMTP_USER",
    "SMTP_PASS":   "PROV_SMTP_PASS",
    "RECIPIENTS":  "PROV_RECIPIENTS",
    "PORTAL_PASS": "PROV_PORTAL_PASS",
}


def parse_env_file(path):
    """Liest die .env Datei ein, ignoriert Leerzeilen und Kommentare."""
    values = {}
    if not os.path.isfile(path):
        print("Warnung: .env Datei nicht gefunden unter", path)
        return values
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" in line:
                key, val = line.split("=", 1)
                values[key.strip()] = val.strip()
    return values


env_data = parse_env_file(ENV_FILE)
defines = []

for env_key, define in KEY_MAP.items():
    if env_key in env_data:
        # StringifyMacro sorgt fuer korrekte Anfuehrungszeichen im Define
        defines.append((define, env.StringifyMacro(env_data[env_key])))
        print("Provisionierung: %s uebernommen" % env_key)
    else:
        print("Warnung: %s fehlt in der .env Datei" % env_key)

env.Append(CPPDEFINES=defines)
