// Tests of fo_graph: the waypoint graph and the A* search. Each test
// builds its own graph. The last test asks the graphs of the three maps.

#import "FOTest.h"
#include <stdio.h>
#include "fo_graph.h"

static FOWorld *newWorld(void) {
    NSString *map = @"<map width=\"1\" height=\"1\" tilewidth=\"1000\" tileheight=\"300\">\n"
                    @" <objectgroup name=\"collisions\">\n"
                    @"  <object id=\"1\" x=\"0\" y=\"280\" width=\"1000\" height=\"20\"/>\n"
                    @" </objectgroup>\n"
                    @"</map>\n";
    return fo_world_new(map.UTF8String, 3);
}

static int ids(const FOGraph *g, const int *path, int n, int *out) {
    for (int i = 0; i < n; i++) out[i] = g->points[path[i]].id;
    return n;
}

// --- The graph and the search ---------------------------------------------

// A square: 1 (0,0), 2 (10,0), 3 (10,10), 4 (0,10), and 5 (5, 1) near the
// side from 1 to 2. 1-5-2 is shorter in squared lengths than 1-2.
static const FOGraphPoint kSquarePoints[] = {{1, 0, 0}, {2, 10, 0}, {3, 10, 10}, {4, 0, 10}, {5, 5, 1}};
static const FOGraphLink kSquareLinks[] = {
    {1, 2, FO_LINK_WALK, FO_LINK_WALK}, {2, 3, FO_LINK_JUMP, FO_LINK_WALK}, {3, 4, FO_LINK_WALK, -1},
    {4, 1, FO_LINK_WALK, FO_LINK_WALK}, {1, 5, FO_LINK_WALK, FO_LINK_WALK}, {5, 2, FO_LINK_WALK, FO_LINK_WALK}};

static FOGraph squareGraph(void) {
    FOGraph g;
    FOGraphData d = {kSquarePoints, 5, kSquareLinks, 6, NULL, 0};
    fo_graph_build(&g, &d, 1, 3);
    return g;
}

FO_TEST(testGraphBuildsTheLinksBothWays) {
    FOGraph g = squareGraph();
    FO_EQUAL_INT(g.count, 5);
    FO_EQUAL_INT(g.minId, 1);
    FO_EQUAL_INT(g.maxId, 5);
    int a = fo_graph_find(&g, 1);
    // 1 links to 2, 4, and 5.
    FO_EQUAL_INT(g.points[a].linkCount, 3);
    // The one way link 3-4 has no way back.
    int four = fo_graph_find(&g, 4);
    for (int i = 0; i < g.points[four].linkCount; i++) {
        FO_EXPECT(g.points[g.points[four].links[i].to].id != 3);
    }
    // A link length is the squared distance.
    FO_EXPECT(g.points[a].links[0].length == 100.0f);
    FO_EQUAL_INT(fo_graph_find(&g, 99), -1);
}

FO_TEST(testGraphClosestWaypoint) {
    FOGraph g = squareGraph();
    FO_EQUAL_INT(g.points[fo_graph_closest(&g, 9, 9)].id, 3);
    FO_EQUAL_INT(g.points[fo_graph_closest(&g, 4.5f, 0.5f)].id, 5);
}

FO_TEST(testGraphExactPathUsesTheSquaredLengths) {
    FOWorld *world = newWorld();
    FOGraph g = squareGraph();
    int path[16], out[16];
    int n = fo_graph_path(&g, world, fo_graph_find(&g, 1), fo_graph_find(&g, 2), 1, NULL, NULL, NULL, NULL, path, 16);
    // 1-5-2 costs 26 + 26, and 1-2 costs 100.
    FO_EQUAL_INT(n, 3);
    ids(&g, path, n, out);
    FO_EQUAL_INT(out[0], 1);
    FO_EQUAL_INT(out[1], 5);
    FO_EQUAL_INT(out[2], 2);
    // A path to the start is the start alone.
    n = fo_graph_path(&g, world, fo_graph_find(&g, 4), fo_graph_find(&g, 4), 1, NULL, NULL, NULL, NULL, path, 16);
    FO_EQUAL_INT(n, 1);
    fo_world_free(world);
}

FO_TEST(testGraphOneWayLink) {
    FOWorld *world = newWorld();
    FOGraph g = squareGraph();
    int path[16], out[16];
    // 3 goes to 4 directly. 4 has no link to 3, so it goes round.
    int n = fo_graph_path(&g, world, fo_graph_find(&g, 3), fo_graph_find(&g, 4), 1, NULL, NULL, NULL, NULL, path, 16);
    FO_EQUAL_INT(n, 2);
    n = fo_graph_path(&g, world, fo_graph_find(&g, 4), fo_graph_find(&g, 3), 1, NULL, NULL, NULL, NULL, path, 16);
    ids(&g, path, n, out);
    FO_EXPECT(n >= 3);
    FO_EQUAL_INT(out[0], 4);
    FO_EQUAL_INT(out[n - 1], 3);
    fo_world_free(world);
}

static int noFive(void *context, int from, const FOLink *l) {
    (void)from;
    const FOGraph *g = context;
    return g->points[l->to].id != 5;
}

static int noLinks(void *context, int from, const FOLink *l) {
    (void)context;
    (void)from;
    (void)l;
    return 0;
}

FO_TEST(testGraphFiltersAndTheSecondTry) {
    FOWorld *world = newWorld();
    FOGraph g = squareGraph();
    int path[16], out[16];
    int n = fo_graph_path(&g, world, fo_graph_find(&g, 1), fo_graph_find(&g, 2), 1, noFive, &g, NULL, NULL, path, 16);
    ids(&g, path, n, out);
    FO_EQUAL_INT(n, 2);
    FO_EQUAL_INT(out[1], 2);
    // The second filter stops each link, so the search runs again without it.
    n = fo_graph_path(&g, world, fo_graph_find(&g, 1), fo_graph_find(&g, 2), 1, NULL, NULL, noLinks, NULL, path, 16);
    FO_EQUAL_INT(n, 3);
    // The first filter has no second try.
    n = fo_graph_path(&g, world, fo_graph_find(&g, 1), fo_graph_find(&g, 2), 1, noLinks, NULL, NULL, NULL, path, 16);
    FO_EQUAL_INT(n, -1);
    fo_world_free(world);
}

FO_TEST(testGraphNoisyPathsStillArrive) {
    FOWorld *world = newWorld();
    FOGraph g = squareGraph();
    int path[16];
    for (int i = 0; i < 50; i++) {
        int n = fo_graph_path(&g, world, fo_graph_find(&g, 4), fo_graph_find(&g, 2), 0, NULL, NULL, NULL, NULL, path, 16);
        FO_EXPECT(n >= 2);
        FO_EQUAL_INT(g.points[path[0]].id, 4);
        FO_EQUAL_INT(g.points[path[n - 1]].id, 2);
    }
    fo_world_free(world);
}

// --- The graphs of the maps -------------------------------------------------

static void expectConnected(const FOGraphData *data, int redSpawn, int bluSpawn) {
    FOWorld *world = newWorld();
    FOGraph g;
    fo_graph_build(&g, data, redSpawn, bluSpawn);
    FO_EQUAL_INT(g.count, data->pointCount);
    int path[FO_GRAPH_MAX];
    int spawns[2] = {redSpawn, bluSpawn};
    for (int s = 0; s < 2; s++) {
        int from = fo_graph_find(&g, spawns[s]);
        FO_EXPECT(from >= 0);
        for (int i = 0; i < g.count; i++) {
            int n = fo_graph_path(&g, world, from, i, 1, NULL, NULL, NULL, NULL, path, FO_GRAPH_MAX);
            if (n < 0) fprintf(stderr, "no path from %d to %d\n", spawns[s], g.points[i].id);
            FO_EXPECT(n > 0);
        }
    }
    fo_world_free(world);
}

FO_TEST(testEachWaypointOfTheMapsCanBeReachedFromEachSpawn) {
    expectConnected(&fo_graph_canyon, 1, 25);
    expectConnected(&fo_graph_egypt, 56, 1);
    expectConnected(&fo_graph_gantown, 78, 79);
}
