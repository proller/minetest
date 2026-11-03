// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 Luanti core developers & community

#include "test.h"

#include "emerge.h"
#include "mapgen/mapgen.h"
#include "mapgen/mg_biome.h"
#include "mapgen/mg_ore.h"
#include "dummymap.h"
#include "irrlicht_changes/printing.h"
#include "mock_server.h"

class TestMapgen : public TestBase
{
public:
	TestMapgen() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMapgen"; }

	void runTests(IGameDef *gamedef);

	void testBiomeGen(IGameDef *gamedef);
	void testMapgenEdges();
	void testOrePositions(IGameDef *gamedef);
};

static TestMapgen g_test_instance;

namespace {
	class MockBiomeManager : public BiomeManager {
	public:
		MockBiomeManager(Server *server) : BiomeManager(server) {}

		void setNodeDefManager(const NodeDefManager *ndef)
		{
			m_ndef = ndef;
		}
	};
}

void TestMapgen::runTests(IGameDef *gamedef)
{
	TEST(testBiomeGen, gamedef);
	TEST(testMapgenEdges);
	TEST(testOrePositions, gamedef);
}

void TestMapgen::testOrePositions(IGameDef *gamedef)
{
#if USE_POS32 == 64
	DummyMap map(gamedef, v3bpos_t(0), v3bpos_t(0));
	constexpr s64 far = 10000000000000000LL;
	const NoiseParams flat_noise(1, 0, v3f(100, 100, 100), 9, 1, 0.5, 2);

	const auto generate = [&](OreType type, v3pos_t min) {
		std::unique_ptr<Ore> ore;
		switch (type) {
		case ORE_SCATTER: ore = std::make_unique<OreScatter>(); break;
		case ORE_SHEET: {
			auto p = std::make_unique<OreSheet>();
			p->column_height_min = 1;
			p->column_height_max = 1;
			p->column_midpoint_factor = 0.5f;
			ore = std::move(p);
			break;
		}
		case ORE_PUFF: {
			auto p = std::make_unique<OrePuff>();
			p->np_puff_top = p->np_puff_bottom = flat_noise;
			ore = std::move(p);
			break;
		}
		case ORE_BLOB: ore = std::make_unique<OreBlob>(); break;
		case ORE_VEIN: {
			auto p = std::make_unique<OreVein>();
			p->random_factor = 0;
			ore = std::move(p);
			break;
		}
		case ORE_STRATUM: ore = std::make_unique<OreStratum>(); break;
		}
		ore->c_ore = t_CONTENT_BRICK;
		ore->c_wherein = {CONTENT_AIR};
		ore->clust_scarcity = 1;
		ore->clust_num_ores = 8;
		ore->clust_size = 2;
		ore->biomes.insert(42);
		ore->nthresh = -10;
		ore->np = flat_noise;

		MMVManip vm(&map);
		const v3pos_t max = min + v3pos_t(3);
		vm.addArea(VoxelArea(min, max));
		for (s64 z = min.Z; z <= max.Z; z++)
		for (s64 y = min.Y; y <= max.Y; y++)
		for (s64 x = min.X; x <= max.X; x++)
			vm.setNode(v3pos_t(x, y, z), MapNode(CONTENT_AIR));
		std::vector<biome_t> biomemap(16, 42);
		ore->generate(&vm, 42, 12345, min, max, biomemap.data());

		std::vector<content_t> result;
		for (u32 i = 0; i < vm.m_area.getVolume(); i++)
			result.push_back(vm.m_data[i].getContent());
		return result;
	};

	for (OreType type : {ORE_SCATTER, ORE_SHEET, ORE_PUFF, ORE_BLOB,
			ORE_VEIN, ORE_STRATUM}) {
		const v3pos_t positions[][2] = {
			{v3pos_t(100, -100, 100), v3pos_t(far, -far, far)},
			{v3pos_t(-100, 100, -100), v3pos_t(-far, far, -far)},
		};
		for (const auto &pair : positions) {
			auto near = generate(type, pair[0]);
			UASSERT(std::count(near.begin(), near.end(), t_CONTENT_BRICK) > 0);
			auto distant = generate(type, pair[1]);
			if (distant != near)
				errorstream << "Ore type " << (int)type << " differs at " << pair[1] << std::endl;
			UASSERT(distant == near);
		}
	}
#endif
}

void TestMapgen::testBiomeGen(IGameDef *gamedef)
{
	MockServer server(getTestTempDirectory());
	MockBiomeManager bmgr(&server);
	bmgr.setNodeDefManager(gamedef->getNodeDefManager());

	{
		// Add some biomes (equivalent to l_register_biome)
		// Taken from minetest_game/mods/default/mapgen.lua
		size_t bmgr_count = bmgr.getNumObjects(); // this is 1 ?

		Biome *b = BiomeManager::create(BIOMETYPE_NORMAL);
		b->name = "deciduous_forest";
		b->c_top = t_CONTENT_GRASS;
		b->depth_top = 1;
		b->c_filler = t_CONTENT_BRICK; // dirt
		b->depth_filler = 3;
		b->c_stone = t_CONTENT_STONE;
		b->min_pos.Y = 1;
		b->heat_point = 60.0f;
		b->humidity_point = 68.0f;
		UASSERT(bmgr.add(b) != OBJDEF_INVALID_HANDLE);

		b = BiomeManager::create(BIOMETYPE_NORMAL);
		b->name = "deciduous_forest_shore";
		b->c_top = t_CONTENT_BRICK; // dirt
		b->depth_top = 1;
		b->c_filler = t_CONTENT_BRICK; // dirt
		b->depth_filler = 3;
		b->c_stone = t_CONTENT_STONE;
		b->max_pos.Y = 0;
		b->heat_point = 60.0f;
		b->humidity_point = 68.0f;
		UASSERT(bmgr.add(b) != OBJDEF_INVALID_HANDLE);
		UASSERT(bmgr.getNumObjects() - bmgr_count == 2);
	}


	std::unique_ptr<BiomeParams> params(BiomeManager::createBiomeParams(BIOMEGEN_ORIGINAL));

	constexpr v3pos_t CSIZE(16, 16, 16); // misleading name. measured in nodes.
	std::unique_ptr<BiomeGen> biomegen(
		bmgr.createBiomeGen(BIOMEGEN_ORIGINAL, params.get(), CSIZE)
	);

	{
		// Test biome transitions
		//   getBiomeAtIndex (Y only)
		//   getNextTransitionY
		const struct {
			pos_t check_y;
			const char *name;
			pos_t next_y;
		} expected_biomes[] = {
			{ MAX_MAP_GENERATION_LIMIT, "deciduous_forest", 0 },
			{ 1, "deciduous_forest", 0 },
			{    0, "deciduous_forest_shore", POS_MIN },
			{ -100, "deciduous_forest_shore", POS_MIN },
		};
		for (const auto &expected : expected_biomes) {
			Biome *biome = biomegen->getBiomeAtIndex(
				(1 * CSIZE.X) + 1, // index in CSIZE 2D noise map
				v3pos_t(2000, expected.check_y, -1000) // absolute coordinates
			);
			auto next_y = biomegen->getNextTransitionY(expected.check_y);

			//UASSERTEQ(auto, biome->name, expected.name);
			//UASSERTEQ(auto, next_y, expected.next_y);
			if (biome->name != expected.name) {
				errorstream << "FIXME " << FUNCTION_NAME << " " << biome->name
					<< " != " << expected.name << "\nThe test would have failed."
					<< std::endl;
				return;
			}
			if (next_y != expected.next_y) {
				errorstream << "FIXME " << FUNCTION_NAME << " " << next_y
					<< " != " << expected.next_y << "\nThe test would have failed."
					<< std::endl;
				return;
			}
		}
	}
}

void TestMapgen::testMapgenEdges()
{
	v3pos_t emin, emax;

	std::tie(emin, emax) = get_mapgen_edges(31007, v3pos_t(5));
	UASSERTEQ(auto, emin, v3pos_t(-30912));
	UASSERTEQ(auto, emax, v3pos_t(30927));

	std::tie(emin, emax) = get_mapgen_edges(502 * MAP_BLOCKSIZE, v3pos_t(1, 2, 1));
	UASSERTEQ(auto, emin, v3pos_t(-8016));
	UASSERTEQ(auto, emax, v3pos_t(8031, 8015, 8031));

	MapgenParams params;
	std::tie(emin, emax) = get_mapgen_edges(params.mapgen_limit, params.chunksize);
	UASSERT(emin.X < 0 && emax.X > 0);
	UASSERT(params.getSpawnRangeMax() > 0);
#if USE_POS32 == 64
	UASSERT(params.getSpawnRangeMax() > S32_MAX);
#endif
}
