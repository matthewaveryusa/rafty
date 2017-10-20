#pragma once

#include <nn.hpp>
#include <lmdb++.h>
#include <rafty.pb.h>

namespace rafty {
  class Clients {
    public:
    Clients();
    void init();
    void event_loop();
    static void main();

   private:
    nn::socket _clients;
    nn::socket _from_brain;
    nn::socket _to_brain;
    char _state;
  };
}
