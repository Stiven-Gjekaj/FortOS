// fo_camera: the camera of GANGFORT (GameCamera, wp.java). The view is
// 22.1875 m wide. The camera follows its target with a lerp of 0.075 in
// each frame, stays inside the map, and shakes.

#ifndef FO_CAMERA_H
#define FO_CAMERA_H

typedef struct {
    float viewW, viewH;    // The size of the view in meters.
    float x, y;            // The smooth position (b in wp.java).
    float targetX, targetY;
    float shakeTime, shakeLength, shakeAmount;  // CameraScreenShakeHelper (ajw.java).
    float lockTime;        // The camera stays still while this is above 0.
    float shownX, shownY;  // The position after the clamp and the shake.
} FOCamera;

void fo_camera_init(FOCamera *c, int width, int height, float x, float y);

// Sets the target and the smooth position, as b(x, y).
void fo_camera_jump(FOCamera *c, float x, float y);

// Starts a shake of ms milliseconds with the amount in meters.
void fo_camera_shake(FOCamera *c, int ms, float amount);

// Moves the camera one frame toward (tx, ty) in a map of mapW x mapH.
void fo_camera_update(FOCamera *c, float dt, float tx, float ty, float mapW, float mapH);

#endif
