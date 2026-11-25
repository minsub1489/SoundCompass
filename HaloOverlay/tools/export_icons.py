"""Export the team's halo icon textures to PNG for the external overlay.

Run inside the Unreal editor (Output Log: py "<this file>") or headless:
UnrealEditor-Cmd.exe SoundCompass.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import os

import unreal

NAMES = ["Airplane", "Bullet", "Door", "Footprint", "GlassBreak", "Grenade", "Throw", "Wheel"]
OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "overlay", "icons"))

os.makedirs(OUT, exist_ok=True)
failed = []
for name in NAMES:
    texture = unreal.load_asset(f"/Game/Ariel_NewMaterial/Ariel_{name}.Ariel_{name}")
    task = unreal.AssetExportTask()
    task.object = texture
    task.filename = os.path.join(OUT, f"{name}.png")
    task.exporter = unreal.TextureExporterPNG()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    if texture is None or not unreal.Exporter.run_asset_export_task(task):
        failed.append(name)
unreal.log(f"export_icons: wrote {len(NAMES) - len(failed)} icons to {OUT}; failed: {failed}")
