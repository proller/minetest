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

#include "mg_biome.h"
#include "gamedef.h"
#include "nodedef.h"
#include "map.h" //for ManualMapVoxelManipulator
#include "log.h"
#include "util/numeric.h"
#include "main.h"
#include "util/mathconstants.h"
#include "porting.h"

const char *BiomeManager::ELEMENT_TITLE = "biome";


///////////////////////////////////////////////////////////////////////////////

BiomeManager::BiomeManager(IGameDef *gamedef)
{
	m_resolver  = gamedef->getNodeDefManager()->getResolver();

	// Create default biome to be used in case none exist
	Biome *b = new Biome;

	b->id             = 0;
	b->name           = "Default";
	b->flags          = 0;
	b->depth_top      = 0;
	b->depth_filler   = 0;
	b->height_min     = -MAP_GENERATION_LIMIT;
	b->height_max     = MAP_GENERATION_LIMIT;
	b->heat_point     = 0.0;
	b->humidity_point = 0.0;

	m_resolver->addNode("air",                 "", CONTENT_AIR, &b->c_top);
	m_resolver->addNode("air",                 "", CONTENT_AIR, &b->c_filler);
	m_resolver->addNode("mapgen_stone",        "", CONTENT_AIR, &b->c_stone);
	m_resolver->addNode("mapgen_water_source", "", CONTENT_AIR, &b->c_water);
	m_resolver->addNode("air",                 "", CONTENT_AIR, &b->c_dust);
	m_resolver->addNode("mapgen_water_source", "", CONTENT_AIR, &b->c_dust_water);

	add(b);
}



BiomeManager::~BiomeManager()
{
	//if (biomecache)
	//	delete[] biomecache;
}



// just a PoC, obviously needs optimization later on (precalculate this)
void BiomeManager::calcBiomes(s16 sx, s16 sy, float *heat_map,
	float *humidity_map, s16 *height_map, u8 *biomeid_map)
{
	int i = 0;
	for (int y = 0; y != sy; y++) {
		for (int x = 0; x != sx; x++, i++) {
			float heat     = (heat_map[i] + 1) * 50;
			float humidity = (humidity_map[i] + 1) * 50;
			biomeid_map[i] = getBiome(heat, humidity, height_map[i])->id;
		}
	}
}


Biome *BiomeManager::getBiome(float heat, float humidity, s16 y)
{
	Biome *b, *biome_closest = NULL;
	float dist_min = FLT_MAX;

	for (size_t i = 1; i < m_elements.size(); i++) {
		b = (Biome *)m_elements[i];
		if (!b || y > b->height_max || y < b->height_min)
			continue;

		float d_heat     = heat     - b->heat_point;
		float d_humidity = humidity - b->humidity_point;
		float dist = (d_heat * d_heat) +
					 (d_humidity * d_humidity);
		if (dist < dist_min) {
			dist_min = dist;
			biome_closest = b;
		}
	}

	return biome_closest ? biome_closest : (Biome *)m_elements[0];
}

void BiomeManager::clear()
{
	for (size_t i = 1; i < m_elements.size(); i++) {
		Biome *b = (Biome *)m_elements[i];
		if (!b)
			continue;

		m_resolver->cancelNode(&b->c_top);
		m_resolver->cancelNode(&b->c_filler);
		m_resolver->cancelNode(&b->c_stone);
		m_resolver->cancelNode(&b->c_water);
		m_resolver->cancelNode(&b->c_dust);
		m_resolver->cancelNode(&b->c_dust_water);
	}
	m_elements.resize(1);
}

