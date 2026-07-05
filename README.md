# Forbidden Effects

**An xOBSE plugin that lets you forbid the *player* from using chosen magic effects.**

Some magic effects are just too strong, too cheesy, or break the kind of run you want
to play. Forbidden Effects lets you switch individual magic effects off **for the player
only** — by editing a simple INI list. Cast a forbidden spell, drink a forbidden potion,
or coat a blade in a forbidden poison and the action still happens, but **the effect
simply never takes hold** (with an optional on‑screen "That effect is forbidden." notice).

Everyone else — NPCs, creatures, enemies, traps, the environment — can still use every
effect normally, **including against you**. This only ever restrains *you*.

---

## Why

- Run a "no Invisibility / no Chameleon" stealth playthrough that actually has stakes.
- Ban Summons, or just the strongest ones, for a solo‑only challenge.
- Drop cheese like Command, Charm, Soul Trap, or Paralyze from your own toolkit.
- Build a self‑imposed ruleset without sacrificing anything for the rest of the world.

Per‑effect and per‑delivery granularity means you can, for example, forbid Invisibility
as a **spell** while still allowing Invisibility **potions** — or vice‑versa.

---

## Features

- **Per‑effect allow/deny list**, keyed by the effect's 4‑letter code (its Construction
  Set EditorID). Vanilla **and Shivering Isles** effects supported out of the box, plus
  any mod‑added effect (codes are matched live from the engine).
- **Player‑only.** Gated on the player being the source; effects cast *on* you by others
  are untouched.
- **Suppress‑only.** Nothing is "blocked" at the UI level — the spell still casts, the
  potion is still drunk — the forbidden effect just doesn't apply. Multi‑effect items
  keep their allowed effects and drop only the forbidden ones.
- **Per‑delivery‑type switches** so you can keep one source while restricting another.
- **Optional message** on suppression, and an **optional debug log**.
- **One‑click effect dump**: write every effect code your game knows (vanilla + SI + mods)
  to a ready‑to‑paste list.
- Tiny, self‑contained native plugin. No ESP, no scripts, no save bloat.

### Currently restrains
- ✅ **Spells, scrolls and powers** (excludes passive abilities and diseases automatically)
- ✅ **Potions** (drunk)
- ✅ **Poisons** (applied to a weapon and delivered on hit)

### In progress
- ⏳ **Worn (constant‑effect) enchantments** and **on‑strike weapon/staff enchantments** —
  the INI switches are present; full enforcement for these is being finished.

---

## Requirements

- **The Elder Scrolls IV: Oblivion**, version **1.2.0.416** (GOTY / latest patch, 32‑bit).
- **xOBSE** (the maintained Oblivion Script Extender). Launch the game through it.
- Shivering Isles is optional (its effects are recognised if installed).

---

## Installation

1. Install xOBSE if you haven't already.
2. Copy `forbiddeneffects.dll` into:
   `...\Oblivion\Data\OBSE\Plugins\`
3. Launch the game via xOBSE. On first run the plugin writes `forbiddeneffects.ini`
   next to the DLL, pre‑seeded with the full effect list (nothing forbidden by default).
4. Edit the INI to choose what to forbid (see below), then play.

**Uninstall:** delete `forbiddeneffects.dll` (and the `.ini`). Nothing else is touched;
the game runs exactly as if the plugin were never there.

---

## Configuration — `forbiddeneffects.ini`

### `[ForbiddenEffects]`
| Key | Default | Meaning |
|---|---|---|
| `Enabled` | `1` | Master switch. `0` = plugin installs no hook; vanilla behaviour. |
| `EnableLogging` | `0` | Write `obse_forbiddeneffects.log` (next to `Oblivion.exe`). Logs every player‑applied effect with its exact code — handy for finding mod codes. |
| `ShowMessage` | `0` | `1` = show a brief "That effect is forbidden." corner message when something is suppressed. |
| `ForbidInSpells` | `1` | Enforce the list for cast spells / scrolls / powers. |
| `ForbidInPotions` | `1` | Enforce the list for drunk potions / eaten ingredients. |
| `ForbidInWornEnchant` | `1` | Enforce the list for worn constant‑effect enchantments. |
| `ForbidInWeaponEnchant` | `1` | Enforce the list for on‑strike weapon/staff enchantments and applied poisons. |
| `DumpEffects` | `0` | One‑shot: set `1`, launch once, and the plugin writes `forbiddeneffects_dump.ini` containing **every** effect code your load order knows. Set back to `0` afterwards. |

### `[Effects]`
One row per effect: `CODE=1` forbids it for the player, `CODE=0` allows it.

```
INVI=1   ; Invisibility   <- forbidden
CHML=1   ; Chameleon      <- forbidden
FIDG=0   ; Fire Damage    <- allowed
```

- Each `CODE` is the effect's **4‑letter Construction Set EditorID** (e.g. `SLNC` = Silence,
  `ZDAE` = Summon Daedroth). Matching is **case‑insensitive**.
- The seeded list already names every vanilla + Shivering Isles effect, so you usually just
  flip a few rows to `1`.
- **Mod‑added effects:** set `DumpEffects=1` and launch once to get their exact codes, then
  paste the rows you want.

---

## How it works (for the curious)

Forbidden Effects hooks the single engine routine that applies one magic effect to a
target. At that point it knows the **source** (is it the player?), the **effect code**, and
the **kind of item** delivering it. If the player is the source, the code is on your list,
and that delivery type is enabled, the plugin makes the engine skip applying that one
effect — nothing else is disturbed. Because it works one effect at a time, a spell or potion
with several effects keeps the ones you allow and loses only the ones you forbid.

It installs a guarded in‑place hook (verifies the expected bytes first) and does nothing in
the Construction Set. If a future game patch changes the target, the plugin refuses to hook
and the game runs vanilla — it won't crash you.

---

## Compatibility

- Plays nicely with other content; it doesn't add or edit any forms.
- Other plugins that hook the same effect‑application path are uncommon; if one does, only
  load‑order interactions with *that* plugin would matter.
- Effects an NPC inflicts on you, and your own passive abilities/diseases, are never affected.

---

## Credits

Made with reverse engineering against `Oblivion.exe` 1.2.0.416. Effect names cross‑checked
against [UESP](https://en.uesp.net/wiki/Oblivion:Magical_Effects).

Permissions: do what you like — just credit if you build on it.
