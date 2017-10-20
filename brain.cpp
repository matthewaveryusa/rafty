#include <brain.hpp>

#include <nn.hpp>
#include <nanomsg/pipeline.h>
#include <nanomsg/pubsub.h>
#include <lmdb++.h>
#include <rafty.pb.h>

#include <state.hpp>
#include <exception.hpp>
#include <msgs.hpp>

#include <string>
#include <sstream>

namespace rafty {

    void Brain::init() {
      std::string cfg;
      _storage.state_get("config", &cfg);
      if(!_config.ParseFromString(cfg)) {
        throw exception("brain config parse failure!");
      }

      _from_brain.bind("inproc://from_brain");
      _to_brain.bind("inproc://to_brain");
    }

    void Brain::start_peers() {
      for(int i = 0; i < _config.peers_size(); ++i) {
        std::ostringstream oss;
        if(_config.peers(i) < _config.listen()) {
          oss << msg::start_peer_connect << _config.peers(i);
        } else {
          oss << msg::start_peer_bind << _config.peers(i);
        }
        auto msg = oss.str();
        _from_brain.send(msg.c_str(), msg.size(), 0);
      }
    }
    
    void Brain::event_loop() {
      struct nn_pollfd pfd[1]; 
      pfd[0].fd = _to_brain.fd();
      pfd[0].events = NN_POLLIN;
      
      start_peers();

      while(1) {
        int rc = nn_poll (pfd, 1, _config.timeout_ms());
        if (rc == 0) {
          printf ("brain idling!\n");
          continue;
        }
        if (rc == -1) {
          throw exception("brain polling error");
        }
        if (pfd[0].revents & NN_POLLIN) {
          char* buf = nullptr;
          int msgsize = _to_brain.recv(&buf, NN_MSG, 0);
          if(msgsize >= 0) {
            switch(buf[0]) {
              case msg::peer_up:
                printf ("peerup! %.*s\n", msgsize, buf);
                std:: ostringstream oss;
                oss << msg::private_message << std::string(buf+1, msgsize-1) << msg::state << _state;
                auto msg = oss.str();
                _from_brain.send(msg.c_str(), msg.size(), 0);
                break;
            }
            nn_freemsg(buf);
          }
        }
      }

    }
    
    void Brain::main(Storage &s) {
      Brain b(s);
      b.init();
      b.event_loop();
    }
    
    Brain::Brain(Storage &s): _storage(s),
    _from_brain(AF_SP, NN_PUB),
    _to_brain(AF_SP, NN_PULL),
    _state(state::follower) {
    }
}
