// Tests of fo_phys, the bridge to Box2D.

#import "FOTest.h"
#include <math.h>
#include "fo_phys.h"

static FOPhysWorld *worldWithGround(void) {
    FOPhysWorld *world = fo_phys_world_new(0, -15);
    FOPhysBody *ground = fo_phys_body_new(world, FO_BODY_STATIC, 0, 0, 0, NULL);
    FOFixtureDef def = {0.5f, 0.7f, 0, 2, 0x7fff, 0};
    fo_phys_box(ground, 10, 0.5f, 0, -0.5f, 0, &def);
    return world;
}

FO_TEST(testPhysBodyFallsOntoTheGround) {
    FOPhysWorld *world = worldWithGround();
    FOPhysBody *box = fo_phys_body_new(world, FO_BODY_DYNAMIC, 0, 5, 0, NULL);
    FOFixtureDef def = {1, 0.7f, 0, 0, 0, 0};
    fo_phys_box(box, 0.5f, 0.5f, 0, 0, 0, &def);
    for (int i = 0; i < 180; i++) fo_phys_step(world, 1 / 60.0f, 8, 8);
    float x, y;
    fo_phys_body_position(box, &x, &y);
    FO_EXPECT(fabsf(y - 0.5f) < 0.02f);
    fo_phys_world_free(world);
}

static int gBegins, gEnds;
static void onBegin(void *c, FOPhysFixture *a, FOPhysFixture *b, float px, float py, float nx, float ny) {
    (void)c; (void)a; (void)b; (void)px; (void)py; (void)nx; (void)ny;
    gBegins++;
}
static void onEnd(void *c, FOPhysFixture *a, FOPhysFixture *b) {
    (void)c; (void)a; (void)b;
    gEnds++;
}

FO_TEST(testPhysFiltersAndContacts) {
    FOPhysWorld *world = worldWithGround();
    gBegins = gEnds = 0;
    FOContactListener listener = {NULL, onBegin, onEnd};
    fo_phys_world_listener(world, &listener);
    // A body that does not collide with category 2 falls through.
    FOPhysBody *ghost = fo_phys_body_new(world, FO_BODY_DYNAMIC, 3, 2, 0, NULL);
    FOFixtureDef def = {1, 0, 0, 4, 0xffff & ~2, 0};
    fo_phys_circle(ghost, 0.4f, 0, 0, &def);
    // A body that collides lands and makes one contact.
    FOPhysBody *solid = fo_phys_body_new(world, FO_BODY_DYNAMIC, -3, 2, 0, NULL);
    FOFixtureDef def2 = {1, 0, 0, 8, 0xffff, 0};
    fo_phys_circle(solid, 0.4f, 0, 0, &def2);
    for (int i = 0; i < 120; i++) fo_phys_step(world, 1 / 60.0f, 8, 8);
    float x, y;
    fo_phys_body_position(ghost, &x, &y);
    FO_EXPECT(y < -1);
    fo_phys_body_position(solid, &x, &y);
    FO_EXPECT(fabsf(y - 0.4f) < 0.02f);
    FO_EQUAL_INT(gBegins, 1);
    fo_phys_world_free(world);
}

static float firstHit(void *context, FOPhysFixture *f, float px, float py, float nx, float ny, float fraction) {
    (void)f; (void)px; (void)nx; (void)ny;
    *(float *)context = py;
    return fraction;
}

FO_TEST(testPhysRayHitsTheGround) {
    FOPhysWorld *world = worldWithGround();
    float hit = 99;
    fo_phys_ray(world, 1, 5, 1, -5, firstHit, &hit);
    FO_EXPECT(fabsf(hit) < 1e-4f);
    hit = 99;
    fo_phys_ray(world, 1, 5, 1, 5, firstHit, &hit);
    FO_EXPECT(hit == 99);
    fo_phys_world_free(world);
}

FO_TEST(testPhysPolygonFallsAndTurns) {
    // The polygon of the grey hat: four points, its bottom on the ground.
    FOPhysWorld *world = worldWithGround();
    FOPhysBody *hat = fo_phys_body_new(world, FO_BODY_DYNAMIC, 0, 3, 0, NULL);
    const float xy[8] = {0.35f, -0.15f, -0.05f, 0.2f, -0.45f, 0, -0.25f, -0.2f};
    FOFixtureDef def = {0.5f, 0.2f, 0, 1, 2, 0};
    FO_EXPECT(fo_phys_polygon(hat, xy, 4, &def) != NULL);
    FO_EXPECT(fo_phys_body_mass(hat) > 0);
    fo_phys_body_set_angular_velocity(hat, 4);
    for (int i = 0; i < 240; i++) fo_phys_step(world, 1 / 60.0f, 8, 8);
    float x, y;
    fo_phys_body_position(hat, &x, &y);
    FO_EXPECT(y > 0 && y < 0.5f);
    const float line[6] = {0, 0, 1, 0, 2, 0};
    FO_EXPECT(fo_phys_polygon(hat, line, 3, &def) == NULL);
    FO_EXPECT(fo_phys_polygon(hat, xy, 2, &def) == NULL);
    fo_phys_world_free(world);
}
