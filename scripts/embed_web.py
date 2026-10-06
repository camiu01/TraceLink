"""Embed web assets into the firmware as a generated C++ header."""
from pathlib import Path
import json

Import("env")

root = Path(env.subst("$PROJECT_DIR"))
generated = Path(env.subst("$BUILD_DIR")) / "generated"
generated.mkdir(parents=True, exist_ok=True)
page = (root / "web" / "index.html").read_text(encoding="utf-8")
systems = json.loads((root / "web" / "systems.json").read_text(encoding="utf-8"))
if not isinstance(systems, list):
	raise ValueError("systems.json must contain a list")
for system in systems:
	if not isinstance(system.get("name"), str) or not isinstance(system.get("bus"), str):
		raise ValueError("Each system needs name and bus strings")
	if not isinstance(system.get("rate"), int) or not isinstance(system.get("commands"), list):
		raise ValueError("Each system needs an integer rate and a commands list")
	for command in system["commands"]:
		if not isinstance(command.get("name"), str) or not isinstance(command.get("data"), str):
			raise ValueError("Each command needs name and data strings")
		if command.get("encoding") not in ("hex", "text"):
			raise ValueError("Each command needs hex or text encoding")
delimiter = "TRACELINK_HTML"
if f"){delimiter}\"" in page:
	raise ValueError("HTML contains the C++ raw string delimiter")
header = (
	"#pragma once\n#include <Arduino.h>\n"
	f'const char webPage[] PROGMEM = R"{delimiter}({page}){delimiter}";\n'
	f'const char systemsJson[] PROGMEM = {json.dumps(json.dumps(systems, ensure_ascii=True))};\n'
)
target = generated / "web_page.h"
if not target.exists() or target.read_text(encoding="utf-8") != header:
	target.write_text(header, encoding="utf-8")
env.Append(CPPPATH=[str(generated)])
