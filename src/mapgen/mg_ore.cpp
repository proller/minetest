// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015-2020 paramat
// Copyright (C) 2014-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#include "mg_ore.h"
#include "mapgen.h"
#include "noise.h"
#include "map.h"
#include <cmath>
#include <algorithm>


const FlagDesc flagdesc_ore[] = {
	{"absheight",                 OREFLAG_ABSHEIGHT}, // Non-functional
	{"puff_cliffs",               OREFLAG_PUFF_CLIFFS},
	{"puff_additive_composition", OREFLAG_PUFF_ADDITIVE},
	{NULL,                        0}
};

// Ore ranges are bounded by a mapchunk; draw an offset so absolute world
// coordinates do not have to fit in PcgRandom's signed 32-bit range.
static pos_t randomPosition(PcgRandom &pr, pos_t min, pos_t max)
{
	if (max < min)
		throw PrngException("Invalid range (max < min)");
	u64 width = static_cast<u64>(max) - static_cast<u64>(min);
	if (width > U32_MAX)
		throw PrngException("Ore position range too large");
	return static_cast<pos_t>(static_cast<s64>(min) + pr.range(static_cast<u32>(width + 1)));
}

// The two coordinates are close together, but their sum may overflow.
static pos_t midpoint(pos_t a, pos_t b)
{
	s64 delta = static_cast<s64>(b) - static_cast<s64>(a);
	s64 mid = static_cast<s64>(a) + delta / 2;
	if (delta % 2 && delta > 0 && mid < 0)
		++mid;
	else if (delta % 2 && delta < 0 && mid > 0)
		--mid;
	return static_cast<pos_t>(mid);
}

static noise_coord_t addNoiseOffset(pos_t position, float offset)
{
	// Preserve existing float rounding in the ordinary coordinate range.
#if USE_POS32 == 64
	if (position >= -16777216 && position <= 16777216)
		return static_cast<float>(position) + offset;
	return static_cast<noise_coord_t>(position) + offset;
#else
	return static_cast<float>(position) + offset;
#endif
}


///////////////////////////////////////////////////////////////////////////////


OreManager::OreManager(IGameDef *gamedef) :
	ObjDefManager(gamedef, OBJDEF_ORE)
{
}


size_t OreManager::placeAllOres(Mapgen *mg, u32 blockseed, v3pos_t nmin, v3pos_t nmax)
{
	size_t nplaced = 0;

	for (size_t i = 0; i != m_objects.size(); i++) {
		Ore *ore = (Ore *)m_objects[i];
		if (!ore)
			continue;

		nplaced += ore->placeOre(mg, blockseed, nmin, nmax);
		blockseed++;
	}

	return nplaced;
}


void OreManager::clear()
{
	for (ObjDef *object : m_objects) {
		Ore *ore = (Ore *) object;
		delete ore;
	}
	m_objects.clear();
}


OreManager *OreManager::clone() const
{
	auto mgr = new OreManager();
	ObjDefManager::cloneTo(mgr);
	return mgr;
}


///////////////////////////////////////////////////////////////////////////////


Ore::~Ore()
{
	delete noise;
}


void Ore::resolveNodeNames()
{
	getIdFromNrBacklog(&c_ore, "", CONTENT_AIR);
	getIdsFromNrBacklog(&c_wherein);
}


size_t Ore::placeOre(Mapgen *mg, u32 blockseed, v3pos_t nmin, v3pos_t nmax)
{
	if (nmin.Y > y_max || nmax.Y < y_min)
		return 0;

	pos_t actual_ymin = MYMAX(nmin.Y, y_min);
	pos_t actual_ymax = MYMIN(nmax.Y, y_max);
	if (clust_size >= actual_ymax - actual_ymin + 1)
		return 0;

	nmin.Y = actual_ymin;
	nmax.Y = actual_ymax;
	generate(mg->vm, mg->seed, blockseed, nmin, nmax, mg->biomemap);

	return 1;
}


void Ore::cloneTo(Ore *def) const
{
	ObjDef::cloneTo(def);
	NodeResolver::cloneTo(def);
	def->c_ore = c_ore;
	def->c_wherein = c_wherein;
	def->clust_scarcity = clust_scarcity;
	def->clust_num_ores = clust_num_ores;
	def->clust_size = clust_size;
	def->y_min = y_min;
	def->y_max = y_max;
	def->ore_param2 = ore_param2;
	def->flags = flags;
	def->nthresh = nthresh;
	def->np = np;
	def->noise = nullptr; // cannot be shared! so created on demand
	def->biomes = biomes;
}


///////////////////////////////////////////////////////////////////////////////


ObjDef *OreScatter::clone() const
{
	auto def = new OreScatter();
	Ore::cloneTo(def);
	return def;
}


void OreScatter::generate(MMVManip *vm, int mapseed, u32 blockseed,
	v3pos_t nmin, v3pos_t nmax, biome_t *biomemap)
{
	PcgRandom pr(blockseed);
	MapNode n_ore(c_ore, 0, ore_param2);

	u32 sizex  = (nmax.X - nmin.X + 1);
	u32 volume = (nmax.X - nmin.X + 1) *
				 (nmax.Y - nmin.Y + 1) *
				 (nmax.Z - nmin.Z + 1);
	u32 csize     = clust_size;
	u32 cvolume    = csize * csize * csize;
	u32 nclusters = volume / clust_scarcity;

	for (u32 i = 0; i != nclusters; i++) {
		pos_t x0 = randomPosition(pr, nmin.X, nmax.X - csize + 1);
		pos_t y0 = randomPosition(pr, nmin.Y, nmax.Y - csize + 1);
		pos_t z0 = randomPosition(pr, nmin.Z, nmax.Z - csize + 1);

		if ((flags & OREFLAG_USE_NOISE) &&
			(NoiseFractal3D(&np, x0, y0, z0, mapseed) < nthresh))
			continue;

		if (biomemap && !biomes.empty()) {
			u32 index = sizex * (z0 - nmin.Z) + (x0 - nmin.X);
			auto it = biomes.find(biomemap[index]);
			if (it == biomes.end())
				continue;
		}

		for (u32 z1 = 0; z1 != csize; z1++)
		for (u32 y1 = 0; y1 != csize; y1++)
		for (u32 x1 = 0; x1 != csize; x1++) {
			if (pr.range(1, cvolume) > clust_num_ores)
				continue;

			u32 i = vm->m_area.index(x0 + x1, y0 + y1, z0 + z1);
			if (!CONTAINS(c_wherein, vm->m_data[i].getContent()))
				continue;

			vm->m_data[i] = n_ore;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////


ObjDef *OreSheet::clone() const
{
	auto def = new OreSheet();
	Ore::cloneTo(def);

	def->column_height_max = column_height_max;
	def->column_height_min = column_height_min;
	def->column_midpoint_factor = column_midpoint_factor;

	return def;
}


void OreSheet::generate(MMVManip *vm, int mapseed, u32 blockseed,
	v3pos_t nmin, v3pos_t nmax, biome_t *biomemap)
{
	PcgRandom pr(blockseed + 4234);
	MapNode n_ore(c_ore, 0, ore_param2);

	u16 max_height = column_height_max;
	pos_t y_start_min = nmin.Y + max_height;
	pos_t y_start_max = nmax.Y - max_height;

	pos_t y_start = y_start_min < y_start_max ?
		randomPosition(pr, y_start_min, y_start_max) :
		midpoint(y_start_min, y_start_max);

	if (!noise) {
		int sx = nmax.X - nmin.X + 1;
		int sz = nmax.Z - nmin.Z + 1;
		noise = new Noise(&np, 0, sx, sz);
	}
	noise->seed = static_cast<s32>(static_cast<u32>(mapseed) + static_cast<u32>(y_start));
	noise->noiseMap2D(nmin.X, nmin.Z);

	size_t index = 0;
	for (s64 z = nmin.Z; z <= nmax.Z; z++)
	for (s64 x = nmin.X; x <= nmax.X; x++, index++) {
		float noiseval = noise->result[index];
		if (noiseval < nthresh)
			continue;

		if (biomemap && !biomes.empty()) {
			auto it = biomes.find(biomemap[index]);
			if (it == biomes.end())
				continue;
		}

		u16 height = pr.range(column_height_min, column_height_max);
		pos_t ymidpoint = addNoiseOffset(y_start, noiseval);
		pos_t y0 = MYMAX(nmin.Y, addNoiseOffset(ymidpoint,
				-height * (1 - column_midpoint_factor)));
		pos_t y1 = MYMIN(nmax.Y, y0 + height - 1);

		for (s64 y = y0; y <= y1; y++) {
			u32 i = vm->m_area.index(x, y, z);
			if (!vm->m_area.contains(i))
				continue;
			if (!CONTAINS(c_wherein, vm->m_data[i].getContent()))
				continue;

			vm->m_data[i] = n_ore;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////


OrePuff::~OrePuff()
{
	delete noise_puff_top;
	delete noise_puff_bottom;
}


ObjDef *OrePuff::clone() const
{
	auto def = new OrePuff();
	Ore::cloneTo(def);

	def->np_puff_top = np_puff_top;
	def->np_puff_bottom = np_puff_bottom;
	def->noise_puff_top = nullptr; // cannot be shared, on-demand
	def->noise_puff_bottom = nullptr;

	return def;
}


void OrePuff::generate(MMVManip *vm, int mapseed, u32 blockseed,
	v3pos_t nmin, v3pos_t nmax, biome_t *biomemap)
{
	PcgRandom pr(blockseed + 4234);
	MapNode n_ore(c_ore, 0, ore_param2);

	pos_t y_start = randomPosition(pr, nmin.Y, nmax.Y);

	if (!noise) {
		int sx = nmax.X - nmin.X + 1;
		int sz = nmax.Z - nmin.Z + 1;
		noise = new Noise(&np, 0, sx, sz);
		noise_puff_top = new Noise(&np_puff_top, 0, sx, sz);
		noise_puff_bottom = new Noise(&np_puff_bottom, 0, sx, sz);
	}

	noise->seed = static_cast<s32>(static_cast<u32>(mapseed) + static_cast<u32>(y_start));
	noise->noiseMap2D(nmin.X, nmin.Z);
	bool noise_generated = false;

	size_t index = 0;
	for (s64 z = nmin.Z; z <= nmax.Z; z++)
	for (s64 x = nmin.X; x <= nmax.X; x++, index++) {
		float noiseval = noise->result[index];
		if (noiseval < nthresh)
			continue;

		if (biomemap && !biomes.empty()) {
			auto it = biomes.find(biomemap[index]);
			if (it == biomes.end())
				continue;
		}

		if (!noise_generated) {
			noise_generated = true;
			noise_puff_top->noiseMap2D(nmin.X, nmin.Z);
			noise_puff_bottom->noiseMap2D(nmin.X, nmin.Z);
		}

		float ntop    = noise_puff_top->result[index];
		float nbottom = noise_puff_bottom->result[index];

		if (!(flags & OREFLAG_PUFF_CLIFFS)) {
			float ndiff = noiseval - nthresh;
			if (ndiff < 1.0f) {
				ntop *= ndiff;
				nbottom *= ndiff;
			}
		}

		pos_t ymid = y_start;
		pos_t y0 = addNoiseOffset(ymid, -nbottom);
		pos_t y1 = addNoiseOffset(ymid, ntop);

		if ((flags & OREFLAG_PUFF_ADDITIVE) && (y0 > y1))
			std::swap(y0, y1);

		for (s64 y = y0; y <= y1; y++) {
			u32 i = vm->m_area.index(x, y, z);
			if (!vm->m_area.contains(i))
				continue;
			if (!CONTAINS(c_wherein, vm->m_data[i].getContent()))
				continue;

			vm->m_data[i] = n_ore;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////


ObjDef *OreBlob::clone() const
{
	auto def = new OreBlob();
	Ore::cloneTo(def);
	return def;
}


void OreBlob::generate(MMVManip *vm, int mapseed, u32 blockseed,
	v3pos_t nmin, v3pos_t nmax, biome_t *biomemap)
{
	PcgRandom pr(blockseed + 2404);
	MapNode n_ore(c_ore, 0, ore_param2);

	u32 sizex  = (nmax.X - nmin.X + 1);
	u32 volume = (nmax.X - nmin.X + 1) *
				 (nmax.Y - nmin.Y + 1) *
				 (nmax.Z - nmin.Z + 1);
	u32 csize  = clust_size;
	u32 nblobs = volume / clust_scarcity;

	if (!noise)
		noise = new Noise(&np, mapseed, csize, csize, csize);

	for (u32 i = 0; i != nblobs; i++) {
		pos_t x0 = randomPosition(pr, nmin.X, nmax.X - csize + 1);
		pos_t y0 = randomPosition(pr, nmin.Y, nmax.Y - csize + 1);
		pos_t z0 = randomPosition(pr, nmin.Z, nmax.Z - csize + 1);

		if (biomemap && !biomes.empty()) {
			u32 bmapidx = sizex * (z0 - nmin.Z) + (x0 - nmin.X);
			auto it = biomes.find(biomemap[bmapidx]);
			if (it == biomes.end())
				continue;
		}

		bool noise_generated = false;
		noise->seed = blockseed + i;

		size_t index = 0;
		for (u32 z1 = 0; z1 != csize; z1++)
		for (u32 y1 = 0; y1 != csize; y1++)
		for (u32 x1 = 0; x1 != csize; x1++, index++) {
			u32 i = vm->m_area.index(x0 + x1, y0 + y1, z0 + z1);
			if (!CONTAINS(c_wherein, vm->m_data[i].getContent()))
				continue;

			// Lazily generate noise only if there's a chance of ore being placed
			// This simple optimization makes calls 6x faster on average
			if (!noise_generated) {
				noise_generated = true;
				noise->noiseMap3D(x0, y0, z0);
			}

			float noiseval = noise->result[index];

			float xdist = (s32)x1 - (s32)csize / 2;
			float ydist = (s32)y1 - (s32)csize / 2;
			float zdist = (s32)z1 - (s32)csize / 2;

			noiseval -= std::sqrt(xdist * xdist + ydist * ydist + zdist * zdist) / csize;

			if (noiseval < nthresh)
				continue;

			vm->m_data[i] = n_ore;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////


OreVein::~OreVein()
{
	delete noise2;
}


ObjDef *OreVein::clone() const
{
	auto def = new OreVein();
	Ore::cloneTo(def);

	def->random_factor = random_factor;
	def->noise2 = nullptr; // cannot be shared, on-demand
	def->sizey_prev = sizey_prev;

	return def;
}


void OreVein::generate(MMVManip *vm, int mapseed, u32 blockseed,
	v3pos_t nmin, v3pos_t nmax, biome_t *biomemap)
{
	PcgRandom pr(blockseed + 520);
	MapNode n_ore(c_ore, 0, ore_param2);

	int sizex = nmax.X - nmin.X + 1;
	int sizey = nmax.Y - nmin.Y + 1;
	// Because this ore uses 3D noise the noisemap Y size can be different in
	// different mapchunks due to ore Y limits. So recreate the noise objects
	// if Y size has changed.
	// Because these noise objects are created multiple times for this ore type
	// it is necessary to 'delete' them here.
	if (!noise || sizey != sizey_prev) {
		delete noise;
		delete noise2;
		int sizez = nmax.Z - nmin.Z + 1;
		noise  = new Noise(&np, mapseed, sizex, sizey, sizez);
		noise2 = new Noise(&np, mapseed + 436, sizex, sizey, sizez);
		sizey_prev = sizey;
	}

	bool noise_generated = false;
	size_t index = 0;
	for (s64 z = nmin.Z; z <= nmax.Z; z++)
	for (s64 y = nmin.Y; y <= nmax.Y; y++)
	for (s64 x = nmin.X; x <= nmax.X; x++, index++) {
		u32 i = vm->m_area.index(x, y, z);
		if (!vm->m_area.contains(i))
			continue;
		if (!CONTAINS(c_wherein, vm->m_data[i].getContent()))
			continue;

		if (biomemap && !biomes.empty()) {
			u32 bmapidx = sizex * (z - nmin.Z) + (x - nmin.X);
			auto it = biomes.find(biomemap[bmapidx]);
			if (it == biomes.end())
				continue;
		}

		// Same lazy generation optimization as in OreBlob
		if (!noise_generated) {
			noise_generated = true;
			noise->noiseMap3D(nmin.X, nmin.Y, nmin.Z);
			noise2->noiseMap3D(nmin.X, nmin.Y, nmin.Z);
		}

		// randval ranges from -1..1
		/*
			Note: can generate values slightly larger than 1
			but this can't be changed as mapgen must be deterministic across versions.
		*/
		float randval   = (float)pr.next() / float(pr.RANDOM_RANGE / 2) - 1.f;
		float noiseval  = contour(noise->result[index]);
		float noiseval2 = contour(noise2->result[index]);
		if (noiseval * noiseval2 + randval * random_factor < nthresh)
			continue;

		vm->m_data[i] = n_ore;
	}
}


///////////////////////////////////////////////////////////////////////////////


OreStratum::~OreStratum()
{
	delete noise_stratum_thickness;
}


ObjDef *OreStratum::clone() const
{
	auto def = new OreStratum();
	Ore::cloneTo(def);

	def->np_stratum_thickness = np_stratum_thickness;
	def->noise_stratum_thickness = nullptr; // cannot be shared, on-demand
	def->stratum_thickness = stratum_thickness;

	return def;
}


void OreStratum::generate(MMVManip *vm, int mapseed, u32 blockseed,
	v3pos_t nmin, v3pos_t nmax, biome_t *biomemap)
{
	PcgRandom pr(blockseed + 4234);
	MapNode n_ore(c_ore, 0, ore_param2);

	if (flags & OREFLAG_USE_NOISE) {
		if (!noise) {
			int sx = nmax.X - nmin.X + 1;
			int sz = nmax.Z - nmin.Z + 1;
			noise = new Noise(&np, 0, sx, sz);
		}
		noise->noiseMap2D(nmin.X, nmin.Z);
	}

	if (flags & OREFLAG_USE_NOISE2) {
		if (!noise_stratum_thickness) {
			int sx = nmax.X - nmin.X + 1;
			int sz = nmax.Z - nmin.Z + 1;
			noise_stratum_thickness = new Noise(&np_stratum_thickness, 0, sx, sz);
		}
		noise_stratum_thickness->noiseMap2D(nmin.X, nmin.Z);
	}

	size_t index = 0;

	for (s64 z = nmin.Z; z <= nmax.Z; z++)
	for (s64 x = nmin.X; x <= nmax.X; x++, index++) {
		if (biomemap && !biomes.empty()) {
			auto it = biomes.find(biomemap[index]);
			if (it == biomes.end())
				continue;
		}

		pos_t y0;
		pos_t y1;

		if (flags & OREFLAG_USE_NOISE) {
			float nhalfthick = ((flags & OREFLAG_USE_NOISE2) ?
				noise_stratum_thickness->result[index] : (float)stratum_thickness) /
				2.0f;
			float nmid = noise->result[index];
			const noise_coord_t low = std::ceil(nmid - nhalfthick);
			const noise_coord_t high = std::trunc(nmid + nhalfthick);
			if (low > nmax.Y || high < nmin.Y)
				continue;
			y0 = low < nmin.Y ? nmin.Y : static_cast<pos_t>(low);
			y1 = high > nmax.Y ? nmax.Y : static_cast<pos_t>(high);
		} else { // Simple horizontal stratum
			y0 = nmin.Y;
			y1 = nmax.Y;
		}

		for (s64 y = y0; y <= y1; y++) {
			if (pr.range(1, clust_scarcity) != 1)
				continue;

			u32 i = vm->m_area.index(x, y, z);
			if (!vm->m_area.contains(i))
				continue;
			if (!CONTAINS(c_wherein, vm->m_data[i].getContent()))
				continue;

			vm->m_data[i] = n_ore;
		}
	}
}
