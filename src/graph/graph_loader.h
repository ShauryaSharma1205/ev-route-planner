#ifndef GRAPH_LOADER_H
#define GRAPH_LOADER_H

#include "graph.h"
#include <stdbool.h>

bool GraphLoadFromCSV(
    Graph *graph,
    const char *verticesFile,
    const char *roadsFile
);

#endif