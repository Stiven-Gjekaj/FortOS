// fo_graph: the waypoint graph of the bots. See fo_graph.h.

#include "fo_graph.h"

#include <string.h>

// MathUtils.random(float, float).
static float rand_float(FOWorld *world, float lo, float hi) {
    return lo + fo_world_random(world) * (hi - lo);
}

static float dist2(float ax, float ay, float bx, float by) {
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
}

static void add_link(FOGraph *g, int from, int to, int type) {
    if (from < 0 || to < 0) return;
    FOWaypoint *a = &g->points[from];
    if (a->linkCount >= FO_GRAPH_LINKS) return;
    FOLink *l = &a->links[a->linkCount++];
    l->to = to;
    l->type = (FOLinkType)type;
    l->length = dist2(a->x, a->y, g->points[to].x, g->points[to].y);
}

void fo_graph_build(FOGraph *g, const FOGraphData *data, int redSpawn, int bluSpawn) {
    memset(g, 0, sizeof(*g));
    for (int i = 0; i < FO_GRAPH_IDS; i++) g->index[i] = -1;
    g->minId = 0x7fffffff;
    g->maxId = -0x7fffffff;
    for (int i = 0; i < data->pointCount && g->count < FO_GRAPH_MAX; i++) {
        const FOGraphPoint *p = &data->points[i];
        if (p->id < 0 || p->id >= FO_GRAPH_IDS) continue;
        FOWaypoint *w = &g->points[g->count];
        w->id = p->id;
        w->x = p->x;
        w->y = p->y;
        w->g = -1;
        g->index[p->id] = g->count++;
        if (p->id < g->minId) g->minId = p->id;
        if (p->id > g->maxId) g->maxId = p->id;
    }
    for (int i = 0; i < data->linkCount; i++) {
        const FOGraphLink *l = &data->links[i];
        int a = fo_graph_find(g, l->from), b = fo_graph_find(g, l->to);
        add_link(g, a, b, l->ab);
        if (l->ba >= 0) add_link(g, b, a, l->ba);
    }
    for (int i = 0; i < data->campCount && g->campCount < FO_GRAPH_CAMPS; i++) g->camps[g->campCount++] = data->camps[i];
    g->spawn[FO_TEAM_RED] = redSpawn;
    g->spawn[FO_TEAM_BLU] = bluSpawn;
}

int fo_graph_find(const FOGraph *g, int id) {
    return id >= 0 && id < FO_GRAPH_IDS ? g->index[id] : -1;
}

int fo_graph_closest(const FOGraph *g, float x, float y) {
    int best = -1;
    float bestD = 3.4028235e38f;
    for (int i = 0; i < g->count; i++) {
        float d = dist2(g->points[i].x, g->points[i].y, x, y);
        if (best < 0 || d < bestD) {
            best = i;
            bestD = d;
        }
    }
    return best;
}

int fo_graph_path(FOGraph *g, FOWorld *world, int start, int goal, int exact, FOLinkFilter filter1, void *context1,
                  FOLinkFilter filter2, void *context2, int *out, int max) {
    if (start < 0 || goal < 0) return -1;
    int open[FO_GRAPH_MAX], openCount = 0;
    unsigned char inOpen[FO_GRAPH_MAX], inClosed[FO_GRAPH_MAX];
    memset(inOpen, 0, sizeof(inOpen));
    memset(inClosed, 0, sizeof(inClosed));
    for (int i = 0; i < g->count; i++) {
        g->points[i].g = -1;
        g->points[i].d = g->points[i].e = g->points[i].f = 0;
    }
    open[openCount++] = start;
    inOpen[start] = 1;
    while (openCount > 0) {
        // The first waypoint with the smallest d.
        int at = 0;
        for (int i = 1; i < openCount; i++) {
            if (g->points[open[i]].d < g->points[open[at]].d) at = i;
        }
        int cur = open[at];
        if (g->points[cur].id == g->points[goal].id) {
            int chain[FO_GRAPH_MAX], n = 0;
            int node = cur;
            while (g->points[node].g >= 0 && n < FO_GRAPH_MAX) {
                chain[n++] = node;
                node = g->points[node].g;
            }
            int count = 0;
            if (count < max) out[count++] = start;
            for (int i = n - 1; i >= 0 && count < max; i--) out[count++] = chain[i];
            return count;
        }
        FOWaypoint *c = &g->points[cur];
        for (int k = 0; k < c->linkCount; k++) {
            const FOLink *l = &c->links[k];
            if (g->points[l->to].id == c->id) continue;
            if (filter1 && !filter1(context1, cur, l)) continue;
            if (filter2 && !filter2(context2, cur, l)) continue;
            int n = l->to;
            FOWaypoint *w = &g->points[n];
            float cost = c->e + l->length;
            if (!exact) cost += rand_float(world, 0.0f, rand_float(world, 0.2f, 0.7f) * cost);
            if (inOpen[n]) {
                // The bytecode of zu.java goes on when the old cost is not
                // bigger. JADX shows this part wrong.
                if (w->e - cost <= 0) continue;
            } else if (inClosed[n]) {
                if (w->e - cost <= 0) continue;
                inClosed[n] = 0;
                open[openCount++] = n;
                inOpen[n] = 1;
            } else {
                open[openCount++] = n;
                inOpen[n] = 1;
                w->f = dist2(w->x, w->y, g->points[goal].x, g->points[goal].y);
            }
            w->e = cost;
            w->d = w->e + w->f;
            w->g = cur;
        }
        memmove(&open[at], &open[at + 1], sizeof(int) * (size_t)(openCount - at - 1));
        openCount--;
        inOpen[cur] = 0;
        inClosed[cur] = 1;
    }
    if (filter2) return fo_graph_path(g, world, start, goal, exact, filter1, context1, NULL, NULL, out, max);
    return -1;
}

const FOLink *fo_graph_link(const FOGraph *g, int from, int to) {
    const FOWaypoint *w = &g->points[from];
    for (int i = 0; i < w->linkCount; i++) {
        if (w->links[i].to == to) return &w->links[i];
    }
    return NULL;
}
