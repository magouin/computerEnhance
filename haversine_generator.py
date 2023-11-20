#!/usr/bin/python3

import json
import sys
import argparse
import math
import random

earth_radius = 6372.8

def haversine(px1, py1, px2, py2, radius):
    dx = math.radians(px2 - px1)
    dy = math.radians(py2 - py1)
    x1 = math.radians(px1)
    x2 = math.radians(px2)

    a = (math.sin(dx / 2.0) ** 2) + math.cos(x1) * math.cos(x2) * ((math.sin(dy / 2.0) ** 2))
    c = 2.0 * math.asin(math.sqrt(a))
    ret = radius * c

    return ret

def parse_arguments(arg):
    parser = argparse.ArgumentParser(
                    prog=arg[0],
                    description='Generate a json file with pairs of point')
    parser.add_argument('-s', '--seed', type=int, required=True,
                        help='seed of the randomness')
    parser.add_argument('-n', '--size', type=int, required=True,
                        help='number of pair of point to generate')
    args = parser.parse_args()
    return args

def generator(size):
    points = {"pairs": []}
    for _ in range(size):
        points["pairs"].append({"x0": round(random.uniform(0, 180), 5), "y0": round(random.uniform(0, 180), 5),
                                "x1": round(random.uniform(0, 180), 5), "y1": round(random.uniform(0, 180), 5)})
    return points

def get_expected_sum(points, radius):
    pairs = points["pairs"]
    ret = 0
    for i in pairs:
        ret += haversine(i["x0"], i["y0"], i["x1"], i["y1"], radius)
    
    return ret / len(pairs)

def main(arg):
    args = parse_arguments(arg)
    random.seed(args.seed)
    points = generator(args.size)
    with open("sample.json", "w") as f:
        json_points = json.dumps(points)
        f.write(json_points)
    expect = get_expected_sum(points, earth_radius)
    print(f"Expected sum: {round(expect, 6)}")
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))