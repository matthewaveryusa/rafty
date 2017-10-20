#pragma once

#include <nn.hpp>
#include <lmdb.h>
#include <rafty.pb.h>

namespace rafty {
  class Storage {
    public:
    Storage();
    void init();

    bool get(uint64_t index, Value *val) const;
    Entries get_since(uint64_t index) const;

    void put(const Value &val);
    void put(const Entries &vals);
    void delete_since(uint64_t index);

    bool state_get(const char* key, std::string *val) const;
    void state_put(const char* key, const MDB_val& val);

   private:
    mutable MDB_env *_env;
    mutable MDB_dbi _keydb;
    mutable MDB_dbi _garbagedb;
    mutable MDB_dbi _statedb;
    size_t _next_index;
  };
}
