# 🗂️ tesmio_lang

**Shared text pack for TesmioLoader plugins**

A Localization text pack with the namespace `tesmio_lang` in which several plugins share their few labels without each needing its own pack folder. The Localization plugin loads it at game start; currently it provides the caption of the "Deposit remaining" row of Depletion.

---

## 📋 Contents

- [Quick start](#-quick-start)
- [Keys](#-keys)
- [Adding your own keys](#-adding-your-own-keys)
- [Use by plugins](#-use-by-plugins)
- [Troubleshooting](#-troubleshooting)
- [File structure](#-file-structure)

---

## 🚀 Quick start

### Requirements
- Localization plugin (included in the package or installed the classic way)
- A plugin that uses the namespace, currently Depletion 1.1.2

### In three steps
1. The text pack ships in the Localization package under `hooks\localization\tesmio_lang` and is loaded from there; for a classic installation copy the folder to `plugins\localization\tesmio_lang`.
2. Enable Localization.
3. Restart the game completely. Depletion's log shows `localization key tesmio_lang.depletion.deposit_remaining resolved to …`.

---

## 🔤 Keys

| Key | German | English | Used by |
|---|---|---|---|
| `depletion.deposit_remaining` | Restvorkommen | Deposit remaining | Depletion 1.1.2 or newer, row in the mine window |

Full key: `tesmio_lang.depletion.deposit_remaining`. The caption carries no colon and no numbers; Depletion appends remaining, reference, t/kt/Mt and percent itself, for example "Deposit remaining: 11.0 kt / 11.5 kt (95.7 %)".

Pack configuration:

```ini
[localization]
namespace = tesmio_lang
fallback = sovietEnglish
missingText = [MISSING TEXT: {key}]
```

---

## ✏️ Adding your own keys

- Add new keys following the pattern `<plugin>.<textname>` under the existing `[strings]`; do not create a second `[strings]` section.
- Define every key at least in `sovietEnglish.ini`, because English is the fallback language.
- Add further languages through `soviet<Language>.ini` with their own `[strings]`; UTF-8, umlauts allowed.
- The Depletion caption at most 63 UTF-16 code units and a single line; short titles keep the game window from overflowing.
- An INI entry alone does not replace a hard-coded label: the plugin in question has to request the key through this namespace explicitly.

The remaining rules (key characters, escape sequences `\n` and `\\`, sizes) are in the Localization guide.

---

## 🔌 Use by plugins

A plugin obtains the `localization` service in `TsmPluginStart()` and resolves the full key:

```cpp
int id = L->resolveFull("tesmio_lang.depletion.deposit_remaining");
```

Depletion 1.1.2 or newer uses the translation of the selected game language while Localization is active, otherwise the pack's English fallback. Without Localization, without the key or without the native text lookup the output stays `[depletion] panel_caption` (default "Deposit remaining"). An empty, overlong or multi-line caption also leads to the fallback text, never to switching mining off. Texts are never executed as format commands; numbers and calculations still come from Depletion.

---

## ⚙️ Troubleshooting

| Message | Cause | What to do |
|---|---|---|
| `localization key tesmio_lang.depletion.deposit_remaining missing/invalid` (Depletion) | pack not loaded or key missing | check the folder and `sovietEnglish.ini`; read the Localization log |
| `Pack was rejected because its localization.ini is invalid` (Localization) | pack configuration faulty | exactly one section `[localization]` with `namespace` and `fallback` |
| `namespace-collision` | a second pack with namespace `tesmio_lang` | remove one of them; equal folder names under `plugins\localization` and beside the DLL are merged, not loaded twice |
| caption stays English | game language without a language file | add `soviet<Language>.ini` |

Changes to language files apply after a complete restart of the game.

---

## 📦 File structure

```
tesmio_lang\
├── localization.ini      (namespace, fallback, placeholder)
├── sovietEnglish.ini     (fallback language)
├── sovietGerman.ini
├── README_DE.md
└── README_EN.md
```

---

**Last update:** tesmio_lang, Localization 1.2  
**For:** WRSR 1.1.1.9 | TesmioLoader API 4
