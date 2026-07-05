/*
 * ForbiddenEffects - xOBSE plugin for original Oblivion (32-bit, 1.2.0.416)
 *
 * Prevents the PLAYER from applying chosen magic effects (an INI allow/deny list,
 * keyed by 4-char effect code). NPCs/creatures/traps are unaffected - they may
 * still use any effect, including against the player. "Suppress only": the action
 * still happens (spell casts, potion drunk, item equips, weapon strikes) but a
 * forbidden, player-originated effect simply never takes hold. See docs/SPEC.md.
 *
 * Chokepoint (RE - Oblivion.exe 1.2.0.416, image base 0x400000; see attempt-history.md):
 *   MagicTarget::ApplyEffectItem @ 0x006A27F0  __thiscall, RET 0xC (3 stack args)
 *     this(ECX) = MagicTarget (victim); this->vtbl[1]() = victim actor
 *     arg0 [ESP+4]  = MagicCaster (CASTER)  -> player cast iff arg0 == *(0x00B333C4)+0x5C
 *     arg1 [ESP+8]  = MagicItem  (Spell/Enchantment/Alchemy; vtbl[6]() = delivery range)
 *     arg2 [ESP+0xC]= ActiveEffect; effectCode = *(*(*(arg2+0xC)+0x1C)+0x98)
 *   A naked prologue detour reads these at entry (before the SEH frame); to SUPPRESS it
 *   mimics the function's own failure exit (XOR AL,AL ; RET 0xC), so the effect is never
 *   created/added to the target. To CONTINUE it jumps to a trampoline (relocated 7 prologue
 *   bytes + JMP 0x006A27F7).
 *
 * PHASE 2 (this build): SUPPRESSION for the SPELLS bucket (confirmed at runtime).
 * MagicItem class is identified by its vtable; SpellItem = 0x00A34BF4. SpellItem+0x1C =
 * spellType (0 Spell, 1 Disease, 2 Power, 3 LesserPower, 4 Ability). The handler suppresses
 * iff: player cast it (arg0 == player+0x5C) AND its code is forbidden AND it is a SpellItem
 * with spellType in {0,2,3} (active spell/power/lesser power - excludes abilities/diseases)
 * AND ForbidInSpells=1.
 * PHASE 3: same scheme extended by vtable to AlchemyItem (potion self / poison other) and
 * EnchantmentItem (0x00A33C3C), routed by enchantType(+0x1C): apparel->worn, weapon/staff->
 * weapon, scroll->spells; and IngredientItem (0x00A33F14, eaten)->potions. All delivery types wired.
 */

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ===================== symbols (Oblivion.exe 1.2.0.416, image base 0x400000) ===================== */
static const uintptr_t kApplyEffectItem   = 0x006A27F0;   // MagicTarget::ApplyEffectItem (hook target)
static const uintptr_t kApplyResumeAddr   = 0x006A27F7;   // = kApplyEffectItem + 7 (after relocated prologue)
static const uintptr_t kPlayerPtr         = 0x00B333C4;   // PlayerCharacter** (g_thePlayer)
static const uintptr_t kPlayerCasterOff   = 0x5C;         // player MagicCaster subobject = player + 0x5C

// Expected first 7 prologue bytes: PUSH -1 ; PUSH 0x009C6051
static const uint8_t kExpectProlog[7]     = { 0x6A, 0xFF, 0x68, 0x51, 0x60, 0x9C, 0x00 };
static const size_t  kPrologLen           = 7;

// ActiveEffect / EffectItem / EffectSetting field offsets (RE-confirmed)
static const uintptr_t kAE_EffectItem     = 0x0C;         // ActiveEffect + 0x0C  -> EffectItem*
static const uintptr_t kEI_EffectSetting  = 0x1C;         // EffectItem   + 0x1C  -> EffectSetting*
static const uintptr_t kEI_EffectCode     = 0x00;         // EffectItem   + 0x00  -> 4-char code (cross-check)
static const uintptr_t kES_EffectCode     = 0x98;         // EffectSetting+ 0x98  -> 4-char code
static const uintptr_t kForm_FormTypeByte = 0x04;         // TESForm + 0x04 -> formType byte (candidate; logged)

// EffectSettingCollection: global NiTMap @ 0x00B33508 (key=EffectID, value=EffectSetting*).
//   map+0x04 = hashSize, map+0x08 = buckets[], map+0x0C = count; entry {+0 next,+4 key,+8 value}.
// Walking it dumps every effect code the engine knows (vanilla + Shivering Isles + mods).
static const uintptr_t kEffectCollection  = 0x00B33508;
static const uintptr_t kMap_HashSize      = 0x04;
static const uintptr_t kMap_Buckets       = 0x08;
static const uintptr_t kMap_Count         = 0x0C;

// MagicItem subclass vtables (delivery-type classifier). MagicItem + 0x1C = subtype field
// (SpellItem.spellType / EnchantmentItem.enchantType / etc.), returned by vtbl[6].
static const uintptr_t kVtbl_SpellItem    = 0x00A34BF4;   // SpellItem   (runtime confirmed)
static const uintptr_t kVtbl_AlchemyItem  = 0x00A32134;   // AlchemyItem (potions & poisons; runtime confirmed)
static const uintptr_t kVtbl_Enchantment  = 0x00A33C3C;   // EnchantmentItem (runtime confirmed: apparel equip)
static const uintptr_t kVtbl_Ingredient   = 0x00A33F14;   // IngredientItem (runtime confirmed: eaten)
static const uintptr_t kMagicItem_SubType = 0x1C;         // MagicItem + 0x1C -> subtype enum
// SpellItem.spellType values: 0 Spell, 1 Disease, 2 Power, 3 LesserPower, 4 Ability.
static bool SpellTypeIsActive(uint32_t t) { return t == 0 || t == 2 || t == 3; }
// EnchantmentItem.enchantType (+0x1C): 0 Scroll, 1 Staff, 2 Weapon, 3 Apparel (constant-effect worn).
// Only apparel(3) is runtime-confirmed (vtbl 0x00A33C3C, playerCast=1 at equip); the
// staff(1)/weapon(2)/scroll(0) values are the standard engine enum, pending a verify run.
enum { kEnch_Scroll = 0, kEnch_Staff = 1, kEnch_Weapon = 2, kEnch_Apparel = 3 };

// Engine corner-message queue: void __cdecl QueueUIMessage(const char* msg, int, int, float time)
// @ 0x0057ACC0 (used inside ApplyEffectItem itself). Default display time = *(float*)0x00A30634.
typedef void (__cdecl *QueueUIMessageFn)(const char *, int, int, float);
static const uintptr_t kQueueUIMessage    = 0x0057ACC0;
static const uintptr_t kDefaultMsgTime    = 0x00A30634;

/* ===================== config (forbiddeneffects.ini) ===================== */
static bool g_enabled        = true;    // master switch
static bool g_logging        = false;   // EnableLogging -> obse_forbiddeneffects.log
static bool g_showMessage    = false;   // (Phase 2) corner message on a suppressed apply
static bool g_forbidSpells   = true;    // cast spells / scrolls / powers
static bool g_forbidPotions  = true;    // drunk potions / eaten ingredients
static bool g_forbidWorn     = true;    // equipped constant-effect apparel buffs
static bool g_forbidWeapon   = true;    // on-strike weapon/staff enchantments & poisons
static bool g_dumpEffects    = false;   // DumpEffects -> write forbiddeneffects_dump.ini once

static char g_iniPath[MAX_PATH]  = {0};
static char g_dumpPath[MAX_PATH] = {0};

// Forbidden effect codes (the rows set to 1 in [Effects]).
static uint32_t g_forbidden[256];
static int      g_forbiddenCount = 0;

/* ===================== logging ===================== */
static void log_message(const char *fmt, ...)
{
    if (!g_logging) return;
    FILE *f = fopen("obse_forbiddeneffects.log", "a");
    if (!f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list args; va_start(args, fmt); vfprintf(f, fmt, args); va_end(args);
    fprintf(f, "\n");
    fclose(f);
}

// Render a 4-char effect code (little-endian int) as text, e.g. 0x46464553 -> "SEFF".
static void code_to_text(uint32_t code, char out[5])
{
    out[0] = (char)( code        & 0xFF);
    out[1] = (char)((code >> 8)  & 0xFF);
    out[2] = (char)((code >> 16) & 0xFF);
    out[3] = (char)((code >> 24) & 0xFF);
    for (int i = 0; i < 4; ++i) if (out[i] < 32 || out[i] > 126) out[i] = '.';
    out[4] = 0;
}

// Upper-case each of the 4 code bytes (case-insensitive matching, so a seeded "ABHe"
// matches the engine's 'ABHe'/'ABHE' regardless of casing).
static uint32_t upcase_code(uint32_t c)
{
    uint8_t b[4] = { (uint8_t)(c & 0xFF), (uint8_t)((c >> 8) & 0xFF),
                     (uint8_t)((c >> 16) & 0xFF), (uint8_t)((c >> 24) & 0xFF) };
    for (int i = 0; i < 4; ++i) if (b[i] >= 'a' && b[i] <= 'z') b[i] -= 32;
    return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
}

static bool is_forbidden_code(uint32_t code)
{
    uint32_t u = upcase_code(code);
    for (int i = 0; i < g_forbiddenCount; ++i)
        if (g_forbidden[i] == u) return true;       // g_forbidden stores upper-cased codes
    return false;
}

/* ===================== effect-code dump =====================
 * Walk the engine's EffectSettingCollection (NiTMap @ 0x00B33508) and write every
 * registered effect code - vanilla + Shivering Isles + any active mod - to
 * forbiddeneffects_dump.ini as a ready-to-paste [Effects] block (all =0). One-shot;
 * the collection is fully populated by the time any effect is applied, so this is
 * driven from the first apply (and attempted at load in case it's already up). */
static bool g_dumped = false;

static int cmp_code(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    char sx[5], sy[5]; code_to_text(x, sx); code_to_text(y, sy);
    return strcmp(sx, sy);   // sort by the printable 4-char text
}

static void MaybeDumpEffects()
{
    if (!g_dumpEffects || g_dumped) return;

    uint32_t *map      = (uint32_t *)kEffectCollection;
    uint32_t  hashSize = map[kMap_HashSize / 4];
    uint32_t *const *buckets = (uint32_t *const *)map[kMap_Buckets / 4];
    uint32_t  count    = map[kMap_Count / 4];
    if (!buckets || hashSize == 0 || hashSize > 0x10000 || count == 0) return;  // not ready yet

    g_dumped = true;   // attempt once; don't retry even if writing fails

    static uint32_t codes[1024];
    int n = 0;
    for (uint32_t i = 0; i < hashSize && n < 1024; ++i) {
        for (const uint32_t *e = buckets[i]; e && n < 1024; e = (const uint32_t *)e[0]) {
            void *es = (void *)e[2];                                  // entry value = EffectSetting*
            if (es) codes[n++] = *(uint32_t *)((char *)es + kES_EffectCode);   // +0x98 = 4-char code
        }
    }
    qsort(codes, n, sizeof(codes[0]), cmp_code);

    FILE *f = fopen(g_dumpPath, "w");
    if (!f) { log_message("DumpEffects: could not write %s", g_dumpPath); return; }
    fprintf(f, "; %d effect codes dumped live from the engine (vanilla + Shivering Isles + mods).\n", n);
    fprintf(f, "; Each code == its Construction Set Magic Effect EditorID. Paste the rows you want\n");
    fprintf(f, "; into forbiddeneffects.ini [Effects] and set them to 1 to forbid for the player.\n");
    fprintf(f, "[Effects]\n");
    for (int i = 0; i < n; ++i) {
        char txt[5]; code_to_text(codes[i], txt);
        fprintf(f, "%s=0\n", txt);
    }
    fclose(f);
    log_message("DumpEffects: wrote %d effect codes to %s", n, g_dumpPath);
}

/* ===================== INI ===================== */
static void ResolveIniPath()
{
    HMODULE hm = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&ResolveIniPath, &hm);
    DWORD n = GetModuleFileNameA(hm, g_iniPath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) { strcpy(g_iniPath, "forbiddeneffects.ini"); return; }
    char *dot = strrchr(g_iniPath, '.');
    if (dot) strcpy(dot, ".ini"); else strcat(g_iniPath, ".ini");

    // sibling "<dll-dir>\forbiddeneffects_dump.ini" for the effect-code dump
    strcpy(g_dumpPath, g_iniPath);
    char *d2 = strrchr(g_dumpPath, '.');
    if (d2) strcpy(d2, "_dump.ini"); else strcat(g_dumpPath, "_dump.ini");
}

// One row in the seeded [Effects] list: "<CODE>=0   ; <name>".
struct EffectRow { const char *code; const char *name; };
static const EffectRow kVanillaEffects[] = {
    {"ABAT","Absorb Attribute"},{"ABFA","Absorb Fatigue"},{"ABHe","Absorb Health"},
    {"ABSk","Absorb Skill"},    {"ABSp","Absorb Magicka"},
    {"BABO","Bound Boots"},{"BACU","Bound Cuirass"},{"BAGA","Bound Gauntlets"},
    {"BAGR","Bound Greaves"},{"BAHE","Bound Helmet"},{"BASH","Bound Shield"},
    {"BWAX","Bound Axe"},{"BWBO","Bound Bow"},{"BWDA","Bound Dagger"},
    {"BWMA","Bound Mace"},{"BWSW","Bound Sword"},
    {"BRDN","Burden"},{"CALM","Calm"},{"CHML","Chameleon"},{"CHRM","Charm"},
    {"COCR","Command Creature"},{"COHU","Command Humanoid"},
    {"CUDI","Cure Disease"},{"CUFA","Cure Fatigue"},{"CUPA","Cure Paralysis"},{"CUPO","Cure Poison"},
    {"DGAT","Damage Attribute"},{"DGFA","Damage Fatigue"},{"DGHE","Damage Health"},{"DGSP","Damage Magicka"},
    {"DEMO","Demoralize"},{"DTCT","Detect Life"},
    {"DIAR","Disintegrate Armor"},{"DIWE","Disintegrate Weapon"},{"DSPL","Dispel"},
    {"DRAT","Drain Attribute"},{"DRFA","Drain Fatigue"},{"DRHE","Drain Health"},
    {"DRSP","Drain Magicka"},{"DRSK","Drain Skill"},
    {"FTHR","Feather"},{"FIDG","Fire Damage"},{"FISH","Fire Shield"},
    {"FOAT","Fortify Attribute"},{"FOFA","Fortify Fatigue"},{"FOHE","Fortify Health"},
    {"FOMM","Fortify Magicka Multiplier"},{"FOSP","Fortify Magicka"},{"FOSK","Fortify Skill"},
    {"FRDG","Frost Damage"},{"FRSH","Frost Shield"},{"FRNZ","Frenzy"},
    {"INVI","Invisibility"},{"LGHT","Light"},{"LISH","Shock Shield"},
    {"DUMM","Detect Life (range)"},
    {"NEYE","Night-Eye"},{"OPNN","Open"},{"PARA","Paralyze"},
    {"RALY","Rally"},{"REAN","Reanimate"},{"REDG","Reflect Damage"},{"RFLC","Reflect Spell"},
    {"RSDI","Resist Disease"},{"RSFI","Resist Fire"},{"RSFR","Resist Frost"},{"RSMA","Resist Magic"},
    {"RSNW","Resist Normal Weapons"},{"RSPA","Resist Paralysis"},{"RSPO","Resist Poison"},{"RSSH","Resist Shock"},
    {"RSAT","Restore Attribute"},{"RSFA","Restore Fatigue"},{"RSHE","Restore Health"},
    {"RSSP","Restore Magicka"},{"RSSK","Restore Skill"},
    {"SEFF","Script Effect"},{"SHDG","Shock Damage"},{"SLNC","Silence"},
    {"STRP","Soul Trap"},{"SABS","Spell Absorption"},{"SUDG","Sun Damage"},
    {"TELE","Telekinesis"},{"TURN","Turn Undead"},
    {"VAMP","Vampirism"},
    {"WABR","Water Breathing"},{"WAWA","Water Walking"},
    {"WKDI","Weakness to Disease"},{"WKFI","Weakness to Fire"},{"WKFR","Weakness to Frost"},
    {"WKMA","Weakness to Magic"},{"WKNW","Weakness to Normal Weapons"},
    {"WKPO","Weakness to Poison"},{"WKSH","Weakness to Shock"},
    {"Z001","Summon Creature (Z*)"},
};

static void WriteDefaultIniIfMissing()
{
    if (GetFileAttributesA(g_iniPath) != INVALID_FILE_ATTRIBUTES) return;
    FILE *f = fopen(g_iniPath, "w");
    if (!f) return;
    fprintf(f,
        "[ForbiddenEffects]\n"
        "; Master on/off. 1 = enabled, 0 = no hook installed (vanilla). Default 1\n"
        "Enabled=1\n\n"
        "; Debug log (obse_forbiddeneffects.log next to Oblivion.exe). 0/1. Default 0.\n"
        "; With logging on, every PLAYER-applied effect is logged with its exact 4-char\n"
        "; code - use it to confirm/spell codes (incl. mod-added effects) for the list below.\n"
        "EnableLogging=0\n\n"
        "; Print a short corner message when an effect is suppressed. 0/1. Default 0.\n"
        "ShowMessage=0\n\n"
        "; One-shot: dump EVERY effect code the engine knows (vanilla + Shivering Isles + mods)\n"
        "; to forbiddeneffects_dump.ini, as a ready-to-paste [Effects] block. 0/1. Default 0.\n"
        "; Each code == its Construction Set Magic Effect EditorID. Set 1, launch once, then 0.\n"
        "DumpEffects=0\n\n"
        "; Per-delivery-type switches: enforce the forbidden list for this source. 1/0.\n"
        "ForbidInSpells=1        ; cast spells / scrolls / powers\n"
        "ForbidInPotions=1       ; drunk potions / eaten ingredients\n"
        "ForbidInWornEnchant=1   ; equipped constant-effect apparel buffs\n"
        "ForbidInWeaponEnchant=1 ; on-strike weapon/staff enchantments & applied poisons\n\n"
        "; ---------------------------------------------------------------------------\n"
        "; [Effects] : <4-char code>=1 forbids that effect for the PLAYER, =0 allows it.\n"
        "; Codes are matched by the engine's exact 4-char effect ID. The seeded list below\n"
        "; covers the vanilla effects (all 0 = nothing forbidden by default); flip rows to 1.\n"
        "; Append mod effect codes by hand (turn EnableLogging on to see exact codes).\n"
        "; ---------------------------------------------------------------------------\n"
        "[Effects]\n");
    for (size_t i = 0; i < sizeof(kVanillaEffects)/sizeof(kVanillaEffects[0]); ++i)
        fprintf(f, "%s=0   ; %s\n", kVanillaEffects[i].code, kVanillaEffects[i].name);
    fclose(f);
}

static void LoadConfig()
{
    ResolveIniPath();
    WriteDefaultIniIfMissing();

    g_enabled       = GetPrivateProfileIntA("ForbiddenEffects", "Enabled", 1, g_iniPath) != 0;
    g_logging       = GetPrivateProfileIntA("ForbiddenEffects", "EnableLogging", 0, g_iniPath) != 0;
    g_showMessage   = GetPrivateProfileIntA("ForbiddenEffects", "ShowMessage", 0, g_iniPath) != 0;
    g_forbidSpells  = GetPrivateProfileIntA("ForbiddenEffects", "ForbidInSpells", 1, g_iniPath) != 0;
    g_forbidPotions = GetPrivateProfileIntA("ForbiddenEffects", "ForbidInPotions", 1, g_iniPath) != 0;
    g_forbidWorn    = GetPrivateProfileIntA("ForbiddenEffects", "ForbidInWornEnchant", 1, g_iniPath) != 0;
    g_forbidWeapon  = GetPrivateProfileIntA("ForbiddenEffects", "ForbidInWeaponEnchant", 1, g_iniPath) != 0;
    g_dumpEffects   = GetPrivateProfileIntA("ForbiddenEffects", "DumpEffects", 0, g_iniPath) != 0;

    // Parse [Effects]: every key whose value != 0 becomes a forbidden 4-char code.
    g_forbiddenCount = 0;
    char keys[8192];
    DWORD n = GetPrivateProfileStringA("Effects", NULL, "", keys, sizeof(keys), g_iniPath);
    if (n > 0 && n < sizeof(keys) - 2) {
        for (const char *k = keys; *k && g_forbiddenCount < 256; k += strlen(k) + 1) {
            if (strlen(k) < 4) continue;                       // need 4 chars to form a code
            if (GetPrivateProfileIntA("Effects", k, 0, g_iniPath) == 0) continue;
            uint32_t code = (uint8_t)k[0] | ((uint8_t)k[1] << 8) | ((uint8_t)k[2] << 16) | ((uint8_t)k[3] << 24);
            g_forbidden[g_forbiddenCount++] = upcase_code(code);   // stored upper-cased
        }
    }
}

/* ===================== OBSE plugin API (minimal, self-contained) ===================== */
struct PluginInfo {
    enum { kInfoVersion = 3 };
    uint32_t     infoVersion;
    const char * name;
    uint32_t     version;
};
struct OBSEInterfaceMin {
    uint32_t obseVersion;
    uint32_t oblivionVersion;
    uint32_t editorVersion;
    uint32_t isEditor;
};

/* ===================== the gating handler (Phase 1: diagnostic only) =====================
 * Returns 1 to SUPPRESS the effect (Phase 2), 0 to let it apply. Phase 1 always returns 0.
 * target = MagicTarget(this), caster = MagicCaster(arg0), item = MagicItem(arg1),
 * ae = ActiveEffect(arg2). */
static int g_lastDecision = 0;    // detour scratch (main-thread only)

extern "C" int __cdecl ForbiddenHandler(void *target, void *caster, void *item, void *ae)
{
    if (g_dumpEffects && !g_dumped) MaybeDumpEffects();   // one-shot, collection is live by now

    // Is this effect originated by the PLAYER? caster == player's MagicCaster subobject.
    void *player        = *(void **)kPlayerPtr;
    void *playerCaster  = player ? (void *)((char *)player + kPlayerCasterOff) : nullptr;
    bool  casterIsPlayer = (caster != nullptr) && (caster == playerCaster);

    // Resolve the effect code (two independent routes, both logged for cross-check).
    uint32_t code = 0, code2 = 0;
    void *effItem = ae ? *(void **)((char *)ae + kAE_EffectItem) : nullptr;
    if (effItem) {
        code2 = *(uint32_t *)((char *)effItem + kEI_EffectCode);
        void *effSetting = *(void **)((char *)effItem + kEI_EffectSetting);
        if (effSetting) code = *(uint32_t *)((char *)effSetting + kES_EffectCode);
    }

    uintptr_t vtbl    = item ? *(uintptr_t *)item : 0;
    uint32_t  subType = item ? *(uint32_t *)((char *)item + kMagicItem_SubType) : 0xFFFFFFFF;
    bool      listed  = is_forbidden_code(code);

    // Victim actor (who the effect lands on); used to split AlchemyItem into drunk-potion (self)
    // vs poison-on-struck-target (other).
    void *victimActor = nullptr;
    if (target) {
        typedef void *(__thiscall *GetActorFn)(void *);
        GetActorFn fn = (GetActorFn)((void **)(*(void **)target))[1];
        victimActor = fn(target);
    }
    bool victimIsPlayer = (victimActor == player);

    // ---- decision: SUPPRESS iff player-cast, forbidden, and an enabled delivery bucket ----
    bool suppress = false;
    const char *bucket = "OTHER";
    if (casterIsPlayer && listed) {
        if (vtbl == kVtbl_SpellItem) {                 // spells / scrolls / powers
            bucket = "SPELL";
            if (g_forbidSpells && SpellTypeIsActive(subType)) suppress = true;   // exclude ability/disease
        } else if (vtbl == kVtbl_AlchemyItem) {        // AlchemyItem = potion (self) or poison (other)
            if (victimIsPlayer) { bucket = "POTION"; if (g_forbidPotions) suppress = true; }
            else                { bucket = "POISON"; if (g_forbidWeapon)  suppress = true; }
        } else if (vtbl == kVtbl_Enchantment) {        // EnchantmentItem, routed by enchantType(+0x1C)
            switch (subType) {
                case kEnch_Apparel: bucket = "WORN";   if (g_forbidWorn)   suppress = true; break; // confirmed
                case kEnch_Weapon:  bucket = "WEAPON"; if (g_forbidWeapon) suppress = true; break; // cast-on-strike
                case kEnch_Staff:   bucket = "STAFF";  if (g_forbidWeapon) suppress = true; break; // cast-when-used
                case kEnch_Scroll:  bucket = "SCROLL"; if (g_forbidSpells) suppress = true; break; // grouped w/ spells
            }
        } else if (vtbl == kVtbl_Ingredient) {         // IngredientItem = eaten ingredient (self)
            bucket = "INGRED"; if (g_forbidPotions) suppress = true;   // grouped with potions per spec
        }
    }

    if (g_logging) {
        char txt[5], txt2[5];
        code_to_text(code, txt);
        code_to_text(code2, txt2);
        log_message("APPLY code=%s/%s playerCast=%d vtbl=%p subType=%u bucket=%s listed=%d -> %s "
                    "item=%p victim=%p victimIsPlayer=%d ae=%p",
                    txt, txt2, casterIsPlayer ? 1 : 0, (void *)vtbl, subType, bucket, listed ? 1 : 0,
                    suppress ? "SUPPRESS" : "allow", item, victimActor, victimIsPlayer ? 1 : 0, ae);
    }

    if (suppress) {
        if (g_showMessage) {
            char txt[5]; code_to_text(code, txt);
            char msg[64]; _snprintf(msg, sizeof(msg), "That effect is forbidden. (%s)", txt);
            msg[sizeof(msg) - 1] = 0;
            ((QueueUIMessageFn)kQueueUIMessage)(msg, 0, 1, *(float *)kDefaultMsgTime);
        }
        return 1;   // -> detour does XOR AL,AL ; RET 0xC : effect never applied
    }
    return 0;
}

/* ===================== inline hook (prologue detour + trampoline) ===================== */
static uint8_t *g_trampoline = nullptr;   // relocated prologue + JMP back to 0x006A27F7

__declspec(naked) static void Detour_ApplyEffect()
{
    __asm {
        push    ebp
        mov     ebp, esp
        pushad
        pushfd
        // ECX = MagicTarget(this); [ebp+8]=caster, [ebp+0xC]=item, [ebp+0x10]=ae
        mov     eax, [ebp+0x10]      // ae
        push    eax
        mov     eax, [ebp+0x0C]      // item
        push    eax
        mov     eax, [ebp+0x08]      // caster
        push    eax
        push    ecx                  // target (this)
        call    ForbiddenHandler     // __cdecl int(target, caster, item, ae)
        add     esp, 16
        mov     g_lastDecision, eax  // save decision before restoring regs/flags
        popfd
        popad
        mov     esp, ebp
        pop     ebp
        cmp     g_lastDecision, 0
        jnz     do_suppress
        jmp     g_trampoline         // continue: relocated prologue -> 0x006A27F7
    do_suppress:
        xor     eax, eax             // mimic the function's own failure exit:
        ret     0x0C                 //   XOR AL,AL ; RET 0xC  (effect not applied)
    }
}

static bool install_hook()
{
    uint8_t *target = (uint8_t *)kApplyEffectItem;

    // Guard: refuse to patch unless the prologue is the exact vanilla bytes we expect
    // (game updated / another plugin already hooked this -> abort, run vanilla).
    if (memcmp(target, kExpectProlog, kPrologLen) != 0) {
        log_message("ERROR: ApplyEffectItem prologue mismatch @ %p - aborting (game updated or already hooked).",
                    target);
        log_message("  got % 02X %02X %02X %02X %02X %02X %02X",
                    target[0], target[1], target[2], target[3], target[4], target[5], target[6]);
        return false;
    }

    // Trampoline: <relocated 7 prologue bytes> <JMP rel32 -> 0x006A27F7>
    g_trampoline = (uint8_t *)VirtualAlloc(NULL, kPrologLen + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_trampoline) { log_message("ERROR: VirtualAlloc trampoline failed (%lu)", GetLastError()); return false; }
    memcpy(g_trampoline, target, kPrologLen);
    g_trampoline[kPrologLen] = 0xE9;   // JMP rel32
    *(int32_t *)(g_trampoline + kPrologLen + 1) =
        (int32_t)(kApplyResumeAddr - (uintptr_t)(g_trampoline + kPrologLen + 5));
    FlushInstructionCache(GetCurrentProcess(), g_trampoline, kPrologLen + 5);

    // Patch: JMP rel32 -> Detour_ApplyEffect, then NOP-pad the remaining prologue bytes.
    DWORD old;
    if (!VirtualProtect(target, kPrologLen, PAGE_EXECUTE_READWRITE, &old)) {
        log_message("ERROR: VirtualProtect failed @ %p (%lu)", target, GetLastError());
        return false;
    }
    target[0] = 0xE9;
    *(int32_t *)(target + 1) = (int32_t)((uintptr_t)&Detour_ApplyEffect - (kApplyEffectItem + 5));
    for (size_t i = 5; i < kPrologLen; ++i) target[i] = 0x90;   // NOP pad
    VirtualProtect(target, kPrologLen, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, kPrologLen);

    log_message("SUCCESS: hooked ApplyEffectItem @ %p (trampoline %p)", target, g_trampoline);
    return true;
}

/* ===================== OBSE plugin entry points ===================== */
extern "C" {

bool OBSEPlugin_Query(const OBSEInterfaceMin *obse, PluginInfo *info)
{
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name        = "ForbiddenEffects";
    info->version     = 1;

    LoadConfig();   // also creates the default ini next to the DLL on first run
    log_message("==================================================");
    log_message("ForbiddenEffects Query - ini=%s obseVer=%08X oblivionVer=%08X editor=%u",
                g_iniPath, obse ? obse->obseVersion : 0, obse ? obse->oblivionVersion : 0,
                obse ? obse->isEditor : 0);
    return true;
}

bool OBSEPlugin_Load(const OBSEInterfaceMin *obse)
{
    log_message("config: Enabled=%d Logging=%d ShowMessage=%d Spells=%d Potions=%d Worn=%d Weapon=%d forbidden=%d",
                g_enabled, g_logging, g_showMessage, g_forbidSpells, g_forbidPotions, g_forbidWorn,
                g_forbidWeapon, g_forbiddenCount);

    if (obse && obse->isEditor) {
        log_message("ForbiddenEffects: editor - hooks NOT installed (runtime only)");
        return true;
    }
    if (!g_enabled) {
        log_message("ForbiddenEffects: disabled via INI (Enabled=0) - hook NOT installed");
        return true;
    }

    MaybeDumpEffects();   // try now (no-op if the collection isn't populated yet; retried on first apply)

    if (install_hook())
        log_message("ForbiddenEffects: active - suppressing forbidden player effects: spells + potions/poisons "
                    "+ enchantments (worn/weapon/staff/scroll) + ingredients (%d codes listed). All delivery types wired.",
                    g_forbiddenCount);
    else
        log_message("ForbiddenEffects: hook NOT installed.");
    return true;
}

}   // extern "C"
