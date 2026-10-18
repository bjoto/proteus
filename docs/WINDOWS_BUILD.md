# Windows boat study builds

The Windows boat study GitHub Actions workflow builds a Release x64 executable
on windows-2022 with Visual Studio 2022. It runs on pushes and can also be
started manually from the Actions tab after the workflow exists on the default branch.
No Windows development tools are needed on your Mac or the test machine.

## Download and test

For versioned downloads, use the ZIP under **Releases** once a version is published;
see [automated releases](RELEASES.md). Extract `boat-study-windows-x64.zip` completely
on Windows and double-click `launch.bat`. For an ordinary branch build:

1. Commit and push the source and .github/workflows/windows-boat.yml to GitHub.
2. Open the repository's **Actions** tab and select **Windows boat study**.
3. Open the successful run and download **boat-study-windows-x64** under **Artifacts**.
4. Transfer the ZIP to Windows and extract it completely before running it.
5. Double-click launch.bat to run boat_study.exe --location eriksberg --gpu --gpu-aa.

Artifacts expire after 30 days. build.txt records the source commit. The package
includes shader/texture assets, background music, and map attribution; geometry is
compiled into the executable. Asset loading uses the executable directory, so the folder can be moved.
raylib and the MSVC runtime are statically linked for this workflow.

The workflow runs the boat handling validator and CPU screenshot checks, including
a relocated executable launched from another working directory. These do not exercise
GPU shader loading. On the Windows test machine, check startup, shadows, reflections,
steering, thrust, camera controls, reset, and exit. Try --gpu without --gpu-aa if
performance is poor. OpenGL 3.3 support is required for GPU mode.

The executable also accepts `--location river` for the bridge expansion, `--waves`
for gentle waves, and `--course` for the basin time trial. Renderer defaults are the
same as on macOS: use `--renderer cpu` explicitly for the software reference.
See the [main controls and replay guide](../README.md#boating-sketch) for the full interface.
Workflow build/CPU checks do not establish a successful human Windows playthrough.

## Connect an existing checkout to GitHub

Create an empty GitHub repository without a generated README, license, or gitignore.
From the local repository root, substituting your account and repository name:

```sh
git remote add origin https://github.com/YOUR-USERNAME/proteus.git
git remote -v
```

After committing the files you want uploaded, push the current branch:

```sh
git push -u origin HEAD
```

Use your configured GitHub credentials for HTTPS, or an SSH remote if you have
already set up a GitHub SSH key. Pushes upload committed files only.

See [GitHub's artifact download instructions](https://docs.github.com/en/actions/managing-workflow-runs/downloading-workflow-artifacts).

## Current status and verification

2026-10-18: the Windows Release run at commit `73bf1ba` failed the mapped reversing
assertion while both CPU capture tests passed. A local Release probe reproduced the
assertion with a starting-heading perturbation of -0.00001 radians: the boat travelled
10.17 units backward, but ended only 2.27 units from where reverse thrust was selected.
The validator now measures actual backward displacement along the hull each tick,
retains the three-unit minimum and shoreline checks, and covers both quay directions
at three nearby starting headings. This changes validation, not boat handling.

The package now includes `a-little-boat.xm`; its relocation check explicitly selects
the CPU renderer. On Apple Silicon/macOS 26, Debug configure/build and all 69 CTests
passed with desktop access (18 GPU checks); the Release handling test also passed.
Workflow YAML parsing, changed documentation links, and `git diff --check` passed.
The corrected [Windows run at `3bffdb7`](https://github.com/doomdavve/proteus/actions/runs/39164837209)
passed the build, all three selected tests, packaging, relocation, and artifact upload.
Next: download and check GPU startup, music, and controls on Windows. Human Windows
playback remains unverified. Version-tag builds are also reused by the
[automated release workflow](RELEASES.md).
