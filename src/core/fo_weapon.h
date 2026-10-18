// fo_weapon: the weapons of GANGFORT (Weapon, alz.java, and its 15
// subclasses). The table holds the numbers of each constructor. All
// vectors are in meters: the art pixels of the original times 0.1.

#ifndef FO_WEAPON_H
#define FO_WEAPON_H

typedef enum {
    FO_SHOT_BULLET,   // One projectile with spread: the guns.
    FO_SHOT_ROCKET,   // RocketLauncher (alr.java).
    FO_SHOT_FLAME,    // Flamethrower (alk.java).
    FO_SHOT_GRENADE,  // GrenadeLauncher (all.java).
    FO_SHOT_HITSCAN,  // SniperRifle (alw.java).
    FO_SHOT_KNIFE,    // SimpleKnife (alv.java): only the backstab.
} FOShotKind;

// The item ids of the weapons (aao.java:71-85).
enum {
    FO_ITEM_SAWNGUN = 31,
    FO_ITEM_ROCKET_LAUNCHER = 32,
    FO_ITEM_FLAMETHROWER = 33,
    FO_ITEM_GRENADE_LAUNCHER = 34,
    FO_ITEM_MINIGUN = 35,
    FO_ITEM_SHOTGUN = 36,
    FO_ITEM_NEEDLE_SQUIRTER = 37,
    FO_ITEM_SNIPER_RIFLE = 38,
    FO_ITEM_REVOLVER = 39,
    FO_ITEM_SENTRY = 40,
    FO_ITEM_KNIFE = 41,
    FO_ITEM_UDDERGUN = 46,
    FO_ITEM_COLORFUL_FLAMETHROWER = 47,
    FO_ITEM_JUSTICE_REVOLVER = 48,
    FO_ITEM_APPLE_LAUNCHER = 50,
    FO_ITEM_PAPER_AIRPLANE = 51,
};

typedef struct {
    int item;
    int cls;               // The class that uses it, or 0 for the sentry.
    FOShotKind kind;
    const char *sprite;    // The region of the held weapon. "%s" is the team.
    const char *sound;     // The sound of a shot (field A).
    float ix, iy;          // i: the muzzle from the arm pivot.
    float hx, hy;          // h: the weapon sprite from the arm pivot.
    float xx, xy;          // x: the muzzle flash from the arm pivot.
    float jx, jy;          // j: the bullet sprite from the bullet body.
    int interval;          // k: ms between two shots.
    int window;            // l: the slow down lasts 2 x l ms after a shot.
    float speed;           // o: m/s of the projectile.
    int life;              // p: ms of the projectile.
    float gravity;         // q: the gravity scale of the projectile.
    int damage;            // r
    float ammo;            // y: the full ammo.
    float afterShot;       // s: the move speed factor after a shot.
    float whileAim;        // t: the move speed factor while it aims.
    float spread;          // The random part of each axis of the direction.
    int shakeMs;           // m: the camera shake of a shot.
    float shake;           // n
    int wallEffect;        // B: a hit on a wall makes the hit effect.
    float w, h;            // The size of the weapon sprite in meters.
} FOWeaponDef;

// The weapon of the item id, or NULL.
const FOWeaponDef *fo_weapon_def(int item);

// The default weapon of a class (xn.java:851-874).
const FOWeaponDef *fo_weapon_default(int cls);

// The arm data of a class (PlayerRenderData, ada.java:459-521): the size
// of the arm (h x i), the place of the arm (j), and its pivot (k).
typedef struct {
    float h, i;
    float jx, jy;
    float kx, ky;
    float bodyW;           // The width of body_idle1, for the place of the arm.
} FOArm;

const FOArm *fo_class_arm(int cls);

#endif
