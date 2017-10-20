#include <clients.hpp>

#include <nn.hpp>
#include <lmdb++.h>
#include <nanomsg/pipeline.h>
#include <nanomsg/pubsub.h>

#include <rafty.pb.h>

#include <msgs.hpp>

namespace rafty {
  Clients::Clients():
    _clients(AF_SP, NN_PULL),
    _from_brain(AF_SP, NN_SUB),
    _to_brain(AF_SP, NN_PUSH) {
  }

  void Clients::main() {
    Clients c;
    c.init();
    c.event_loop();
  }

  void Clients::init() {
    _clients.bind("tcp://127.0.0.1:8080");
    
    _from_brain.connect("inproc://from_brain");
    _from_brain.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::state, 1);
    _from_brain.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::republish_state, 1);
    
    _to_brain.connect("inproc://to_brain");
  }
  void Clients::event_loop() {
    struct nn_pollfd pfd[2];

    pfd[0].fd = _clients.fd();
    pfd[0].events = NN_POLLIN;

    pfd[1].fd = _from_brain.fd();
    pfd[1].events = NN_POLLIN;

    while(1) {
      int rc = nn_poll (pfd, 2, 60000);
      if (rc == 0) {
        printf ("clients idling!\n");
        continue;
      }
      if (rc == -1) {
        printf ("clients Error!\n");
        return;
      }
      if (pfd[0].revents & NN_POLLIN) {
        char* buf = nullptr;
        int msgsize = _clients.recv(&buf, NN_MSG, 0);
        if(msgsize >= 0) {
          printf ("clients Message can be received from client socket! %.*s\n", msgsize, buf);
          _to_brain.send(buf, msgsize, 0);
        }
      }
      if (pfd[1].revents & NN_POLLIN) {
        char* buf = nullptr;
        int msgsize = _from_brain.recv(&buf, NN_MSG, 0);
        printf ("clients Message can be received from brain socket %.*s!\n", msgsize, buf);
        nn_freemsg(buf);
      }
    }
  }
}
