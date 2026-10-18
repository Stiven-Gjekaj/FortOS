// fo_phys: a C interface to Box2D 2.3.1, the physics of GANGFORT. The game
// core is C99, and Box2D is C++. This file is the only bridge.

#ifndef FO_PHYS_H
#define FO_PHYS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FOPhysWorld FOPhysWorld;
typedef struct FOPhysBody FOPhysBody;
typedef struct FOPhysFixture FOPhysFixture;

enum {
    FO_BODY_STATIC = 0,
    FO_BODY_KINEMATIC = 1,
    FO_BODY_DYNAMIC = 2,
};

typedef struct {
    float density;
    float friction;
    float restitution;
    unsigned short category;  // 0 means the Box2D default, 1.
    unsigned short mask;      // 0 means the Box2D default, 0xFFFF.
    int sensor;
} FOFixtureDef;

// The calls of the contact listener. Box2D calls them during a step.
typedef struct {
    void *context;
    // A contact starts. (px, py) is the first point of the manifold, and
    // (nx, ny) is its normal from a to b.
    void (*begin)(void *context, FOPhysFixture *a, FOPhysFixture *b, float px, float py, float nx, float ny);
    void (*end)(void *context, FOPhysFixture *a, FOPhysFixture *b);
} FOContactListener;

FOPhysWorld *fo_phys_world_new(float gravity_x, float gravity_y);
void fo_phys_world_free(FOPhysWorld *world);
void fo_phys_world_listener(FOPhysWorld *world, const FOContactListener *listener);
void fo_phys_step(FOPhysWorld *world, float dt, int velocity_iterations, int position_iterations);

FOPhysBody *fo_phys_body_new(FOPhysWorld *world, int type, float x, float y, float angle, void *user);
void fo_phys_body_free(FOPhysWorld *world, FOPhysBody *body);
void *fo_phys_body_user(const FOPhysBody *body);
FOPhysFixture *fo_phys_box(FOPhysBody *body, float half_w, float half_h, float cx, float cy, float angle,
                           const FOFixtureDef *def);
FOPhysFixture *fo_phys_circle(FOPhysBody *body, float radius, float cx, float cy, const FOFixtureDef *def);

void fo_phys_body_position(const FOPhysBody *body, float *x, float *y);
float fo_phys_body_angle(const FOPhysBody *body);
void fo_phys_body_velocity(const FOPhysBody *body, float *vx, float *vy);
void fo_phys_body_set_velocity(FOPhysBody *body, float vx, float vy);
void fo_phys_body_set_transform(FOPhysBody *body, float x, float y, float angle);
void fo_phys_body_set_angular_velocity(FOPhysBody *body, float omega);
void fo_phys_body_set_fixed_rotation(FOPhysBody *body, int fixed);
void fo_phys_body_set_bullet(FOPhysBody *body, int bullet);
void fo_phys_body_set_gravity_scale(FOPhysBody *body, float scale);
void fo_phys_body_set_mass(FOPhysBody *body, float mass);
float fo_phys_body_mass(const FOPhysBody *body);
void fo_phys_body_set_type(FOPhysBody *body, int type);
void fo_phys_body_set_awake(FOPhysBody *body, int awake);
void fo_phys_body_set_sleeping_allowed(FOPhysBody *body, int allowed);
void fo_phys_body_set_active(FOPhysBody *body, int active);
void fo_phys_body_apply_impulse(FOPhysBody *body, float ix, float iy);
// Sets the mask of each fixture with the category cat.
void fo_phys_body_set_mask(FOPhysBody *body, unsigned short category, unsigned short mask);
// Gives each fixture with the category match the new category and mask.
void fo_phys_body_set_filter(FOPhysBody *body, unsigned short match, unsigned short category, unsigned short mask);

FOPhysBody *fo_phys_fixture_body(const FOPhysFixture *fixture);
unsigned short fo_phys_fixture_category(const FOPhysFixture *fixture);
int fo_phys_fixture_sensor(const FOPhysFixture *fixture);
void fo_phys_fixture_set_filter(FOPhysFixture *fixture, unsigned short category, unsigned short mask);
int fo_phys_fixture_test_point(const FOPhysFixture *fixture, float x, float y);
// 1 when a fixture of the body holds the point.
int fo_phys_body_test_point(const FOPhysBody *body, float x, float y);

// World.rayCast: calls report for each fixture on the ray from (x1, y1) to
// (x2, y2). The return value of report controls the ray as in Box2D: -1
// skips the fixture, 0 stops, the fraction clips the ray, 1 goes on.
typedef float (*FORayReport)(void *context, FOPhysFixture *fixture, float px, float py, float nx, float ny,
                             float fraction);
void fo_phys_ray(FOPhysWorld *world, float x1, float y1, float x2, float y2, FORayReport report, void *context);

#ifdef __cplusplus
}
#endif

#endif
