#!/usr/bin/env python3
# ps5-native-app-boilerplate - Host tooling regression tests.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Exercises identity initialization and deployment resolution without a console.

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ToolTests(unittest.TestCase):
    def run_init(self, param, **values):
        environment = os.environ.copy()
        environment.update(values)
        return subprocess.run(
            ["bash", str(ROOT / "tools/init-project.sh"), str(param)],
            cwd=ROOT,
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )

    def test_init_coordinates_media_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            param = Path(directory) / "param.json"
            param.write_text(
                json.dumps(
                    {
                        "contentId": "UP9000-PPSA99999_00-HELLOWORLD000001",
                        "localizedParameters": {
                            "defaultLanguage": "en-US",
                            "en-US": {"titleName": "Old"},
                        },
                        "gameIntent": {"permittedIntents": [{"intentType": "launchActivity"}]},
                    }
                ),
                encoding="utf-8",
            )
            result = self.run_init(
                param,
                TITLE_ID="PPSA12345",
                APP_NAME="Moon Client",
                APP_CATEGORY="media",
                CONTENT_SUFFIX="MOONCLIENT000001",
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            configured = json.loads(param.read_text(encoding="utf-8"))
            self.assertEqual(configured["titleId"], "PPSA12345")
            self.assertEqual(configured["conceptId"], "12345")
            self.assertEqual(configured["contentId"], "UP9000-PPSA12345_00-MOONCLIENT000001")
            self.assertEqual(configured["localizedParameters"]["en-US"]["titleName"], "Moon Client")
            self.assertEqual(configured["applicationCategoryType"], 65536)
            self.assertEqual(configured["contentBadgeType"], 2)
            self.assertNotIn("gameIntent", configured)

    def test_init_rejects_invalid_title_without_rewriting(self):
        with tempfile.TemporaryDirectory() as directory:
            param = Path(directory) / "param.json"
            original = '{"contentId":"UP9000-PPSA99999_00-HELLOWORLD000001"}\n'
            param.write_text(original, encoding="utf-8")
            result = self.run_init(param, TITLE_ID="PPSA12", APP_NAME="Broken")
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(param.read_text(encoding="utf-8"), original)

    def test_init_derives_game_suffix_and_preserves_mode(self):
        with tempfile.TemporaryDirectory() as directory:
            param = Path(directory) / "param.json"
            param.write_text(
                json.dumps(
                    {
                        "contentId": "UP9000-PPSA99999_00-HELLOWORLD000001",
                        "localizedParameters": {
                            "defaultLanguage": "en-US",
                            "en-US": {"titleName": "Old"},
                        },
                    }
                ),
                encoding="utf-8",
            )
            param.chmod(0o640)
            result = self.run_init(
                param, TITLE_ID="PPSA54321", APP_NAME="Native Sample", CONTENT_SUFFIX=""
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            configured = json.loads(param.read_text(encoding="utf-8"))
            self.assertEqual(configured["contentId"], "UP9000-PPSA54321_00-NATIVESAMPLE0000")
            self.assertEqual(configured["applicationCategoryType"], 0)
            self.assertEqual(configured["contentBadgeType"], 1)
            self.assertEqual(
                configured["gameIntent"]["permittedIntents"],
                [{"intentType": "launchActivity"}],
            )
            self.assertEqual(param.stat().st_mode & 0o777, 0o640)

    def test_undeploy_dry_run_resolves_only_current_title(self):
        environment = os.environ.copy()
        environment.update(PS5_HOST="192.0.2.1", DEPLOY_DRY_RUN="1")
        result = subprocess.run(
            ["bash", str(ROOT / "tools/deploy.sh"), "undeploy"],
            cwd=ROOT,
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        param = json.loads((ROOT / "sce_sys/param.json").read_text(encoding="utf-8"))
        title = param["titleId"]
        self.assertIn(f"/data/homebrew/{title}/", result.stdout)
        # An image left by an older version is still cleaned up.
        self.assertIn(f"{title}.{{ffpkg,ffpfsc}}", result.stdout)
        self.assertIn("no network request was sent", result.stdout)

    def test_deploy_dry_run_uses_mocked_build_and_no_network(self):
        with tempfile.TemporaryDirectory() as directory:
            sandbox = Path(directory)
            (sandbox / "tools").mkdir()
            (sandbox / "sce_sys").mkdir()
            shutil.copy2(ROOT / "tools/deploy.sh", sandbox / "tools/deploy.sh")
            (sandbox / "sce_sys/param.json").write_text(
                '{"titleId":"PPSA12345"}\n', encoding="utf-8"
            )

            mock_bin = sandbox / "mock-bin"
            mock_bin.mkdir()
            mock_make = mock_bin / "make"
            mock_make.write_text(
                "#!/usr/bin/env bash\n"
                "printf '%s\\n' \"$*\" > \"$MOCK_ROOT/make-arguments\"\n"
                "mkdir -p \"$MOCK_ROOT/dist/PPSA12345/sce_sys\"\n"
                "printf eboot > \"$MOCK_ROOT/dist/PPSA12345/eboot.bin\"\n"
                "printf param > \"$MOCK_ROOT/dist/PPSA12345/sce_sys/param.json\"\n",
                encoding="utf-8",
            )
            mock_make.chmod(0o755)

            environment = os.environ.copy()
            environment.update(
                PS5_HOST="192.0.2.1",
                DEPLOY_DRY_RUN="1",
                MOCK_ROOT=str(sandbox),
                PATH=f"{mock_bin}{os.pathsep}{environment['PATH']}",
            )
            result = subprocess.run(
                ["bash", str(sandbox / "tools/deploy.sh")],
                cwd=sandbox,
                env=environment,
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("/data/homebrew/PPSA12345/\n", result.stdout)
            self.assertIn("Would publish 2 files", result.stdout)
            self.assertIn("no network request was sent", result.stdout)
            built = sandbox / "make-arguments"
            self.assertEqual(built.read_text(encoding="utf-8").split()[-1], "app")

            # The folder is the only output: an image format is refused before any build.
            built.unlink()
            environment["DEPLOY_FORMAT"] = "ffpkg"
            refused = subprocess.run(
                ["bash", str(sandbox / "tools/deploy.sh")],
                cwd=sandbox,
                env=environment,
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(refused.returncode, 2, refused.stdout)
            self.assertIn("DEPLOY_FORMAT", refused.stderr)
            self.assertFalse(built.exists())

    def test_automation_builds_the_zip_only(self):
        workflow = (ROOT / ".github/workflows/tooling.yml").read_text(encoding="utf-8")
        self.assertNotIn("ffpfsc", workflow.lower())
        self.assertNotIn("mkpfs", workflow.lower())
        self.assertIn("run: make app", workflow)
        self.assertIn('sha256sum "$TITLE_ID.zip" > SHA256SUMS', workflow)
        self.assertIn('assets=("release/$FOLDER_ZIP" "release/$CHECKSUM")', workflow)
        # The ZIP is attested (signed provenance) once final, before the upload.
        attest = (
            "uses: actions/attest@1e69f48acb82d1966a394da916b4c1698aa569d6"
            " # v4.2.2"
        )
        self.assertEqual(workflow.count(attest), 1)
        checked = workflow.index("- name: Check every ZIP entry is stored as 0777\n")
        upload = workflow.index("- name: Upload build\n")
        self.assertLess(checked, workflow.index(attest))
        self.assertLess(workflow.index(attest), upload)
        self.assertIn("subject-path: dist/${{ env.TITLE_ID }}.zip\n", workflow)
        self.assertIn(
            "if: github.event_name != 'pull_request' && !github.event.repository.private\n",
            workflow,
        )
        needed = ("contents: read", "id-token: write", "attestations: write")
        for permission in needed:
            self.assertIn(f"      {permission}\n", workflow)

        # No image is built: no target, no build mode, no script branch, no tooling.
        gone = ("ffpkg", "ffpfsc", "exfat", "ufs2tool", "mkpfs")
        for name in ("Makefile", "tools/build.sh", "build.ps1",
                     ".github/workflows/tooling.yml"):
            text = (ROOT / name).read_text(encoding="utf-8").lower()
            for word in gone:
                self.assertNotIn(word, text, name)
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertNotIn("\npackages:", makefile)
        self.assertNotIn("DEPLOY_FORMAT", makefile)
        self.assertNotIn("OutputFormat", (ROOT / "build.ps1").read_text(encoding="utf-8"))
        for name in ("setup-packaging-dependencies.sh", "setup-ffpkg-tooling.ps1",
                     "setup-mkpfs-tooling.ps1", "pack-exfat.py"):
            self.assertFalse((ROOT / "tools" / name).exists(), name)
        # A deploy builds and uploads the folder; it only deletes an old image.
        deploy = (ROOT / "tools/deploy.sh").read_text(encoding="utf-8")
        self.assertIn('make -C "$root" --no-print-directory app\n', deploy)
        self.assertNotIn("$format", deploy)

    def test_release_job_never_replaces_published_files(self):
        workflow = (ROOT / ".github/workflows/tooling.yml").read_text(encoding="utf-8")
        self.assertNotIn("--clobber", workflow)
        self.assertNotIn("delete-asset", workflow)
        self.assertNotIn("gh release edit", workflow)
        # One upload, to a release that has no ZIP yet; one warning otherwise.
        self.assertEqual(workflow.count("gh release upload"), 1)
        self.assertIn("--json assets --jq '.assets[].name'", workflow)
        self.assertIn("::warning title=Release files not from this run::", workflow)

    def test_pull_request_builds_are_named_and_labelled(self):
        workflow = (ROOT / ".github/workflows/tooling.yml").read_text(encoding="utf-8")
        self.assertIn(
            'echo "artifact=${GITHUB_REPOSITORY##*/}-PR$PR_NUMBER-$short" >> "$GITHUB_OUTPUT"',
            workflow,
        )
        self.assertIn('echo "BUILD_LABEL=PR $PR_NUMBER, $short" >> "$GITHUB_ENV"', workflow)
        self.assertIn("name: ${{ steps.label.outputs.artifact }}", workflow)
        # The release job still finds a tag's build under its commit.
        self.assertIn('--name "ps5-homebrew-ui-$GITHUB_SHA"', workflow)
        self.assertIn('echo "artifact=ps5-homebrew-ui-$GITHUB_SHA" >> "$GITHUB_OUTPUT"', workflow)
        # A contributor's code is never built with write access or secrets.
        self.assertNotIn("pull_request_target:", workflow)
        build = (ROOT / "tools/build.sh").read_text(encoding="utf-8")
        self.assertIn('> "$app/build-label.txt"', build)
        self.assertIn("{1,40}$", build)
        self.assertLess(build.index("BUILD_LABEL must be"), build.index("ninja_run\n\napp="))

    def test_build_label_is_checked_before_anything_is_built(self):
        build = (ROOT / "tools/build.sh").read_text(encoding="utf-8")
        start = build.index("if [[ -n ${BUILD_LABEL:-} ]]; then")
        check = build[start : build.index("\nfi\n", start) + 4]
        cases = {
            "PR 12, 1a2b3c4": 0,
            "pacing_test-2.#1": 0,
            "x" * 40: 0,
            "x" * 41: 2,
            "PR 12; rm -rf": 2,
            "line\nbreak": 2,
            "$(id)": 2,
            "a/b": 2,
        }
        for label, expected in cases.items():
            environment = os.environ.copy()
            environment["BUILD_LABEL"] = label
            result = subprocess.run(
                ["bash", "-c", "set -euo pipefail\n" + check],
                env=environment,
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, expected, label)

    def test_native_writer_anchors_relro_and_checks_load_congruence(self):
        source = (ROOT / "tooling/native/sce_module_writer.cpp").read_text(
            encoding="utf-8"
        )
        # RELRO is anchored at its first section: .data.rel.ro whenever lld
        # keeps it, otherwise the GOT, so a program without relocated
        # read-only data still converts.
        self.assertIn("relro_origin(image, relro_start)", source)
        self.assertIn('if (input.name == ".data.rel.ro")', source)
        self.assertIn("relro_source.file_offset", source)
        self.assertIn(
            "header.offset % header.alignment == header.address % header.alignment",
            source,
        )
        self.assertNotIn("const std::uint64_t relro_file = got.file_offset", source)


if __name__ == "__main__":
    unittest.main()
