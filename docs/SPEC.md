# Forbidden Effects — xOBSE plugin spec

A self-contained xOBSE plugin for Oblivion (32-bit, 1.2.0.416), same shape as the
sibling **SplitDifficulty** plugin: one DLL + one sibling `.ini`, vtable/function
hook, source-gated. It prevents the PLAYER from applying chosen magic effects.
NPCs, creatures, traps and the environment are unaffected — they may still use
any effect, including against the player.

## Goal

Given an INI list of magic effects (by 4-char effect code), each flagged 0/1, when
the flag is 1 the player cannot *apply* that effect. The block is **suppress-only**:
the action still happens (spell casts, potion is drunk, item equips, weapon strikes)
but the forbidden effect simply never takes hold.

## Locked decisions (from the design interview)

1. **Scope — all four delivery types**, each independently switchable in the INI:
   - Spells / scrolls / powers
   - Potions / ingredients (drunk / eaten)
   - Worn enchantment buffs (constant-effect apparel)
   - Enchanted weapons / staffs & poisons (on-strike, player-originated onto a target)
2. **Strategy — suppress only.** Never block the action; only stop the forbidden
   effect from applying. One central chokepoint, maximum stability.
3. **Multi-effect items — drop only the forbidden effect(s).** Allowed effects on the
   same spell/potion/item still apply (per-effect granularity).
4. **INI shape — one global forbidden list + per-delivery-type on/off switches.**
   The same code can thus be forbidden for spells yet allowed for poisons.
5. **Trigger gating — active use only.** Block only effects the player actively
   initiates (cast / drink / eat / equip / strike). NEVER block racial/birthsign
   *abilities*, *diseases*, or effects an NPC/trap inflicts ON the player — even when
   the effect code is on the forbidden list.
6. **Already-active effects — block new applications only.** No per-frame scanning or
   stripping of effects already running; a pre-existing buff persists until it expires
   or its item is re-equipped.
7. **Feedback — optional, default off.** INI `ShowMessage=0/1`; when on, prints a short
   corner message (e.g. "That effect is forbidden.") on a suppressed apply.
8. **INI seeding — pre-populate every vanilla effect (code + readable-name comment) =0.**
   User flips chosen rows to 1; mod-added codes can be appended by hand.

## Why one chokepoint covers everything

All four delivery types funnel through the same engine path: a `MagicItem`
(`SpellItem` | `EnchantmentItem` | `AlchemyItem`/`IngredientItem`) holds a list of
`EffectItem`s; when applied to a `MagicTarget` by a `MagicCaster`, each `EffectItem`
spawns an `ActiveEffect`. Hooking that per-effect application point gives us, at once:

- **the effect code** — `EffectItem -> EffectSetting -> effectCode` (the INI row key);
- **the caster** — gate on `caster == *g_thePlayer`;
- **the delivery type** — from the `MagicItem` subtype, which also separates
  *active* (Spell/Power, potion, enchant equip/strike) from *passive*
  (Ability/Disease) for the trigger-gating rule above, AND selects the per-type switch.

Skipping the spawn for a forbidden (code, type, player-originated) tuple yields
suppress-only + drop-only-forbidden automatically.

## INI sketch (`forbiddeneffects.ini`, written next to the DLL on first run)

```
[ForbiddenEffects]
Enabled=1
ShowMessage=0
EnableLogging=0

; Per-delivery-type master switches. 1 = enforce the list for this source, 0 = ignore.
ForbidInSpells=1        ; cast spells / scrolls / powers
ForbidInPotions=1       ; drunk potions / eaten ingredients
ForbidInWornEnchant=1   ; equipped constant-effect apparel buffs
ForbidInWeaponEnchant=1 ; on-strike weapon/staff enchantments & applied poisons

[Effects]
; <4-char effect code>=0/1   ; <readable name>   (1 = forbidden for the player)
ABAT=0   ; Absorb Fatigue
...        ; (all vanilla effects pre-seeded =0)
```

## Open RE items (to pin against Oblivion.exe 1.2.0.416, image base 0x400000)

- [ ] The per-effect application chokepoint (ActiveEffect spawn / `MagicTarget`
      apply). Prefer a single function reachable for all four delivery types.
- [ ] Layout to read at that point: `EffectItem -> EffectSetting -> effectCode`;
      caster pointer; `MagicItem` subtype + `SpellItem.spellType` /
      `EnchantmentItem.castType` / `AlchemyItem` poison flag for delivery-type + active/passive.
- [ ] Confirm the player pointer global (SplitDifficulty used `g_thePlayer` @ 0x00B333C4).
- [ ] Confirm on-strike weapon/poison effects carry the wielder (player) as caster.
- [ ] Hook mechanism: vtable slot vs call-site detour (mirror SplitDifficulty's
      verify-expected-bytes-then-patch safety check).

## Non-goals

- No effect-magnitude editing, no balance tweaks — purely allow/deny per effect.
- No NPC/creature restriction. No stripping of running effects. No CS/editor behavior.
