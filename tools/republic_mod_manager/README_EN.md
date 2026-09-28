# Republic Mod Manager – Guide

**English** | [Deutsch](README_DE.md)

Republic Mod Manager (RMM for short) is one window for all your TesmioLoader plugins. You see which plugins you have, switch them on or off, set their values with a description and range check, and start the game. The TesmioLoader itself stays untouched; RMM only writes settings files.

---

## 📋 Contents

1. [Requirements](#-requirements)
2. [The window](#-the-window)
3. [The "Plugin active" switch](#-the-plugin-active-switch)
4. [Save and start](#-save-and-start)
5. [The "Notes" card](#-the-notes-card)
6. [How a plugin gets into the game](#-how-a-plugin-gets-into-the-game)
7. [Content packages](#-content-packages-resources-deposits-buildings)
8. [Files local only](#-files-local-only)
9. [Before the game starts](#-before-the-game-starts)
10. [Saved games](#-saved-games)
11. [The log window](#-the-log-window)
12. [The settings window](#-the-settings-window)
13. [Profiles and restore points](#-profiles-and-restore-points)
14. [List editors: Resources, Needs, Deposits and package editors](#-list-editors)
15. [Generated buildings (the "SML buildings" tab)](#️-generated-buildings-the-sml-buildings-tab)
16. [Game version and rmm.ini](#-game-version-and-rmmini)
17. [Where your files are](#-where-your-files-are)
18. [When something does not work](#-when-something-does-not-work)
19. [For plugin authors](#-for-plugin-authors)

---

## 🚀 Requirements

- *Workers & Resources: Soviet Republic* 1.1.1.9
- TesmioLoader by MaxLegend in the game folder (`tesmioloader\build` with `tesmiolauncher.exe` and `tesmioloader.ini`). The RMM package brings a corrected build of `tesmioloader.dll` (b0.3.6-rmm.1, same API) and installs it over the one you have; the old one goes into `tesmioloader\rmm_install_backup`
- Windows 10 or 11

RMM lives as `rmm.exe` in `tesmioloader\build`. Start it from there or through the desktop shortcut. It finds the loader, the game and the Steam Workshop folder on its own. Only one RMM window is open at a time; a second start brings the existing one to the front.

---

## 🖥️ The window

**The list on the left.** Every row is a plugin: subscribed Workshop packages and everything that lies as a DLL in `tesmioloader\build\plugins`. The icon shows where it comes from: the Steam icon for Workshop packages, the TesmioLauncher icon for plugins in the plugins folder, a gear for anything else. A green dot means this plugin runs on the next game start. A yellow "Update" means the Workshop package is newer than what you saved last. An amber dot means you changed something here and have not saved yet.

**Four filters under the search box:** "All", "Active", "Problems" and "Updates". They show exactly the entries that match — a plugin that is not under "Problems" does not have one. Only an entry with unsaved changes stays under every filter; the amber dot tells you why. The open page stays open even while its entry is filtered out; "All" brings it back.

The refresh button sits right next to the search box: it reads the package folders again after you subscribed to something or copied a file in between.

**The header at the top.** Name, version and description of the plugin, on the right the "Plugin active" switch and the language of the window. RMM speaks German and English; "Automatic" picks German when Windows runs in German, otherwise English.

**The tabs below.** The tabs come from the plugin. A plugin without its own settings page gets a "General" tab that RMM builds from the comments of its INI file.

**The status line** under the tabs says how the plugin is loaded (Workshop Bridge, Soviet Mod Loader or TesmioLoader), where its files are (Workshop package, local in plugins\ or plugins\) and when the loader loaded it last. Hover a dot and a tip explains it. When a plugin steps aside because Soviet Mod Loader brings its capability itself (Deposits Plus, Resources Plus, Needs Plus), no load path is lit; an amber "Paused" dot takes its place and the right side says "Stepped aside on the last game start".

**The cards** carry the settings. Every row has a title, a short text on what the setting changes in the game, and the field on the right:

- Switches for ON and OFF.
- Number fields with plus and minus. Hover the field to see the allowed range. You may type a decimal comma; it is saved with a point.
- Choice lists, text fields and multi-line fields with a counter.
- A reset button appears only when your value differs from the plugin's default.

Right-click on any text copies it to the clipboard, handy for bug reports.

**The footer** shows the state: green "Saved", amber "Unsaved changes", red "Configuration invalid" with the reason. Next to it the buttons Reset, Save and Save + Start. Save is only clickable when there is something valid to save. If you changed several plugins, the footer names them all.

The window remembers size, language, selected plugin and tab. It needs at least 1560 by 760 points; in narrow windows the labels move above the fields.

---

## 🔛 The "Plugin active" switch

The switch in the header decides whether the plugin runs on the next game start. What exactly it writes depends on who loads the DLL (see the status line):

- **Workshop Bridge:** the switch adds the package to the bridge's list (`user_config\workshop_bridge.ini`) or removes it. A package you never switched on is not loaded by the bridge.
- **TesmioLoader:** the switch is the TesmioLauncher's checkbox, the entry `[plugins] <name>` in `tesmioloader.ini`. When switching on, RMM also sets the `enabled` field in the plugin's INI to 1, if there is one, so the DLL does not refuse right away.
- **Soviet Mod Loader:** SML loads every subscribed package itself. The switch then only sets the `enabled` field in the INI. If the INI has no such field, the switch is missing and a note says that only unsubscribing turns the package off.

The switch shows "on" only when everything fits, just like the green dot in the list. Content packages without a DLL (resources, deposits, buildings) carry the switch "Provide in the game" instead.

---

## 💾 Save and start

**Save** writes your changes:

- Your personal values go to `tesmioloader\build\user_config\<name>.ini`. The package's INI stays as it is, so a package update never overwrites your values.
- For plugins that lie only as a DLL in the plugins folder, RMM backs up the INI it found as the original under `user_config\.autoload\<name>.upstream.ini` on the first save and then writes the finished INI with your values to `plugins\`. "Restore original" in the footer brings the backed-up file back byte for byte.
- Before every save RMM creates a restore point (see [Profiles and restore points](#-profiles-and-restore-points)).
- If a new DLL has to go to `plugins\` (only on the TesmioLoader load path), RMM first shows package, source, target and the file's checksum and asks.

Saving works only while the game and the TesmioLauncher are closed and all values are within their ranges. Just opening, searching and switching between plugins or tabs never writes a file.

**Several plugins at once:** You do not have to save after every plugin. Just switch to the next entry; your changes stay in memory, and the entry gets an amber dot in the list. The footer lists every unsaved plugin. Save or Save + Start then writes them all at once, one after the other. If a value in one plugin is wrong, Save stays locked and the footer tells you which plugin it is. Reset only ever applies to the plugin that is open. When you close RMM it asks once more: save all, discard all or cancel.

**Save + Start** saves the same way and then starts the game through `tesmiolauncher.exe` without its window. RMM closes, unless you told it to stay open (see below). If you want to see the launcher window, set `tesmiolauncher_window = 1` under `[settings]` in `rmm.ini`.

After the start RMM watches the game for about 15 seconds and only then closes. If the game quits **immediately** - within the first five seconds, before it has even loaded - RMM tells you and names the most common reason: a Steam client that is running but right now does not accept a game that starts outside Steam; the only cure is to quit Steam completely and start it again. If you close the game yourself, even right after the loading screen, RMM stays quiet and simply closes. For the old behaviour (RMM closes at once) set `launch_watch_seconds = 0` under `[settings]` in `rmm.ini`.

**If you want RMM to stay put,** switch "Keep the window open" on in the settings window (or set `keep_open = 1` in `rmm.ini`). The game starts as always and RMM stays on screen until you close it yourself — handy when you want to look something up while the game loads. The footer then tells you the game is running. The warning about an immediate exit still appears.

Before the start RMM also checks that every resource that switched-on plugins refer to exists in the Resources plugin. If one is missing, the message names the plugin and the start waits.

---

## 💡 The "Notes" card

The first tab of every plugin has a "Notes" card at the top. Everything you should know before saving appears there:

- **Blue:** information, such as a note from the plugin author, a pending update with old and new version, or that a dependency is active.
- **Yellow:** warnings, such as a dependency that exists but is not switched on. Switch it on, otherwise the plugin cannot be saved.
- **Red:** errors, such as a missing dependency, a DLL that exists twice, or a Workshop folder that differs between RMM and the Workshop Bridge. A red box blocks saving.

For packages that allow it, the "Files local only" switch is here as well.

---

## 🛤️ How a plugin gets into the game

The TesmioLoader alone loads only DLLs from `tesmioloader\build\plugins`. For Workshop packages there are three ways, and RMM detects on its own which one applies to you:

1. **Workshop Bridge** (comes with RMM): the plugin `workshop_bridge` loads the DLLs of the switched-on packages straight from the Steam Workshop folder. Steam keeps them up to date, nothing is copied. RMM only provides the INI.
2. **Soviet Mod Loader:** if SML is installed and switched on, it loads every subscribed package. RMM then writes only INI files and never a DLL. If a DLL of a package still lies in `plugins\`, RMM refuses to save because the plugin would load twice. The bridge stays idle under SML.
3. **TesmioLoader classic:** without bridge and SML, RMM copies DLL and INI to `plugins\` and switches the plugin on in `tesmioloader.ini`.

**Under SML, Resources, Deposits, Needs and Buildings belong to the mod loader.** These four capabilities are built into SML, so there is no `plugins\resources.dll` of its own and so on, and SML rewrites their `plugins\*.ini` on every game start (the first line of the file says so). You can still edit them: SML keeps its baseline under `tesmioloader\build\soviet_mod_loader\base`, reads it at every start and never writes it back — and that is exactly where the editors put your entries. A blue notice above the cards says so, and the status line reads "Soviet Mod Loader baseline". As long as SML has never run there is no baseline yet — the page then says that one game start creates it.

**What a package brings is listed with a padlock.** So that you can see what is really in the game, the editor does not only show your baseline but every entry of the finished, merged file — an entry from a package included, with a padlock and the package's name below it. Looking is always possible. Hiding never is: merging can replace a line, it cannot take one back.

**Changing it is possible anyway — through the "RMM Overlay" layer.** Your baseline is the weakest layer, a package beats it. So when you change a package entry, RMM puts it into a little mod of its own in your Workshop folder (`rmm_overlay`, for resources and needs only). SML reads it with a high priority after every package, so your line is the one that counts. Two things come with that: SML writes a "conflict" next to the package it overruled in its confirmation window — the package stays active, it is only the note that somebody else claims that key. And once you take your change back, the layer disappears by itself. "Start over" removes it too.

**Switching between SML and the Bridge.** So that both load paths give the same world, RMM keeps three things aligned: the order of the resources follows SML's catalogue, so saves find their resources where they were. Deposits from content packages get the type number SML gave them. And the own resource list lives in both files: saving under the Bridge in Resources Plus writes your own entries (without the packages') into SML's baseline as well; saving under SML in Resources writes the baseline plus the provided packages into `plugins\resources_plus.ini`. Buildings Plus handles buildings itself: it adopts SML's numbers from its catalogue, both paths write the same folder. On a switch every package building is written once more by the other generator; that is normal.

**A plugin SML replaces glows amber.** Some packages do the same job as one of the four built-in capabilities — Deposits Plus, for example. While SML runs they step aside at game start. The dot in the list is then not grey (that would mean "you switched it off") but amber. On the page itself **every** tab carries a yellow warning, and everything below it is greyed out and locked — otherwise you carefully set up something nobody is reading right now. If you do want to prepare something for later, the button "Edit anyway" unlocks the page for this session.

The Workshop Bridge appears as a plugin in the list itself. Its card has the "Bridge active" switch, the Workshop folder (normally "auto" = your game's Steam Workshop folder), the rule which packages are loaded, and a button "Tidy up now" that removes entries of packages you no longer have. You never edit the package list by hand; the "Plugin active" switch of the packages does that.

---

## 📦 Content packages (resources, deposits, buildings)

Some Workshop packages ship no DLL, only content: new resources, deposits, needs or buildings as INI sections, plus files such as icons and models. You recognise them in the list because their page has no settings, just a switch called "Provide in the game".

Switching on and saving does three things:

- The entries go into the INIs of the plugins in charge: resources to **Resources**, deposits to **Deposits Plus**, needs to **Needs**, buildings to **Buildings Plus**.
- The files that came with the package land under `tesmioloader\vfs`, where the game reads them instead of its own.
- For deposits RMM assigns the type number itself, so it can never clash with another deposit. A number once assigned stays.

In the editors the entries appear as originals with a lock: you can override their values for yourself, but you cannot delete the entry.

**You win a collision.** If a package brings an id your game already has — your own resource, an entry from another package, or one from the plugin's shipped INI — the package is **not provided at all**: no entries, no files in the vfs. The package page shows a red message naming the id that is in the way. Take it out first — one of your own entries in RMM, an original entry straight in `plugins\<plugin>.ini` — and switch the package on again. Nothing of yours is ever replaced quietly, and a file that was in the vfs before the package is neither overwritten nor removed later anyway. While Soviet Mod Loader runs, that message stays away: there is no provide switch then, SML carries the package itself, and the entry "in the way" is usually the one it built from that very package.

Switching off and saving takes entries and files out again. Your own entries and your overrides stay. If the package changes in the Workshop, the yellow "Update" mark appears in the list; saving once takes over the new state.

If a plugin is not set up, the package page says so (for example "No matching plugin is set up for buildings") and skips that part. Under Soviet Mod Loader the switch is not there at all: SML merges such packages itself at game start, subscribing is enough. The package page says so with a blue notice and still shows what is inside.

---

## 📁 Files local only

Some packages allow their files to be copied completely into the plugins folder, for example because they bring textures. The "Notes" card then has the switch "Files local only". Switching it on and saving first shows every file that will be copied, with the DLL's checksum; after "OK" RMM copies DLL, INI and the asset folder to `plugins\`. The bridge skips the package from then on, and the "Plugin active" switch works through `tesmioloader.ini`.

Switching it off and saving removes exactly these files again, after a confirmation with the list. RMM never touches foreign files in `plugins\`. Steam updates reach local files only when you save again; the yellow "Update" in the list reminds you.

---

## ✅ Before the game starts

The clipboard icon in the sidebar opens a page that sums up what the next game start will do:

- **Overview:** how many plugins will load, how many are switched off, how many packages carry an unsaved update, when the game last ran and how many saved games were found.
- **What stands in the way:** missing dependencies, refused packages, waiting updates — and dependencies that load **too late**. A click on a row closes the window and shows the entry it belongs to.
- **Building lines without a resource:** a `$STORAGE`, `$PRODUCTION` or `$CONSUMPTION` line in Vanilla Buildings or Buildings Plus that names a resource no plugin publishes. The game crashes while reading the buildings with it; RMM names plugin, section and resource here and asks once more on "Save + Start".
- **Load order:** the list in the order the loader works through it. Everything from `plugins\` first, in the order of `tesmioloader.ini`, then the Workshop packages through the bridge in the order of their folder names. That matters when one plugin needs a service of another: the service only exists once its provider has loaded.

---

## 💾 Saved games

RMM reads `tesmioloader.save.ini`, the file the loader puts beside every saved game. It lists the plugins that were loaded and the resources and deposits the world knows. Nothing is ever written there.

Two things come out of it:

- The page of a plugin or content package carries a line **"Used by … saved games"** with their names.
- **Switching something off** asks first and names exactly those saves. "No" leaves the switch where it was.

---

## 📜 The log window

The log icon in the sidebar opens a window with three kinds of sources: the journal of this RMM session, `tesmioloader.log` from the loader folder, and every plugin log `tesmioloader.<plugin>.log`, whether it lies in the `logs\` subfolder or directly in the loader folder. The files can be read while the game runs; "Refresh" reads again, "Open folder" shows the loader folder in Explorer.

"Search" filters by text, "Sender" by the first word of a line (`plugin`, `bridge`, `hook`, a plugin name), "Problems and warnings only" hides everything unremarkable. Error lines are red, warnings yellow. The footer names lines, hits and the loader's last closing line.

---


## ⚙️ The settings window

The sliders icon in the sidebar opens the settings of RMM itself — not those of the selected plugin:

- **Show the TesmioLauncher window:** OFF starts the game right away, ON shows the launcher first.
- **Watch after the start:** how many seconds RMM checks whether the game stays up. 0 closes at once.
- **Keep the window open:** ON leaves RMM on screen after the start; you close it yourself.
- **Language** of the interface, the same as the `DE` button next to it.
- **Warn about an unknown game version:** ON tells you when your game is not the version the plugins were built for.
- **Messages from the game:** a switch with four positions, how much the game may write into `tesmioloader.log`. Left to right: off, errors only, errors and warnings, everything. The line under the switch says what the chosen position means.
- **Show repeats:** how often the same message is written out before it is only counted. The tally is at the end of the log. 0 writes out every repeat.
- **Details for a bug report:** puts versions, folders, the plugins that were found and the state of Steam into the clipboard.

The two values of the **Log** card belong to the loader, not to RMM: they go into `[tesmioloader]` of `tesmioloader.ini` and apply from the next game start. RMM changes only those two lines there; comments and every other key stay. The card appears only when that file exists.

The other values take effect at once and are saved when the window closes — in the place where RMM remembers everything, not in `rmm.ini`. Where your value differs from what `rmm.ini` says, a small line under the field tells you the shipped default.

**Start over.** At the bottom of the settings window sit two clearly separated buttons:

- **Delete RMM data** (amber) clears only what RMM remembers about itself: profiles, restore points and the remembered window state. Your plugins, their settings and your saved games are not touched.
- **Take everything back** (red) additionally takes back everything RMM ever wrote into the loader folder: your overrides, local copies in `plugins\`, files in the `vfs`, the entries in `tesmioloader.ini` and in the Workshop Bridge list; the protected original INIs are written back.

Before the red button RMM shows what you lose — **including the names of the saved games** that build on the packages involved. After that you have to type the word `DELETE` before the final button becomes clickable at all. The focus sits on Cancel everywhere, so the Enter key cannot break anything.

Beforehand RMM copies every INI file to `tesmioloader\rmm_reset_backup\<timestamp>`. Not touched: the game folder with your saved games, your Workshop subscriptions, the TesmioLoader itself and everything RMM never wrote. To get rid of the program as well, run `Uninstall-RMM.bat` from the Workshop package.

---
## 🗂️ Profiles and restore points

The archive icon in the sidebar opens the window "Profiles and restore" with two tabs.

**Profiles** save the complete settings state under a name: `tesmioloader.ini`, all INIs in `plugins\` and `user_config\`, and the backed-up originals. DLLs are never part of it. "Save current state…" asks for a name and a note. On the right you see per file whether it matches the saved state. "Apply profile" writes everything back; INIs of plugins the profile does not know stay. Profiles live under `user_config\.autoload\profiles\`.

**Restore points** are created on every save: the state of all touched files from before the save, exactly one per plugin. "Reset to this point" writes these files back, including a DLL copied at the time; the current state becomes the new point, so you can go back again. Points whose files are missing are only listed.

Both actions need a closed game and a closed TesmioLauncher.

---

## 📝 List editors

Some plugins manage lists instead of single values. RMM shows them as a list on the left and fields on the right. This applies to the three plugins that come with the TesmioLoader and to Workshop packages that bring such a page (Deposits Plus, Research Expansion, Technical Service Storage, Vanilla Buildings, UI Layout Fixes, for example).

**Common to all:**

- Entries from the plugin or package carry a lock. You can change them but not delete them; the red button hides them, and "Hidden original entries" shows them again.
- You create your own entries with "+". The dialog asks only for what is needed and suggests what it can; everything else you set on the right afterwards. Own entries can be deleted.
- Identifiers that must name something in the game (a resource, a research entry, a building file, a text id) are checked by RMM before saving. If one is wrong, the footer turns red and names entry and field.
- A yellow warning reminds you when a change affects the save game, such as new resources or deposits that a running game then needs.

**Resources:** the list of all resources of the plugin. **Resources Plus** has the same page for `plugins\resources_plus.ini`, the file of the fixed fork; the installer of the RMM package switches the original off and Resources Plus on, both entries stay in the list. Under SML, Resources Plus shows amber: it pauses there. Template resource, transport class and material family are choice lists with the names from the game. There is no off switch, because the plugin has no safe one. **Needs Plus** is the same kind of fork for needs: the same page as Needs, for `plugins\needs_plus.ini`, switched on by the installer, amber under SML.

**Needs:** the list of needs with donor, factor, shop category, chance and dissatisfaction. Eight entries at most. The "General" tab carries the plugin's switches.

**Deposits:** the "General" tab with the plugin switches and the "Deposits" tab with one entry per deposit: identifier, type number, map, slot on the map, icon and more. The dialog suggests the next free type number; a duplicate is refused.

**Buildings Plus:** a "General" tab with the plugin switches, a "Buildings" tab with your own declarations - and the "SML buildings" tab, see below.

The "Buildings" tab is a list with one row per building: switch, name, number, object folder and donor, on the right the state and the buttons **Change...**, **Open** and the bin. The state says what happens at the next game start - whether the building is in the game, is still to be written, will be rebuilt, or whether something is in the way: a donor that is not on your machine, or a number that already belongs to somebody else's folder. Either of those otherwise stops the plugin at game start and is only mentioned in the log.

Behind "Change..." are the details (number, donor, object name, name in the game, description, life) and a second tab for the lines. There the donor's `building.ini` is on the left - **struck through where your declaration removes something**. Because a single `$STORAGE` line of yours removes *every* storage of the donor, and a `$PRODUCTION` line the whole recipe including its consumption. What you want to keep comes back with one button: **"Keep these N lines"**. The view switches to **Result** - the file that will really be written at the next start, with the number of every storage for `$RESOURCE_VISUALIZATION` and a warning when water or sewage sits in front of a shown storage.

**Package editors** bring their own tabs, lists and help texts. Some have buttons that open the package's guides, picker windows for buildings, research entries or game texts, picture previews or a tab for translations.

---

## 🏗️ Generated buildings (the "SML buildings" tab)

A building from a content package is written out as a complete Workshop item at game start, into `media_soviet\workshop_wip\<number>\` - by Buildings Plus or, when Soviet Mod Loader is running, by its own buildings component. It looks like a subscription, but it is your own file on your own disk.

The tab shows name, number, object folder and origin per folder. Two things it reports on its own:

- **Owner missing.** With `$OWNER_ID 0` in the `workshopconfig.ini` the game reports missing Workshop items **every single time** a saved game is loaded. Soviet Mod Loader writes that zero into every building it generates. The "Fill in the owner" button puts your Steam id in - exactly that one number changes, every other byte of the file stays as it was.
- **Stale.** When no package declares a folder any more, **Soviet Mod Loader closes the game at startup** with an error box and no way to repair it. RMM tells you beforehand and names the folders. Delete them once no saved game uses the buildings.

With Buildings Plus switched on it fills the missing number in at game start anyway - the button is for when you have it off. Nothing else is changed: the `tesmioloader.stamp` is left alone, because a folder without one makes Soviet Mod Loader stop as well.

### Changing a generated building

"Change..." opens the building's `building.ini`. On the left the lines the generator writes, on the right **your changes**: replace a line, remove a line, add a line. A `*` on the left marks every line something of yours is attached to.

Clicking a line puts the **whole block** into the text box - a token together with the lines that belong to it (`$RESOURCE_VISUALIZATION` with its `position`, `rotation`, `scale`). That is not just convenient: a line like `rotation 0.0` often occurs three times in such a file, and RMM never changes the wrong one on a guess. As a block it is unique, and only that makes it editable at all.

What matters is **what** gets stored: not the changed file, but your changes. That sounds like hair-splitting until the day it counts - when the package gets an update the generator rewrites the file, and RMM applies your changes to the **new** version. Whatever the author improved in the meantime is kept; a stored copy would have thrown it away. If a line no longer matches after the update (gone, or there twice now), RMM says so and leaves that change out instead of quietly doing nothing.

That happens the moment you open the tab - so before you start the game. A blue line tells you it did.

If you changed the file **by hand**, outside RMM, it notices (the file is not the one it wrote) and leaves it alone. Your stored changes then sit idle until you hit "Apply" in the window.

"Reset everything" in the window drops your changes and restores the generator's version. "Undo everything" in the settings window does the same for every building at once.

---

## 🎮 Game version and rmm.ini

On start RMM reads the build stamp of `SOVIET64.exe`. If it belongs to no game version the plugins are made for, a warning appears and "Save + Start" asks before starting. Known is 1.1.1.9. A new stamp goes into `settings_schemas\game_versions.ini` under `[supported]`.

`rmm.ini` next to `rmm.exe` has a few switches, all explained in the file:

| Key | Meaning |
|---|---|
| `[paths] workshop_root` | Folder with the packages. Without the line (or with a `;` in front) RMM uses your game's Steam Workshop folder. An empty value is not allowed. |
| `[settings] language` | `auto` (German on a German Windows, otherwise English), `de` or `en`. |
| `[settings] version_check` | 0 turns the game version warning off. |
| `[settings] tesmiolauncher_window` | 1 shows the TesmioLauncher window on "Save + Start". |
| `[settings] launch_watch_seconds` | How long RMM watches after "Save + Start" whether the game stays up. Default 15, 0 turns it off. |
| `[settings] keep_open` | 1 leaves the RMM window open after "Save + Start"; you close it yourself. Default 0. |

What you set in the window takes priority over the file.
**Saving without the window.** For maintenance from the command line:

```bash
rmm.exe --save --build "<game>\tesmioloader\build" --package "<folder of the plugin or package>"
```

This is the same path as the Save button, with the same checks: the game and the TesmioLauncher must be closed and every value valid. Any question the window would ask is declined instead of answered, so a DLL never moves into `plugins\` without you. The answer is one line beginning with PASS or FAIL.

`--activate` moves the header switch before the save:

```bash
rmm.exe --activate on --save --build "<game>\tesmioloader\build" --package "<folder>"
```

`on` switches it on, `off` off. It is the same click as in the window, only without a mouse: on a content package that switch reads "Provide in the game", on a plugin "Plugin active". Nothing is written until `--save` runs; without `--save` the option is refused.

**Check the Steam login.** The game is started directly here and not through Steam, so it needs a logged-in Steam client. Whether one is there:

```bash
rmm.exe --steam-check
```

The answer is one line with PASS or FAIL plus what RMM found in Steam's registry entry. The same check runs before "Save + Start"; it only warns when Steam has no signed-in player on record or no client is running at all. If RMM cannot look, it says nothing and starts.

---

## 📦 Where your files are

```
SovietRepublic\tesmioloader\build\
├── rmm.exe, rmm.ini                    RMM and its base settings
├── settings_schemas\                   settings pages for the loader plugins
├── plugins\                            DLLs and effective INIs of the plugins
│   ├── workshop_bridge.dll, .ini       the Workshop Bridge
│   └── buildings_plus.dll, .ini        Buildings Plus (new buildings from a declaration)
├── user_config\
│   ├── <name>.ini                      your personal values per plugin
│   ├── workshop_bridge.ini             package list of the bridge
│   └── .autoload\
│       ├── <name>.upstream.ini         backed-up originals
│       ├── backups\<package>\previous\ restore points
│       └── profiles\                   your profiles
└── logs\                               logs of the plugins
```

Window size, language and selection are remembered under `%LOCALAPPDATA%\RepublicModManager`.

---

## 🛠️ When something does not work

| What you see | What is behind it | What helps |
|---|---|---|
| Red box "Dependency missing" | A package this plugin needs is not subscribed or not switched on | Subscribe to the package and switch it on with "Plugin active" |
| Red box "Duplicate DLL" | The DLL lies in `plugins\` and is loaded by SML or the bridge as well | Remove the copy in `plugins\` or use "Files local only" |
| Red box about the Workshop folder | RMM and the bridge read different folders | Set the folder to "auto" in the Workshop Bridge's card |
| "Not loaded on the last game start" | The plugin was off on the last start, or the game did not run since you switched it on | Save and start the game once |
| Save does not work | The game or the TesmioLauncher is running, or a value is out of range | Close the programs, read the red note in the footer |
| A value is different again after saving | The plugin read another INI than expected | Check the status line: where are the files? Open the log window |

For a bug report: right-click the message, "Copy text", plus the log window with "Problems and warnings only".

---

## 🧩 For plugin authors

How a Workshop package has to be built so that RMM shows it, and how a settings page with tabs, cards, fields, lists and translations is described, is in `SCHEMA_EN.md` (German: `SCHEMA_DE.md`).

---

**Attention, comrade:** this program was written with the help of an artificial intelligence. The five-year plans behind it were still drawn up, tested and sworn at by a human every time the game crashed. If you do not want AI in your code, just stick to the base game. No hard feelings, no re-education.

**GNU GPL v3.** Source: https://github.com/Kespri/WRSR-MODS/tree/main/tools/republic_mod_manager
