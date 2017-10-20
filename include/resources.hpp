#pragma once
#include <thread>
#include <unordered_map>
#include <nn.hpp>

#include <storage.hpp>

namespace rafty {
  class Resources {
    public:
    Resources(Storage &s);
    void init();
    void event_loop(); 
    static void  main(Storage &s);
    private:

    Storage &_storage;
    std::unordered_map<std::string, std::thread> _threads;
    nn::socket _from_brain;
  };
}
