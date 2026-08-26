# Cutting a Windows release

The goal: your friend clicks one link, unzips, double-clicks, and it works —
with no Qt, no Visual Studio and no admin rights on their machine.

Everything below happens on your machine. There is one script and three git
commands.

---

## One-time setup

**GitHub CLI.** `winget install GitHub.cli`, then `gh auth login` once. You can
do the upload through the GitHub web page instead if you prefer — see
[Publishing by hand](#publishing-by-hand).

**A repository.** If the project is not on GitHub yet:

```powershell
cd E:\tools\SchnellerTypeProject\schnellerTyp-e-local
git init                                  # skip if it is already a repo
git add -A
git commit -m "schnellerTyp-e"
gh repo create schnellerTyp-e --public --source=. --push
```

Use `--private` if you would rather only share the download link. Release
assets on a private repo are only reachable by people you have added to it, so
for a friend who is not a collaborator, `--public` is the simpler path.

---

## Every release

### 1. Build the package

**Run it from PowerShell, not `cmd`.** `cmd.exe` does not execute `.ps1`
files — it opens them with whatever program is associated with the extension,
which is why typing the name there gets you an "open with" prompt instead of a
build.

In the project folder, whichever of these suits you:

```powershell
# PowerShell — open it in this folder by typing `powershell` into
# Explorer's address bar and pressing Enter
.\tools\package-windows.ps1
```

```bat
:: cmd.exe, or a double-click in Explorer — a wrapper that calls PowerShell
:: correctly and passes arguments through
tools\package-windows.cmd
```

```bat
:: cmd.exe, spelled out, if you would rather not use the wrapper
powershell -NoProfile -ExecutionPolicy Bypass -File tools\package-windows.ps1
```

`-ExecutionPolicy Bypass` applies to that one invocation. It changes nothing on
the machine and needs no administrator rights — the default policy blocks
script *files* from running at all, which is the other way this can stop before
it starts.

That is the whole build step. The script:

1. finds Visual Studio and enters its **x64** developer shell, so you do not
   have to remember to start a "x64 Native Tools" prompt;
2. configures a **fresh** `build\release-package` tree in Release — fresh
   because reusing your Debug tree is how you end up shipping debug Qt DLLs,
   which refuse to run on a machine without Visual Studio installed;
3. builds, then runs the unit tests and stops if any fail;
4. runs `windeployqt` to collect the Qt DLLs **and the QML runtime**, adds
   `uiohook.dll`, and verifies the Visual C++ runtime is present;
5. **launches the staged executable and waits for it to reach its event loop**;
6. zips it to `dist\schnellerTyp-e-0.1.0-windows-x64.zip` and prints the
   SHA-256.

Step 5 is the one that matters. The classic way to ship a broken build is to
package an executable that works perfectly on the machine that compiled it —
because Qt is on that machine's PATH — and dies on every other machine with
*"the code execution cannot proceed because Qt6Core.dll was not found."* The
script refuses to produce a zip it has not watched start.

If a copy of schnellerTyp-e is already running, the single-instance guard makes
the smoke test inconclusive and the script says so. Quit it from the tray and
run again.

Non-default locations — arguments work the same either way:

```powershell
.\tools\package-windows.ps1 -QtDir C:\Qt\6.8.0\msvc2022_64 -UiohookRoot D:\libs\uiohook
```

```bat
tools\package-windows.cmd -QtDir C:\Qt\6.8.0\msvc2022_64 -UiohookRoot D:\libs\uiohook
```

### 2. Check the zip yourself, once

Worth doing for the first release and any time the dependencies change: unzip
it somewhere that is **not** the build folder — the Desktop is fine — and run
it. That is the only way to be sure nothing is silently being picked up from
your development environment.

The real test is a machine that has never had Qt or Visual Studio on it. If you
have a spare laptop or a VM, use it. Failing that, your friend is the test, so
tell them where the log lives (it is in `READ-ME-FIRST.txt`).

### 3. Tag and publish

```powershell
git add -A
git commit -m "Release 0.1.0"
git tag -a v0.1.0 -m "schnellerTyp-e 0.1.0"
git push origin main --tags

gh release create v0.1.0 `
    "dist\schnellerTyp-e-0.1.0-windows-x64.zip" `
    --title "schnellerTyp-e 0.1.0" `
    --notes-file RELEASE-NOTES.md
```

The script prints these three lines with the version filled in, so you can copy
them from its output.

Send your friend the link it prints. It looks like
`https://github.com/<you>/schnellerTyp-e/releases/tag/v0.1.0`.

### 4. Bumping the version

Change one line — `VERSION 0.1.0` in `CMakeLists.txt`. The zip name, the git
tag the script suggests, and the version Windows shows in the file's Properties
pane are all read from it, so they cannot drift apart.

---

## What your friend sees

**A SmartScreen warning, on first run.** A blue box: *"Windows protected your
PC"*. There is no visible Run button — the way through is **More info** →
**Run anyway**. It appears once per download.

This is not a virus warning and there is nothing wrong with the build. It is
Microsoft's reputation system, and an unsigned executable that nobody has
downloaded before has no reputation. Reputation is per-signature, so it only
goes away by buying a code-signing certificate — an OV certificate is roughly
$200–400 a year and still needs downloads to build reputation, while an EV
certificate clears SmartScreen immediately and costs more. For sending a tool
to a friend, neither is worth it. `READ-ME-FIRST.txt` in the zip explains the
dialog so they are not alarmed.

**Possibly an antivirus prompt.** A program that installs a global keyboard
hook looks structurally like a keylogger, because the mechanism is identical —
the difference is what it does with the keystrokes, which a scanner cannot see.
Some scanners flag it heuristically. If your friend's does, they can add the
folder to their exclusions. Being honest about this up front lands better than
having them discover it.

**Nothing to install.** No admin rights, no registry beyond the app's own
settings, no start-menu entry. Deleting the folder removes it, apart from
settings under `%APPDATA%\schnellerTyp-e` and the log under
`%LOCALAPPDATA%\schnellerTyp-e`.

---

## What goes in the zip, and why

| | |
|---|---|
| `schnellerTyp-e.exe` | the application, with its icon and version resource |
| `Qt6*.dll`, `platforms\`, `qml\`, `styles\` … | the Qt runtime, chosen by `windeployqt` |
| `uiohook.dll` | keyboard capture |
| `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll` | the Visual C++ runtime, so no redistributable install is needed |
| `READ-ME-FIRST.txt` | the SmartScreen explanation and the tray controls |
| `README.md`, `BUILD.md`, `NOTICE.md`, `LICENSE` | the project's own documents |
| `licenses\` | Qt's and libuiohook's licence texts |

**On licensing.** schnellerTyp-e is MIT. Qt 6 and libuiohook are both LGPL-3.0,
and both are shipped here as **separate DLLs** — that is what keeps this
straightforward. The LGPL's central requirement is that a recipient can replace
the library with their own build, which dynamic linking satisfies by itself:
drop a different `Qt6Core.dll` in the folder and it is used. The licence texts
travel in `licenses\`. Static linking would drag in obligations that are much
more work, which is why this project does not do it.

That is a description of the setup, not legal advice. If this ever stops being
a tool you send to a friend, read the licences properly.

---

## Publishing by hand

Without `gh`:

1. `git push origin main --tags`
2. On GitHub: **Releases** → **Draft a new release**
3. Choose the tag `v0.1.0`, give it a title
4. Drag `dist\schnellerTyp-e-0.1.0-windows-x64.zip` onto the attachment box
5. **Publish release**

---

## If the script fails

| Message | What to do |
|---|---|
| `windeployqt.exe not found under …` | Pass `-QtDir` pointing at your MSVC 64-bit Qt, e.g. `C:\Qt\6.11.2\msvc2022_64`. A MinGW Qt cannot be used. |
| `uiohook.lib not found at …` | Run `.\tools\rebuild-libuiohook-x64.ps1`, or pass `-UiohookRoot`. |
| `cmake is not on PATH` | Use the terminal Qt Creator provides, or add `C:\Qt\Tools\CMake_64\bin` and `C:\Qt\Tools\Ninja` to PATH. |
| `vswhere.exe not found` | Visual Studio's C++ workload is not installed. |
| Windows asks which program to open the `.ps1` with | You are in `cmd.exe`. Use `tools\package-windows.cmd`, or open PowerShell. |
| `… cannot be loaded because running scripts is disabled` | Use `tools\package-windows.cmd`, or add `-ExecutionPolicy Bypass` as above. |
| `… is not digitally signed` / the file is "blocked" | `Unblock-File .\tools\package-windows.ps1`, then run it again. |
| `A copy of schnellerTyp-e is already running` | Quit it from the tray, then re-run. |
| `The staged executable exited immediately` | The script prints the log underneath. A missing DLL names itself there. |
| `Tests failed` | A real regression — fix it rather than packaging around it. |
