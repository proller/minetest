/*
event.h
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
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

#ifndef EVENT_HEADER
#define EVENT_HEADER

class MtEvent
{
public:
	virtual ~MtEvent(){};
	//virtual MtEvent* clone(){ return new IEvent; }
	virtual const char* getType() const = 0;

	MtEvent* checkIs(const std::string &type)
	{
		if(type == getType())
			return this;
		return NULL;
	}
};

// An event with no parameters and customizable name
class SimpleTriggerEvent: public MtEvent
{
	const char *type;
public:
	SimpleTriggerEvent(const char *type):
		type(type)
	{}
	const char* getType() const
	{return type;}
};

class MtEventReceiver
{
public:
	virtual ~MtEventReceiver(){};
	virtual void onEvent(MtEvent *e) = 0;
};

typedef void (*event_receive_func)(MtEvent *e, void *data);

class MtEventManager
{
public:
	virtual ~MtEventManager(){};
	virtual void put(MtEvent *e) = 0;
	virtual void reg(const char *type, event_receive_func f, void *data) = 0;
	// If data==NULL, every occurence of f is deregistered.
	virtual void dereg(const char *type, event_receive_func f, void *data) = 0;
	virtual void reg(MtEventReceiver *r, const char *type) = 0;
	virtual void dereg(MtEventReceiver *r, const char *type) = 0;
};

#endif

