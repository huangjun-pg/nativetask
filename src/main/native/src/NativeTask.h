/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NATIVETASK_H_
#define NATIVETASK_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include "NativeObject.h"

/**
 * Entrance for a object library, all user level object
 * libraries should provide a function of this type,
 * with function name "<LIBRARYNAME>CreateObject"
 * Notice: the return object must be an instance of
 *         type NativeObject or NULL
 */
typedef void * (*CreateObjectFunc)(const char * name);

/**
 * Entrance for a user module, all user level application
 * module should provide a function of this type with
 * the name of "<LIBRARYNAME>Init"
 */
typedef int32_t (*InitLibraryFunc)();


namespace Hadoop {

using std::string;
using std::vector;
using std::map;
using std::pair;

/**
 * Exceptions
 */
class HadoopException: public std::exception {
private:
  std::string _reason;
public:
  HadoopException(const string & what);
  virtual ~HadoopException() throw () { }

  virtual const char* what() const throw () {
    return _reason.c_str();
  }
};

class OutOfMemoryException: public HadoopException {
public:
  OutOfMemoryException(const string & what) :
    HadoopException(what) {
  }
};

class IOException: public HadoopException {
public:
  IOException(const string & what) :
    HadoopException(what) {
  }
};

class UnsupportException : public HadoopException {
public:
  UnsupportException(const string & what) :
    HadoopException(what) {
  }
};

class Config {
protected:
  map<string, string> _configs;
public:
  Config() {}
  ~Config() {}

  const char * get(const string & name);

  string get(const string & name, const string & defaultValue);

  bool getBool(const string & name, bool defaultValue);

  int64_t getInt(const string & name, int64_t defaultValue=-1);

  float getFloat(const string & name, float defaultValue=-1);

  void getStrings(const string & name, vector<string> & dest);

  void getInts(const string & name, vector<int64_t> & dest);

  void getFloats(const string & name, vector<float> & dest);

  void set(const string & key, const string & value);

  void setInt(const string & name, int64_t value);

  void setBool(const string & name, bool value);

  /**
   * Load configs from a config file with the following format:
   * # comment
   * key1=value1
   * key2=value2
   * ...
   */
  void load(const string & path);


  /**
   * Load configs form command line args
   * key1=value1 key2=value2,value2
   */
  void parse(int32_t argc, const char ** argv);
};

class Buffer {
protected:
  char * _data;
  uint32_t _length;

public:
  Buffer() :
    _data(NULL),
    _length(0) {
  }

  Buffer(char * data, uint32_t length) :
    _data(data),
    _length(length) {
  }

  ~Buffer() {}

  void reset(char * data, uint32_t length) {
    this->_data = data;
    this->_length = length;
  }

  char * data() const {
    return _data;
  }

  uint32_t length() const {
    return _length;
  }

  void data(char * data) {
    this->_data = data;
  }

  void length(uint32_t length) {
    this->_length = length;
  }

  string toString() const {
    return string(_data, _length);
  }
};

class InputSplit {
public:
  virtual uint64_t getLength() = 0;
  virtual vector<string> & getLocations() = 0;
  virtual void readFields(const string & data) = 0;
};

class Configurable : public NativeObject {
public:
  Configurable() {
  }

  virtual void configure(Config & config) {}
};

class Collector {
public:
  virtual void collect(const void * key, uint32_t keyLen,
                       const void * value, uint32_t valueLen) {
  }

  virtual void collect(const void * key, uint32_t keyLen,
                       const void * value, uint32_t valueLen,
                       int32_t partition) {
    collect(key, keyLen, value, valueLen);
  }
};

class Counter {
private:
  // not thread safe
  volatile uint64_t _count;

  string _group;
  string _name;
public:
  Counter(const string & group, const string & name) :
    _count(0),
    _group(group),
    _name(name) {
  }

  const string & group() const {
    return _group;
  }
  const string & name() const {
    return _name;
  }

  uint64_t get() const {
    return _count;
  }

  void increase() {
    _count++;
  }

  void increase(uint64_t cnt) {
    _count += cnt;
  }

  bool operator<(const Counter & rhs) const {
    if (_group == rhs._group) {
      return _name < rhs._name;
    }
    return _group < rhs._group;
  }
};

class RecordReader : public Configurable {
public:
  virtual NativeObjectType type() {
    return RecordReaderType;
  }

  virtual bool next(Buffer & key, Buffer & value) = 0;

  virtual float getProgress() = 0;

  virtual void close() = 0;
};

class RecordWriter : public Configurable {
public:
  virtual NativeObjectType type() {
    return RecordWriterType;
  }

  virtual void write(const Buffer & key, const Buffer & value) = 0;

  virtual void close() = 0;
};


class ProcessorBase : public Configurable {
private:
  Collector * _collector;
public:
  ProcessorBase():_collector(NULL) {
  }

  void setCollector(Collector * collector) {
    _collector = collector;
  }

  void collect(const void * key, uint32_t keyLen,
               const void * value, uint32_t valueLen) {
    _collector->collect(key, keyLen, value, valueLen);
  }

  void collect(const void * key, uint32_t keyLen,
               const void * value, uint32_t valueLen,
               int32_t partition) {
    _collector->collect(key, keyLen, value, valueLen, partition);
  }

  Counter * getCounter(const string & group, const string & name);

  virtual void close() {}
};

class Mapper: public ProcessorBase {
public:
  virtual NativeObjectType type() {
    return MapperType;
  }

  /**
   * Map interface, default IdenticalMapper
   */
  virtual void map(const char * key, uint32_t keyLen,
                   const char * value, uint32_t valueLen) {
    collect(key, keyLen, value, valueLen);
  }
};

class Partitioner: public Configurable {
public:
  virtual NativeObjectType type() {
    return PartitionerType;
  }

  /**
   * Partition interface
   * @param key key buffer
   * @param keyLen key length, can be modified to smaller value
   *               to truncate key
   * @return partition number
   */
  virtual uint32_t getPartition(const char * key, uint32_t & keyLen,
      uint32_t numPartition);
};

class KeyGroup {
public:
  /**
   * Get key of this input group
   */
  virtual const char * getKey(uint32_t & len) = 0;

  /**
   * Get next value of this input group
   * @return NULL if no more
   */
  virtual const char * nextValue(uint32_t & len) = 0;
};

class Reducer: public ProcessorBase {
public:
  virtual NativeObjectType type() {
    return ReducerType;
  }

  /**
   * Reduce interface, default IdenticalReducer
   */
  virtual void reduce(KeyGroup & input) {
    const char * key;
    const char * value;
    uint32_t keyLen;
    uint32_t valueLen;
    key = input.getKey(keyLen);
    while (NULL != (value=input.nextValue(valueLen))) {
      collect(key, keyLen, value, valueLen);
    }
  }
};


/**
 * Folder API used for hashtable based aggregation
 * Folder will be used in this way:
 * on(key, value):
 *   state = hashtable.get(key)
 *   if state == None:
 *     size = size()
 *     if size == -1:
 *       state = init(null, -1)
 *     elif size > 0:
 *       state = fixallocator.get(key)
 *       init(state, size)
 *   folder(state, value, value.len)
 *
 * final():
 *   for k,state in hashtable:
 *     final(key, key.len, state)
 */
class Folder : public ProcessorBase {
public:
  virtual NativeObjectType type() {
    return FolderType;
  }

  /**
   * Get aggregator state size
   * @return state storage size
   *         -1 size not fixed or unknown, default
   *            e.g. list map tree
   *         0  don't need to store state
   *         >0  fixed sized state
   *            e.g. int32 int64 float.
   */
  virtual int32_t size() {
    return -1;
  }

  /**
   * Create and/or init new state
   */
  virtual void * init(const char * key, uint32_t keyLen) {}

  /**
   * Aggregation function
   */
  virtual void folder(void * dest, const char * value, uint32_t valueLen) {}

  virtual void final(const char * key, uint32_t keyLen, void * dest) {}
};

} // namespace Hadoop;

#endif /* NATIVETASK_H_ */
