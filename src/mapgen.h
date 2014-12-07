/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

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

#ifndef MAPGEN_HEADER
#define MAPGEN_HEADER

#include "nodedef.h"
#include "mapnode.h"
#include "util/string.h"
#include "util/container.h"

#define DEFAULT_MAPGEN "v6"

/////////////////// Mapgen flags
#define MG_TREES         0x01
#define MG_CAVES         0x02
#define MG_DUNGEONS      0x04
#define MG_FLAT          0x08
#define MG_LIGHT         0x10

class Settings;
class ManualMapVoxelManipulator;
class INodeDefManager;

extern FlagDesc flagdesc_mapgen[];
extern FlagDesc flagdesc_gennotify[];

class Biome;
class EmergeManager;
class MapBlock;
class ManualMapVoxelManipulator;
class VoxelManipulator;
struct BlockMakeData;
class VoxelArea;
class Map;

enum MapgenObject {
	MGOBJ_VMANIP,
	MGOBJ_HEIGHTMAP,
	MGOBJ_BIOMEMAP,
	MGOBJ_HEATMAP,
	MGOBJ_HUMIDMAP,
	MGOBJ_GENNOTIFY
};

enum GenNotifyType {
	GENNOTIFY_DUNGEON,
	GENNOTIFY_TEMPLE,
	GENNOTIFY_CAVE_BEGIN,
	GENNOTIFY_CAVE_END,
	GENNOTIFY_LARGECAVE_BEGIN,
	GENNOTIFY_LARGECAVE_END,
	GENNOTIFY_DECORATION,
	NUM_GENNOTIFY_TYPES
};

struct GenNotifyEvent {
	GenNotifyType type;
	v3s16 pos;
	u32 id;
};

class GenerateNotifier {
public:
	GenerateNotifier();
	GenerateNotifier(u32 notify_on, std::set<u32> *notify_on_deco_ids);

	void setNotifyOn(u32 notify_on);
	void setNotifyOnDecoIds(std::set<u32> *notify_on_deco_ids);

	bool addEvent(GenNotifyType type, v3s16 pos, u32 id=0);
	void getEvents(std::map<std::string, std::vector<v3s16> > &event_map,
		bool peek_events=false);

private:
	u32 m_notify_on;
	std::set<u32> *m_notify_on_deco_ids;
	std::list<GenNotifyEvent> m_notify_events;
};

struct MapgenSpecificParams {
	virtual void readParams(Settings *settings) = 0;
	virtual void writeParams(Settings *settings) = 0;
	virtual ~MapgenSpecificParams() {}
};

struct MapgenParams {
	std::string mg_name;
	s16 chunksize;
	u64 seed;
	s16 water_level;
	u32 flags;

	MapgenSpecificParams *sparams;

	MapgenParams()
	{
		mg_name     = DEFAULT_MAPGEN;
		seed        = 0;
		water_level = 1;
		chunksize   = 5;
		flags       = MG_TREES | MG_CAVES | MG_LIGHT;
		sparams     = NULL;
	}
};

class Mapgen {
public:
	int seed;
	int water_level;
	u32 flags;
	bool generating;
	int id;
	ManualMapVoxelManipulator *vm;
	INodeDefManager *ndef;

	s16 *heightmap;
	u8 *biomemap;
	v3s16 csize;

	GenerateNotifier gennotify;

	Mapgen();
	Mapgen(int mapgenid, MapgenParams *params, EmergeManager *emerge);
	virtual ~Mapgen();

	s16 findGroundLevelFull(v2s16 p2d);
	s16 findGroundLevel(v2s16 p2d, s16 ymin, s16 ymax);
	void updateHeightmap(v3s16 nmin, v3s16 nmax);
	void updateLiquid(UniqueQueue<v3s16> *trans_liquid, v3s16 nmin, v3s16 nmax);
	void setLighting(v3s16 nmin, v3s16 nmax, u8 light);
	void lightSpread(VoxelArea &a, v3s16 p, u8 light);
	void calcLighting(v3s16 nmin, v3s16 nmax);
	void calcLightingOld(v3s16 nmin, v3s16 nmax);

	virtual void makeChunk(BlockMakeData *data) {}
	virtual int getGroundLevelAtPoint(v2s16 p) { return 0; }
};

struct MapgenFactory {
	virtual Mapgen *createMapgen(int mgid, MapgenParams *params,
		EmergeManager *emerge) = 0;
	virtual MapgenSpecificParams *createMapgenParams() = 0;
	virtual ~MapgenFactory() {}
};

class GenElement {
public:
	virtual ~GenElement() {}
	u32 id;
	std::string name;
};

class GenElementManager {
public:
	static const char *ELEMENT_TITLE;
	static const size_t ELEMENT_LIMIT = -1;

	GenElementManager() {}
	virtual ~GenElementManager();

	virtual GenElement *create(int type) = 0;

	virtual u32 add(GenElement *elem);
	virtual GenElement *get(u32 id);
	virtual GenElement *update(u32 id, GenElement *elem);
	virtual GenElement *remove(u32 id);
	virtual void clear();

	virtual GenElement *getByName(const std::string &name);

protected:
	std::vector<GenElement *> m_elements;
};

#endif
