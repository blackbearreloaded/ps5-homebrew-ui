# ps5-native-app-boilerplate - Incremental compiler dependency regression.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class IncrementalBuildTests(unittest.TestCase):
    def test_headers_flags_and_unchanged_build(self):
        compiler = shutil.which("clang++-18") or shutil.which("clang++")
        if not compiler or not shutil.which("ninja"):
            self.skipTest("Clang and Ninja are required")
        with tempfile.TemporaryDirectory(prefix="ninja test ") as directory:
            work = Path(directory)
            header = work / "value.hpp"
            header.write_text("#define VALUE 1\n", encoding="utf-8")
            (work / "main.cpp").write_text(
                '#include "value.hpp"\nint value() { return VALUE + EXTRA; }\n',
                encoding="utf-8",
            )
            script = r'''
set -euo pipefail
source "$1/tools/ninja-build.sh"
cd "$2"
ninja_begin "$2/build.ninja"
ninja_inputs=("$2/main.cpp" "$3")
ninja_edge CXX "$2/main.o" "$3" -DEXTRA="$4" -MD -MF "$2/main.o.d" -c "$2/main.cpp" -o "$2/main.o"
ninja_run
'''

            def build(extra="0"):
                result = subprocess.run(
                    ["bash", "-c", script, "test", str(ROOT), directory, compiler, extra],
                    env={**os.environ, "USE_CCACHE": "0", "BUILD_JOBS": "2"},
                    capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return result.stdout

            self.assertIn("CXX main.o", build())
            first = (work / "main.o").read_bytes()
            self.assertIn("no work to do", build())
            header.write_text("#define VALUE 2\n", encoding="utf-8")
            self.assertIn("CXX main.o", build())
            self.assertNotEqual(first, (work / "main.o").read_bytes())
            self.assertIn("CXX main.o", build("3"))
            self.assertIn("no work to do", build("3"))
