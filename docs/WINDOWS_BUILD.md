# Windows boat study builds

The Windows boat study GitHub Actions workflow builds a Release x64 executable
on windows-2022 with Visual Studio 2022. It runs on pushes and can also be
started manually from the Actions tab after the workflow exists on the default branch.
No Windows development tools are needed on your Mac or the test machine.

## Download and test

1. Commit and push the source and .github/workflows/windows-boat.yml to GitHub.
2. Open the repository's **Actions** tab and select **Windows boat study**.
3. Open the successful run and download **boat-study-windows-x64** under **Artifacts**.
4. Transfer the ZIP to Windows and extract it completely before running it.
5. Double-click launch.bat to run boat_study.exe --location eriksberg --gpu --gpu-aa.

Artifacts expire after 30 days. build.txt records the source commit. The package
includes shader/texture assets and map attribution; geometry is compiled into the
executable. Asset loading uses the executable directory, so the folder can be moved.
raylib and the MSVC runtime are statically linked for this workflow.

The workflow runs the boat handling validator and CPU screenshot checks, including
a relocated executable launched from another working directory. These do not exercise
GPU shader loading. On the Windows test machine, check startup, shadows, reflections,
steering, thrust, camera controls, reset, and exit. Try --gpu without --gpu-aa if
performance is poor. OpenGL 3.3 support is required for GPU mode.

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
