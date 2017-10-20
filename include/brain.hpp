#pragma once

#include <nn.hpp>
#include <lmdb++.h>
#include <rafty.pb.h>

#include <storage.hpp>

namespace rafty {
  class Brain {
    public:
    Brain(Storage &env);
    void init();
    void start_peers();
    void event_loop();
    static void main(Storage &env);

   private:
    Storage &_storage;
    rafty::Config _config;
    nn::socket _from_brain;
    nn::socket _to_brain;
    char _state;
  };
}
