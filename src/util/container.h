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

#ifndef UTIL_CONTAINER_HEADER
#define UTIL_CONTAINER_HEADER

#include "../irrlichttypes.h"
#include "../exceptions.h"
#include "../jthread/jmutex.h"
#include "../jthread/jmutexautolock.h"
#include "../jthread/jsemaphore.h"
#include <list>
#include <vector>
#include <map>
#include <set>
#include <queue>

/*
Queue with unique values with fast checking of value existence
*/

template<typename Value>
class UniqueQueue
{
public:

	/*
	Does nothing if value is already queued.
	Return value:
	true: value added
	false: value already exists
	*/
	bool push_back(const Value& value)
	{
		if (m_set.insert(value).second)
		{
			m_queue.push(value);
			return true;
		}
		return false;
	}

	void pop_front()
	{
		m_set.erase(m_queue.front());
		m_queue.pop();
	}

	const Value& front() const
	{
		return m_queue.front();
	}

	u32 size() const
	{
		return m_queue.size();
	}

private:
	std::set<Value> m_set;
	std::queue<Value> m_queue;
};

template<typename Key, typename Value>
class MutexedMap
{
public:
	MutexedMap()
	{
	}

	void set(const Key &name, const Value &value)
	{
		JMutexAutoLock lock(m_mutex);

		m_values[name] = value;
	}

	bool get(const Key &name, Value *result)
	{
		JMutexAutoLock lock(m_mutex);

		typename std::map<Key, Value>::iterator n;
		n = m_values.find(name);

		if(n == m_values.end())
			return false;

		if(result != NULL)
			*result = n->second;

		return true;
	}

	std::vector<Value> getValues()
	{
		std::vector<Value> result;
		for(typename std::map<Key, Value>::iterator
			i = m_values.begin();
			i != m_values.end(); ++i){
			result.push_back(i->second);
		}
		return result;
	}

	void clear ()
	{
		m_values.clear();
	}

private:
	std::map<Key, Value> m_values;
	JMutex m_mutex;
};

/*
Generates ids for comparable values.
Id=0 is reserved for "no value".

Is fast at:
- Returning value by id (very fast)
- Returning id by value
- Generating a new id for a value

Is not able to:
- Remove an id/value pair (is possible to implement but slow)
*/
template<typename T>
class MutexedIdGenerator
{
public:
	MutexedIdGenerator()
	{
	}

	// Returns true if found
	bool getValue(u32 id, T &value)
	{
		if(id == 0)
			return false;
		JMutexAutoLock lock(m_mutex);
		if(m_id_to_value.size() < id)
			return false;
		value = m_id_to_value[id-1];
		return true;
	}

	// If id exists for value, returns the id.
	// Otherwise generates an id for the value.
	u32 getId(const T &value)
	{
		JMutexAutoLock lock(m_mutex);
		typename std::map<T, u32>::iterator n;
		n = m_value_to_id.find(value);
		if(n != m_value_to_id.end())
			return n->second;
		m_id_to_value.push_back(value);
		u32 new_id = m_id_to_value.size();
		m_value_to_id.insert(value, new_id);
		return new_id;
	}

private:
	JMutex m_mutex;
	// Values are stored here at id-1 position (id 1 = [0])
	std::vector<T> m_id_to_value;
	std::map<T, u32> m_value_to_id;
};

/*
Thread-safe FIFO queue (well, actually a FILO also)
*/

template<typename T>
class MutexedQueue
{
public:
	template<typename Key, typename U, typename Caller, typename CallerData>
	friend class RequestQueue;

	MutexedQueue()
	{
	}
	bool empty()
	{
		JMutexAutoLock lock(m_mutex);
		return (m_queue.size() == 0);
	}
	void push_back(T t)
	{
		JMutexAutoLock lock(m_mutex);
		m_queue.push_back(t);
		m_size.Post();
	}

	/* this version of pop_front returns a empty element of T on timeout.
	* Make sure default constructor of T creates a recognizable "empty" element
	*/
	T pop_frontNoEx(u32 wait_time_max_ms)
	{
		if (m_size.Wait(wait_time_max_ms)) {
			JMutexAutoLock lock(m_mutex);

			T t = m_queue.front();
			m_queue.pop_front();
			return t;
		}
		else {
			return T();
		}
	}

	T pop_front(u32 wait_time_max_ms)
	{
		if (m_size.Wait(wait_time_max_ms)) {
			JMutexAutoLock lock(m_mutex);

			T t = m_queue.front();
			m_queue.pop_front();
			return t;
		}
		else {
			throw ItemNotFoundException("MutexedQueue: queue is empty");
		}
	}

	T pop_frontNoEx()
	{
		m_size.Wait();

		JMutexAutoLock lock(m_mutex);

		T t = m_queue.front();
		m_queue.pop_front();
		return t;
	}

	T pop_back(u32 wait_time_max_ms=0)
	{
		if (m_size.Wait(wait_time_max_ms)) {
			JMutexAutoLock lock(m_mutex);

			T t = m_queue.back();
			m_queue.pop_back();
			return t;
		}
		else {
			throw ItemNotFoundException("MutexedQueue: queue is empty");
		}
	}

	/* this version of pop_back returns a empty element of T on timeout.
	* Make sure default constructor of T creates a recognizable "empty" element
	*/
	T pop_backNoEx(u32 wait_time_max_ms=0)
	{
		if (m_size.Wait(wait_time_max_ms)) {
			JMutexAutoLock lock(m_mutex);

			T t = m_queue.back();
			m_queue.pop_back();
			return t;
		}
		else {
			return T();
		}
	}

	T pop_backNoEx()
	{
		m_size.Wait();

		JMutexAutoLock lock(m_mutex);

		T t = m_queue.back();
		m_queue.pop_back();
		return t;
	}

protected:
	JMutex & getMutex()
	{
		return m_mutex;
	}

	std::deque<T> & getQueue()
	{
		return m_queue;
	}

	std::deque<T> m_queue;
	JMutex m_mutex;
	JSemaphore m_size;
};

#endif

