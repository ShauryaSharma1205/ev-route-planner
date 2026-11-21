import osmium
import csv
import math
import os
import re
from collections import Counter

# ============================================================
# CONFIGURATION
# ============================================================

INPUT_FILE = "data/raw/osm/uttarakhand-roads.osm.pbf"
OUTPUT_DIR = "data/processed"

os.makedirs(OUTPUT_DIR, exist_ok=True)

ALLOWED_HIGHWAYS = {
    "motorway", "motorway_link",
    "trunk", "trunk_link",
    "primary", "primary_link",
    "secondary", "secondary_link",
    "tertiary", "tertiary_link",
    "unclassified", "residential",
    "living_street", "service"
}

DEFAULT_SPEEDS = {
    "motorway": 90,
    "motorway_link": 50,
    "trunk": 70,
    "trunk_link": 45,
    "primary": 55,
    "primary_link": 40,
    "secondary": 40,
    "secondary_link": 30,
    "tertiary": 30,
    "tertiary_link": 25,
    "unclassified": 25,
    "residential": 25,
    "living_street": 10,
    "service": 15
}


# ============================================================
# DISTANCE CALCULATION
# ============================================================

def haversine(lat1, lon1, lat2, lon2):

    radius = 6371.0

    lat1 = math.radians(lat1)
    lon1 = math.radians(lon1)
    lat2 = math.radians(lat2)
    lon2 = math.radians(lon2)

    dlat = lat2 - lat1
    dlon = lon2 - lon1

    a = (
        math.sin(dlat / 2) ** 2
        + math.cos(lat1)
        * math.cos(lat2)
        * math.sin(dlon / 2) ** 2
    )

    return radius * 2 * math.asin(math.sqrt(a))


# ============================================================
# SPEED LIMIT PROCESSING
# ============================================================

def parse_speed(value, highway):

    if value:

        match = re.search(r"\d+(?:\.\d+)?", value)

        if match:

            speed = float(match.group())

            if "mph" in value.lower():
                speed *= 1.60934

            if 1 <= speed <= 250:
                return speed, "osm"

    return DEFAULT_SPEEDS.get(highway, 25), "estimated"


# ============================================================
# OSM DATA COLLECTION
# ============================================================

class OSMCollector(osmium.SimpleHandler):

    def __init__(self):

        super().__init__()

        self.coordinates = {}
        self.ways = []

    def node(self, node):

        if node.location.valid():

            self.coordinates[node.id] = (
                node.location.lat,
                node.location.lon
            )

    def way(self, way):

        highway = way.tags.get("highway")

        if highway not in ALLOWED_HIGHWAYS:
            return

        # Exclude roads inaccessible to motor vehicles.

        if way.tags.get("access") in {"no", "private"}:
            return

        if way.tags.get("motor_vehicle") == "no":
            return

        if way.tags.get("motorcar") == "no":
            return

        if way.tags.get("area") == "yes":
            return

        refs = [node.ref for node in way.nodes]

        if len(refs) < 2:
            return

        speed, speed_source = parse_speed(
            way.tags.get("maxspeed"),
            highway
        )

        oneway = way.tags.get("oneway", "").lower()

        # Handle one-way roads and roundabouts.

        if oneway in {"yes", "1", "true"}:
            direction = 1

        elif oneway == "-1":
            direction = -1

        elif (
            way.tags.get("junction") == "roundabout"
            and oneway not in {"no", "0", "false"}
        ):
            direction = 1

        else:
            direction = 0

        self.ways.append({

            "osm_way_id": way.id,
            "refs": refs,
            "name": way.tags.get("name", ""),
            "highway": highway,
            "speed": speed,
            "speed_source": speed_source,
            "direction": direction

        })


# ============================================================
# MAIN PROCESSING
# ============================================================

def main():

    print("\n==========================================")
    print(" UTTARAKHAND ROAD NETWORK PROCESSOR")
    print("==========================================\n")

    if not os.path.exists(INPUT_FILE):

        raise FileNotFoundError(
            f"Input file not found: {INPUT_FILE}"
        )

    print("[1/6] Reading OpenStreetMap data...")

    collector = OSMCollector()
    collector.apply_file(INPUT_FILE)

    print(
        f"      Nodes loaded: {len(collector.coordinates):,}"
    )

    print(
        f"      Road ways loaded: {len(collector.ways):,}"
    )

    # --------------------------------------------------------
    # IDENTIFY INTERSECTIONS
    # --------------------------------------------------------

    print("\n[2/6] Identifying road intersections...")

    node_usage = Counter()

    for way in collector.ways:

        for node_id in set(way["refs"]):

            node_usage[node_id] += 1

    # --------------------------------------------------------
    # SPLIT ROADS INTO GRAPH SEGMENTS
    # --------------------------------------------------------

    print("\n[3/6] Splitting roads into graph segments...")

    segments = []

    missing_coordinates = 0

    for way in collector.ways:

        refs = way["refs"]

        breakpoints = [

            i for i, node_id in enumerate(refs)

            if (
                i == 0
                or i == len(refs) - 1
                or node_usage[node_id] > 1
            )

        ]

        for start, end in zip(
            breakpoints,
            breakpoints[1:]
        ):

            segment_refs = refs[start:end + 1]

            if len(segment_refs) < 2:
                continue

            if any(
                node_id not in collector.coordinates
                for node_id in segment_refs
            ):

                missing_coordinates += 1
                continue

            distance = 0.0

            for a, b in zip(
                segment_refs,
                segment_refs[1:]
            ):

                lat1, lon1 = collector.coordinates[a]
                lat2, lon2 = collector.coordinates[b]

                distance += haversine(
                    lat1, lon1,
                    lat2, lon2
                )
            # Ignore zero-length segments and closed self-loop segments.
            if distance <= 0 or segment_refs[0] == segment_refs[-1]:
                continue

            segments.append({

                "way": way,
                "refs": segment_refs,
                "distance": distance

            })

    print(
        f"      Road segments created: {len(segments):,}"
    )

    print(
        f"      Segments skipped (missing coordinates): "
        f"{missing_coordinates:,}"
    )

    # --------------------------------------------------------
    # ASSIGN GRAPH VERTEX IDS
    # --------------------------------------------------------

    print("\n[4/6] Creating graph vertices...")

    used_nodes = set()

    for segment in segments:

        used_nodes.add(segment["refs"][0])
        used_nodes.add(segment["refs"][-1])

    sorted_nodes = sorted(used_nodes)

    vertex_ids = {

        osm_id: index

        for index, osm_id in enumerate(sorted_nodes)

    }

    # --------------------------------------------------------
    # WRITE VERTICES
    # --------------------------------------------------------

    print("\n[5/6] Writing vertices.csv...")

    vertices_path = os.path.join(
        OUTPUT_DIR,
        "vertices.csv"
    )

    with open(
        vertices_path,
        "w",
        newline="",
        encoding="utf-8"
    ) as file:

        writer = csv.writer(file)

        writer.writerow([

            "vertex_id",
            "osm_node_id",
            "latitude",
            "longitude"

        ])

        for osm_id in sorted_nodes:

            lat, lon = collector.coordinates[osm_id]

            writer.writerow([

                vertex_ids[osm_id],
                osm_id,
                f"{lat:.7f}",
                f"{lon:.7f}"

            ])

    # --------------------------------------------------------
    # WRITE DIRECTED EDGES AND ROAD GEOMETRY
    # --------------------------------------------------------

    print("\n[6/6] Writing road edges and geometry...")

    roads_path = os.path.join(
        OUTPUT_DIR,
        "roads.csv"
    )

    geometry_path = os.path.join(
        OUTPUT_DIR,
        "road_geometry.csv"
    )

    edge_count = 0

    with open(
        roads_path,
        "w",
        newline="",
        encoding="utf-8"
    ) as roads_file, open(
        geometry_path,
        "w",
        newline="",
        encoding="utf-8"
    ) as geometry_file:

        road_writer = csv.writer(roads_file)
        geometry_writer = csv.writer(geometry_file)

        road_writer.writerow([

            "edge_id",
            "source",
            "destination",
            "osm_way_id",
            "road_name",
            "highway",
            "distance_km",
            "speed_kmh",
            "speed_source",
            "oneway"

        ])

        geometry_writer.writerow([

            "edge_id",
            "sequence",
            "latitude",
            "longitude"

        ])

        for segment in segments:

            way = segment["way"]
            refs = segment["refs"]

            # Determine permitted travel directions.

            if way["direction"] == 0:

                directions = [
                    refs,
                    list(reversed(refs))
                ]

            elif way["direction"] == 1:

                directions = [refs]

            else:

                directions = [
                    list(reversed(refs))
                ]

            for directed_refs in directions:

                edge_count += 1

                source = vertex_ids[
                    directed_refs[0]
                ]

                destination = vertex_ids[
                    directed_refs[-1]
                ]

                road_writer.writerow([

                    edge_count,
                    source,
                    destination,
                    way["osm_way_id"],
                    way["name"],
                    way["highway"],
                    f"{segment['distance']:.6f}",
                    f"{way['speed']:.2f}",
                    way["speed_source"],
                    way["direction"]

                ])

                # Preserve every intermediate coordinate.

                for sequence, osm_id in enumerate(
                    directed_refs
                ):

                    lat, lon = collector.coordinates[osm_id]

                    geometry_writer.writerow([

                        edge_count,
                        sequence,
                        f"{lat:.7f}",
                        f"{lon:.7f}"

                    ])

    # --------------------------------------------------------
    # FINAL SUMMARY
    # --------------------------------------------------------

    print("\n==========================================")
    print(" PROCESSING COMPLETE")
    print("==========================================")

    print(
        f"Graph vertices:       {len(sorted_nodes):,}"
    )

    print(
        f"Road segments:        {len(segments):,}"
    )

    print(
        f"Directed road edges:  {edge_count:,}"
    )

    print("\nGenerated files:")

    print(f"  {vertices_path}")
    print(f"  {roads_path}")
    print(f"  {geometry_path}")

    print("\n==========================================\n")


if __name__ == "__main__":
    main()