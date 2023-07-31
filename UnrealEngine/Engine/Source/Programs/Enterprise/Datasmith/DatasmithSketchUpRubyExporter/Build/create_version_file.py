import sys

from pathlib import Path

import json



engine_path, output_file_path = sys.argv[1:]



build_version_fpath = (Path(engine_path)/'Build'/'Build.version')

assert build_version_fpath.is_file(), f"Expected Unreal Build version file '{build_version_fpath}' not found"



build_version_json = json.load(build_version_fpath.open())

version_str = f"{build_version_json['MajorVersion']}.{build_version_json['MinorVersion']}.{build_version_json['PatchVersion']}"

open(output_file_path, 'w').write(version_str)

