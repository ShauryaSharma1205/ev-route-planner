#include "../src/graph/graph.h"

#include <stdio.h>

int main(void)
{
    Graph graph;

    GraphInit(&graph);

    int dehradun = GraphAddVertex(
        &graph, "Dehradun", 30.3165, 78.0322
    );

    int mussoorie = GraphAddVertex(
        &graph, "Mussoorie", 30.4598, 78.0644
    );

    int rishikesh = GraphAddVertex(
        &graph, "Rishikesh", 30.0869, 78.2676
    );

    GraphAddRoadWithSpeed(
        &graph,
        dehradun,
        mussoorie,
        35.0,
        25.0,
        "Mussoorie Road",
        true
    );

    GraphAddRoadWithSpeed(
        &graph,
        dehradun,
        rishikesh,
        45.0,
        45.0,
        "Rishikesh Road",
        true
    );

    printf("\n========== ROAD SPEED TEST ==========\n");

    GraphPrint(&graph);

    printf("\nExpected travel times:\n");

    printf("Mussoorie: %.2f hours\n",
           graph.nodes[dehradun].edges->travelTimeHours);

    printf("=====================================\n");

    GraphEdge *edge = graph.nodes[dehradun].edges;

    while (edge != NULL)
    {
        printf(
            "%s: %.2f hours\n",
            edge->roadName,
            edge->travelTimeHours
        );

        edge = edge->next;
    }

    GraphFree(&graph);

    return 0;
}