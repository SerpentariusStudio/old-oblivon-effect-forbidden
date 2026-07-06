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

## List of Effects
ABAT=0    Absorb Attribute
ABFA=1    Absorb Fatigue
ABHE=1    Absorb Health
ABSK=1    Absorb Skill
ABSP=0    Absorb Magicka
BA01=0    Bound Armor (extra/reserved slot)
BA02=0    Bound Armor (extra/reserved slot)
BA03=0    Bound Armor (extra/reserved slot)
BA04=0    Bound Armor (extra/reserved slot)
BA05=0    Bound Armor (extra/reserved slot)
BA06=0    Bound Armor (extra/reserved slot)
BA07=0    Bound Armor (extra/reserved slot)
BA08=0    Bound Armor (extra/reserved slot)
BA09=0    Bound Armor (extra/reserved slot)
BA10=0    Bound Armor (extra/reserved slot)
BABO=0    Bound Boots
BACU=0    Bound Cuirass
BAGA=0    Bound Gauntlets
BAGR=0    Bound Greaves
BAHE=0    Bound Helmet
BASH=0    Bound Shield
BRDN=0    Burden
BW01=0    Bound Weapon (extra/reserved slot)
BW02=0    Bound Weapon (extra/reserved slot)
BW03=0    Bound Weapon (extra/reserved slot)
BW04=0    Bound Weapon (extra/reserved slot)
BW05=0    Bound Weapon (extra/reserved slot)
BW06=0    Bound Weapon (extra/reserved slot)
BW07=0    Bound Weapon (extra/reserved slot)
BW08=0    Bound Weapon (extra/reserved slot)
BW09=0    Bound Weapon (extra/reserved slot)
BW10=0    Bound Weapon (extra/reserved slot)
BWAX=0    Bound Axe
BWBO=0    Bound Bow
BWDA=0    Bound Dagger
BWMA=0    Bound Mace
BWSW=0    Bound Sword
CALM=0    Calm
CHML=1    Chameleon
CHRM=1    Charm
COCR=1    Command Creature
COHU=1    Command Humanoid
CUDI=1    Cure Disease
CUPA=0    Cure Paralysis
CUPO=1    Cure Poison
DARK=0    (internal/unused effect)
DEMO=1    Demoralize
DGAT=1    Damage Attribute
DGFA=0    Damage Fatigue
DGHE=0    Damage Health
DGSP=0    Damage Magicka
DIAR=0    Disintegrate Armor
DISE=0    (internal: Disease)
DIWE=0    Disintegrate Weapon
DRAT=0    Drain Attribute
DRFA=0    Drain Fatigue
DRHE=0    Drain Health
DRSK=0    Drain Skill
DRSP=0    Drain Magicka
DSPL=0    Dispel
DTCT=0    Detect Life
DUMY=0    (internal: Dummy / test effect)
FIDG=0    Fire Damage
FISH=0    Fire Shield
FOAT=0    Fortify Attribute
FOFA=0    Fortify Fatigue
FOHE=0    Fortify Health
FOMM=0    Fortify Magicka Multiplier
FOSK=0    Fortify Skill
FOSP=0    Fortify Magicka
FRDG=0    Frost Damage
FRNZ=0    Frenzy
FRSH=0    Frost Shield
FTHR=1    Feather
INVI=1    Invisibility
LGHT=1    Light
LISH=0    Shock Shield (Lightning Shield)
LOCK=0    Lock
MYHL=0    (internal/unused effect)
MYTH=0    (internal/unused effect)
NEYE=0    Night-Eye
OPEN=1    Open
PARA=1    Paralyze
POSN=0    (internal: Poison)
RALY=0    Rally
REAN=0    Reanimate
REAT=1    Restore Attribute
REDG=0    Reflect Damage
REFA=0    Restore Fatigue
REHE=0    Restore Health
RESP=0    Restore Magicka
RFLC=0    Reflect Spell
RSDI=1    Resist Disease
RSFI=0    Resist Fire
RSFR=0    Resist Frost
RSMA=0    Resist Magic
RSNW=0    Resist Normal Weapons
RSPA=0    Resist Paralysis
RSPO=0    Resist Poison
RSSH=0    Resist Shock
RSWD=0    Resist Water Damage (drowning)
SABS=0    Spell Absorption
SEFF=0    Script Effect
SHDG=0    Shock Damage
SHLD=1    Shield
SLNC=1    Silence
STMA=0    (internal: Stunted Magicka?)
STRP=0    Soul Trap
SUDG=0    Sun Damage
TELE=0    Telekinesis
TURN=0    Turn Undead
VAMP=0    (internal: Vampirism)
WABR=1    Water Breathing
WAWA=1    Water Walking
WKDI=0    Weakness to Disease
WKFI=0    Weakness to Fire
WKFR=0    Weakness to Frost
WKMA=1    Weakness to Magic
WKNW=0    Weakness to Normal Weapons
WKPO=0    Weakness to Poison
WKSH=0    Weakness to Shock
Z001=1    Summon Rufio's Ghost
Z002=1    Summon Ancestor Guardian
Z003=1    Summon Spiderling
Z004=1    Summon Flesh Atronach (SI)
Z005=1    Summon Black Bear (Spriggan, Spell Tomes)
Z006=1    Summon Gluttonous Hunger (SI)
Z007=1    Summon Ravenous Hunger (SI)
Z008=1    Summon Voracious Hunger (SI)
Z009=1    Summon Dark Seducer (SI)
Z010=1    Summon Golden Saint (SI)
Z011=1    Wabba Summon (varies by level)
Z012=1    Summon Decrepit Shambles (SI)
Z013=1    Summon Shambles (SI)
Z014=1    Summon Replete Shambles (SI)
Z015=1    Summon Hunger (SI)
Z016=1    Summon Flesh Atronach - Mangled (SI)
Z017=1    Summon Flesh Atronach - Torn (SI)
Z018=1    Summon Flesh Atronach - Stitched (SI)
Z019=1    Summon Flesh Atronach - Sewn (SI)
Z020=0    Extra Summon 20 (blank/unused)
ZCLA=1    Summon Clannfear
ZDAE=1    Summon Daedroth
ZDRE=1    Summon Dremora
ZDRL=1    Summon Dremora Lord
ZFIA=1    Summon Flame Atronach
ZFRA=1    Summon Frost Atronach
ZGHO=1    Summon Ghost
ZHDZ=1    Summon Headless Zombie
ZLIC=1    Summon Lich
ZSCA=1    Summon Scamp
ZSKA=1    Summon Skeleton Guardian
ZSKC=1    Summon Skeleton Champion
ZSKE=1    Summon Skeleton
ZSKH=1    Summon Skeleton Hero
ZSPD=1    Summon Spider Daedra
ZSTA=1    Summon Storm Atronach
ZWRA=1    Summon Faded Wraith
ZWRL=1    Summon Gloom Wraith
ZXIV=1    Summon Xivilai
ZZOM=1    Summon Zombie

---
## Compatibility

- Plays nicely with other content; it doesn't add or edit any forms.
- Other plugins that hook the same effect‑application path are uncommon; if one does, only
  load‑order interactions with *that* plugin would matter.
- Effects an NPC inflicts on you, and your own passive abilities/diseases, are never affected.

---

## Credits

Thanks to [llde](https://www.nexusmods.com/profile/llde) for maintaining **xOBSE**, the
script extender this plugin runs on.

Made with reverse engineering against `Oblivion.exe` 1.2.0.416. Effect names cross‑checked
against [UESP](https://en.uesp.net/wiki/Oblivion:Magical_Effects).

## License

Released under the [MIT License](LICENSE) — do what you like, just keep the credit.
