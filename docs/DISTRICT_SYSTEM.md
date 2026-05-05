# District system (map locations / `info_location`)

In Doom 3, the **HUD area name** comes from **`info_location`** entities (`idLocationEntity`). At map load, **`SpreadLocations`** assigns each **portal area** the location whose origin lies in that area, then **floods** the same label through all areas connected without a **location-blocking** portal (`PS_BLOCK_LOCATION`). **`location_separator`** entities (`idLocationSeparatorEntity`) mark portals so the flood stops at doorways and corridors.

This fork extends that behavior for **overlapping** or **nested** location volumes and optional **district** metadata.

## Spawn keys on `info_location`

| Key | Purpose |
|-----|---------|
| `location` | String shown on the HUD (`location` GUI state). Defaults to the entity **name** if omitted. |
| `district` | Optional **grouping** label (e.g. city ward). Exposed on the HUD as `location_district` when **`g_districtHudDistrict`** is `1`. |
| `locationPriority` | Integer; **higher wins** when two `info_location` volumes compete for the same area after the flood. Default `0`. Equal priority uses **lower entity number** first. |

## CVars

| CVar | Default | Meaning |
|------|---------|---------|
| `g_districtHudDistrict` | `0` | When `1`, the player HUD gets **`location_district`** from the current location’s `district` key (empty if unset). |
| `g_districtSpreadVerbose` | `0` | When `1`, **`SpreadLocations`** prints a line whenever one `info_location` **replaces** another for an area (priority / tie-break). |

## Console commands

| Command | Description |
|---------|-------------|
| `locationInfo` | Print **area index**, resolved **location** string, **district**, **priority**, and entity name for the **local player’s eye** position. Optional: `locationInfo <x> <y> <z>`. |
| `listLocationEntities` | List every **`info_location`** in the map with origin, `location`, `district`, and `locationPriority`. |
| `spreadLocationsDebug` | Rebuild the area→location table and **log** replacements (same as load-time spread with verbose on). Use with **`g_districtSpreadVerbose 1`** for full overlap tracing. |

## Implementation notes

- **`SpreadLocations`** runs from **`idGameLocal::MapPopulate`** after entities spawn. **`spreadLocationsDebug`** is for **development** after editing entities in a live map session; it does not re-run automatically when you move `info_location` origins in Radiant until you reload the map or call this command.
- **`location_separator`**: if the entity does not touch a portal, it **warns** and **does not** call `SetPortalState` with a null handle (avoids undefined behavior).

## HUD

To show a district line in a custom HUD, bind a GUI field to **`location_district`** and enable **`g_districtHudDistrict 1`**. The stock HUD only uses **`location`** unless you extend the `.gui`.
