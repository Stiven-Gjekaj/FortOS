#import "FOAppDelegate.h"
#import "FOGameViewController.h"
#include "fo_audio.h"

@implementation FOAppDelegate {
    FOGameViewController *_game;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    [application setStatusBarHidden:YES];
    // The iOS backend of libGDX keeps the screen on (preventScreenDimming).
    // A match has long times with no touch, as the wait for a respawn.
    application.idleTimerDisabled = YES;
    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    _game = [[FOGameViewController alloc] init];
    self.window.rootViewController = _game;
    [self.window makeKeyAndVisible];
    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {
    // iOS stops the app when it draws with OpenGL in the background.
    [_game setPaused:YES];
    fo_audio_set_active(0);
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    [_game setPaused:NO];
    fo_audio_set_active(1);
}

@end
