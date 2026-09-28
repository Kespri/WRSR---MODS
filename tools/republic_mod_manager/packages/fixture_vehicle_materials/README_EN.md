# Vehicle Materials Plugin

This plugin lets you add **extra custom resources** to vehicle production in *Workers & Resources: Soviet Republic*—for example glass, cable, or copper.

The resources themselves are provided by the **Resources plugin**. Vehicle Materials then defines which of those resources are required for road vehicles, rail vehicles, ships, and airplanes, and how strongly they affect production requirements.

The original game executable is not permanently modified. The plugin only adds the material requirements while the game is running.

---

## Important dependency

The Vehicle Materials plugin requires:

- the Resources plugin (`resources.dll`);
- a matching `resources.ini`;
- every custom resource that you want to use in `vehicle_materials.ini` to be defined there.

The Resources plugin creates and registers the additional resources. Vehicle Materials uses this service and cannot create resources on its own.

If the Resources plugin is missing or inactive, Vehicle Materials does not start. The exact reason is written to `tesmioloader.log`.

---

## Requirements

- TesmioLoader with API **4**
- the Resources plugin enabled
- *Workers & Resources: Soviet Republic* **1.1.1.9**
- suitable import storages in every production building that should process additional vehicle materials

> Only resources provided by the Resources plugin can be used for vehicle production. Make sure that the resource name is written exactly the same way in both INI files.

---

## Installation and folder structure

After installation, the folder structure should look at least like this:

```text
SovietRepublic\
└── tesmioloader\
    └── build\
        └── plugins\
            ├── resources.dll
            ├── resources.ini
            ├── vehicle_materials.dll
            └── vehicle_materials.ini
```

### Required files

- `resources.dll`
- `resources.ini`
- `vehicle_materials.dll`
- `vehicle_materials.ini`

Copy these files to `tesmioloader\build\plugins\` and enable both plugins in TesmioLauncher.

Changing `vehicle_materials.ini` does **not** require recompiling the DLL. However, fully restart the game after a change so that the configuration is loaded again.

---

## Preparing production buildings

A vehicle can only consume an additional material if the responsible production building can also receive and store that resource.

This applies, for example, to production factories for:

- road vehicles;
- rail vehicles;
- ships;
- airplanes.

The building needs a suitable `$STORAGE_IMPORT_SPECIAL` line for each additional material. Example:

```ini
$STORAGE_IMPORT_CARPLANT RESOURCE_TRANSPORT_COVERED 250
$STORAGE_IMPORT_CARPLANT RESOURCE_TRANSPORT_OPEN 300
$STORAGE_EXPORT RESOURCE_TRANSPORT_VEHICLES 15

--> newly added:
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_OPEN 100 glass
$STORAGE_IMPORT_SPECIAL RESOURCE_TRANSPORT_OPEN 50 cable
```

In this example:

- `100` and `50` are the storage capacities;
- `glass` and `cable` are the exact resource IDs;
- the transport type must be suitable for the corresponding resource.

You can update the buildings through your own building mod or a suitable TesmioLoader plugin. Afterwards, check in the game that the additional storage slots are displayed and can receive deliveries.

> Vehicle Materials only adds the material requirements for vehicle construction. It does not automatically add new storage slots to production buildings.

---

## Configuration: `vehicle_materials.ini`

The configuration contains the following sections:

```ini
[general]
[resources]
[road]
[rail]
[ship]
[airplane]
[mapping]
```

The four vehicle categories use the resource names listed under `[resources]`.

Save the file as **UTF-8 without a BOM**. Only full-line comments beginning
with `;` or `#` are supported; inline comments become part of the value. All
seven sections must remain present. Unknown or repeated sections and keys,
empty values, and malformed numbers reject the complete configuration. Lines
made only of `-` characters are accepted as visual separators.

---

## General settings: `[general]`

```ini
[general]
enabled = 0
debug = 0
debug_limit = 80
```

| Key | Meaning | Default |
|-----|---------|---------|
| `enabled` | `1` enables the plugin, `0` disables it | `0` |
| `debug` | `1` writes additional vehicle-type and category information to the log | `0` |
| `debug_limit` | Limits additional diagnostic and resource messages per game session | `80` |

For normal gameplay, you can keep `debug = 0`. Enable the additional messages only when you want to verify a category assignment or material requirement.

`enabled` and `debug` accept only `0` or `1`. `debug_limit` must be between
`0` and `10000`.

---

## Selecting resources: `[resources]`

This section defines which resources may be used for vehicles.

```ini
[resources]
count = 0
```

| Key | Meaning |
|-----|---------|
| `count` | Number of following `resource0`, `resource1`, … entries |
| `resource0`, `resource1`, … | Exact resource ID from `resources.ini` |

The shipped plugin intentionally contains no fixed initial resource. In Tesmio
Settings, select the desired entries from the active `resources.ini` with `+`;
the required keys are created automatically.

### Example with one resource

```ini
[resources]
count = 1
resource0 = copper
```

Important rules:

- Numbering begins with `resource0`.
- `count` must match the number of entries.
- The plugin accepts at most **32 resources**; larger values are rejected.
- Every index below `count` requires a non-empty entry.
- Entries at or above `count` are rejected.
- Duplicate names are rejected regardless of uppercase or lowercase spelling.
- Every name must be published by the Resources plugin from `resources.ini`.
- A resource without a positive value in at least one vehicle category is ignored.

---

## Material requirements by vehicle category

You can set a coefficient for each listed resource in every vehicle category:

| Section | Applies to |
|---------|------------|
| `[road]` | Road vehicles |
| `[rail]` | Rail vehicles and trains |
| `[ship]` | Ships |
| `[airplane]` | Airplanes |

Example:

```ini
[road]
copper = 0.030

[rail]
copper = 0.025

[ship]
copper = 0.005

[airplane]
copper = 0.015
```

Each value is a **coefficient**. The game multiplies it by the vehicle's internal production value. Larger or more complex vehicles can therefore require more material than smaller vehicles in the same category.

The following rules apply:

- a higher value creates a higher material requirement;
- `0` or a missing entry disables the resource for that category;
- values must be finite and between `0` and `1,000,000`;
- negative, partial, or otherwise malformed values reject the complete configuration;
- a key in a category section must also be listed under `[resources]`;
- the plugin does not add a resource again if it already exists in the vehicle's material requirements.

It is best to begin with small values and check the resulting quantities in the game. You can then adjust the balance step by step.

---

## Mapping vehicle types: `[mapping]`

The plugin normally detects the vehicle category automatically:

- type `1` → road
- type `6` → ship
- type `7` → airplane
- all other production types → rail

The automatic mapping is normally sufficient:

```ini
[mapping]
type0 = -1
type1 = -1
type2 = -1
type3 = -1
type4 = -1
type5 = -1
type6 = -1
type7 = -1
type8 = -1
type9 = -1
type10 = -1
type11 = -1
type12 = -1
type13 = -1
type14 = -1
type15 = -1
```

### Values for a manual mapping

| Value | Category |
|-------|----------|
| `-1` | automatic detection |
| `0` | road |
| `1` | rail |
| `2` | ship |
| `3` | airplane |

Example:

```ini
[mapping]
type2 = 0
```

This manually treats vehicle type `2` as a road vehicle. Values outside `-1`
through `3` are rejected; an omitted `typeN` key continues to use automatic
mapping.

Only change this section if a vehicle is demonstrably assigned to the wrong category. With `debug = 1`, you can check the detected type and selected category in the log.

---

## Log file

You can find the plugin messages here:

```text
tesmioloader\build\tesmioloader.log
```

The plugin also writes its own detail log:

```text
tesmioloader\build\tesmioloader.vehicle_materials.log
```

At startup, the plugin records information including:

- the plugin and API versions;
- the number of active materials;
- with `debug = 1`, the coefficients of every loaded resource;
- invalid, missing, or duplicate configuration entries;
- an unsupported game version;
- a missing Resources service.

With `debug = 1`, the log also includes detected vehicle types, categories, and the number of materials added. `debug_limit` prevents repeated messages from making the log unnecessarily large.

---

## Quick start

1. Define your custom resources in `resources.ini`.
2. Enable `resources.dll` and `vehicle_materials.dll` in TesmioLauncher.
3. Add the desired resources with `+` in Tesmio Settings (or enter them manually under `[resources]`).
4. Set suitable coefficients for every required vehicle category.
5. Add suitable import storages for those resources to the affected production buildings.
6. Fully restart the game through TesmioLauncher.
7. Check `tesmioloader.log` and test the material quantities in the game.

---

## Troubleshooting

| Message or problem | Cause and solution |
|--------------------|--------------------|
| `[resources-service]` | The Resources plugin is missing, disabled, or could not start. |
| `[resource-not-registered]` | The Resources plugin does not publish this name. Check its spelling and definition in `resources.ini`. |
| `[duplicate-key]` or `[duplicate-resource]` | A section, key, or resource name occurs more than once. Remove the duplicate. |
| `[missing-value]` or `[missing-resource]` | A required value or a `resourceN` entry implied by `count` is missing. |
| `[zero-coefficient]` | The resource has no positive coefficient in any category and is ignored. |
| `[config-range]`, `[coefficient-range]`, or `[mapping-range]` | A value is outside its documented range or is not a complete number. |
| `[unsupported-build]` or `[builder-prologue]` | The installed game is not **1.1.1.9**, or another plugin already occupies the hook site. |
| The vehicle requires the material, but the factory cannot receive it | The production building does not have a suitable import storage for the resource. |
| An INI change does not appear in the game | Fully close the game and start it again through TesmioLauncher. |

If something does not work, open `tesmioloader.log` first. Search for lines beginning with `vehicle_materials`.

---

## Safety and behavior

- The original `SOVIET64.exe` is not modified on disk.
- The plugin only works in the game's memory during the current session.
- The complete configuration is validated before a hook is installed.
- The PE structure, image size, timestamp, address ranges, and hook prologue are checked before any change.
- A protected runtime fault disables further custom additions for the session while the native material builder continues to run.
- If the game is started without TesmioLoader or the plugin is disabled, no additional vehicle materials are added.
- Vehicle Materials does not automatically modify your existing game or building files.

### Migrating from 1.0.0

Version 1.1.0 consistently uses the name `vehicle_materials`. Remove the old
`vehiclematerials.dll` and `vehiclematerials.ini` files so that both plugin
versions cannot be loaded together. Transfer your values to the new
`vehicle_materials.ini`; invalid values are no longer silently corrected or
skipped.

---

## Version

- Plugin version: **0.4.0**
- built for TesmioLoader API: **4**
- supported game version: **WRSR 1.1.1.9**

### New in 0.4.0: Workshop packaging

The INI is read beside this plugin's DLL, including when SML forwards a different
loader base directory. A missing adjacent INI never falls back to an unrelated
local configuration. Material calculations are unchanged.

The shared package contains `soviet.mod.ini`, `hooks/vehicle_materials.dll`,
`hooks/vehicle_materials.ini` and `config/vehicle_materials.launcher.ini`.
The separate schema is not parsed as plugin configuration. Tesmio Settings
stages a local copy with personal INI overrides. Do not run SML and Autoload
together. An actual SML game test remains necessary beyond offline checks.
