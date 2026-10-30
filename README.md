# Paper Pack for Filmora (OpenFX)

Ten automatic graphics effects in one plugin file, built for documentary and travel videos.

## 1. Build the plugin (one time, free)
1. Create a free account at https://github.com
2. New repository -> name it `PaperPack` -> Create.
3. Click "uploading an existing file", then drag in everything from this ZIP
   (the `src` folder, the `.github` folder, `CMakeLists.txt`, `tests`, the guides) -> Commit changes.
4. Open the **Actions** tab. The build starts by itself (2-5 minutes). Wait for the green tick.
   If it does not start: Actions -> "Build Paper Pack (Windows)" -> Run workflow.
5. Open the finished run, scroll to **Artifacts**, download `PaperPack-Windows`, unzip it.
   You get a folder called `PaperPack.ofx.bundle`.
6. If the run shows a red X, open it, copy the red error text and send it to Claude.

## 2. Install in Filmora
1. Close Filmora.
2. Copy the whole `PaperPack.ofx.bundle` folder into:
   `C:\Program Files\Common Files\OFX\Plugins`
   (create the `OFX` and `Plugins` folders if they do not exist).
3. Open Filmora 15 and look in the Effects panel for **Paper Pack**.
4. If nothing shows up, tell Claude. Filmora's plugin folder is the first thing to check.

## 3. Use
Drag an effect from Effects onto a clip on the timeline. See EFFECTS_GUIDE.md for every effect.

## Notes
* The effects were previewed on sample frames during development, but have not yet been run inside Filmora.
* Text is not drawn by the plugin. Write text with Filmora's own Title tool and put it above the clip.
* `tests/harness.cpp` is a developer tool that renders any effect on an image without Filmora.
