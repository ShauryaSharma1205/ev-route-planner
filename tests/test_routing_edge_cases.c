/*
 * Edge-case tests for BatteryAwareDijkstra and CalculateRoute.
 *
 * Each test builds a tiny hand-made graph where the correct answer can be
 * worked out by hand, then checks the router against it.
 *
 * Battery used unless stated otherwise:
 *   capacity 20 kWh, reserve 2 kWh, consumption 0.30 kWh/km
 *   -> usable 18 kWh = 60 km per full charge.
 */

#include "../src/graph/graph.h"
#include "../src/battery/battery.h"
#include "../src/stations/stations.h"
#include "../src/routing/battery_routing.h"
#include "../src/routing/dijkstra.h"
#include "../src/routing/route_service.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        checks++;                                                     \
        if (!(cond)) {                                                \
            failures++;                                               \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, msg);    \
        }                                                             \
    } while (0)

#define NEAR(a, b) (fabs((a) - (b)) < 1e-6)

typedef struct {
    Graph graph;
    StationManager stations;
    BatterySpec spec;
    BatteryState state;
    BatteryRoute route;
} Fixture;

static void Setup(Fixture *f, double socPercent)
{
    memset(f, 0, sizeof(*f));
    GraphInit(&f->graph);
    StationManagerInit(&f->stations);
    BatteryInit(&f->spec, &f->state, 20.0, socPercent, 0.30, 2.0);
}

static void Teardown(Fixture *f)
{
    BatteryRouteFree(&f->route);
    StationManagerFree(&f->stations);
    GraphFree(&f->graph);
}

static int AddCity(Fixture *f, const char *name)
{
    return GraphAddVertex(&f->graph, name, 30.0, 78.0);
}

static bool Route(Fixture *f, int from, int to)
{
    return BatteryAwareDijkstra(&f->graph, &f->spec, &f->state,
                                &f->stations, from, to, &f->route);
}

static size_t CountStops(const BatteryRoute *r)
{
    size_t n = 0;
    for (size_t i = 0; i < r->stepCount; i++)
        if (r->steps[i].isChargingStop)
            n++;
    return n;
}

static bool NeverBelowReserve(const Fixture *f)
{
    for (size_t i = 0; i < f->route.stepCount; i++)
        if (f->route.steps[i].batteryAfterKWh < f->spec.reserveKWh - 1e-9)
            return false;
    return true;
}

/* ------------------------------------------------------------------ */

/* Direct trip inside the range: no charging, exact energy accounting. */
static void TestNoChargeNeeded(void)
{
    Fixture f;
    Setup(&f, 100.0);

    int a = AddCity(&f, "A");
    int b = AddCity(&f, "B");
    GraphAddRoad(&f.graph, a, b, 40.0, "A-B", true);

    CHECK(Route(&f, a, b), "40 km trip with 60 km range is feasible");
    CHECK(CountStops(&f.route) == 0, "no charging stop needed");
    CHECK(NEAR(f.route.totalDistanceKm, 40.0), "distance is 40 km");
    CHECK(NEAR(f.route.finalBatteryKWh, 20.0 - 40.0 * 0.30),
          "arrives with 8 kWh");
    CHECK(NEAR(f.route.totalTimeHours, 1.0), "40 km at 40 km/h = 1 h");

    Teardown(&f);
}

/* 150 km in three 50 km legs: must charge at both middle stations. */
static void TestTwoChargingStops(void)
{
    Fixture f;
    Setup(&f, 100.0);

    int a = AddCity(&f, "A");
    int s1 = AddCity(&f, "S1");
    int s2 = AddCity(&f, "S2");
    int b = AddCity(&f, "B");

    GraphAddRoad(&f.graph, a, s1, 50.0, "A-S1", true);
    GraphAddRoad(&f.graph, s1, s2, 50.0, "S1-S2", true);
    GraphAddRoad(&f.graph, s2, b, 50.0, "S2-B", true);

    StationAdd(&f.stations, "S1", s1, 60.0, STATION_OPERATIONAL);
    StationAdd(&f.stations, "S2", s2, 60.0, STATION_OPERATIONAL);

    CHECK(Route(&f, a, b), "chain with two stations is feasible");
    CHECK(CountStops(&f.route) == 2, "exactly two charging stops");
    CHECK(NEAR(f.route.totalDistanceKm, 150.0), "distance is 150 km");
    CHECK(NeverBelowReserve(&f), "battery never drops below reserve");

    /* 3.75 h driving + 2 x (15 kWh at 60 kW = 0.25 h) charging */
    CHECK(fabs(f.route.totalTimeHours - 4.25) < 1e-3,
          "total time = 3.75 h driving + 0.5 h charging");

    Teardown(&f);
}

/* Same chain, but the second station is out of service. */
static void TestUnavailableStationBlocksRoute(void)
{
    Fixture f;
    Setup(&f, 100.0);

    int a = AddCity(&f, "A");
    int s1 = AddCity(&f, "S1");
    int s2 = AddCity(&f, "S2");
    int b = AddCity(&f, "B");

    GraphAddRoad(&f.graph, a, s1, 50.0, "A-S1", true);
    GraphAddRoad(&f.graph, s1, s2, 50.0, "S1-S2", true);
    GraphAddRoad(&f.graph, s2, b, 50.0, "S2-B", true);

    StationAdd(&f.stations, "S1", s1, 60.0, STATION_OPERATIONAL);
    StationAdd(&f.stations, "S2", s2, 60.0, STATION_UNAVAILABLE);

    CHECK(!Route(&f, a, b), "no route when the needed station is down");
    CHECK(f.route.steps == NULL, "no steps returned on failure");
    CHECK(f.route.statesSettled > 0, "search still explored states");

    Teardown(&f);
}

/* Starting exactly at the reserve: can only leave if you can charge. */
static void TestStartAtReserve(void)
{
    /* 10% of 20 kWh = 2 kWh = the reserve. */
    Fixture f;
    Setup(&f, 10.0);

    int a = AddCity(&f, "A");
    int b = AddCity(&f, "B");
    GraphAddRoad(&f.graph, a, b, 30.0, "A-B", true);

    CHECK(!Route(&f, a, b), "cannot move at reserve without a charger");
    CHECK(f.route.statesSettled == 1, "only the start state is settled");

    BatteryRouteFree(&f.route);
    StationAdd(&f.stations, "A", a, 60.0, STATION_OPERATIONAL);

    CHECK(Route(&f, a, b), "feasible once the start has a charger");
    CHECK(f.route.stepCount >= 2 && f.route.steps[1].isChargingStop &&
          f.route.steps[1].vertexId == a,
          "first action is charging at the start");
    CHECK(NeverBelowReserve(&f), "battery never drops below reserve");

    Teardown(&f);
}

/*
 * The reserve changes the plan: 55 km needs 16.5 kWh. Starting with
 * 18 kWh you would arrive with 1.5 kWh, below the 2 kWh reserve, so the
 * router must top up at the start instead of driving straight there.
 */
static void TestReserveForcesCharge(void)
{
    Fixture f;
    Setup(&f, 90.0);   /* 18 kWh */

    int a = AddCity(&f, "A");
    int b = AddCity(&f, "B");
    GraphAddRoad(&f.graph, a, b, 55.0, "A-B", true);
    StationAdd(&f.stations, "A", a, 60.0, STATION_OPERATIONAL);

    CHECK(Route(&f, a, b), "feasible after topping up");
    CHECK(CountStops(&f.route) == 1, "charges once, at the start");
    CHECK(NEAR(f.route.finalBatteryKWh, 20.0 - 16.5),
          "arrives with 3.5 kWh after a full charge");
    CHECK(NeverBelowReserve(&f), "battery never drops below reserve");

    /* Without the charger the same trip is impossible. */
    BatteryRouteFree(&f.route);
    StationSetStatus(&f.stations, 0, STATION_UNAVAILABLE);
    CHECK(!Route(&f, a, b), "infeasible when arrival would break reserve");

    Teardown(&f);
}

/* A leg that needs more than a full battery can never be driven. */
static void TestLegLongerThanRange(void)
{
    Fixture f;
    Setup(&f, 100.0);

    int a = AddCity(&f, "A");
    int b = AddCity(&f, "B");
    GraphAddRoad(&f.graph, a, b, 61.0, "A-B", true);
    StationAdd(&f.stations, "A", a, 120.0, STATION_OPERATIONAL);

    CHECK(!Route(&f, a, b), "61 km leg exceeds the 60 km usable range");

    Teardown(&f);
}

/* Disconnected destination. */
static void TestUnreachable(void)
{
    Fixture f;
    Setup(&f, 100.0);

    int a = AddCity(&f, "A");
    int b = AddCity(&f, "B");
    int island = AddCity(&f, "Island");
    GraphAddRoad(&f.graph, a, b, 10.0, "A-B", true);

    CHECK(!Route(&f, a, island), "no route to a disconnected vertex");

    Teardown(&f);
}

/* The router must take a detour to a charger rather than fail. */
static void TestDetourToCharger(void)
{
    Fixture f;
    Setup(&f, 100.0);

    /*        C (station)
     *       / \
     *  50 km   50 km
     *     /     \
     *    A--70--B        direct road is out of range (60 km usable)
     */
    int a = AddCity(&f, "A");
    int b = AddCity(&f, "B");
    int c = AddCity(&f, "C");

    GraphAddRoad(&f.graph, a, b, 70.0, "A-B", true);
    GraphAddRoad(&f.graph, a, c, 50.0, "A-C", true);
    GraphAddRoad(&f.graph, c, b, 50.0, "C-B", true);
    StationAdd(&f.stations, "C", c, 60.0, STATION_OPERATIONAL);

    CHECK(Route(&f, a, b), "feasible via the charger");
    CHECK(NEAR(f.route.totalDistanceKm, 100.0), "takes the 100 km detour");
    CHECK(CountStops(&f.route) == 1, "one stop at C");

    Teardown(&f);
}

/*
 * Reference check: with a big battery (never binding) and the same speed
 * on every road, the fastest route is the shortest route, so the
 * battery-aware search must agree with plain Dijkstra.
 */
static void TestAgreesWithPlainDijkstra(void)
{
    Fixture f;
    Setup(&f, 100.0);
    BatteryInit(&f.spec, &f.state, 100.0, 100.0, 0.10, 1.0);

    int v[6];
    const char *names[6] = {"A", "B", "C", "D", "E", "F"};
    for (int i = 0; i < 6; i++)
        v[i] = AddCity(&f, names[i]);

    GraphAddRoad(&f.graph, v[0], v[1], 7, "", true);
    GraphAddRoad(&f.graph, v[0], v[2], 9, "", true);
    GraphAddRoad(&f.graph, v[0], v[5], 14, "", true);
    GraphAddRoad(&f.graph, v[1], v[2], 10, "", true);
    GraphAddRoad(&f.graph, v[1], v[3], 15, "", true);
    GraphAddRoad(&f.graph, v[2], v[3], 11, "", true);
    GraphAddRoad(&f.graph, v[2], v[5], 2, "", true);
    GraphAddRoad(&f.graph, v[3], v[4], 6, "", true);
    GraphAddRoad(&f.graph, v[4], v[5], 9, "", true);

    PathResult ref;
    CHECK(Dijkstra(&f.graph, v[0], &ref), "plain Dijkstra runs");

    for (int t = 1; t < 6; t++) {
        BatteryRouteFree(&f.route);
        char msg[96];
        snprintf(msg, sizeof msg, "A->%s distance matches plain Dijkstra",
                 names[t]);
        CHECK(Route(&f, v[0], v[t]) &&
              NEAR(f.route.totalDistanceKm, ref.distance[v[t]]), msg);
    }

    /* Classic example: A->E shortest path is A-C-F-E = 20 km. */
    CHECK(NEAR(ref.distance[v[4]], 20.0), "A->E reference distance is 20");
    CHECK(ref.settledCount == 6, "plain Dijkstra settles every vertex");

    DijkstraFree(&ref);
    Teardown(&f);
}

/* The statistics reported for the analysis panel are consistent. */
static void TestSearchStatistics(void)
{
    Fixture f;
    Setup(&f, 100.0);

    int a = AddCity(&f, "A");
    int s = AddCity(&f, "S");
    int b = AddCity(&f, "B");
    GraphAddRoad(&f.graph, a, s, 50.0, "", true);
    GraphAddRoad(&f.graph, s, b, 50.0, "", true);
    StationAdd(&f.stations, "S", s, 60.0, STATION_OPERATIONAL);

    CHECK(Route(&f, a, b), "route found");

    /* 3 vertices x (20 kWh / 0.01 + 1) battery levels */
    CHECK(f.route.stateSpace == 3 * 2001, "state space = V x levels");
    CHECK(f.route.statesSettled >= 1 &&
          f.route.statesSettled <= f.route.stateSpace,
          "settled count within the state space");
    CHECK(f.route.heapPushes >= f.route.statesSettled,
          "every settled state was pushed at least once");

    Teardown(&f);
}

/* CalculateRoute input validation and stats plumbing. */
static void TestCalculateRoute(void)
{
    Fixture f;
    Setup(&f, 100.0);

    int a = AddCity(&f, "Dehradun");
    int b = AddCity(&f, "Mussoorie");
    GraphAddRoad(&f.graph, a, b, 34.0, "", true);

    RouteSummary r;

    CHECK(!CalculateRoute(&f.graph, &f.stations, "Dehradun", "dehradun",
                          60, 80, 0.2, &r),
          "same origin and destination is rejected (case-insensitive)");
    RouteSummaryFree(&r);

    CHECK(!CalculateRoute(&f.graph, &f.stations, "Dehradun", "Atlantis",
                          60, 80, 0.2, &r),
          "unknown city is rejected");
    RouteSummaryFree(&r);

    CHECK(!CalculateRoute(&f.graph, &f.stations, "Dehradun", "Mussoorie",
                          60, 5, 0.2, &r),
          "charge below the 10% reserve is rejected");
    RouteSummaryFree(&r);

    CHECK(CalculateRoute(&f.graph, &f.stations, "Dehradun", "Mussoorie",
                         60, 80, 0.2, &r),
          "valid trip succeeds");
    CHECK(r.statsValid && r.baselineValid, "stats are filled in");
    CHECK(NEAR(r.baselineDistanceKm, 34.0), "baseline distance is 34 km");
    CHECK(NEAR(r.energyRequiredKWh, 34.0 * 0.2), "energy = 6.8 kWh");
    RouteSummaryFree(&r);

    Teardown(&f);
}

int main(void)
{
    struct { const char *name; void (*fn)(void); } tests[] = {
        {"no charge needed",              TestNoChargeNeeded},
        {"two charging stops",            TestTwoChargingStops},
        {"unavailable station",           TestUnavailableStationBlocksRoute},
        {"start exactly at reserve",      TestStartAtReserve},
        {"reserve forces a top-up",       TestReserveForcesCharge},
        {"leg longer than range",         TestLegLongerThanRange},
        {"unreachable destination",       TestUnreachable},
        {"detour to charger",             TestDetourToCharger},
        {"agrees with plain Dijkstra",    TestAgreesWithPlainDijkstra},
        {"search statistics",             TestSearchStatistics},
        {"CalculateRoute validation",     TestCalculateRoute},
    };

    size_t n = sizeof(tests) / sizeof(tests[0]);

    for (size_t i = 0; i < n; i++) {
        int before = failures;
        tests[i].fn();
        printf("%s %s\n", failures == before ? "ok  " : "FAIL",
               tests[i].name);
    }

    printf("\n%d checks, %d failed\n", checks, failures);

    return failures ? 1 : 0;
}
