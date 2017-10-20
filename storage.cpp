#include <storage.hpp>

#include <nn.hpp>
#include <rafty.pb.h>
#include <exception.hpp>

namespace rafty {

  Storage::Storage(): 
    _env(nullptr),
    _keydb(0),
    _garbagedb(0),
    _statedb(0) {
  }

  void Storage::init() {
    mdb_env_create(&_env);
    mdb_env_set_maxdbs(_env, 3);
    mdb_env_set_mapsize(_env, 1UL * 1024UL * 1024UL);
    mdb_env_open(_env,"./rafty.mdb", 0, 0664);
 
    MDB_txn *txn;
    mdb_txn_begin(_env, nullptr, 0, &txn);
    mdb_dbi_open(txn, "keys", MDB_CREATE, &_keydb);
    mdb_dbi_open(txn, "state", MDB_CREATE, &_statedb);
    mdb_dbi_open(txn, "garbage", MDB_CREATE, &_garbagedb);
    mdb_txn_commit(txn);
  }

  bool Storage::get(uint64_t index, rafty::Value *val) const {
    MDB_txn *txn;
    mdb_txn_begin(_env, nullptr, MDB_RDONLY, &txn);
    MDB_val k{sizeof(index), &index};
    MDB_val v;
    if(mdb_get(txn, _keydb, &k, &v) == MDB_NOTFOUND) {
      return false;
    }
    if(!val->ParseFromArray(v.mv_data, v.mv_size)) {
      throw exception("storage value parse error");
    }
    mdb_txn_abort(txn);
    return true;
  }

  Entries Storage::get_since(uint64_t index) const {
    Entries e;
    MDB_txn *txn;
    MDB_cursor *cursor;
    mdb_txn_begin(_env, nullptr, MDB_RDONLY, &txn);
    mdb_cursor_open(txn, _keydb, &cursor);
    MDB_val v;
    MDB_val k{sizeof(index), &index};
    while(mdb_cursor_get(cursor, &k, &v, MDB_NEXT) == 0) {
      if(!e.add_entries()->ParseFromArray(v.mv_data, v.mv_size)) {
        throw exception("storage value parse error");
      }
    }
    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);
    return e;
  }

  void Storage::delete_since(uint64_t index) {
    MDB_txn *txn;
    MDB_cursor *cursor;
    mdb_txn_begin(_env, nullptr, 0, &txn);
    mdb_cursor_open(txn, _keydb, &cursor);
    MDB_val k{sizeof(index), &index};
    MDB_val emptyval{0,nullptr};
    MDB_val v;
    mdb_cursor_get(cursor, &k, nullptr, MDB_SET_RANGE);
    while(mdb_cursor_get(cursor, &k, &v, MDB_NEXT) == 0) {
      mdb_put(txn, _garbagedb, &v, &emptyval, 0);
      mdb_cursor_del(cursor, 0);
    }
    mdb_cursor_close(cursor);
    mdb_txn_commit(txn);
    _next_index = index;
  }

  void Storage::put(const rafty::Value &val) {
    std::string s;
    if(!val.SerializeToString(&s)) {
      throw exception("storage can't serialize value");
    }
    MDB_txn *txn;
    uint64_t key = val.index();
    MDB_val k{sizeof(key), &key};
    MDB_val v{s.size(), &s[0]};
    mdb_txn_begin(_env, nullptr, 0, &txn);
    int flags = 0;
    if(_next_index == val.index()) {
      _next_index++;
      flags = MDB_APPEND;
    }
    _next_index = val.index() + 1;
    mdb_put(txn, _keydb, &k, &v, flags);
    mdb_txn_commit(txn);
  }

  void Storage::put(const rafty::Entries &vals) {
    MDB_txn *txn;
    mdb_txn_begin(_env, nullptr, 0, &txn);
    for(int i =0; i < vals.entries_size(); ++i) {
      std::string s;
      auto &val = vals.entries(i);
      uint64_t key = val.index();
      MDB_val k{sizeof(key), &key};
      MDB_val v{s.size(), &s[0]};
      if(!val.SerializeToString(&s)) {
        throw exception("storage can't serialize value");
      }
      int flags = 0;
      if(_next_index == val.index()) {
        _next_index++;
        flags = MDB_APPEND;
      }
      _next_index = val.index() + 1;
      mdb_put(txn, _keydb, &k, &v, flags);
    }
    mdb_txn_commit(txn);
  }
  
  void Storage::state_put(const char* key, const MDB_val &v) {
    MDB_txn *txn;
    MDB_val k{strlen(key), (void*)key};
    mdb_txn_begin(_env, nullptr, 0, &txn);
    int flags = 0;
    mdb_put(txn, _statedb, &k,(MDB_val*) &v, 0);
    mdb_txn_commit(txn);
  }
  
  bool Storage::state_get(const char* key, std::string *val) const {
    MDB_txn *txn;
    mdb_txn_begin(_env, nullptr, MDB_RDONLY, &txn);
    MDB_val k{strlen(key), (void*)key};
    MDB_val v;
    if(mdb_get(txn, _statedb, &k, &v) == MDB_NOTFOUND) {
      return false;
    }
    val->resize(v.mv_size, '\0');
    memcpy(&(*val)[0], v.mv_data, v.mv_size);
    mdb_txn_abort(txn);
    return true;
  }

}
