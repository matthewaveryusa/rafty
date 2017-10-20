#include <peer.hpp>

#include <nn.hpp>
#include <nanomsg/pair.h>
#include <nanomsg/pipeline.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/reqrep.h>
#include <msgs.hpp>
#include <storage.hpp>

#include <string>
#include <sstream>

namespace rafty {
  Peer::Peer(std::string address, nn::socket peer, Storage &storage): 
    _storage(storage), 
    _address(std::move(address)), 
    _peer(std::move(peer)),
    _from_brain(AF_SP, NN_SUB),
    _to_brain(AF_SP, NN_PUSH),
    _is_leader(false),
    _sequence(0){
  }

  void Peer::init() {
    _from_brain.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::state, 1);
    _from_brain.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::republish_state, 1);
    {
    std::ostringstream oss;
    oss << msg::private_message << _address;
    _private_msg_topic = oss.str();
    }
    _from_brain.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, _private_msg_topic.c_str(), _private_msg_topic.size());
    _from_brain.connect("inproc://from_brain");
    _to_brain.connect("inproc://to_brain");
  }
      
  void Peer::main(std::string address, nn::socket s, Storage &storage) {
    Peer p(std::move(address), std::move(s), storage);
    p.init();
    p.event_loop();
  }

  void Peer::event_loop() {
    struct nn_pollfd pfd[2];
    pfd[0].fd = _peer.fd();
    pfd[0].events = NN_POLLIN;
    pfd[1].fd = _from_brain.fd();
    pfd[1].events = NN_POLLIN;


    std::ostringstream oss;
    {
      oss << msg::peer_up << _address;
      auto msg = oss.str();
      _to_brain.send(msg.c_str(), msg.size(), 0);
    }

    uint64_t sequenceid = 0;

    while(1) {
      int rc = nn_poll (pfd, 2, 60000);
      if (rc == 0) {
        printf ("peer idling!\n");
        continue;
      }
      if (rc == -1) {
        printf ("peers Error!\n");
        return;
      }
      if (pfd[0].revents & NN_POLLIN) {
        char* buf = nullptr;
        int msgsize = _from_brain.recv(&buf, NN_MSG, 0);
        switch(buf[0]) {
          case msg::append_request:
          case msg::vote_request:
            _to_brain.send(buf, msgsize, 0);
        }
        nn_freemsg(buf);
      }
      if (pfd[1].revents & NN_POLLIN) {
        char* buf = nullptr;
        int msgsize = _from_brain.recv(&buf, NN_MSG, 0);
        if(buf[0] == msg::private_message) {
          size_t offset = _private_msg_topic.size();
            private_message_switch(buf + offset, msgsize - offset);
        } else {
          message_switch(buf, msgsize);
        }
        nn_freemsg(buf);
      }
    }
  }

  void Peer::private_message_switch(const char* buf, int msgsize) {
    switch(buf[0]) {
      case msg::append_response:
      case msg::vote_response:
        _peer.send(buf, msgsize, 0);
        break;
      case msg::state:
        printf("got pm from brain about state\n");
        if(buf[1] == 'L') {
          _is_leader = true;
        }
        break;
    }
  }

    void Peer::message_switch(const char* buf, int msgsize) {
      switch(buf[0]) {
        case msg::state:
          if(buf[1] == 'L') {
            _is_leader = true;
          }
      }
    }

}
