#include "../src/graph/graph.h"
#include "../src/routing/dijkstra.h"

#include <stdio.h>

int main(void)
{
    Graph graph;

    GraphInit(&graph);

    /* Create sample locations */

    int dehradun = GraphAddVertex(
        &graph, "Dehradun", 30.3165, 78.0322
    );

    int mussoorie = GraphAddVertex(
        &graph, "Mussoorie", 30.4598, 78.0644
    );

    int rishikesh = GraphAddVertex(
        &graph, "Rishikesh", 30.0869, 78.2676
    );

    int chakrata = GraphAddVertex(
        &graph, "Chakrata", 30.7037, 77.8628
    );

    /* Add sample roads */

    GraphAddRoad(
        &graph, dehradun, mussoorie,
        35.0, "Mussoorie Road", true
    );

    GraphAddRoad(
        &graph, dehradun, rishikesh,
        45.0, "Rishikesh Road", true
    );

    GraphAddRoad(
        &graph, mussoorie, chakrata,
        75.0, "Hill Road", true
    );

    GraphAddRoad(
        &graph, rishikesh, chakrata,
        100.0, "Alternative Road", true
    );

    /* Run Dijkstra */

    PathResult result;

    if (!Dijkstra(&graph, dehradun, &result))
    {
        printf("Dijkstra failed.\n");
        GraphFree(&graph);
        return 1;
    }

    int path[16];

    size_t pathLength = DijkstraBuildPath(
        &result,
        chakrata,
        path,
        16
    );

    printf("\n========== SHORTEST ROUTE ==========\n");

    if (pathLength == 0)
    {
        printf("No route found.\n");
    }
    else
    {
        for (size_t i = 0; i < pathLength; i++)
        {
            printf("%s", graph.nodes[path[i]].vertex.name);

            if (i < pathLength - 1)
                printf(" -> ");
        }

        printf("\nTotal distance: %.2f km\n",
               result.distance[chakrata]);
    }

    printf("====================================\n");

    DijkstraFree(&result);
    GraphFree(&graph);

    return 0;
}