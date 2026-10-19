# Browser boating build

## Current status

Implemented on 2026-10-19: a CMake/Emscripten build of the released `boat_study` game,
WebGL 2 assets, click-to-start audio, IndexedDB best times, a small HTML launcher, local
HTTP serving, and a separate GitHub Pages workflow. The workflow now builds and deploys on
`v*` tag pushes, alongside the existing native release workflow. Ordinary branch pushes do
not deploy; manual deployment is available from `main` or a `v*` tag. Native releases keep
their existing behavior. The user confirmed local startup works after SDK activation.
Next task: enable the GitHub Actions Pages source, allow version tags in the `github-pages`
environment if restricted, commit/push the web support, and push the next version tag.
Actual Pages deployment and full browser handling/music acceptance remain pending.

## Build and deployment

The [README](../README.md#browser-build-and-github-pages) owns the SDK installation,
build/serve commands, Pages setup, and browser controls. The toolchain is pinned in
[web/emsdk-version](../web/emsdk-version); the workflow also pins the official SDK checkout.
Keep SDK updates coordinated with browser smoke checks.

The Pages and native release workflows run independently on the same pushed version tag;
a failure in one does not roll back the other. Any matching tag, including prereleases,
publishes to the same site. Deployments are serialized without cancelling an active run;
the last successful deployment wins, with no semantic version ordering. Publishing a
GitHub Release for an already-existing tag alone does not trigger this Pages workflow;
push a new tag containing it or dispatch the workflow manually. The `github-pages`
environment must permit **Tag** refs matching `v*` (and **Branch** `main` for manual main
deployments), as described in GitHub's
[environment setup guide](https://docs.github.com/en/actions/how-tos/deploy/configure-and-manage-deployments/manage-environments).

[CMakeLists.txt](../CMakeLists.txt) defines the same boat executable and source list for
both platforms. Emscripten selects raylib 6.0's `PLATFORM=Web`, OpenGL ES 3.0, and a static
library, then [cmake/Web.cmake](../cmake/Web.cmake) configures the web-only output. Native
validators, studies, and the four-minute demo are excluded from web configuration.
No CTest executables are run as web programs.

The first configure needs network access to fetch raylib; SDK installation also needs network.
A local raylib checkout can be reused with CMake's `FETCHCONTENT_SOURCE_DIR_RAYLIB` option.
The web build requires GNU extensions only in raylib for its `EM_ASM` macros; application
sources remain C17. Its POSIX clock calls are enabled through `_POSIX_C_SOURCE=200809L`.

## Runtime decisions

- **Loop:** raylib's documented Asyncify approach preserves the existing synchronous loop,
  fixed physics steps, and cleanup. Its `WindowShouldClose()` yields to the browser.
  This costs some binary size and CPU time versus extracting a callback and moving the
  loop's many local variables into long-lived state. That refactor is not needed for this
  port; consider it only if measured browser performance warrants it. Optimized builds,
  a 1 MiB C stack, 64 KiB Asyncify stack, and growable memory are configured.
- **Graphics:** existing GLSL 330 sources are converted at build time to GLSL ES 300 with
  precision declarations. The sky/downsample fragments match raylib's default vertex
  shader precision. Shader bodies are shared; source assets are never overwritten.
  WebGL 2 supports the renderer's depth textures, derivatives, integer operations, and
  texel fetches. Antialiasing is optional because it shades sixteen times as many pixels.
- **Window:** the browser keeps a fixed 960x600 drawing buffer, containing the 320x200 scene,
  and scales it to whole scene-pixel multiples. This avoids raylib's web resize handler,
  which otherwise takes the entire browser viewport including page controls. Escape does
  not terminate the browser game. All other simulation/input/audio paths remain shared.
- **Assets:** preload only `a-little-boat.xm`, textures, and the used shaders into `/assets`.
  `boat_asset_path()` already resolves that path under Wasm. Geometry is compiled from C
  headers; geometry manifests, editor samples, dance files, and unrelated music are not
  runtime dependencies. The engine/water sounds are synthesized by the existing C audio code.
- **Audio:** preload before enabling Play; invoke C `main` synchronously from its click
  handler so raylib/miniaudio creates and starts audio during a user gesture. The same XM
  decoder and streaming music code are used as on desktop.
- **Storage:** mount IDBFS at `/saves` with `autoPersist`, set `XDG_DATA_HOME=/saves`, and
  restore IndexedDB before enabling Play. The existing POSIX path, temporary-file/rename
  saves, format validation, bests, ghosts, and replay code operate unchanged. Browser writes
  are asynchronous; closing immediately after finishing can lose a pending write. Storage
  denial during restoration is reported on the page and gameplay can continue. Browser
  quota/eviction/private-mode policy can still prevent durable saves; these are not backups.
- **Delivery:** [tools/package_web.py](../tools/package_web.py) hashes the actual JS/Wasm/data
  bytes and writes the URL mapping into `index.html`. Relative URLs work under a repository
  Pages subpath. There is no service worker or preload cache. Only the generated `site/`
  directory is published; raylib's license is included. SDK/build/cache files stay local.

## Verification evidence

2026-10-19, Apple Silicon macOS:

- Emscripten **6.0.12**, CMake `MinSizeRel`, raylib 6.0, WebGL 2: configure and
  `cmake --build build-web --target web --parallel 4` passed. The existing pinned raylib
  checkout was reused through `FETCHCONTENT_SOURCE_DIR_RAYLIB`. Application compilation
  is warning-clean; raylib/miniaudio emits upstream deprecation/third-party warnings with
  this SDK. The generated JS/Wasm/data total **3,949,313 bytes** (about 1.65 MB using local
  gzip as an estimate, not a measured Pages transfer). The data archive is 445,132 bytes.
- Started the actual `web-serve` target and exercised the output in installed Chrome
  headless with SwiftShader, using temporary Playwright tooling outside the repository.
  River, Eriksberg, Basin, and time-trial scenes rendered; their captures were inspected.
  Optional 4x supersampling, keyboard throttle, help, best-replay playback, fullscreen,
  and resize to a 640x400 displayed canvas worked. No JavaScript, asset-load, or shader
  errors occurred. Chrome reported the upstream ScriptProcessorNode deprecation and
  software-renderer readback performance warnings.
- The Play gesture produced a running AudioContext; audio instrumentation observed
  nonzero output (68 of 89 sampled callbacks in the final targeted run). This confirms
  generated audio, not subjective sound quality or a full musical listening review.
- Before Play, checked all eight shaders, three textures, and the XM in the virtual
  filesystem. Tested IDBFS write/close/rename persistence across reload with a real native
  recording fixture; the C game then displayed its best time/ghost and played its replay.
  This verifies storage integration, not a human-completed browser race. Denying IndexedDB
  still enabled Play and displayed the storage warning.
- Native Debug configure/build passed; **70/70 CTests passed**, including **18 GPU tests**
  with `PROTEUS_GPU_TESTS=ON`. The initial sandboxed run stalled at its first GPU case;
  it was stopped and the full suite rerun with desktop graphics access and a 120-second
  per-test timeout. A native GPU river/waves scripted demo (`--demo --frames 120`) also
  completed without logged errors. This is a startup/playback smoke check, not human
  audio or handling acceptance.
- Initially parsed workflow YAML and checked the then-current main-branch trigger (superseded
  by the tag trigger below), job dependency, deployment
  permissions, and official Pages action usage. Verified packaged content hashes and URL
  mappings, local documentation links, C formatting, JS syntax, and `git diff --check`.
  Existing native/Release workflow files have no changes.

Local evidence is in ignored `build-web/browser-*.png`, `build-web/browser-validation.json`,
`build-web/build.log`, and `build/web-native-ctest-desktop.log`. The browser automation and
SDK installation were temporary verification tools, not new project dependencies.

Remaining limits: the GitHub workflow/deployment itself has not run; Linux-hosted Emscripten
CI, Safari, Firefox, physical GPU browser performance, long sessions, browser audio latency,
private-mode/quota behavior beyond a denied-storage check, and human gameplay/listening
acceptance are unverified. No touch controls or recording import/export UI is provided.

### Tag deployment follow-up — 2026-10-19

Changed the push trigger to `v*` tags and allowed those refs in the deployment job;
retained explicit manual deployment from `main`. Updated setup instructions to cover tag
environment rules and the requirement to commit all web build inputs before tagging.
User feedback: local browser startup works. Actionlint 1.7.12 passed for the updated workflow
(ShellCheck unavailable); embedded build commands passed `bash -n`. Parsed the YAML and
checked tag/manual deployment conditions for stable tags, prerelease tags, manual `main`,
manual tags, ordinary main pushes, and other branches. Deployment permissions, job dependency,
changed local documentation links, and `git diff --check` passed. Native Debug configure/build
and all 70 CTests passed on Apple Silicon macOS, including 18 GPU cases with desktop access
and a 120-second per-test timeout (`build/web-tags-ctest.log`). No new audiovisual review was
needed for this workflow/documentation change. The first remote tag build and deployment
remain unverified; no tag was created or pushed during this change.

## References

- [raylib web build conventions](https://github.com/raysan5/raylib/wiki/Working-for-Web-%28HTML5%29)
- [Emscripten Asyncify](https://emscripten.org/docs/porting/asyncify.html)
- [Emscripten filesystem / IDBFS](https://emscripten.org/docs/api_reference/Filesystem-API.html)
- [GitHub Pages custom workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
