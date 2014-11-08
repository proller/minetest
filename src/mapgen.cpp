/*
mapgen.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <fstream>
#include "mapgen.h"
#include "voxel.h"
#include "noise.h"
#include "mg_biome.h"
#include "mapblock.h"
#include "mapnode.h"
#include "map.h"
#include "content_sao.h"
#include "nodedef.h"
#include "content_mapnode.h" // For content_mapnode_get_new_name
#include "voxelalgorithms.h"
#include "profiler.h"
#include "settings.h" // For g_settings
#include "main.h" // For g_profiler
#include "treegen.h"
#include "serialization.h"
#include "util/serialize.h"
#include "filesys.h"
#include "log.h"


FlagDesc flagdesc_mapgen[] = {
	{"trees",    MG_TREES},
	{"caves",    MG_CAVES},
	{"dungeons", MG_DUNGEONS},
	{"flat",     MG_FLAT},
	{"light",    MG_LIGHT},
	{NULL,       0}
};

FlagDesc flagdesc_gennotify[] = {
	{"dungeon",          1 << GENNOTIFY_DUNGEON},
	{"temple",           1 << GENNOTIFY_TEMPLE},
	{"cave_begin",       1 << GENNOTIFY_CAVE_BEGIN},
	{"cave_end",         1 << GENNOTIFY_CAVE_END},
	{"large_cave_begin", 1 << GENNOTIFY_LARGECAVE_BEGIN},
	{"large_cave_end",   1 << GENNOTIFY_LARGECAVE_END},
	{NULL,               0}
};

///////////////////////////////////////////////////////////////////////////////





///////////////////////////////////////////////////////////////////////////////


Mapgen::Mapgen() {
	seed        = 0;
	water_level = 0;
	generating  = false;
	id          = -1;
	vm          = NULL;
	ndef        = NULL;
	heightmap   = NULL;
	biomemap    = NULL;

	for (unsigned int i = 0; i != NUM_GEN_NOTIFY; i++)
		gen_notifications[i] = new std::vector<v3s16>;
}


Mapgen::~Mapgen() {
	for (unsigned int i = 0; i != NUM_GEN_NOTIFY; i++)
		delete gen_notifications[i];
}


// Returns Y one under area minimum if not found
s16 Mapgen::findGroundLevelFull(v2s16 p2d) {
	v3s16 em = vm->m_area.getExtent();
	s16 y_nodes_max = vm->m_area.MaxEdge.Y;
	s16 y_nodes_min = vm->m_area.MinEdge.Y;
	u32 i = vm->m_area.index(p2d.X, y_nodes_max, p2d.Y);
	s16 y;

	for (y = y_nodes_max; y >= y_nodes_min; y--) {
		MapNode &n = vm->m_data[i];
		if (ndef->get(n).walkable)
			break;

		vm->m_area.add_y(em, i, -1);
	}
	return (y >= y_nodes_min) ? y : y_nodes_min - 1;
}


s16 Mapgen::findGroundLevel(v2s16 p2d, s16 ymin, s16 ymax) {
	v3s16 em = vm->m_area.getExtent();
	u32 i = vm->m_area.index(p2d.X, ymax, p2d.Y);
	s16 y;

	for (y = ymax; y >= ymin; y--) {
		MapNode &n = vm->m_data[i];
		if (ndef->get(n).walkable)
			break;

		vm->m_area.add_y(em, i, -1);
	}
	return y;
}


void Mapgen::updateHeightmap(v3s16 nmin, v3s16 nmax) {
	if (!heightmap)
		return;

	//TimeTaker t("Mapgen::updateHeightmap", NULL, PRECISION_MICRO);
	int index = 0;
	for (s16 z = nmin.Z; z <= nmax.Z; z++) {
		for (s16 x = nmin.X; x <= nmax.X; x++, index++) {
			s16 y = findGroundLevel(v2s16(x, z), nmin.Y, nmax.Y);

			// if the values found are out of range, trust the old heightmap
			if (y == nmax.Y && heightmap[index] > nmax.Y)
				continue;
			if (y == nmin.Y - 1 && heightmap[index] < nmin.Y)
				continue;

			heightmap[index] = y;
		}
	}
	//printf("updateHeightmap: %dus\n", t.stop());
}


void Mapgen::updateLiquid(v3s16 nmin, v3s16 nmax) {
	bool isliquid, wasliquid, rare;
	v3s16 em  = vm->m_area.getExtent();
	rare = g_settings->getBool("liquid_real");
	int rarecnt = 0;

	for (s16 z = nmin.Z; z <= nmax.Z; z++) {
		for (s16 x = nmin.X; x <= nmax.X; x++) {
			wasliquid = true;

			u32 i = vm->m_area.index(x, nmax.Y, z);
			for (s16 y = nmax.Y; y >= nmin.Y; y--) {
				isliquid = ndef->get(vm->m_data[i]).isLiquid();

				// there was a change between liquid and nonliquid, add to queue. no need to add every with liquid_real
				if (isliquid != wasliquid && (!rare || !(rarecnt++ % 36))) {
						auto p = v3s16(x, y, z);
						vm->m_map->transforming_liquid_push_back(p);
					}

				wasliquid = isliquid;
				vm->m_area.add_y(em, i, -1);
			}
		}
	}
}


void Mapgen::setLighting(v3s16 nmin, v3s16 nmax, u8 light) {
	ScopeProfiler sp(g_profiler, "EmergeThread: mapgen lighting update", SPT_AVG);
	VoxelArea a(nmin, nmax);

	for (int z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++) {
		for (int y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++) {
			u32 i = vm->m_area.index(a.MinEdge.X, y, z);
			for (int x = a.MinEdge.X; x <= a.MaxEdge.X; x++, i++)
				vm->m_data[i].param1 = light;
		}
	}
}


void Mapgen::lightSpread(VoxelArea &a, v3s16 p, u8 light) {
	if (light <= 1 || !a.contains(p))
		return;

	u32 vi = vm->m_area.index(p);
	MapNode &nn = vm->m_data[vi];

	light--;
	// should probably compare masked, but doesn't seem to make a difference
	if (light <= nn.param1 || !ndef->get(nn).light_propagates)
		return;

	nn.param1 = light;

	lightSpread(a, p + v3s16(0, 0, 1), light);
	lightSpread(a, p + v3s16(0, 1, 0), light);
	lightSpread(a, p + v3s16(1, 0, 0), light);
	lightSpread(a, p - v3s16(0, 0, 1), light);
	lightSpread(a, p - v3s16(0, 1, 0), light);
	lightSpread(a, p - v3s16(1, 0, 0), light);
}


void Mapgen::calcLighting(v3s16 nmin, v3s16 nmax) {
	VoxelArea a(nmin, nmax);
	bool block_is_underground = (water_level >= nmax.Y);

	ScopeProfiler sp(g_profiler, "EmergeThread: mapgen lighting update", SPT_AVG);
	//TimeTaker t("updateLighting");

	// first, send vertical rays of sunshine downward
	v3s16 em = vm->m_area.getExtent();
	for (int z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++) {
		for (int x = a.MinEdge.X; x <= a.MaxEdge.X; x++) {
			// see if we can get a light value from the overtop
			u32 i = vm->m_area.index(x, a.MaxEdge.Y + 1, z);
			if (vm->m_data[i].getContent() == CONTENT_IGNORE) {
				if (block_is_underground)
					continue;
			} else if ((vm->m_data[i].param1 & 0x0F) != LIGHT_SUN) {
				u32 ii = 0;
				if (
				(x < a.MaxEdge.X && (ii = vm->m_area.index(x + 1, a.MaxEdge.Y + 1, z    )) &&
				(vm->m_data[ii].getContent() != CONTENT_IGNORE) &&
				((vm->m_data[ii].param1 & 0x0F) == LIGHT_SUN))||
				(x > a.MinEdge.X && (ii = vm->m_area.index(x - 1, a.MaxEdge.Y + 1, z    )) &&
				(vm->m_data[ii].getContent() != CONTENT_IGNORE) &&
				((vm->m_data[ii].param1 & 0x0F) == LIGHT_SUN))||
				(z > a.MinEdge.Z && (ii = vm->m_area.index(x    , a.MaxEdge.Y + 1, z - 1)) &&
				(vm->m_data[ii].getContent() != CONTENT_IGNORE) &&
				((vm->m_data[ii].param1 & 0x0F) == LIGHT_SUN))||
				(z < a.MaxEdge.Z && (ii = vm->m_area.index(x    , a.MaxEdge.Y + 1, z + 1)) &&
				(vm->m_data[ii].getContent() != CONTENT_IGNORE) &&
				((vm->m_data[ii].param1 & 0x0F) == LIGHT_SUN))
				) {
				} else
				continue;
			}
			vm->m_area.add_y(em, i, -1);

			for (int y = a.MaxEdge.Y; y >= a.MinEdge.Y; y--) {
				MapNode &n = vm->m_data[i];
				if (!ndef->get(n).sunlight_propagates)
					break;
				n.param1 = LIGHT_SUN;
				vm->m_area.add_y(em, i, -1);
			}
		}
	}

	// now spread the sunlight and light up any sources
	for (int z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++) {
		for (int y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++) {
			u32 i = vm->m_area.index(a.MinEdge.X, y, z);
			for (int x = a.MinEdge.X; x <= a.MaxEdge.X; x++, i++) {
				MapNode &n = vm->m_data[i];
				if (n.getContent() == CONTENT_IGNORE ||
					!ndef->get(n).light_propagates)
					continue;

				u8 light_produced = ndef->get(n).light_source & 0x0F;
				if (light_produced)
					n.param1 = light_produced;

				u8 light = n.param1 & 0x0F;
				if (light) {
					lightSpread(a, v3s16(x,     y,     z + 1), light - 1);
					lightSpread(a, v3s16(x,     y + 1, z    ), light - 1);
					lightSpread(a, v3s16(x + 1, y,     z    ), light - 1);
					lightSpread(a, v3s16(x,     y,     z - 1), light - 1);
					lightSpread(a, v3s16(x,     y - 1, z    ), light - 1);
					lightSpread(a, v3s16(x - 1, y,     z    ), light - 1);
				}
			}
		}
	}

	//printf("updateLighting: %dms\n", t.stop());
}


void Mapgen::calcLightingOld(v3s16 nmin, v3s16 nmax) {
	enum LightBank banks[2] = {LIGHTBANK_DAY, LIGHTBANK_NIGHT};
	VoxelArea a(nmin, nmax);
	bool block_is_underground = (water_level > nmax.Y);
	bool sunlight = !block_is_underground;

	ScopeProfiler sp(g_profiler, "EmergeThread: mapgen lighting update", SPT_AVG);

	for (int i = 0; i < 2; i++) {
		enum LightBank bank = banks[i];
		std::set<v3s16> light_sources;
		std::map<v3s16, u8> unlight_from;

		voxalgo::clearLightAndCollectSources(*vm, a, bank, ndef,
											 light_sources, unlight_from);
		voxalgo::propagateSunlight(*vm, a, sunlight, light_sources, ndef);

		vm->unspreadLight(bank, unlight_from, light_sources, ndef);
		vm->spreadLight(bank, light_sources, ndef);
	}
}
