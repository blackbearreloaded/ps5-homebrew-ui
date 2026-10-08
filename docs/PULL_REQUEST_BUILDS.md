# Pull-request builds

Every pull request is built in full by the [Build workflow](../.github/workflows/tooling.yml):
lint, host tests, and the same packaging a release gets. The result is an installable copy
of the demo app, **Homebrew UI Lab** (`PPSA99050`), kept for 14 days, that a reviewer can
put on a console before merging.

## What a pull request produces

| | Pull request | Tag, or a run started by hand |
| --- | --- | --- |
| Artifact name | `ps5-homebrew-ui-PR<number>-<commit>` | `ps5-homebrew-ui-<full commit>` |
| `<commit>` | First seven characters of the pull request's own head commit | The built commit, in full |
| Label file in the app folder | `PR <number>, <commit>` | None |
| `contentVersion` | Unchanged | Unchanged |

Pull request 12 at commit `1a2b3c4` gives the artifact `ps5-homebrew-ui-PR12-1a2b3c4` and
the label `PR 12, 1a2b3c4`. In a fork or a renamed repository the first part follows the
repository's name.

Two details are deliberate:

- **The commit is the pull request's head**, not `github.sha`. For a pull request,
  `github.sha` is a temporary merge commit that appears nowhere on the pull request's page,
  so an artifact named after it cannot be matched to what is being reviewed.
- **The version is not touched.** A test build carries the same `contentVersion` in
  `param.json` as the release it is based on. The label is a separate file.

## Getting the build

1. Open the pull request, then **Checks** and the **Build** run (or the run's page under
   **Actions**).
2. Download the artifact named `ps5-homebrew-ui-PR<number>-<commit>` from the run's
   **Artifacts** list. GitHub requires a signed-in account for this.
3. Unpack it: it holds `PPSA99050.zip` (the app folder) and `SHA256SUMS`. Check the ZIP
   with `sha256sum -c SHA256SUMS`, then install as described in
   [Deployment](platform/DEPLOYMENT.md).

A first-time contributor's pull request does not build until a maintainer approves the
workflow run. That is GitHub's default for public repositories and is worth keeping: the
build runs the pull request's code.

## The label file

`tools/build.sh` writes the environment variable `BUILD_LABEL` as one line to
`build-label.txt` at the root of the app folder, next to `eboot.bin`
(`dist/PPSA99050/build-label.txt` after a build, `/app0/build-label.txt` on the console).
The workflow sets it for pull requests only. A build without it writes no file, so a
release never carries one.

`BUILD_LABEL` must be 1 to 40 characters from letters, digits, spaces and `, . _ # -`. The
build refuses anything else before compiling, so the text is safe to show as it is.

Homebrew UI Lab does not show the label on screen. To tell which build is installed, read
the file in the app folder, or in the ZIP before installing:

```bash
unzip -p PPSA99050.zip PPSA99050/build-label.txt
```

An app built on the kit can read `/app0/build-label.txt` where it shows its version, such
as an About page, and show nothing when the file is missing. Do not put the label into
`param.json` or compare it with anything: it is for people.

The same works on your PC, for a build you want to tell apart on the console:

```bash
BUILD_LABEL="pacing test 2" make
```

## Names and permissions

The name used for tags and runs started by hand, `ps5-homebrew-ui-<full commit>`, appears twice in the
workflow: the build job's "Name this build" step and the release job's download. A tag
never takes the pull-request branch, so the release job finds its build under that name;
rename both together if the name ever changes.

Pull-request runs have a read-only token and no secrets, including for forks. Do not move
this build to `pull_request_target` to post links or comments: that event runs with write
access and secrets, and building a contributor's code under it hands both to that code.
