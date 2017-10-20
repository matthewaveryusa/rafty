#pragma once
#include <nn.hpp>

#include <sstream>
#include <string>
#include <storage.hpp>

namespace rafty {

  class Peer {
    public:
      Peer(std::string address, nn::socket peer, Storage &s);
      void init();

      static void main(std::string address, nn::socket s, Storage &storage);

      void private_message_switch(const char* buf, int msgsize);
      void message_switch(const char* buf, int msgsize);
      void event_loop();

    private:
      Storage &_storage;
      std::string _address;
      nn::socket _peer;
      nn::socket _from_brain;
      nn::socket _to_brain;
      bool _is_leader;
      uint64_t _sequence;
      std::string _private_msg_topic;
  };
}
