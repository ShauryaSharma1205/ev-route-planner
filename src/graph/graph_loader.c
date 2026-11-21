#include "graph_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <limits.h>

#define CSV_BUFFER_SIZE 8192
#define CSV_MAX_FIELDS 16

/* ================= CSV PARSER ================= */

static size_t ParseCSVLine(
    char *line,
    char *fields[],
    size_t maxFields
)
{
    char *read = line;
    char *write = line;
    size_t count = 0;

    while (*read && count < maxFields)
    {
        fields[count++] = write;

        if (*read == '"')
        {
            read++;

            while (*read)
            {
                if (*read == '"')
                {
                    if (read[1] == '"')
                    {
                        *write++ = '"';
                        read += 2;
                    }
                    else
                    {
                        read++;
                        break;
                    }
                }
                else
                {
                    *write++ = *read++;
                }
            }

            while (*read == ' ' || *read == '\t')
                read++;
        }
        else
        {
            while (
                *read &&
                *read != ',' &&
                *read != '\n' &&
                *read != '\r'
            )
            {
                *write++ = *read++;
            }
        }

        if (*read == ',')
        {
            read++;
            *write++ = '\0';
        }
        else
        {
            while (*read == '\r' || *read == '\n')
                read++;

            *write = '\0';
            break;
        }
    }

    return count;
}

/* ================= NUMBER PARSING ================= */

static bool ParseInteger(const char *text, long *value)
{
    if (!text || !*text)
        return false;

    errno = 0;

    char *end;
    long result = strtol(text, &end, 10);

    if (errno || end == text || *end != '\0')
        return false;

    *value = result;
    return true;
}

static bool ParseDouble(const char *text, double *value)
{
    if (!text || !*text)
        return false;

    errno = 0;

    char *end;
    double result = strtod(text, &end);

    if (
        errno ||
        end == text ||
        *end != '\0' ||
        !isfinite(result)
    )
        return false;

    *value = result;
    return true;
}

/* ================= LOAD VERTICES ================= */

static bool LoadVertices(Graph *graph, const char *filename)
{
    FILE *file = fopen(filename, "r");

    if (!file)
    {
        perror("Unable to open vertices CSV");
        return false;
    }

    char line[CSV_BUFFER_SIZE];
    char *fields[CSV_MAX_FIELDS];

    if (!fgets(line, sizeof(line), file))
    {
        fclose(file);
        return false;
    }

    size_t loaded = 0;

    while (fgets(line, sizeof(line), file))
    {
        size_t count = ParseCSVLine(
            line,
            fields,
            CSV_MAX_FIELDS
        );

        if (count != 4)
        {
            fprintf(stderr,
                    "Invalid vertex CSV row %zu\n",
                    loaded + 2);

            fclose(file);
            return false;
        }

        long vertexId;
        double latitude;
        double longitude;

        if (
            !ParseInteger(fields[0], &vertexId) ||
            !ParseDouble(fields[2], &latitude) ||
            !ParseDouble(fields[3], &longitude)
        )
        {
            fprintf(stderr,
                    "Invalid vertex data at row %zu\n",
                    loaded + 2);

            fclose(file);
            return false;
        }

        if (
            vertexId < 0 ||
            (size_t)vertexId != graph->vertexCount
        )
        {
            fprintf(stderr,
                    "Unexpected vertex ID at row %zu\n",
                    loaded + 2);

            fclose(file);
            return false;
        }

        char name[64];

        snprintf(
            name,
            sizeof(name),
            "OSM %s",
            fields[1]
        );

        if (GraphAddVertex(
                graph,
                name,
                latitude,
                longitude
            ) < 0)
        {
            fprintf(stderr,
                    "Failed to add vertex at row %zu\n",
                    loaded + 2);

            fclose(file);
            return false;
        }

        loaded++;

        if (loaded % 25000 == 0)
            printf("Loaded %zu vertices...\n", loaded);
    }

    if (ferror(file))
    {
        fclose(file);
        return false;
    }

    fclose(file);

    printf("Vertices loaded: %zu\n", loaded);

    return loaded > 0;
}

/* ================= LOAD ROADS ================= */

static bool LoadRoads(Graph *graph, const char *filename)
{
    FILE *file = fopen(filename, "r");

    if (!file)
    {
        perror("Unable to open roads CSV");
        return false;
    }

    char line[CSV_BUFFER_SIZE];
    char *fields[CSV_MAX_FIELDS];

    if (!fgets(line, sizeof(line), file))
    {
        fclose(file);
        return false;
    }

    size_t loaded = 0;
    size_t skippedSelfLoops = 0;
    size_t skippedInvalid = 0;
    size_t row = 1;

    while (fgets(line, sizeof(line), file))
    {
        row++;

        size_t count = ParseCSVLine(
            line,
            fields,
            CSV_MAX_FIELDS
        );

        if (count != 10)
        {
            fprintf(stderr,
                    "Invalid road CSV record at row %zu\n",
                    row);

            fclose(file);
            return false;
        }

        long edgeId;
        long source;
        long destination;

        double distance;
        double speed;

        if (
            !ParseInteger(fields[0], &edgeId) ||
            !ParseInteger(fields[1], &source) ||
            !ParseInteger(fields[2], &destination) ||
            !ParseDouble(fields[6], &distance) ||
            !ParseDouble(fields[7], &speed)
        )
        {
            fprintf(stderr,
                    "Invalid road data at row %zu\n",
                    row);

            fclose(file);
            return false;
        }

        if (
            edgeId <= 0 ||
            source < 0 ||
            destination < 0 ||
            source > INT_MAX ||
            destination > INT_MAX
        )
        {
            fprintf(stderr,
                    "Invalid road identifiers at row %zu\n",
                    row);

            fclose(file);
            return false;
        }

        /*
         * Self-loop edges are skipped, not treated as errors: they are
         * useless for routing and GraphAddRoadWithSpeed rejects them.
         */

        if (source == destination)
        {
            skippedSelfLoops++;
            continue;
        }

        if (
            (size_t)source >= graph->vertexCount ||
            (size_t)destination >= graph->vertexCount ||
            distance <= 0 ||
            speed <= 0 ||
            speed > 250
        )
        {
            skippedInvalid++;
            continue;
        }

        /*
         * Every CSV row is already a directed edge.
         * Do not create a second reverse edge.
         */

        if (!GraphAddRoadWithSpeed(
                graph,
                (int)source,
                (int)destination,
                distance,
                speed,
                fields[4],
                false
            ))
        {
            fprintf(stderr,
                    "Failed to add road edge %ld at row %zu\n",
                    edgeId,
                    row);

            fclose(file);
            return false;
        }

        loaded++;

        if (loaded % 50000 == 0)
            printf("Loaded %zu road edges...\n", loaded);
    }

    if (ferror(file))
    {
        fclose(file);
        return false;
    }

    fclose(file);

    printf("\n========== ROAD LOADING SUMMARY ==========\n");
    printf("Loaded road edges:       %zu\n", loaded);
    printf("Skipped self-loops:      %zu\n", skippedSelfLoops);
    printf("Skipped invalid edges:   %zu\n", skippedInvalid);
    printf("==========================================\n");

    return loaded > 0;
}

/* ================= PUBLIC GRAPH LOADER ================= */

bool GraphLoadFromCSV(
    Graph *graph,
    const char *verticesFile,
    const char *roadsFile
)
{
    if (!graph || !verticesFile || !roadsFile)
        return false;

    if (
        graph->nodes != NULL ||
        graph->vertexCount != 0 ||
        graph->edgeCount != 0
    )
    {
        fprintf(stderr,
                "Graph must be initialized and empty before loading.\n");

        return false;
    }

    printf("\n====================================\n");
    printf(" LOADING OPENSTREETMAP ROAD NETWORK\n");
    printf("====================================\n\n");

    if (!LoadVertices(graph, verticesFile))
    {
        GraphFree(graph);
        return false;
    }

    if (!LoadRoads(graph, roadsFile))
    {
        GraphFree(graph);
        return false;
    }

    printf("\nGraph loaded successfully!\n");

    GraphPrintSummary(graph);

    return true;
}