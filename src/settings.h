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

#ifndef SETTINGS_HEADER
#define SETTINGS_HEADER

#include "irrlichttypes_bloated.h"
#include "util/string.h"
#include "jthread/jmutex.h"
#include <string>
#include <map>
#include <list>
#include <set>

class Settings;
struct NoiseParams;

/** function type to register a changed callback */
typedef void (*setting_changed_callback)(const std::string);

enum ValueType {
	VALUETYPE_STRING,
	VALUETYPE_FLAG // Doesn't take any arguments
};

enum SettingsParseEvent {
	SPE_NONE,
	SPE_INVALID,
	SPE_COMMENT,
	SPE_KVPAIR,
	SPE_END,
	SPE_GROUP,
	SPE_MULTILINE,
};

struct ValueSpec {
	ValueSpec(ValueType a_type, const char *a_help=NULL)
	{
		type = a_type;
		help = a_help;
	}
	ValueType type;
	const char *help;
};

/** function type to register a changed callback */

struct SettingsEntry {
	SettingsEntry()
	{
		group = NULL;
	}

	SettingsEntry(const std::string &value_)
	{
		value = value_;
		group = NULL;
	}

	SettingsEntry(Settings *group_)
	{
		group = group_;
	}

	SettingsEntry(const std::string &value_, Settings *group_)
	{
		value = value_;
		group = group_;
	}

	std::string value;
	Settings *group;
};


class Settings {
public:
	Settings() {}
	~Settings();

	Settings & operator += (const Settings &other);
	Settings & operator = (const Settings &other);


	/***********************
	 * Reading and writing *
	 ***********************/

	// Read configuration file.  Returns success.
	bool readConfigFile(const char *filename);
	//Updates configuration file.  Returns success.
	bool updateConfigFile(const char *filename);
	// NOTE: Types of allowed_options are ignored.  Returns success.
	bool parseCommandLine(int argc, char *argv[],
			std::map<std::string, ValueSpec> &allowed_options);
	bool parseConfigLines(std::istream &is, const std::string &end = "");
	void writeLines(std::ostream &os, u32 tab_depth=0) const;

	SettingsParseEvent parseConfigObject(const std::string &line,
		const std::string &end, std::string &name, std::string &value);
	void getNamesPresent(std::istream &is, const std::string &end,
		std::set<std::string> &present_values,
		std::set<std::string> &present_groups);
	bool updateConfigObject(std::istream &is, std::ostream &os,
		const std::string &end, u32 tab_depth=0);

	static std::string getMultiline(std::istream &is, size_t *num_lines=NULL);
	static std::string sanitizeString(const std::string &value);
	static bool printEntry(std::ostream &os, const std::string &name,
		const SettingsEntry &entry, u32 tab_depth=0);
	static void printValue(std::ostream &os, const std::string &name,
		const std::string &value, u32 tab_depth=0);
	static void printGroup(std::ostream &os, const std::string &name,
		const Settings *group, u32 tab_depth=0);

	/***********
	 * Getters *
	 ***********/

	const SettingsEntry &getEntry(const std::string &name) const;
	Settings *getGroup(const std::string &name) const;
	std::string get(const std::string &name) const;
	bool getBool(const std::string &name) const;
	u16 getU16(const std::string &name) const;
	s16 getS16(const std::string &name) const;
	s32 getS32(const std::string &name) const;
	u64 getU64(const std::string &name) const;
	float getFloat(const std::string &name) const;
	v2f getV2F(const std::string &name) const;
	v3f getV3F(const std::string &name) const;
	u32 getFlagStr(const std::string &name, const FlagDesc *flagdesc,
			u32 *flagmask) const;
	// N.B. if getStruct() is used to read a non-POD aggregate type,
	// the behavior is undefined.
	bool getStruct(const std::string &name, const std::string &format,
			void *out, size_t olen) const;
	bool getNoiseParams(const std::string &name, NoiseParams &np) const;
	bool getNoiseParamsFromValue(const std::string &name, NoiseParams &np) const;
	bool getNoiseParamsFromGroup(const std::string &name, NoiseParams &np) const;

	// return all keys used
	std::vector<std::string> getNames() const;
	bool exists(const std::string &name) const;


	/***************************************
	 * Getters that don't throw exceptions *
	 ***************************************/

	bool getEntryNoEx(const std::string &name, SettingsEntry &val) const;
	bool getGroupNoEx(const std::string &name, Settings *&val) const;
	bool getNoEx(const std::string &name, std::string &val) const;
	bool getFlag(const std::string &name) const;
	bool getU16NoEx(const std::string &name, u16 &val) const;
	bool getS16NoEx(const std::string &name, s16 &val) const;
	bool getS32NoEx(const std::string &name, s32 &val) const;
	bool getU64NoEx(const std::string &name, u64 &val) const;
	bool getFloatNoEx(const std::string &name, float &val) const;
	bool getV2FNoEx(const std::string &name, v2f &val) const;
	bool getV3FNoEx(const std::string &name, v3f &val) const;
	// N.B. getFlagStrNoEx() does not set val, but merely modifies it.  Thus,
	// val must be initialized before using getFlagStrNoEx().  The intention of
	// this is to simplify modifying a flags field from a default value.
	bool getFlagStrNoEx(const std::string &name, u32 &val, FlagDesc *flagdesc) const;


	/***********
	 * Setters *
	 ***********/

	// N.B. Groups not allocated with new must be set to NULL in the settings
	// tree before object destruction.
	void set(const std::string &name, const std::string &value);
	void setGroup(const std::string &name, Settings *group);
	void setDefault(const std::string &name, const std::string &value);
	void setGroupDefault(const std::string &name, Settings *group);
	void setBool(const std::string &name, bool value);
	void setS16(const std::string &name, s16 value);
	void setU16(const std::string &name, u16 value);
	void setS32(const std::string &name, s32 value);
	void setU64(const std::string &name, u64 value);
	void setFloat(const std::string &name, float value);
	void setV2F(const std::string &name, v2f value);
	void setV3F(const std::string &name, v3f value);
	void setFlagStr(const std::string &name, u32 flags,
		const FlagDesc *flagdesc, u32 flagmask);
	void setNoiseParams(const std::string &name, const NoiseParams &np);
	// N.B. if setStruct() is used to write a non-POD aggregate type,
	// the behavior is undefined.
	bool setStruct(const std::string &name, const std::string &format, void *value);

	// remove a setting
	bool remove(const std::string &name);
	void clear();
	void updateValue(const Settings &other, const std::string &name);
	void update(const Settings &other);
	void registerChangedCallback(std::string name, setting_changed_callback cbf);

private:

	void updateNoLock(const Settings &other);
	void clearNoLock();

	void doCallbacks(std::string name);

	std::map<std::string, SettingsEntry> m_settings;
	std::map<std::string, SettingsEntry> m_defaults;
	std::map<std::string, std::vector<setting_changed_callback> > m_callbacks;
	// All methods that access m_settings/m_defaults directly should lock this.
	mutable JMutex m_mutex;
};

#endif

