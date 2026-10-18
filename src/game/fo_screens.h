// fo_screens: the screens and the dialogs around a match: SplashScreen
// (ahe.java), MainMenuScreen (afk.java), CreditsScreen (adf.java),
// SettingsScreen (ago.java), SingleplayerScreen (agv.java), TeamSelectionDialog (ajq.java),
// ClassSelectionDialog (ahl.java), and MenuDialog (aja.java). The units are
// pixels of the screen with y up. One UI unit is 1/281 of the width.

#ifndef FO_SCREENS_H
#define FO_SCREENS_H

#include "fo_assets.h"
#include "fo_font.h"
#include "fo_text.h"
#include "fo_world.h"

typedef struct {
    const FOAtlasTextures *sprites;  // textures/sprites.atlas
    const FOAtlasTextures *ui;       // textures/ui.atlas
    const FOAtlasTextures *splash;   // textures/splash.atlas, or NULL
    const FOFont *pixel4, *pixel6, *pixel12, *pixel21;
    const FOText *texts;
    int width, height;
} FOScreensSetup;

void fo_screens_init(const FOScreensSetup *setup);

// The things that a tap on a screen asks for.
typedef enum {
    FO_SCREEN_NONE = 0,
    FO_SCREEN_PLAY,          // Single player: play the map with the players.
    FO_SCREEN_TEAM,          // Team dialog: join the team.
    FO_SCREEN_CLASS,         // Class dialog: play the class.
    FO_SCREEN_RESUME,        // Menu dialog.
    FO_SCREEN_CHANGE_CLASS,
    FO_SCREEN_SWITCH_TEAM,
    FO_SCREEN_QUIT,
    FO_SCREEN_BACK,          // Go back to the main menu.
    FO_SCREEN_SINGLE,        // Main menu: open the single player screen.
    FO_SCREEN_ITEMS,
    FO_SCREEN_CREDITS,
    FO_SCREEN_SETTINGS,
    FO_SCREEN_CONTROLS,      // Settings: open the control setup.
    FO_SCREEN_SETTING,       // Settings: a checkbox changed.
} FOScreenAction;

// The checkboxes of the settings screen, from the bottom.
enum { FO_SETTING_KILLCAM = 0, FO_SETTING_MUSIC, FO_SETTING_VIGNETTE, FO_SETTING_COUNT };

typedef struct {
    FOScreenAction action;
    int map;                 // FO_SCREEN_PLAY: the index in the list of maps.
    int players;             // FO_SCREEN_PLAY: 8, 10, or 12.
    int team;                // FO_SCREEN_TEAM
    int cls;                 // FO_SCREEN_CLASS
    int setting;             // FO_SCREEN_SETTING: the checkbox.
    int value;               // FO_SCREEN_SETTING: 1 when it is on.
    int click;               // A button took the tap: play menu_click1.
} FOScreenResult;

// SplashScreen: the logo falls and plays its frames. fo_splash_draw
// returns 1 when the splash is done, 2.5 s after the start.
void fo_splash_open(void);
int fo_splash_draw(float dt);

// MainMenuScreen: single player, items, credits, and the settings.
void fo_main_draw(void);
FOScreenResult fo_main_tap(float x, float y);

// CreditsScreen.
void fo_credits_draw(void);
FOScreenResult fo_credits_tap(float x, float y);

// SettingsScreen. on holds the state of each checkbox.
void fo_settings_open(const int on[FO_SETTING_COUNT]);
void fo_settings_draw(void);
FOScreenResult fo_settings_tap(float x, float y);

// SingleplayerScreen. The maps are in the order of the original:
// flag_egypt, bomb_canyon, gts_gantown.
void fo_single_open(void);
void fo_single_draw(float dt);
FOScreenResult fo_single_tap(float x, float y);
const char *fo_single_map_name(int index);

// TeamSelectionDialog over the world.
void fo_team_draw(const FOWorld *world);
FOScreenResult fo_team_tap(float x, float y);

// ClassSelectionDialog. cls is the class of the player, or 0.
void fo_class_open(int cls, short team);
void fo_class_draw(const FOWorld *world, float dt);
FOScreenResult fo_class_tap(float x, float y);

// MenuDialog of single player.
void fo_menu_draw(void);
FOScreenResult fo_menu_tap(float x, float y);

#endif
