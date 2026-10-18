// fo_bots: the bots of single player. See fo_bots.h.

#include "fo_bots.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fo_world_private.h"

// --- Small helpers --------------------------------------------------------

// MathUtils.random(int, int): from lo to hi, both included.
static int rand_int(FOWorld *world, int lo, int hi) {
    int v = lo + (int)(fo_world_random(world) * (float)(hi - lo + 1));
    return v > hi ? hi : v;
}

// MathUtils.random(float, float).
static float rand_float(FOWorld *world, float lo, float hi) {
    return lo + fo_world_random(world) * (hi - lo);
}

static float dist2(float ax, float ay, float bx, float by) {
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
}

static FOPlayer *bot_player(FOWorld *world, const FOBot *bot) {
    return fo_world_player(world, bot->id);
}

static int player_index(const FOWorld *world, const FOPlayer *p) {
    return (int)(p - world->players);
}

static void pos(const FOPlayer *p, float *x, float *y) {
    fo_player_position(p, x, y);
}

static int enemy_team(short team) {
    return team == FO_TEAM_RED ? FO_TEAM_BLU : team == FO_TEAM_BLU ? FO_TEAM_RED : 0;
}

static int hidden_spy(const FOPlayer *p) {
    return p->cls == FO_CLASS_SPY && (p->disguise.active || p->cloaked);
}

// --- The map hooks (the inner class a of acj, aco, and acp) ---------------

static void objective(const FOWorld *world, const FOPlayer *p, float *x, float *y) {
    const FOMode *m = &world->mode;
    *x = *y = 0;
    if (m->kind == FO_MODE_BOMB) {
        fo_phys_body_position(m->bomb.obj.body, x, y);
    } else if (m->kind == FO_MODE_KOTH) {
        const FORect *r = fo_tmx_area(&world->map, FO_AREA_KOTH);
        if (r) {
            *x = r->x + r->w / 2;
            *y = r->y + r->h / 2;
        }
    } else if (m->kind == FO_MODE_CTF) {
        // FLAG_Eypt.a.b(xn) (aco.java:273-285).
        short own = p->team, other = (short)enemy_team(p->team);
        if (own != FO_TEAM_RED && own != FO_TEAM_BLU) return;
        if (m->flags[other].carrier == p->id) {
            const FORect *r = fo_tmx_area(&world->map, own == FO_TEAM_RED ? FO_AREA_CAPTURE_RED : FO_AREA_CAPTURE_BLU);
            if (r) {
                *x = r->x + r->w / 2;
                *y = r->y + r->h / 2;
            }
            return;
        }
        const FOFlag *f = (m->flags[own].carrier != -1 || m->flags[own].dropped) ? &m->flags[own] : &m->flags[other];
        fo_phys_body_position(f->obj.body, x, y);
    }
}

static float objective_spread(const FOWorld *world) {
    if (world->mode.kind == FO_MODE_KOTH) {
        const FORect *r = fo_tmx_area(&world->map, FO_AREA_KOTH);
        return r ? r->w : 0;
    }
    return 1.5f;
}

static int in_objective(const FOWorld *world, const FOPlayer *p, float x, float y) {
    const FOMode *m = &world->mode;
    if (m->kind == FO_MODE_BOMB) {
        float bx, by;
        fo_phys_body_position(m->bomb.obj.body, &bx, &by);
        return sqrtf(dist2(bx, by, x, y)) < 1.7f;
    }
    if (m->kind == FO_MODE_KOTH) {
        const FORect *r = fo_tmx_area(&world->map, FO_AREA_KOTH);
        return r && fo_rect_contains(r, x, y);
    }
    if (m->kind == FO_MODE_CTF) {
        float ox, oy;
        objective(world, p, &ox, &oy);
        return sqrtf(dist2(ox, oy, x, y)) < 1.5f;
    }
    return 0;
}

// GameMapBotData.a(int) (zq.java:23-38): a random waypoint that is not the
// spawn waypoint of the other team.
static int random_waypoint(FOBots *bots, FOWorld *world, short team) {
    FOGraph *g = &bots->graph;
    int avoid = g->spawn[enemy_team(team)];
    if (g->count == 0) return -1;
    for (int tries = 0; tries < 10000; tries++) {
        int id = rand_int(world, g->minId, g->maxId);
        int at = fo_graph_find(g, id);
        if (at >= 0 && id != avoid) return at;
    }
    return 0;
}

// GameMapBotData.a(xn) (zq.java:40-46): a random waypoint out of the zone
// of the objective.
static int random_waypoint_out(FOBots *bots, FOWorld *world, const FOPlayer *p) {
    int at = random_waypoint(bots, world, p->team);
    for (int tries = 0; tries < 1000 && at >= 0; tries++) {
        if (!in_objective(world, p, bots->graph.points[at].x, bots->graph.points[at].y)) break;
        at = random_waypoint(bots, world, p->team);
    }
    return at;
}

static int spawn_waypoint(const FOBots *bots, short team) {
    return team == FO_TEAM_RED || team == FO_TEAM_BLU ? bots->graph.spawn[team] : 0;
}

// --- The navigator (zn.java) ----------------------------------------------

typedef struct {
    FOBots *bots;
    FOWorld *world;
    FOBot *bot;
    FOPlayer *p;
} FOBotContext;

// zo.java: no link into the spawn of the other team, and the double jumps
// only for the rusher.
static int nav_filter(void *context, int from, const FOLink *l) {
    (void)from;
    FOBotContext *c = context;
    if (c->bots->graph.points[l->to].id == spawn_waypoint(c->bots, (short)enemy_team(c->p->team))) return 0;
    if ((l->type == FO_LINK_DOUBLE1000 || l->type == FO_LINK_DOUBLE500 || l->type == FO_LINK_MIDAIR700) &&
        c->p->cls != FO_CLASS_SCOUT) {
        return 0;
    }
    return 1;
}

// zy.java: the agent keeps away from the objective.
static int spy_filter(void *context, int from, const FOLink *l) {
    (void)from;
    FOBotContext *c = context;
    const FOWaypoint *w = &c->bots->graph.points[l->to];
    if (in_objective(c->world, c->p, w->x, w->y)) return 0;
    float ox, oy;
    objective(c->world, c->p, &ox, &oy);
    return fo_graph_closest(&c->bots->graph, ox, oy) != l->to;
}

static int nav_path(FOBotContext *c, int start, int goal, int exact, int navFilter, int brainFilter, int *out) {
    FOLinkFilter f2 = NULL;
    if (brainFilter && c->p->cls == FO_CLASS_SPY) f2 = spy_filter;
    return fo_graph_path(&c->bots->graph, c->world, start, goal, exact, navFilter ? nav_filter : NULL, c, f2, c, out,
                         FO_BOT_PATH);
}

// zn.a(aaa): a path to the waypoint.
static void nav_goto(FOBotContext *c, int goal) {
    FOBot *b = c->bot;
    if (goal < 0) return;
    int first = b->pathCount > 0 ? b->path[0] : -1;
    float x, y;
    pos(c->p, &x, &y);
    int path[FO_BOT_PATH];
    int n = nav_path(c, fo_graph_closest(&c->bots->graph, x, y), goal, 0, 1, 1, path);
    if (n >= 0) {
        b->retry = -1;
        memcpy(b->path, path, sizeof(int) * (size_t)n);
        b->pathCount = n;
        if (first >= 0 && b->pathCount > 1 && first == b->path[1]) {
            memmove(&b->path[0], &b->path[1], sizeof(int) * (size_t)(b->pathCount - 1));
            b->pathCount--;
        }
    } else {
        b->retry = goal;
        b->retryTime = c->world->now;
    }
    b->free = 0;
}

// zn.a(float, float): a path to a free point.
static void nav_point(FOBotContext *c, float tx, float ty) {
    FOBot *b = c->bot;
    float x, y;
    pos(c->p, &x, &y);
    int target = fo_graph_closest(&c->bots->graph, tx, ty);
    int here = fo_graph_closest(&c->bots->graph, x, y);
    if (here != target) {
        nav_goto(c, target);
        if (b->pathCount >= 2 && b->path[0] == here && b->path[1] == target) {
            memmove(&b->path[0], &b->path[1], sizeof(int) * (size_t)(b->pathCount - 1));
            b->pathCount--;
        }
    }
    b->free = 1;
    b->freeX = tx;
    b->freeY = ty;
}

// zn.a(aaa, aaa): the length of the exact path, as the sum of the squared
// lengths of its links.
static float nav_length(FOBotContext *c, int from, int to) {
    int path[FO_BOT_PATH];
    int n = fo_graph_path(&c->bots->graph, c->world, from, to, 1, NULL, NULL, NULL, NULL, path, FO_BOT_PATH);
    if (n < 0) return 2.1474836e9f;
    float sum = 0;
    for (int i = 0; i + 1 < n; i++) {
        const FOLink *l = fo_graph_link(&c->bots->graph, path[i], path[i + 1]);
        if (l) sum += l->length;
    }
    return sum;
}

static int near_waypoint(const FOBotContext *c, int at) {
    float x, y;
    pos(c->p, &x, &y);
    const FOWaypoint *w = &c->bots->graph.points[at];
    return fabsf(x - w->x) <= 0.45000002f && fabsf(y - w->y) <= 0.95f;
}

// zn.c(): the next waypoint is reached.
static int nav_reached(const FOBotContext *c) {
    return c->bot->pathCount == 0 || near_waypoint(c, c->bot->path[0]);
}

// zn.d(): the goal is reached.
static int nav_done(const FOBotContext *c) {
    const FOBot *b = c->bot;
    if (b->free) {
        float x, y;
        pos(c->p, &x, &y);
        return fabsf(x - b->freeX) < 0.2f;
    }
    return b->pathCount == 0 || near_waypoint(c, b->path[b->pathCount - 1]);
}

// zn.e(): the id of the goal, or 0.
static int nav_goal_id(const FOBotContext *c) {
    const FOBot *b = c->bot;
    return b->pathCount == 0 ? 0 : c->bots->graph.points[b->path[b->pathCount - 1]].id;
}

static void nav_clear(FOBot *b) {
    b->pathCount = 0;
    b->free = 0;
}

// zn.a() and zn.b(): the bot turns to the left or to the right at once.
static void nav_face(FOPlayer *p, int right) {
    p->move = right ? FO_MOVE_RIGHT : FO_MOVE_LEFT;
    p->facingRight = right;
}

static int weapon_busy(const FOWorld *world, const FOPlayer *p) {
    // Weapon.r() (alz.java:346-348).
    return p->weapon.def && (float)(world->now - p->weapon.lastShot) < (float)p->weapon.def->interval * 2.0f;
}

static int touches_wall(const FOTouchSet *s) {
    if (s->ground > 0) return 1;
    for (int i = 0; i < s->count; i++) {
        if (s->objs[i]->kind == FO_OBJ_PLAYER || s->objs[i]->kind == FO_OBJ_SENTRY) return 1;
    }
    return 0;
}

static int link_jumps(int type) {
    return type == FO_LINK_JUMP || type == FO_LINK_JUMP_WAIT100 || type == FO_LINK_JUMP_WAIT300 ||
           type == FO_LINK_DOUBLE1000 || type == FO_LINK_DOUBLE500;
}

static int link_holds(int type) {
    return type == FO_LINK_JUMP_WAIT100 || type == FO_LINK_JUMP_WAIT300;
}

static int link_hold_ms(int type) {
    return type == FO_LINK_JUMP_WAIT100 ? 100 : type == FO_LINK_JUMP_WAIT300 ? 300 : 0;
}

static int link_doubles(int type) {
    return type == FO_LINK_DOUBLE1000 || type == FO_LINK_DOUBLE500 || type == FO_LINK_MIDAIR700;
}

static int link_second_ms(int type) {
    return type == FO_LINK_DOUBLE1000 ? 1000 : type == FO_LINK_DOUBLE500 ? 500 : type == FO_LINK_MIDAIR700 ? 700 : 0;
}

// zn.a(float) (zn.java:31-100).
static void nav_update(FOBotContext *c) {
    FOBot *b = c->bot;
    FOPlayer *p = c->p;
    FOWorld *world = c->world;
    FOGraph *g = &c->bots->graph;
    // A paused navigator does not call Player.h(float), so the speed of the
    // bot stays as it is (zn.java:32).
    p->noMove = b->paused;
    if (b->paused) return;
    float x, y;
    pos(p, &x, &y);
    if (!nav_done(c)) {
        float tx = b->pathCount > 0 ? g->points[b->path[0]].x : b->freeX;
        float d = fabsf(x - tx);
        if (x > tx && d > 0.1f) p->move = FO_MOVE_LEFT;
        else if (x < tx && d > 0.1f) p->move = FO_MOVE_RIGHT;
        else p->move = FO_MOVE_STOP;
        if (b->pathCount > 0) {
            const FOWaypoint *w = &g->points[b->path[0]];
            if (p->move == FO_MOVE_STOP && fo_player_grounded(p) && fabsf(w->y - y) > 0.95f) {
                int goal = fo_graph_find(g, nav_goal_id(c));
                nav_goto(c, goal);
            } else {
                if (world->now - b->stuckTime >= 200) {
                    if (sqrtf(dist2(x, y, b->lastX, b->lastY)) < 0.1f) {
                        if (p->move == FO_MOVE_LEFT && touches_wall(&p->left) && !p->backstab) {
                            fo_player_jump(world, p);
                        } else if (p->move == FO_MOVE_RIGHT && touches_wall(&p->right) && !p->backstab) {
                            fo_player_jump(world, p);
                        }
                    }
                    b->lastX = x;
                    b->lastY = y;
                    b->stuckTime = world->now;
                }
                int cur = b->path[0];
                const FOLink *next = b->pathCount >= 2 ? fo_graph_link(g, cur, b->path[1]) : NULL;
                int nextType = next ? (int)next->type : FO_LINK_WALK;
                if (nav_reached(c) && (fo_player_grounded(p) || (b->pathCount >= 2 && nextType == FO_LINK_WALK)) &&
                    (b->pathCount < 2 || !link_jumps(nextType) || !weapon_busy(world, p))) {
                    memmove(&b->path[0], &b->path[1], sizeof(int) * (size_t)(b->pathCount - 1));
                    b->pathCount--;
                    if (b->pathCount > 0) {
                        const FOLink *l = fo_graph_link(g, cur, b->path[0]);
                        int type = l ? (int)l->type : FO_LINK_WALK;
                        b->link = type;
                        if (type == FO_LINK_WALK && rand_int(world, 0, 4) == 2) {
                            if (g->points[cur].x - g->points[b->path[0]].x < 0.2f) fo_player_jump(world, p);
                        } else if (link_jumps(type)) {
                            fo_player_jump(world, p);
                            b->linkTime = world->now;
                            b->secondJump = 0;
                        } else if (type == FO_LINK_MIDAIR700) {
                            b->linkTime = world->now;
                            b->secondJump = 0;
                        }
                    }
                }
                if (b->link >= 0 && link_holds(b->link) && world->now - b->linkTime < link_hold_ms(b->link)) {
                    p->move = FO_MOVE_STOP;
                } else if (b->link >= 0 && link_doubles(b->link) && world->now - b->linkTime > link_second_ms(b->link) &&
                           !b->secondJump) {
                    fo_player_jump(world, p);
                    b->secondJump = 1;
                }
            }
        }
    } else {
        p->move = FO_MOVE_STOP;
        if (b->retry >= 0 && world->now - b->retryTime >= 3000) nav_goto(c, b->retry);
    }
}

// --- The brains (zk.java and the subclasses) ------------------------------

static int brain_kind(int cls) {
    return cls >= 6 && cls <= 9 ? cls : cls >= 1 && cls <= 5 ? cls : 0;
}

// The fields of a new brain of the class.
static void brain_new(FOBotContext *c) {
    FOBot *b = c->bot;
    FOWorld *world = c->world;
    b->brainClass = brain_kind(c->p->cls);
    b->goalTime = b->huntTime = b->huntMs = b->stopTime = 0;
    b->stop = 0;
    b->goalMs = rand_int(world, 4000, 8000);
    b->huntChance = (float)rand_int(world, 20, 35);
    b->stopMs = rand_int(world, 50, 120);
    b->buildTime = b->buildMs = 0;
    b->follow = -1;
    b->followLength = 0;
    b->followTime = b->followMs = b->healTime = b->healMs = 0;
    b->campStart = b->campMs = 0;
    b->toCamp = b->campRight = 0;
    b->lookTime = b->lookMs = b->waitStart = b->waitMs2 = 0;
    if (c->p->cls == FO_CLASS_ENGINEER) {
        b->buildFar = rand_float(world, 20.0f, 35.0f);
        b->moveFar = rand_float(world, 15.0f, 24.0f);
        const FOGraph *g = &c->bots->graph;
        int enemy = fo_graph_find(g, spawn_waypoint(c->bots, (short)enemy_team(c->p->team)));
        int own = fo_graph_find(g, spawn_waypoint(c->bots, c->p->team));
        b->enemyRight = enemy >= 0 && own >= 0 && g->points[enemy].x > g->points[own].x;
    }
}

// zm.p(): a new brain when the class changes.
static void brain_check(FOBotContext *c) {
    if (c->bot->brainClass != brain_kind(c->p->cls)) brain_new(c);
}

static FOPlayer *player_at(FOWorld *world, int index) {
    if (index < 0 || index >= FO_MAX_PLAYERS) return NULL;
    FOPlayer *p = &world->players[index];
    return p->used ? p : NULL;
}

// zm.v(): the bot has no ammo.
static int no_ammo(const FOPlayer *p) {
    return !p->weapon.def || p->weapon.ammo <= 0.0f;
}

// BotBrain.g() (zk.java:104-123): the bot wants the objective.
static int wants_objective(const FOBotContext *c) {
    const FOMode *m = &c->world->mode;
    const FOPlayer *p = c->p;
    if (m->kind == FO_MODE_KOTH) return m->owner != p->team;
    if (m->kind == FO_MODE_BOMB) {
        if (p->team == FO_TEAM_RED) {
            // RollableBomb.b(xn) is 0 for a player out of the list too.
            int index = 0;
            for (int i = 0; i < m->bomb.redCount; i++) {
                if (m->bomb.red[i] == p->id) index = i;
            }
            return m->bomb.redCount < 2 || index < 2;
        }
        if (p->team == FO_TEAM_BLU) return m->bomb.redCount > m->bomb.bluCount;
        return 0;
    }
    if (m->kind == FO_MODE_CTF) {
        short own = p->team, other = (short)enemy_team(p->team);
        if (own != FO_TEAM_RED && own != FO_TEAM_BLU) return 0;
        return m->flags[own].carrier != -1 || m->flags[own].dropped || m->flags[other].carrier == -1 ||
               m->flags[other].carrier == p->id;
    }
    return 0;
}

// BotBrain.d() (zk.java:69-74): to the objective, with a random spread.
static void go_objective(FOBotContext *c) {
    float x, y;
    objective(c->world, c->p, &x, &y);
    float s = objective_spread(c->world);
    x += rand_float(c->world, -s / 2, s / 2);
    nav_point(c, x, y);
}

// SniperBotBrain.s(): the camper waits on a camp spot.
static int camping(const FOBotContext *c) {
    const FOBot *b = c->bot;
    return !(b->campStart == 0 || b->campMs == 0 || c->world->now - b->campStart >= b->campMs);
}

// BotBrain.o(): the bot may aim.
static int brain_aims(const FOBotContext *c) {
    return c->p->cls == FO_CLASS_SPY ? !c->p->disguise.active : 1;
}

// BotBrain.n(): the bot may shoot.
static int brain_fires(const FOBotContext *c) {
    const FOPlayer *p = c->p;
    if (p->cls == FO_CLASS_SNIPER) return p->weapon.def != NULL && p->sniperCharge >= 30.0f;
    if (p->cls == FO_CLASS_MEDIC) return p->healTarget == 0;
    if (p->cls == FO_CLASS_SPY) return !p->disguise.active && !p->cloaked;
    return 1;
}

// BotBrain.l(): the bot looks around with no target.
static int brain_idles(const FOBotContext *c) {
    const FOPlayer *p = c->p;
    if (p->cls == FO_CLASS_SNIPER) return camping(c);
    if (p->cls == FO_CLASS_MEDIC) return p->healTarget == 0 || p->move == FO_MOVE_STOP;
    return 1;
}

// BotBrain.m(): the side that the bot looks to when it stands.
static int brain_faces_right(const FOBotContext *c) {
    const FOPlayer *p = c->p;
    if (p->cls == FO_CLASS_SNIPER && camping(c)) return c->bot->campRight;
    if (p->cls == FO_CLASS_MEDIC && c->bot->follow >= 0) {
        const FOPlayer *t = player_at(c->world, c->bot->follow);
        if (t) {
            float tx, ty, x, y;
            pos(t, &tx, &ty);
            pos(p, &x, &y);
            return tx > x;
        }
    }
    return p->facingRight;
}

static void target_position(const FOWorld *world, int target, float *x, float *y) {
    if (target < FO_MAX_PLAYERS) {
        pos(&world->players[target], x, y);
    } else {
        fo_phys_body_position(world->players[target - FO_MAX_PLAYERS].sentry.obj.body, x, y);
    }
}

// BotBrain.a(zv): the brain lets the bot aim at the target.
static int brain_target_ok(const FOBotContext *c, int target, const FOBotTarget *t) {
    if (c->p->cls == FO_CLASS_SNIPER) {
        if (camping(c)) {
            float tx, ty, x, y;
            target_position(c->world, target, &tx, &ty);
            pos(c->p, &x, &y);
            if (c->bot->campRight && tx < x) return 0;
            if (!c->bot->campRight && tx > x) return 0;
        }
        return t->shootable;
    }
    return 1;
}

typedef struct {
    int hit;
    const FOBotContext *c;
} FORayHit;

// zj.java and zt.java: the ray stops at the ground or at a spawn door.
static float ray_wall(void *context, FOPhysFixture *fixture, float px, float py, float nx, float ny, float fraction) {
    (void)px;
    (void)py;
    (void)nx;
    (void)ny;
    (void)fraction;
    FORayHit *r = context;
    FOObj *o = fo_world_fixture_obj(fixture);
    if (!o || (o->kind != FO_OBJ_GROUND && o->kind != FO_OBJ_DOOR)) return 1.0f;
    r->hit = 1;
    return 0.0f;
}

static int wall_between(FOWorld *world, float x1, float y1, float x2, float y2) {
    FORayHit r = {0, NULL};
    if (x1 == x2 && y1 == y2) return 0;
    fo_phys_ray(world->phys, x1, y1, x2, y2, ray_wall, &r);
    return r.hit;
}

// zl.java: an enemy that the bot can shoot stands on the way.
static float ray_enemy(void *context, FOPhysFixture *fixture, float px, float py, float nx, float ny, float fraction) {
    (void)px;
    (void)py;
    (void)nx;
    (void)ny;
    (void)fraction;
    FORayHit *r = context;
    FOObj *o = fo_world_fixture_obj(fixture);
    if (!o || o->kind != FO_OBJ_PLAYER) return 1.0f;
    const FOPlayer *e = (const FOPlayer *)o;
    if (e->team == r->c->p->team || hidden_spy(e)) return 1.0f;
    float x, y, ex, ey;
    pos(r->c->p, &x, &y);
    pos(e, &ex, &ey);
    const FOBotTarget *t = &r->c->bot->players[player_index(r->c->world, e)];
    if (fabsf(x - ex) < 4.0f && t->used && t->shootable) {
        r->hit = 1;
        return 0.0f;
    }
    return 1.0f;
}

// zz.java: an enemy on the way that does not walk away from the agent.
static float ray_spy(void *context, FOPhysFixture *fixture, float px, float py, float nx, float ny, float fraction) {
    (void)px;
    (void)py;
    (void)nx;
    (void)ny;
    (void)fraction;
    FORayHit *r = context;
    FOObj *o = fo_world_fixture_obj(fixture);
    if (!o || o->kind != FO_OBJ_PLAYER) return 1.0f;
    const FOPlayer *e = (const FOPlayer *)o;
    if (e->team == r->c->p->team) return 1.0f;
    if (e->cls == FO_CLASS_SPY && (e->cloaked || e->disguise.active)) return 1.0f;
    float x, y, ex, ey, vx, vy;
    pos(r->c->p, &x, &y);
    pos(e, &ex, &ey);
    fo_player_velocity(e, &vx, &vy);
    int left = x > ex;
    if (left && vx < -0.5f) return 1.0f;
    if (!left && vx > 0.5f) return 1.0f;
    r->hit = 1;
    return 0.0f;
}

// BotBrain.b() (zk.java:37-50) and the goals of the subclasses.
static void brain_goal(FOBotContext *c) {
    FOBot *b = c->bot;
    FOPlayer *p = c->p;
    FOWorld *world = c->world;
    switch (p->cls) {
    case FO_CLASS_ENGINEER:
        if (nav_done(c)) nav_goto(c, random_waypoint(c->bots, world, p->team));
        return;
    case FO_CLASS_SPY:
        if (nav_done(c)) nav_goto(c, random_waypoint_out(c->bots, world, p));
        return;
    case FO_CLASS_SNIPER: {
        // SniperBotBrain.b() (zw.java:41-114).
        if (b->toCamp && nav_done(c)) {
            b->toCamp = 0;
            b->campStart = world->now;
            b->campMs = rand_int(world, 17000, 29000);
        }
        if (b->toCamp || camping(c)) return;
        float x, y;
        pos(p, &x, &y);
        const FORect *fr = fo_tmx_area(&world->map, p->team == FO_TEAM_RED ? FO_AREA_FRIDGE_BLU : FO_AREA_FRIDGE_RED);
        int right = (p->team == FO_TEAM_RED || p->team == FO_TEAM_BLU) && fr ? fr->x > x : 0;
        const FOGraph *g = &c->bots->graph;
        int steps = rand_int(world, 1, 4), count = 0, next = -1;
        for (int i = 0; i < g->campCount; i++) {
            const FOGraphCamp *s = &g->camps[i];
            float from = next >= 0 ? g->points[fo_graph_find(g, g->camps[next].waypoint)].x : x;
            float sx = g->points[fo_graph_find(g, s->waypoint)].x;
            if ((right && sx > from) || (!right && sx < from)) {
                count++;
                next = i;
            }
            if (count >= steps) break;
        }
        if (next < 0) {
            int back = !right, need = rand_int(world, 2, 3), n = 0;
            for (int i = 0; i < g->campCount && n < need; i++) {
                const FOGraphCamp *s = &g->camps[i];
                float from = next >= 0 ? g->points[fo_graph_find(g, g->camps[next].waypoint)].x : x;
                float sx = g->points[fo_graph_find(g, s->waypoint)].x;
                if ((back && sx > from) || (!back && sx < from)) {
                    next = i;
                    n++;
                }
            }
        }
        if (next < 0) return;
        nav_goto(c, fo_graph_find(g, g->camps[next].waypoint));
        b->toCamp = 1;
        b->campStart = 0;
        b->campRight = p->team == FO_TEAM_RED ? g->camps[next].c : g->camps[next].d;
        return;
    }
    case FO_CLASS_MEDIC: {
        // MedicBotBrain.b() (zs.java:73-123).
        if (world->now - b->followTime < b->followMs) return;
        FOPlayer *t = player_at(world, b->follow);
        if (t && (!t->alive || fo_player_healer(world, t) != p->id || hidden_spy(t))) b->follow = -1;
        float x, y;
        pos(p, &x, &y);
        int here = fo_graph_closest(&c->bots->graph, x, y);
        for (int i = 0; i < FO_MAX_PLAYERS; i++) {
            FOPlayer *q = &world->players[i];
            if (!b->players[i].used || !q->used || q == p) continue;
            if (q->team != p->team || !q->alive || fo_player_healer(world, q) != 0 || q->cls == FO_CLASS_SNIPER) continue;
            float qx, qy;
            pos(q, &qx, &qy);
            if (b->follow < 0) {
                b->follow = i;
                b->followLength = nav_length(c, here, fo_graph_closest(&c->bots->graph, qx, qy));
            } else if (q->health < world->players[b->follow].health) {
                float len = nav_length(c, here, fo_graph_closest(&c->bots->graph, qx, qy));
                if (b->followLength > len) {
                    b->follow = i;
                    b->followLength = len;
                }
            }
        }
        b->followTime = world->now;
        t = player_at(world, b->follow);
        if (t) {
            float ox, oy, tx, ty;
            objective(world, p, &ox, &oy);
            pos(t, &tx, &ty);
            if (sqrtf(dist2(x, y, ox, oy)) < 15.0f && wants_objective(c)) {
                go_objective(c);
            } else if (sqrtf(dist2(x, y, tx, ty)) > 1.0f) {
                float gx = tx > x ? tx - rand_float(world, 0.0f, 10.0f) * 0.1f : tx + rand_float(world, 0.0f, 10.0f) * 0.1f;
                nav_point(c, gx, ty);
            }
            b->followMs = rand_int(world, 300, 600);
            return;
        }
        if (wants_objective(c)) go_objective(c);
        else nav_goto(c, random_waypoint(c->bots, world, p->team));
        b->followMs = rand_int(world, 700, 2300);
        return;
    }
    default:
        break;
    }
    if (nav_done(c) || world->now - b->goalTime >= b->goalMs) {
        if (nav_done(c)) {
            float x, y;
            pos(p, &x, &y);
            if (wants_objective(c)) {
                if (!in_objective(world, p, x, y)) go_objective(c);
            } else {
                nav_goto(c, random_waypoint(c->bots, world, p->team));
            }
        }
        b->goalTime = world->now;
    }
}

// BotBrain.c() (zk.java:52-67): the bot stops when an enemy that it can
// shoot stands between it and the next waypoint.
static void brain_stop_check(FOBotContext *c) {
    FOBot *b = c->bot;
    FOWorld *world = c->world;
    int cls = c->p->cls;
    if (cls == FO_CLASS_SNIPER || cls == FO_CLASS_SPY) return;
    if (cls == FO_CLASS_ENGINEER && world->now - b->buildTime < b->buildMs) return;
    if (world->now - b->stopTime < b->stopMs) return;
    b->stopTime = world->now;
    b->stop = 0;
    if (b->target >= 0) {
        const FOBotTarget *t = b->target < FO_MAX_PLAYERS ? &b->players[b->target] : &b->sentries[b->target - FO_MAX_PLAYERS];
        if (!t->shootable) {
            if (b->pathCount == 0) return;
            const FOWaypoint *w = &c->bots->graph.points[b->path[0]];
            float x, y;
            pos(c->p, &x, &y);
            FORayHit r = {0, c};
            if (x != w->x || y != w->y) fo_phys_ray(world->phys, x, y, w->x, w->y, ray_enemy, &r);
            b->stop = r.hit;
        }
    }
    b->paused = b->stop;
}

// BotBrain.e() (zk.java:76-81): with no ammo, the bot goes home to the
// fridge.
static void brain_ammo(FOBotContext *c) {
    int cls = c->p->cls;
    if (cls == FO_CLASS_MEDIC) return;
    if (cls == FO_CLASS_SPY && (c->p->disguise.active || c->p->cloaked)) return;
    int home = spawn_waypoint(c->bots, c->p->team);
    float ammo = c->p->weapon.def ? c->p->weapon.ammo : 0;
    if (ammo <= 0.0f && nav_goal_id(c) != home) nav_goto(c, fo_graph_find(&c->bots->graph, home));
}

// BotBrain.f() (zk.java:83-102): sometimes the bot walks to an enemy
// sentry that it saw.
static void brain_hunt(FOBotContext *c) {
    FOBot *b = c->bot;
    FOWorld *world = c->world;
    if (world->now - b->huntTime <= b->huntMs) return;
    int any = 0;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) any |= b->sentries[i].used;
    if (any) {
        float chance = c->p->cls == FO_CLASS_SPY ? 100.0f : b->huntChance;
        if ((float)rand_int(world, 0, 100) < chance) {
            for (int i = 0; i < FO_MAX_PLAYERS; i++) {
                if (!b->sentries[i].used) continue;
                const FOSentry *s = &world->players[i].sentry;
                if (!s->active || !s->obj.body) {
                    b->sentries[i].used = 0;
                    continue;
                }
                float sx, sy;
                fo_phys_body_position(s->obj.body, &sx, &sy);
                nav_point(c, sx, sy);
                break;
            }
        }
    }
    b->huntTime = world->now;
    b->huntMs = rand_int(world, 2000, 4000);
}

// The first goal after a spawn: BotBrain.a() and the subclasses.
static void brain_first(FOBotContext *c) {
    FOBot *b = c->bot;
    FOPlayer *p = c->p;
    FOWorld *world = c->world;
    const FOGraph *g = &c->bots->graph;
    switch (p->cls) {
    case FO_CLASS_ENGINEER:
        if (p->sentry.active && p->sentry.obj.body) {
            float sx, sy;
            fo_phys_body_position(p->sentry.obj.body, &sx, &sy);
            nav_point(c, sx, sy);
        } else {
            nav_goto(c, random_waypoint(c->bots, world, p->team));
        }
        break;
    case FO_CLASS_MEDIC:
        if (wants_objective(c)) go_objective(c);
        else nav_goto(c, random_waypoint(c->bots, world, p->team));
        break;
    case FO_CLASS_SNIPER: {
        if (g->campCount == 0) break;
        int pick = -1;
        for (int tries = 0; tries < 10000 && pick < 0; tries++) {
            int i = rand_int(world, 0, g->campCount - 1);
            if (g->camps[i].team == p->team) pick = i;
        }
        if (pick < 0) break;
        b->campRight = p->team == FO_TEAM_RED ? g->camps[pick].c : g->camps[pick].d;
        nav_goto(c, fo_graph_find(g, g->camps[pick].waypoint));
        b->toCamp = 1;
        b->campStart = 0;
        b->campMs = 0;
        break;
    }
    case FO_CLASS_SPY:
        nav_goto(c, random_waypoint_out(c->bots, world, p));
        break;
    default:
        break;
    }
}

static int touches_enemy(const FOTouchSet *s, short team) {
    for (int i = 0; i < s->count; i++) {
        if (s->objs[i]->kind == FO_OBJ_PLAYER && ((const FOPlayer *)s->objs[i])->team != team) return 1;
    }
    return 0;
}

// SpyBotBrain.a(Vector2) (zx.java:142-152): the height is near.
static int spy_height(float y, float ty) {
    if (y == ty) return 1;
    if (y > ty && y - ty < 1.5f) return 1;
    return y < ty && ty - y < 1.8000001f;
}

// EngineerBotBrain.u() (zp.java:78-88): the enemies come from the right.
static int enemy_from_right(FOBotContext *c) {
    float x, y;
    pos(c->p, &x, &y);
    const FOGraph *g = &c->bots->graph;
    int from = fo_graph_find(g, spawn_waypoint(c->bots, (short)enemy_team(c->p->team)));
    int path[FO_BOT_PATH];
    int n = nav_path(c, from, fo_graph_closest(g, x, y), 1, 0, 0, path);
    if (n <= 0) return 0;
    int at = n >= 2 ? path[n - 2] : path[n - 1];
    return g->points[at].x > x;
}

// PlayerTouchingSensorData.d() and e() (aks.java): the foot stands on a
// slope, or near the edge of the object under it.
static int on_slope(const FOPlayer *p) {
    if (p->foot.ground <= 0) return 0;
    for (int i = 0; i < p->foot.count; i++) {
        const FOObj *o = p->foot.objs[i];
        if (o->body && fo_angle_norm(fo_phys_body_angle(o->body)) != 0.0f) return 1;
    }
    return 0;
}

static int near_edge(const FOPlayer *p) {
    if (p->foot.ground <= 0) return 0;
    float x, y;
    pos(p, &x, &y);
    for (int i = 0; i < p->foot.count; i++) {
        const FOObj *o = p->foot.objs[i];
        if (!o->body) continue;
        float ox, oy;
        fo_phys_body_position(o->body, &ox, &oy);
        // The bytecode of aks.e(): JADX drops the "go on" of this loop.
        if (x - 0.45000002f < ox) return 1;
        if (x + 0.45000002f > o->w + ox) return 1;
    }
    return 0;
}

// The extra update of each subclass, after BotBrain.a(float).
static void brain_extra(FOBotContext *c) {
    FOBot *b = c->bot;
    FOPlayer *p = c->p;
    FOWorld *world = c->world;
    float x, y;
    pos(p, &x, &y);
    if (p->cls == FO_CLASS_ENGINEER) {
        // EngineerBotBrain.a(float) (zp.java:25-37).
        if (world->now - b->buildTime >= b->buildMs) b->paused = 0;
        int has = p->sentry.active && p->sentry.obj.body;
        if (p->steel < 100.0f || !fo_player_grounded(p) || on_slope(p)) return;
        float rx, ry;
        if (has) {
            fo_phys_body_position(p->sentry.obj.body, &rx, &ry);
        } else {
            int home = fo_graph_find(&c->bots->graph, spawn_waypoint(c->bots, p->team));
            if (home < 0) return;
            rx = c->bots->graph.points[home].x;
        }
        int ahead = (!b->enemyRight || x >= rx) && (b->enemyRight || x <= rx);
        if (!ahead || fabsf(rx - x) < (has ? b->moveFar : b->buildFar) || near_edge(p)) return;
        if (has) {
            fo_player_ability(world, p);
            return;
        }
        int right = enemy_from_right(c);
        if (right && !p->facingRight) nav_face(p, 1);
        else if (!right && p->facingRight) nav_face(p, 0);
        fo_player_ability(world, p);
        b->buildFar = rand_float(world, 20.0f, 35.0f);
        b->moveFar = rand_float(world, 15.0f, 24.0f);
        b->buildTime = world->now;
        b->buildMs = rand_int(world, 3000, 6000);
        b->paused = 1;
    } else if (p->cls == FO_CLASS_MEDIC) {
        // MedicBotBrain.a(float) (zs.java:30-57).
        if (!p->alive) {
            b->follow = -1;
            return;
        }
        if (world->now - b->healTime >= b->healMs) {
            int best = -1;
            for (int i = 0; i < FO_MAX_PLAYERS; i++) {
                FOPlayer *q = &world->players[i];
                if (!b->players[i].used || !b->players[i].seen || !q->used || q == p) continue;
                if (q->team != p->team || hidden_spy(q)) continue;
                float qx, qy;
                pos(q, &qx, &qy);
                if (sqrtf(dist2(qx, qy, x, y)) > 6.0f || wall_between(world, x, y, qx, qy)) continue;
                if (best < 0 || world->players[best].health > q->health) best = i;
            }
            if (best >= 0) {
                fo_player_heal(world, p, world->players[best].id);
                if (p->uberCharge >= 20.0f) fo_player_ability(world, p);
            }
            b->healTime = world->now;
            b->healMs = rand_int(world, 300, 700);
        }
        if (p->move != FO_MOVE_STOP) fo_player_set_aim(p, 0);
    } else if (p->cls == FO_CLASS_SPY) {
        // SpyBotBrain.a(float) (zx.java:28-83).
        if (!p->alive) return;
        if (world->now - b->lookTime > b->lookMs) {
            int blocked = 0;
            if (b->pathCount > 0) {
                const FOWaypoint *w = &c->bots->graph.points[b->path[0]];
                FORayHit r = {0, c};
                if (x != w->x || y != w->y) fo_phys_ray(world->phys, x, y, w->x, w->y, ray_spy, &r);
                blocked = r.hit;
            }
            if (blocked) {
                b->paused = 1;
                if (b->waitStart == 0) {
                    b->waitStart = world->now;
                    b->waitMs2 = rand_int(world, 1500, 3000);
                }
                if (b->waitStart != 0 && world->now - b->waitStart >= b->waitMs2) {
                    nav_goto(c, random_waypoint_out(c->bots, world, p));
                }
            } else {
                b->paused = 0;
                b->waitStart = 0;
            }
            b->lookTime = world->now;
            b->lookMs = rand_int(world, 30, 150);
        }
        if (!p->backstab && fo_player_grounded(p)) {
            int stab = p->move == FO_MOVE_RIGHT ? touches_enemy(&p->right, p->team) : touches_enemy(&p->left, p->team);
            if (p->disguise.active && !p->cloaked) {
                for (int i = 0; i < FO_MAX_PLAYERS && !stab; i++) {
                    FOPlayer *q = &world->players[i];
                    if (!b->players[i].used || !b->players[i].seen || !q->used || q->team == p->team) continue;
                    float qx, qy;
                    pos(q, &qx, &qy);
                    if (sqrtf(dist2(qx, qy, x, y)) <= 2.0f && spy_height(y, qy)) {
                        stab = 1;
                    } else if (q->sentry.active && q->sentry.obj.body) {
                        float sx, sy;
                        fo_phys_body_position(q->sentry.obj.body, &sx, &sy);
                        if (sqrtf(dist2(sx, sy, x, y)) <= 1.5f && spy_height(y, sy)) stab = 1;
                    }
                }
            }
            if (stab) fo_player_backstab(world, p);
        }
        if (no_ammo(p) && !p->cloaked && p->cloakEnergy >= 30.0f) fo_player_set_cloak(world, p, 1);
        if (!p->disguise.active) {
            int sees = 0;
            for (int i = 0; i < FO_MAX_PLAYERS; i++) {
                if (b->players[i].used && b->players[i].seen && world->players[i].used &&
                    world->players[i].team != p->team) {
                    sees = 1;
                }
            }
            if (!sees) fo_player_disguise(world, p, 0);
        }
    }
}

// BotBrain.a(float) (zk.java:27-32) and the subclasses.
static void brain_update(FOBotContext *c) {
    brain_goal(c);
    brain_ammo(c);
    brain_hunt(c);
    brain_stop_check(c);
    brain_extra(c);
}

// --- The aimer (zi.java) --------------------------------------------------

// The view of the bot: an OrthographicCamera of 22.1875 m by 12.5 m on the
// bot, inside the map (zi.java:31, wp.java:20, wp.java:61-65).
static int bot_sees(const FOWorld *world, float bx, float by, float x, float y, float w, float h) {
    float hw = 22.1875f / 2, hh = 22.1875f / (1136.0f / 640.0f) / 2;
    float cx = bx, cy = by;
    if (cx < hw) cx = hw;
    if (cx > world->map.width - hw) cx = world->map.width - hw;
    if (cy < hh) cy = hh;
    if (cy > world->map.height - hh) cy = world->map.height - hh;
    return fabsf(x - cx) <= hw + w / 2 && fabsf(y - cy) <= hh + h / 2;
}

// BotAimer.a(Vector2) (zi.java:192-208): the bot can hit the point.
static int shootable(const FOBotContext *c, float tx, float ty) {
    const FOWeaponDef *def = c->p->weapon.def;
    if (!def) return 0;
    float x, y;
    pos(c->p, &x, &y);
    if (sqrtf(dist2(x, y, tx, ty)) >= def->speed * (float)def->life / 1000.0f + 1.8000001f) return 0;
    float dx = tx - x, dy = ty - y;
    float g = 15.0f * def->gravity;
    float v = def->speed;
    float a = v * v - dy * g;
    if (a * a - (dy * dy + dx * dx) * (g * g) < 0.0f) return 0;
    return !wall_between(c->world, x, y, tx, ty);
}

// BotAimer.b(Vector2) (zi.java:210-229): the angle of the low arc.
static float aim_angle(const FOBotContext *c, float tx, float ty) {
    const FOWeaponDef *def = c->p->weapon.def;
    float x, y;
    pos(c->p, &x, &y);
    float q = def ? def->gravity : 0;
    if (q < 0.01f) return atan2f(ty - y, tx - x);
    float dx = tx - x, dy = ty - y;
    float g = q * 15.0f;
    float v = def->speed;
    float a = v * v - dy * g;
    float disc = a * a - g * g * (dx * dx + dy * dy);
    if (disc < 0.0f) return -1.0f;
    float root = sqrtf(disc);
    float t = (float)(sqrt((a - root) * 2.0f) / fabs(g));
    return (float)atan2((g * t) / 2.0f + dy / t, dx / t);
}

// BotAimer.a(float, float, float): a turn from f to f2 by the part f3, in
// [0, 2 pi).
static float turn_angle(float f, float f2, float f3) {
    return fmodf((fmodf((f2 - f) + 6.2831855f + 3.1415927f, 6.2831855f) - 3.1415927f) * f3 + f + 6.2831855f, 6.2831855f);
}

// alf.b(float): the angle in [0, 2 pi).
static float angle_positive(float f) {
    float r = fmodf(f, 6.2831855f);
    return r < 0 ? r + 6.2831855f : r;
}

static void aim_update(FOBotContext *c) {
    FOBot *b = c->bot;
    FOPlayer *p = c->p;
    FOWorld *world = c->world;
    float x, y;
    pos(p, &x, &y);
    if (world->now - b->scanTime >= b->rescanMs) {
        for (int i = 0; i < FO_MAX_PLAYERS; i++) {
            FOPlayer *q = &world->players[i];
            if (!q->used || q->id == p->id) continue;
            FOBotTarget *t = &b->players[i];
            t->used = 1;
            if (q->sentry.active && q->sentry.obj.body && q->team != p->team) {
                FOBotTarget *s = &b->sentries[i];
                float sx, sy;
                fo_phys_body_position(q->sentry.obj.body, &sx, &sy);
                s->used = 1;
                s->seen = bot_sees(world, x, y, sx, sy, 1.1f, 1.4f);
                s->dist2 = dist2(sx, sy, x, y);
                s->shootable = shootable(c, sx, sy);
            }
            float qx, qy;
            pos(q, &qx, &qy);
            if (!q->alive) {
                t->seen = 0;
            } else if (!bot_sees(world, x, y, qx, qy, 0.90000004f, 1.9f)) {
                t->seen = 0;
            } else if (q->cls == FO_CLASS_SPY && q->cloaked) {
                t->seen = 0;
            } else if (q->cls == FO_CLASS_SPY && q->disguise.active) {
                // A disguised agent shows only when it touches the bot.
                int touch = 0;
                for (int k = 0; k < p->left.count; k++) touch |= p->left.objs[k] == &q->obj;
                for (int k = 0; k < p->right.count; k++) touch |= p->right.objs[k] == &q->obj;
                if (touch) {
                    t->seen = 1;
                    t->dist2 = dist2(qx, qy, x, y);
                    if (q->team != p->team) t->shootable = shootable(c, qx, qy);
                }
            } else {
                t->seen = 1;
                t->dist2 = dist2(qx, qy, x, y);
                if (q->team != p->team) t->shootable = shootable(c, qx, qy);
            }
        }
        for (int i = 0; i < FO_MAX_PLAYERS; i++) {
            if (b->sentries[i].used && !(world->players[i].used && world->players[i].sentry.active)) b->sentries[i].used = 0;
        }
        b->scanTime = world->now;
    }
    b->target = -1;
    if (brain_aims(c)) {
        int best = -1;
        float bestD = 0, targetD = 0;
        for (int i = 0; i < FO_MAX_PLAYERS; i++) {
            const FOBotTarget *t = &b->players[i];
            const FOPlayer *q = &world->players[i];
            if (!t->used || !q->used || q->team == p->team || !q->alive || !t->seen || !brain_target_ok(c, i, t)) continue;
            if (!(best >= 0 && bestD <= t->dist2)) {
                best = i;
                bestD = t->dist2;
            }
            if (t->shootable && (b->target < 0 || targetD > t->dist2)) {
                b->target = i;
                targetD = t->dist2;
            }
        }
        if (b->target < 0) {
            for (int i = 0; i < FO_MAX_PLAYERS; i++) {
                const FOBotTarget *t = &b->sentries[i];
                const FOSentry *s = &world->players[i].sentry;
                if (!t->used || !s->active || !s->obj.body || !t->seen) continue;
                if (!brain_target_ok(c, FO_MAX_PLAYERS + i, t)) continue;
                if (best < 0 || bestD > t->dist2) {
                    best = FO_MAX_PLAYERS + i;
                    bestD = t->dist2;
                }
                if (t->shootable && (b->target < 0 || targetD > t->dist2)) {
                    b->target = FO_MAX_PLAYERS + i;
                    targetD = t->dist2;
                }
            }
        }
        if (b->target < 0) b->target = best;
    }
    float part = b->turn;
    if (b->target >= 0 && !no_ammo(p)) {
        float tx, ty;
        target_position(world, b->target, &tx, &ty);
        b->goal = aim_angle(c, tx, ty);
    } else if (brain_idles(c)) {
        int right = p->move == FO_MOVE_RIGHT ? 1 : p->move == FO_MOVE_LEFT ? 0 : brain_faces_right(c);
        int roll = world->now - b->idleTime >= 1000;
        float deg = angle_positive(b->goal) * 57.295776f;
        int facesRight = deg <= 90.0f || deg >= 270.0f;
        int z = 1;
        if (right && !facesRight) {
            fo_player_set_aim(p, 360.0f - (deg + 180.0f));
        } else if (right || !facesRight) {
            z = roll;
        } else {
            fo_player_set_aim(p, 360.0f - (deg + 180.0f));
        }
        if (z) {
            b->idle = right ? (float)rand_int(world, -45, 45) * 0.017453292f : (float)rand_int(world, 135, 225) * 0.017453292f;
            b->idleTime = world->now;
        }
        b->goal = b->idle;
        part = b->turn / 20.0f;
    } else {
        b->goal = 0;
    }
    float now = fo_angle_norm(turn_angle(p->weapon.angle * 0.017453292f, b->goal, part));
    if (fabsf(now - b->goal) < 0.05f) now = b->goal;
    fo_player_set_aim(p, 57.295776f * now);
    if (b->target >= 0) {
        const FOBotTarget *t = b->target < FO_MAX_PLAYERS ? &b->players[b->target] : &b->sentries[b->target - FO_MAX_PLAYERS];
        if (t->shootable && fabsf(now - b->goal) < 0.34906584f && p->weapon.ammo > 0.0f && brain_fires(c) &&
            fo_player_can_shoot(world, p)) {
            fo_player_shoot(world, p, now);
            // The aimer also takes one ammo, so a bot uses 2 for each shot
            // (zi.java:171).
            p->weapon.ammo -= 1.0f;
        }
    }
}

// BotAimer.e() (zi.java:265-278): forget the players and the sentries
// that are gone.
static void aim_prune(FOBotContext *c) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (!c->world->players[i].used) {
            c->bot->players[i].used = 0;
            c->bot->sentries[i].used = 0;
        } else if (!c->world->players[i].sentry.active) {
            c->bot->sentries[i].used = 0;
        }
    }
}

// --- The controller (zm.java) and the server ------------------------------

void fo_world_change_class(FOWorld *world, FOPlayer *p, int cls) {
    if (cls < 1 || cls > 9 || p->cls == cls) return;
    // The class limit of single player is 420, so it never stops a change.
    if (p->alive) {
        if (fo_world_in_own_spawn(world, p)) {
            fo_world_spawn(world, p, cls);
        } else {
            fo_player_die(world, p);
        }
    } else if (!fo_player_waiting(p)) {
        fo_world_spawn(world, p, cls);
    }
    p->cls = cls;
}

void fo_world_change_team(FOWorld *world, FOPlayer *p, short team) {
    if (p->team == team) return;
    // The team difference limit of single player is 420, so it never stops
    // a change.
    fo_world_set_team(world, p, team);
    fo_classes_reset(world, p, 0);
    if (team != FO_TEAM_NONE && p->alive) fo_player_die(world, p);
}

// BotController.x() (zm.java:84-116): a class of the smallest number in
// the team.
static void choose_class(FOBots *bots, FOWorld *world, FOBot *bot, FOPlayer *p) {
    int count[10] = {0};
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        const FOPlayer *q = &world->players[i];
        if (q->used && q->cls != 0 && q->team == p->team) count[q->cls]++;
    }
    int least = bots->maxPlayers;
    for (int k = 1; k <= 9; k++) {
        if (least > count[k]) least = count[k];
    }
    int choices[9], n = 0;
    for (int k = 1; k <= 9; k++) {
        if (count[k] == least) choices[n++] = k;
    }
    if (n > 0) fo_world_change_class(world, p, choices[rand_int(world, 0, n - 1)]);
    bot->nextClass = world->now + rand_int(world, 120000, 300000);
}

// BotController.a(float) (zm.java:39-82).
static void bot_update(FOBots *bots, FOWorld *world, FOBot *bot, float dt) {
    (void)dt;
    FOPlayer *p = bot_player(world, bot);
    if (!p) return;
    FOBotContext c = {bots, world, bot, p};
    aim_prune(&c);
    if (!p->alive || fo_mode_over(world)) {
        // The controller returns before the navigator, so Player.h(float)
        // does not run, and the friction of the ground stops the bot.
        p->noMove = 1;
        bot->spawnWait = 0;
        bot->started = 0;
        nav_clear(bot);
        return;
    }
    brain_check(&c);
    if (!bot->started) {
        if (bot->spawnWait == 0) {
            bot->spawnWait = world->now;
            bot->waitMs = rand_int(world, 1000, 1700);
        }
        if (world->now >= bot->nextClass) {
            choose_class(bots, world, bot, p);
            if (!p->alive) return;
            brain_check(&c);
        }
        if (world->now - bot->spawnWait >= bot->waitMs) {
            brain_first(&c);
            bot->started = 1;
        }
    }
    if (world->now - bot->spawnWait >= bot->waitMs) brain_update(&c);
    if (!p->used || !p->alive) return;
    aim_update(&c);
    nav_update(&c);
}

void fo_bots_init(FOBots *bots, FOWorld *world, int maxPlayers, int localId) {
    memset(bots, 0, sizeof(*bots));
    bots->maxPlayers = maxPlayers;
    bots->localId = localId;
    // The graph and the spawn waypoints of each map: BOMB_Canyon
    // (acj.java:233-242), FLAG_Eypt (aco.java:255-261), and GTS_Gantown
    // (acp.java:333-342).
    switch (world->mode.kind) {
    case FO_MODE_BOMB: fo_graph_build(&bots->graph, &fo_graph_canyon, 1, 25); break;
    case FO_MODE_CTF: fo_graph_build(&bots->graph, &fo_graph_egypt, 56, 1); break;
    case FO_MODE_KOTH: fo_graph_build(&bots->graph, &fo_graph_gantown, 78, 79); break;
    default: break;
    }
}

// The names of the bots of SingleplayerGameController (xa.java:17), with
// the slurs, the jokes about real people, and the hate removed. See
// docs/decisions.md.
static const char *const kNames[] = {
    "gintrux69", "morlicando", "weedska", "bastys", "sagdiyev", "marek", "homie", "bro", "john", "westside", "west",
    "wester", "bruh", "jeff", "borat", "ali g", "stalker", "fartacus", "ali", "lasha", "kalisha", "tyrone", "sawshuh",
    "shebooboo", "deandre", "jordan", "hakeem", "jamal", "tyrone", "jayrone", "byrone", "antuon", "tyrell", "deshawn",
    "demarcus", "xavier", "jake", "connor", "darnell", "trevon", "donald", "america", "us and a", "lukas", "robke",
    "markas", "camaro", "mustang", "snow man", "fat daddy", "chicken", "bookie", "big man", "fats", "poo poo", "spiffy",
    "bull head", "alex", "ice bird", "ice block", "fake cube", "faker", "husky", "tookie", "tango", "big o", "swag kid",
    "lil t", "big t", "tom", "swagger", "triple og", "jackson", "scammy", "scammer", "snoop", "snoopy", "dawg",
    "snoup dawg", "lobez", "shadawg", "piedawg", "big shug", "g rizzle", "muffin", "lightweit", "bench bro",
    "bench dawg", "deadlifter", "bench 225", "weeeed", "weedsmoker", "bix", "da exit", "rock", "puppie", "jacky",
    "the sore", "big g", "butcher", "big tuna", "bear", "wiper", "swiper", "lunatic", "turtle", "lietuva", "vycka",
    "gusto", "dimitry", "che che", "tee tee", "pumper", "kostic", "zombie", "compton", "gudaitis", "sacky", "kynas",
    "gytis", "protein", "pumpndump"};

static int name_used(const FOWorld *world, const char *name) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (world->players[i].used && strcmp(world->players[i].name, name) == 0) return 1;
    }
    return 0;
}

FOBot *fo_bots_add(FOBots *bots, FOWorld *world, int id, const char *name) {
    FOBot *bot = NULL;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (!bots->bots[i].used) {
            bot = &bots->bots[i];
            break;
        }
    }
    if (!bot) return NULL;
    // BotController.y() (zm.java:118-130): BLU when RED has more players.
    int red = 0, blu = 0;
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (!world->players[i].used) continue;
        if (world->players[i].team == FO_TEAM_RED) red++;
        else if (world->players[i].team == FO_TEAM_BLU) blu++;
    }
    FOPlayer *p = fo_world_add_player(world, id, red > blu ? FO_TEAM_BLU : FO_TEAM_RED);
    if (!p) return NULL;
    memset(bot, 0, sizeof(*bot));
    bot->used = 1;
    bot->id = id;
    bot->link = -1;
    bot->retry = -1;
    bot->target = -1;
    bot->follow = -1;
    if (name) snprintf(p->name, sizeof(p->name), "%s", name);
    FOBotContext c = {bots, world, bot, p};
    choose_class(bots, world, bot, p);
    bot->turn = rand_float(world, 0.04f, 0.5f);
    bot->rescanMs = rand_int(world, 300, 700);
    brain_check(&c);
    return bot;
}

void fo_bots_fill(FOBots *bots, FOWorld *world) {
    const FOPlayer *local = fo_world_player(world, bots->localId);
    int mates = 0;
    for (;;) {
        int count = 0;
        for (int i = 0; i < FO_MAX_PLAYERS; i++) count += world->players[i].used;
        if (count >= bots->maxPlayers || count >= FO_MAX_PLAYERS) break;
        int id = count + 69;
        while (fo_world_player(world, id)) id++;
        FOBot *bot = fo_bots_add(bots, world, id, NULL);
        if (!bot) break;
        FOPlayer *p = bot_player(world, bot);
        int n = (int)(sizeof(kNames) / sizeof(kNames[0]));
        const char *name = kNames[rand_int(world, 0, n - 1)];
        // The first three bots of the team of the human get the first
        // three names (xa.java:151-165).
        if (local && p->team == local->team) {
            mates++;
            if (mates <= 3) name = kNames[mates - 1];
        }
        while (name_used(world, name)) name = kNames[rand_int(world, 0, n - 1)];
        snprintf(p->name, sizeof(p->name), "%s", name);
        if (!p->alive) fo_world_spawn(world, p, p->cls);
    }
}

void fo_bots_update(FOBots *bots, FOWorld *world, float dt) {
    for (int i = 0; i < FO_MAX_PLAYERS; i++) {
        if (bots->bots[i].used) bot_update(bots, world, &bots->bots[i], dt);
    }
}
