#include "fo_phys.h"

#include <Box2D/Box2D.h>

// The C types are the Box2D types under another name.
static b2World *W(FOPhysWorld *w) { return reinterpret_cast<b2World *>(w); }
static b2Body *B(FOPhysBody *b) { return reinterpret_cast<b2Body *>(b); }
static const b2Body *CB(const FOPhysBody *b) { return reinterpret_cast<const b2Body *>(b); }
static b2Fixture *F(FOPhysFixture *f) { return reinterpret_cast<b2Fixture *>(f); }
static const b2Fixture *CF(const FOPhysFixture *f) { return reinterpret_cast<const b2Fixture *>(f); }
static FOPhysFixture *OUT(b2Fixture *f) { return reinterpret_cast<FOPhysFixture *>(f); }

class Listener : public b2ContactListener {
public:
    FOContactListener calls;
    Listener() { calls.context = 0; calls.begin = 0; calls.end = 0; }
    void BeginContact(b2Contact *contact) {
        if (!calls.begin) return;
        b2WorldManifold m;
        contact->GetWorldManifold(&m);
        calls.begin(calls.context, OUT(contact->GetFixtureA()), OUT(contact->GetFixtureB()), m.points[0].x,
                    m.points[0].y, m.normal.x, m.normal.y);
    }
    void EndContact(b2Contact *contact) {
        if (calls.end) calls.end(calls.context, OUT(contact->GetFixtureA()), OUT(contact->GetFixtureB()));
    }
};

struct WorldBox {
    b2World world;
    Listener listener;
    WorldBox(float gx, float gy) : world(b2Vec2(gx, gy)) {
        world.SetAllowSleeping(true);
        world.SetContactListener(&listener);
    }
};

// The world is the first member of WorldBox, so the two pointers agree.
static WorldBox *BOX(FOPhysWorld *w) { return reinterpret_cast<WorldBox *>(w); }

extern "C" {

FOPhysWorld *fo_phys_world_new(float gravity_x, float gravity_y) {
    WorldBox *box = new WorldBox(gravity_x, gravity_y);
    return reinterpret_cast<FOPhysWorld *>(box);
}

void fo_phys_world_free(FOPhysWorld *world) {
    delete BOX(world);
}

void fo_phys_world_listener(FOPhysWorld *world, const FOContactListener *listener) {
    BOX(world)->listener.calls = *listener;
}

void fo_phys_step(FOPhysWorld *world, float dt, int velocity_iterations, int position_iterations) {
    W(world)->Step(dt, velocity_iterations, position_iterations);
}

FOPhysBody *fo_phys_body_new(FOPhysWorld *world, int type, float x, float y, float angle, void *user) {
    b2BodyDef def;
    def.type = type == FO_BODY_STATIC ? b2_staticBody : type == FO_BODY_KINEMATIC ? b2_kinematicBody : b2_dynamicBody;
    def.position.Set(x, y);
    def.angle = angle;
    def.userData = user;
    return reinterpret_cast<FOPhysBody *>(W(world)->CreateBody(&def));
}

void fo_phys_body_free(FOPhysWorld *world, FOPhysBody *body) {
    if (body) W(world)->DestroyBody(B(body));
}

void *fo_phys_body_user(const FOPhysBody *body) {
    return CB(body)->GetUserData();
}

static b2Fixture *add(b2Body *body, b2Shape *shape, const FOFixtureDef *def) {
    b2FixtureDef fd;
    fd.shape = shape;
    fd.density = def->density;
    fd.friction = def->friction;
    fd.restitution = def->restitution;
    fd.isSensor = def->sensor != 0;
    if (def->category) fd.filter.categoryBits = def->category;
    if (def->mask) fd.filter.maskBits = def->mask;
    return body->CreateFixture(&fd);
}

FOPhysFixture *fo_phys_box(FOPhysBody *body, float half_w, float half_h, float cx, float cy, float angle,
                           const FOFixtureDef *def) {
    b2PolygonShape shape;
    shape.SetAsBox(half_w, half_h, b2Vec2(cx, cy), angle);
    return OUT(add(B(body), &shape, def));
}

FOPhysFixture *fo_phys_circle(FOPhysBody *body, float radius, float cx, float cy, const FOFixtureDef *def) {
    b2CircleShape shape;
    shape.m_radius = radius;
    shape.m_p.Set(cx, cy);
    return OUT(add(B(body), &shape, def));
}

// The number of points in the convex hull of the points, with the weld of
// b2PolygonShape::Set. Box2D stops on a hull of fewer than 3 points.
static int hull_count(const b2Vec2 *in, int count) {
    b2Vec2 ps[b2_maxPolygonVertices];
    int n = 0;
    for (int i = 0; i < count; i++) {
        bool unique = true;
        for (int j = 0; j < n; j++) {
            if (b2DistanceSquared(in[i], ps[j]) < (0.5f * b2_linearSlop) * (0.5f * b2_linearSlop)) unique = false;
        }
        if (unique) ps[n++] = in[i];
    }
    if (n < 3) return n;
    // Gift wrapping from the point with the largest x.
    int first = 0;
    for (int i = 1; i < n; i++) {
        if (ps[i].x > ps[first].x || (ps[i].x == ps[first].x && ps[i].y < ps[first].y)) first = i;
    }
    int hull = 0, ih = first;
    for (;;) {
        hull++;
        int ie = 0;
        for (int j = 1; j < n; j++) {
            if (ie == ih) {
                ie = j;
                continue;
            }
            b2Vec2 r = ps[ie] - ps[ih], v = ps[j] - ps[ih];
            float c = b2Cross(r, v);
            if (c < 0.0f || (c == 0.0f && v.LengthSquared() > r.LengthSquared())) ie = j;
        }
        ih = ie;
        if (ie == first || hull > n) break;
    }
    return hull;
}

FOPhysFixture *fo_phys_polygon(FOPhysBody *body, const float *xy, int count, const FOFixtureDef *def) {
    if (count < 3 || count > b2_maxPolygonVertices) return NULL;
    b2Vec2 points[b2_maxPolygonVertices];
    for (int i = 0; i < count; i++) points[i].Set(xy[2 * i], xy[2 * i + 1]);
    if (hull_count(points, count) < 3) return NULL;
    b2PolygonShape shape;
    shape.Set(points, count);
    return OUT(add(B(body), &shape, def));
}

void fo_phys_body_position(const FOPhysBody *body, float *x, float *y) {
    b2Vec2 p = CB(body)->GetPosition();
    *x = p.x;
    *y = p.y;
}

float fo_phys_body_angle(const FOPhysBody *body) {
    return CB(body)->GetAngle();
}

void fo_phys_body_velocity(const FOPhysBody *body, float *vx, float *vy) {
    b2Vec2 v = CB(body)->GetLinearVelocity();
    *vx = v.x;
    *vy = v.y;
}

void fo_phys_body_set_velocity(FOPhysBody *body, float vx, float vy) {
    B(body)->SetLinearVelocity(b2Vec2(vx, vy));
}

void fo_phys_body_set_transform(FOPhysBody *body, float x, float y, float angle) {
    B(body)->SetTransform(b2Vec2(x, y), angle);
}

void fo_phys_body_set_angular_velocity(FOPhysBody *body, float omega) {
    B(body)->SetAngularVelocity(omega);
}

void fo_phys_body_set_fixed_rotation(FOPhysBody *body, int fixed) {
    B(body)->SetFixedRotation(fixed != 0);
}

void fo_phys_body_set_bullet(FOPhysBody *body, int bullet) {
    B(body)->SetBullet(bullet != 0);
}

void fo_phys_body_set_gravity_scale(FOPhysBody *body, float scale) {
    B(body)->SetGravityScale(scale);
}

void fo_phys_body_set_mass(FOPhysBody *body, float mass) {
    // As libGDX: resetMassData, then setMassData with the new mass and the
    // same centre and inertia.
    B(body)->ResetMassData();
    b2MassData data;
    B(body)->GetMassData(&data);
    data.mass = mass;
    B(body)->SetMassData(&data);
}

float fo_phys_body_mass(const FOPhysBody *body) {
    return CB(body)->GetMass();
}

void fo_phys_body_set_type(FOPhysBody *body, int type) {
    B(body)->SetType(type == FO_BODY_STATIC ? b2_staticBody : type == FO_BODY_KINEMATIC ? b2_kinematicBody
                                                                                          : b2_dynamicBody);
}

void fo_phys_body_set_awake(FOPhysBody *body, int awake) {
    B(body)->SetAwake(awake != 0);
}

void fo_phys_body_set_sleeping_allowed(FOPhysBody *body, int allowed) {
    B(body)->SetSleepingAllowed(allowed != 0);
}

void fo_phys_body_set_active(FOPhysBody *body, int active) {
    B(body)->SetActive(active != 0);
}

void fo_phys_body_apply_impulse(FOPhysBody *body, float ix, float iy) {
    B(body)->ApplyLinearImpulse(b2Vec2(ix, iy), B(body)->GetWorldCenter(), true);
}

void fo_phys_body_set_mask(FOPhysBody *body, unsigned short category, unsigned short mask) {
    for (b2Fixture *f = B(body)->GetFixtureList(); f; f = f->GetNext()) {
        b2Filter filter = f->GetFilterData();
        if (filter.categoryBits != category) continue;
        filter.maskBits = mask;
        f->SetFilterData(filter);
    }
}

void fo_phys_body_set_filter(FOPhysBody *body, unsigned short match, unsigned short category, unsigned short mask) {
    for (b2Fixture *f = B(body)->GetFixtureList(); f; f = f->GetNext()) {
        b2Filter filter = f->GetFilterData();
        if (filter.categoryBits != match) continue;
        filter.categoryBits = category;
        filter.maskBits = mask;
        f->SetFilterData(filter);
    }
}

FOPhysBody *fo_phys_fixture_body(const FOPhysFixture *fixture) {
    return reinterpret_cast<FOPhysBody *>(const_cast<b2Body *>(CF(fixture)->GetBody()));
}

unsigned short fo_phys_fixture_category(const FOPhysFixture *fixture) {
    return CF(fixture)->GetFilterData().categoryBits;
}

int fo_phys_fixture_sensor(const FOPhysFixture *fixture) {
    return CF(fixture)->IsSensor() ? 1 : 0;
}

void fo_phys_fixture_set_filter(FOPhysFixture *fixture, unsigned short category, unsigned short mask) {
    b2Filter filter = F(fixture)->GetFilterData();
    filter.categoryBits = category;
    filter.maskBits = mask;
    F(fixture)->SetFilterData(filter);
}

int fo_phys_fixture_test_point(const FOPhysFixture *fixture, float x, float y) {
    return CF(fixture)->TestPoint(b2Vec2(x, y)) ? 1 : 0;
}

int fo_phys_body_test_point(const FOPhysBody *body, float x, float y) {
    for (const b2Fixture *f = CB(body)->GetFixtureList(); f; f = f->GetNext()) {
        if (f->TestPoint(b2Vec2(x, y))) return 1;
    }
    return 0;
}

class Ray : public b2RayCastCallback {
public:
    FORayReport report;
    void *context;
    float32 ReportFixture(b2Fixture *fixture, const b2Vec2 &point, const b2Vec2 &normal, float32 fraction) {
        return report(context, OUT(fixture), point.x, point.y, normal.x, normal.y, fraction);
    }
};

void fo_phys_ray(FOPhysWorld *world, float x1, float y1, float x2, float y2, FORayReport report, void *context) {
    // libGDX GameWorld skips a ray of length 0, which Box2D does not allow.
    if ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) <= 0) return;
    Ray ray;
    ray.report = report;
    ray.context = context;
    W(world)->RayCast(&ray, b2Vec2(x1, y1), b2Vec2(x2, y2));
}

}
