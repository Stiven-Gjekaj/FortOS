// fo_graph: the waypoint graph of the bots, as GameMapBotData (zq.java),
// Waypoint (aaa.java), WaypointLink (aab.java), and the A* search of
// PathFinder (zu.java).

#ifndef FO_GRAPH_H
#define FO_GRAPH_H

#include "fo_world.h"

// WaypointLinkType (aac.java).
typedef enum {
    FO_LINK_WALK = 0,
    FO_LINK_JUMP,
    FO_LINK_JUMP_WAIT100,
    FO_LINK_JUMP_WAIT300,
    FO_LINK_DOUBLE1000,
    FO_LINK_DOUBLE500,
    FO_LINK_MIDAIR700,
} FOLinkType;

// The data of a graph, as the calls of GameMapBotData (zq.java).
typedef struct {
    int id;
    float x, y;
} FOGraphPoint;

typedef struct {
    int from, to;
    int ab;   // The type from "from" to "to".
    int ba;   // The type back, or -1 for a one way link.
} FOGraphLink;

typedef struct {
    int waypoint;
    int team;
    int c, d; // SniperCampSpot.c and d: the facing of RED and of BLU.
} FOGraphCamp;

typedef struct {
    const FOGraphPoint *points;
    int pointCount;
    const FOGraphLink *links;
    int linkCount;
    const FOGraphCamp *camps;
    int campCount;
} FOGraphData;

// The graphs of fo_graphs.c.
extern const FOGraphData fo_graph_canyon;
extern const FOGraphData fo_graph_egypt;
extern const FOGraphData fo_graph_gantown;

#define FO_GRAPH_MAX 96
#define FO_GRAPH_IDS 128
#define FO_GRAPH_LINKS 12
#define FO_GRAPH_CAMPS 32

// WaypointLink (aab.java).
typedef struct {
    int to;          // The index of the target waypoint.
    FOLinkType type;
    float length;    // d: the squared distance.
} FOLink;

// Waypoint (aaa.java). d, e, f, and g are the fields of the search.
typedef struct {
    int id;
    float x, y;
    FOLink links[FO_GRAPH_LINKS];
    int linkCount;
    float d, e, f;
    int g;           // The index of the parent, or -1.
} FOWaypoint;

typedef struct {
    FOWaypoint points[FO_GRAPH_MAX];
    int count;
    int index[FO_GRAPH_IDS];   // The index of each id, or -1.
    int minId, maxId;
    FOGraphCamp camps[FO_GRAPH_CAMPS];
    int campCount;
    int spawn[3];              // c(team): the spawn waypoint id of each team.
} FOGraph;

// Builds the graph from its data. redSpawn and bluSpawn are the ids of
// the spawn waypoints (c(int) of each map class).
void fo_graph_build(FOGraph *graph, const FOGraphData *data, int redSpawn, int bluSpawn);

// The index of the waypoint id, or -1.
int fo_graph_find(const FOGraph *graph, int id);

// GameMapBotData.a(float, float): the index of the closest waypoint.
int fo_graph_closest(const FOGraph *graph, float x, float y);

// The link from the index "from" to the index "to", or NULL.
const FOLink *fo_graph_link(const FOGraph *graph, int from, int to);

// A filter of the links for the search. It returns 1 to keep the link
// from the waypoint index "from" over link.
typedef int (*FOLinkFilter)(void *context, int from, const FOLink *link);

// PathFinder.a(...) (zu.java:24-91): the A* search from the index start
// to the index goal. With exact 0, each step gets random noise. A failed
// search with the second filter runs again without it. Writes the
// indices of the path into out, and returns their number, or -1.
int fo_graph_path(FOGraph *graph, FOWorld *world, int start, int goal, int exact, FOLinkFilter filter1,
                  void *context1, FOLinkFilter filter2, void *context2, int *out, int max);

#endif
