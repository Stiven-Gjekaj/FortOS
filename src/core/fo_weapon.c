#include "fo_weapon.h"

#include <stddef.h>

#define Z 0.1f

// Each line is the constructor of one weapon class. The flamethrowers and
// the needle squirter have a sprite for each team.
static const FOWeaponDef kWeapons[] = {
    // Sawngun (als.java:22-34)
    {FO_ITEM_SAWNGUN, 1, FO_SHOT_BULLET, "sawngun", "shotgun_shoot", 10 * Z, -3 * Z, 4 * Z, -4 * Z, 11 * Z, -3.5f * Z,
     -4 * Z, 0, 625, 300, 14, 1000, 0.4f, 40, 40, 0.75f, 1, 0.075f, 300, 0.1f, 1, 10 * Z, 4 * Z},
    // RocketLauncher (alr.java:25-38)
    {FO_ITEM_ROCKET_LAUNCHER, 2, FO_SHOT_ROCKET, "rocket_launcher", "rocket_launcher_shoot", 13 * Z, 2 * Z, 3 * Z,
     2 * Z, 12.5f * Z, 2 * Z, 0, 0, 800, 400, 10.56f, 10000, 1.0E-4f, 60, 20, 0.75f, 1, 0, 300, 0.1f, 1, 14 * Z, 6 * Z},
    // Flamethrower (alk.java:25-36)
    {FO_ITEM_FLAMETHROWER, 3, FO_SHOT_FLAME, "%s_flamethrower", "flamethrower_shoot", 12 * Z, -4 * Z, 5 * Z, -3 * Z, 0,
     0, 0, 0, 35, 35, 8, 300, 0.4f, 5, 250, 0.8f, 1, 0.1f, 300, 0.1f, 0, 13 * Z, 4 * Z},
    // GrenadeLauncher (all.java:25-38)
    {FO_ITEM_GRENADE_LAUNCHER, 4, FO_SHOT_GRENADE, "grenade_launcher", "grenade_launcher_shoot", 15 * Z, -4 * Z,
     6 * Z, -4 * Z, 14 * Z, -4 * Z, -1 * Z, 0, 700, 300, 15, 3000, 1.0f, 50, 20, 0.8f, 1, 0, 300, 0.1f, 1, 12 * Z, 5 * Z},
    // Minigun (aln.java:30-44)
    {FO_ITEM_MINIGUN, 5, FO_SHOT_BULLET, "minigun", "minigun_shoot", 15 * Z, -8 * Z, 7 * Z, -9 * Z, 17.5f * Z, -8 * Z,
     -3 * Z, 0, 100, 100, 11, 2000, 0.15f, 8, 110, 0.3f, 1, 0.05f, 300, 0.1f, 1, 16 * Z, 9 * Z},
    // Shotgun (alu.java:22-34)
    {FO_ITEM_SHOTGUN, 6, FO_SHOT_BULLET, "shotgun", "shotgun_shoot", 11 * Z, -4.5f * Z, 4 * Z, -5.5f * Z, 12 * Z,
     -4.5f * Z, -4 * Z, 0, 625, 300, 14, 1000, 0.4f, 35, 15, 0.75f, 1, 0.075f, 300, 0.1f, 1, 12 * Z, 3 * Z},
    // NeedleSquirter (alo.java:21-33)
    {FO_ITEM_NEEDLE_SQUIRTER, 7, FO_SHOT_BULLET, "%s_needle_squirter", "needle_squirter_shoot", 11 * Z, -2.5f * Z,
     5 * Z, -3 * Z, 10.5f * Z, -2.5f * Z, -3 * Z, 0, 150, 150, 9, 2000, 1.0f, 13, 50, 0.75f, 1, 0.05f, 300, 0.1f, 1, 9 * Z, 6 * Z},
    // SniperRifle (alw.java:32-47)
    {FO_ITEM_SNIPER_RIFLE, 8, FO_SHOT_HITSCAN, "sniper_rifle", "sniper_rifle_shoot", 14 * Z, 2 * Z, 5.5f * Z, -4 * Z,
     14.5f * Z, 1.5f * Z, -4.5f * Z, 0, 2000, 0, 100, 3000, 1.0E-4f, 0, 10, 1.0f, 0.2f, 0, 1000, 0.3f, 1, 15 * Z, 6 * Z},
    // Revolver (alq.java:22-34). Its i is not scaled by z() (alq.java:18),
    // so the muzzle is 1 m out. FortOS keeps this.
    {FO_ITEM_REVOLVER, 9, FO_SHOT_BULLET, "revolver", "revolver_shoot", 1.0f, 0.1f, 8 * Z, 1.5f * Z, 12 * Z, 3 * Z,
     -4 * Z, 0, 580, 200, 14, 1000, 0.15f, 30, 18, 0.9f, 1, 0, 300, 0.1f, 1, 6 * Z, 3 * Z},
    // SimpleKnife (alv.java:12): only the damage of a backstab.
    {FO_ITEM_KNIFE, 9, FO_SHOT_KNIFE, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1000, 0, 1, 1, 0, 300, 0.1f,
     0, 0 * Z, 0 * Z},
    // Uddergun (aly.java:34-48)
    {FO_ITEM_UDDERGUN, 5, FO_SHOT_BULLET, "uddergun_1", "uddergun_shoot", 15 * Z, -9 * Z, 7 * Z, -9 * Z, 17.5f * Z,
     -8 * Z, -3 * Z, 0, 90, 90, 12, 2000, 0.15f, 8, 140, 0.3f, 1, 0.05f, 300, 0.1f, 1, 17 * Z, 9 * Z},
    // ColorfulFlamethrower (alj.java:25-36)
    {FO_ITEM_COLORFUL_FLAMETHROWER, 3, FO_SHOT_FLAME, "colorful_flamethrower", "colorfulflamethrower_shoot", 12 * Z,
     -4 * Z, 5 * Z, -3 * Z, 0, 0, 0, 0, 35, 35, 9, 300, 0.4f, 5, 310, 0.8f, 1, 0.1f, 300, 0.1f, 0, 13 * Z, 5 * Z},
    // JusticeRevolver (alm.java:22-34)
    {FO_ITEM_JUSTICE_REVOLVER, 9, FO_SHOT_BULLET, "justice_revolver", "justicerevolver_shoot", 12 * Z, 1 * Z, 9 * Z,
     1.5f * Z, 14.5f * Z, 2.5f * Z, -4 * Z, 0, 580, 200, 17.5f, 1000, 0.15f, 40, 25, 0.9f, 1, 0, 300, 0.1f, 1, 8 * Z, 4 * Z},
    // AppleLauncher (ali.java:34-47)
    {FO_ITEM_APPLE_LAUNCHER, 4, FO_SHOT_GRENADE, "applelauncher_1", "applelauncher_shoot", 15 * Z, -5 * Z, 6 * Z,
     -3 * Z, 14 * Z, -4 * Z, 0, 0.5f * Z, 650, 300, 16, 3000, 1.1f, 50, 25, 0.8f, 1, 0, 300, 0.1f, 1, 12 * Z, 8 * Z},
    // PaperAirplaneLauncher (alp.java:20-31)
    {FO_ITEM_PAPER_AIRPLANE, 7, FO_SHOT_BULLET, "paper_airplane_launcher", "paperairplanelauncher_shoot", 10 * Z,
     -3 * Z, 7 * Z, -3 * Z, 0, 0, -3 * Z, 0, 400, 400, 10, 2000, 1.0f, 35, 45, 0.75f, 1, 0.05f, 300, 0.1f, 1, 13 * Z, 8 * Z},
    // SentryGunWeapon (alt.java:25-47). The damage is 8 in the constructor
    // and 7 after each tick.
    {FO_ITEM_SENTRY, 0, FO_SHOT_BULLET, NULL, "minigun_shoot", 10 * Z, 0, 0, 0, 12 * Z, 2 * Z, -4 * Z, 0, 170, 0, 14,
     1000, 1.0E-4f, 7, 100, 1, 1, 0, 300, 0.1f, 1, 0 * Z, 0 * Z},
};

const FOWeaponDef *fo_weapon_def(int item) {
    for (size_t i = 0; i < sizeof(kWeapons) / sizeof(kWeapons[0]); i++) {
        if (kWeapons[i].item == item) return &kWeapons[i];
    }
    return NULL;
}

const FOWeaponDef *fo_weapon_default(int cls) {
    static const int kDefaults[10] = {0, 31, 32, 33, 34, 35, 36, 37, 38, 39};
    return cls >= 1 && cls <= 9 ? fo_weapon_def(kDefaults[cls]) : NULL;
}

// The last number is the width of body_idle1 of the class.
static const FOArm kArms[10] = {
    {2 * Z, 7 * Z, 1 * Z, 6 * Z, 1 * Z, 6 * Z, 12 * Z},
    {2 * Z, 6 * Z, 0.5f * Z, 7 * Z, 1.5f * Z, 5.5f * Z, 10 * Z},
    {2 * Z, 7 * Z, 1 * Z, 6 * Z, 1 * Z, 6 * Z, 12 * Z},
    {2 * Z, 6 * Z, 1 * Z, 6 * Z, 1 * Z, 5 * Z, 11 * Z},
    {2 * Z, 7 * Z, 1 * Z, 5 * Z, 1 * Z, 6 * Z, 12 * Z},
    {3 * Z, 8 * Z, -2 * Z, 7 * Z, 2 * Z, 7 * Z, 11 * Z},
    {2 * Z, 7 * Z, 1 * Z, 5 * Z, 1 * Z, 6 * Z, 11 * Z},
    {2 * Z, 7 * Z, 1 * Z, 6 * Z, 1 * Z, 6 * Z, 12 * Z},
    {2 * Z, 7 * Z, 1 * Z, 6 * Z, 1 * Z, 6 * Z, 13 * Z},
    {2 * Z, 7 * Z, 1 * Z, 6 * Z, 1 * Z, 6 * Z, 11 * Z},
};

const FOArm *fo_class_arm(int cls) {
    return &kArms[cls >= 1 && cls <= 9 ? cls : 0];
}
