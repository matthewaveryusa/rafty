#include <resources.hpp>

#include <nn.hpp>
#include <nanomsg/pubsub.h>
#include <nanomsg/pair.h>

#include <msgs.hpp>
#include <peer.hpp>
#include <brain.hpp>
#include <clients.hpp>
#include <storage.hpp>


namespace rafty {
  Resources::Resources(Storage &s): _storage(s),
  _from_brain(AF_SP, NN_SUB){
  }

  void Resources::init() {
    _from_brain.connect("inproc://from_brain");
    _from_brain.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::end_peer, 1);
    _from_brain.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::start_peer_bind, 1);
    _from_brain.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::start_peer_connect, 1);

    _threads.emplace("brain", std::thread{Brain::main, std::ref(_storage)});
    _threads.emplace("clients", std::thread{Clients::main});
  }

  void Resources::event_loop() {
    while(1) {
      char* buf = nullptr;
      int msgsize = _from_brain.recv(&buf, NN_MSG, 0);
      if(buf[0] == msg::start_peer_bind || buf[0] == msg::start_peer_connect) {
        std::string p(&buf[1], msgsize-1);
        nn::socket s(AF_SP, NN_PAIR);
        if(buf[0] == msg::start_peer_bind) {
          s.bind(p.c_str());
        } else {
          s.connect(p.c_str());
        }
        _threads.emplace(":" + p, std::thread{Peer::main, std::move(p), std::move(s), std::ref(_storage)});
      } else {
        std::string peer(&buf[1], msgsize-1);
      }
      nn_freemsg(buf);
    }
  }

  void Resources::main(Storage &s) {
    Resources r(s);
    r.init();
    r.event_loop();
  }

}
