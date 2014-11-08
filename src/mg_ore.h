/*
Minetest
Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef MG_ORE_HEADER
#define MG_ORE_HEADER

#include "util/string.h"
#include "mapnode.h"

struct NoiseParams;
class Noise;
class Mapgen;
class ManualMapVoxelManipulator;

/////////////////// Ore generation flags
// Use absolute value of height to determine ore placement
#define OREFLAG_ABSHEIGHT 0x01
// Use 3d noise to get density of ore placement, instead of just the position
#define OREFLAG_DENSITY   0x02 // not yet implemented
// For claylike ore types, place ore if the number of surrounding
// nodes isn't the specified node
#define OREFLAG_NODEISNT  0x04 // not yet implemented

#define ORE_RANGE_ACTUAL 1
#define ORE_RANGE_MIRROR 2

extern FlagDesc flagdesc_ore[];

enum OreType {
	ORE_SCATTER,
	ORE_SHEET,
	ORE_CLAYLIKE
};

class Ore {
public:
	content_t c_ore;                  // the node to place
	std::vector<content_t> c_wherein; // the nodes to be placed in
	u32 clust_scarcity; // ore cluster has a 1-in-clust_scarcity chance of appearing at a node
	s16 clust_num_ores; // how many ore nodes are in a chunk
	s16 clust_size;     // how large (in nodes) a chunk of ore is
	s16 height_min;
	s16 height_max;
	u8 ore_param2;		// to set node-specific attributes
	u32 flags;          // attributes for this ore
	float nthresh;      // threshhold for noise at which an ore is placed
	NoiseParams *np;    // noise for distribution of clusters (NULL for uniform scattering)
	Noise *noise;

	Ore() {
		c_ore   = CONTENT_IGNORE;
		np      = NULL;
		noise   = NULL;
	}

	virtual ~Ore();

	void placeOre(Mapgen *mg, u32 blockseed, v3s16 nmin, v3s16 nmax);
	virtual void generate(ManualMapVoxelManipulator *vm, int seed,
						u32 blockseed, v3s16 nmin, v3s16 nmax) = 0;
};

class OreScatter : public Ore {
	~OreScatter() {}
	virtual void generate(ManualMapVoxelManipulator *vm, int seed,
						u32 blockseed, v3s16 nmin, v3s16 nmax);
};

class OreSheet : public Ore {
	~OreSheet() {}
	virtual void generate(ManualMapVoxelManipulator *vm, int seed,
						u32 blockseed, v3s16 nmin, v3s16 nmax);
};

Ore *createOre(OreType type);


#endif
