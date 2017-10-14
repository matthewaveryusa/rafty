#include <thread>
#include <chrono>
#include <string>
#include <iostream>
#include <nn.hpp>
#include <nanomsg/pair.h>
#include <nanomsg/pipeline.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/reqrep.h>
#include <lmdb++.h>
#include <sstream>
#include <unordered_map>
#include <rafty.pb.h>
#include <gflags/gflags.h>

namespace msg {
  constexpr char end_peer = 'A';
  constexpr char start_peer_bind = 'B';
  constexpr char start_peer_connect = 'C';
  constexpr char peer_up = 'U';
  constexpr char to_all_peers = 'P';
}

void storage(lmdb::env &env) {
  struct nn_pollfd pfd[1];

  nn::socket pipeline_s(AF_SP, NN_PULL);
  pipeline_s.bind("inproc://to_storage");
  pfd[0].fd = pipeline_s.fd();
  pfd[0].events = NN_POLLIN;

  nn::socket pipeline2_s(AF_SP, NN_PUSH);
  pipeline2_s.connect("inproc://to_brain");

  auto txn = lmdb::txn::begin(env);
  auto keydb = lmdb::dbi::open(txn, "keys", MDB_CREATE);
  txn.commit();
  while(1) {
    int rc = nn_poll (pfd, 1, 60000);
    if (rc == 0) {
      printf ("storage idling\n");
      continue;
    }
    if (rc == -1) {
      printf ("storage Error!\n");
      return;
    }
    if (pfd[0].revents & NN_POLLIN) {
      char* buf = nullptr;
      int msgsize = pipeline_s.recv(&buf, NN_MSG, 0);
      if(msgsize >= 0) {
        printf ("pipeline Message received!\n");
        rafty::Value val;
        if(!val.ParseFromArray(buf, msgsize)) {
          printf ("storage parse failure!\n");
        }
        txn = lmdb::txn::begin(env);
        keydb.put(txn,val.index(), lmdb::val{buf, (size_t)msgsize});
        txn.commit();
        nn_freemsg(buf);
      }
    }
  }
}

void brain(lmdb::env &env) {
  auto txn = lmdb::txn::begin(env);
  auto statedb = lmdb::dbi::open(txn, "state", MDB_CREATE);
  txn.commit();

  txn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
  auto cursor = lmdb::cursor::open(txn, statedb);
  lmdb::val val;
  cursor.get(lmdb::val{"config"}, val, MDB_NEXT);
  rafty::Config config;
  if(!config.ParseFromArray(val.data(), val.size())) {
    printf ("brain config parse failure!\n");
  }

  struct nn_pollfd pfd[1];

  nn::socket pub_s(AF_SP, NN_PUB);
  pub_s.bind("inproc://from_brain");

  nn::socket pipeline_s(AF_SP, NN_PULL);
  pipeline_s.bind("inproc://to_brain");
  
  //fire up peers that are in config file
  for(int i = 0; i < config.peers_size(); ++i) {
    std::ostringstream oss;
    if(config.peers(i) < config.listen()) {
      oss << msg::start_peer_connect << config.peers(i);
    } else {
      oss << msg::start_peer_bind << config.peers(i);
    }
    auto msg = oss.str();
    pub_s.send(msg.c_str(), msg.size(), 0);
  }


  pfd[0].fd = pipeline_s.fd();
  pfd[0].events = NN_POLLIN;
  bool is_leader = true;
  
  while(1) {
    int rc = nn_poll (pfd, 1, config.timeout_ms());
    if (rc == 0) {
      printf ("brain idling!\n");
      continue;
    }
    if (rc == -1) {
      printf ("brain Error!\n");
      return;
    }
    if (pfd[0].revents & NN_POLLIN) {
      char* buf = nullptr;
      int msgsize = pipeline_s.recv(&buf, NN_MSG, 0);
      if(msgsize >= 0) {
        if(buf[0] == msg::peer_up) {
          printf ("peerup! %.*s\n", msgsize, buf);
          if(is_leader) {
            std::ostringstream oss;
            oss << msg::to_all_peers << "is_leader";
            auto msg = oss.str();
            pub_s.send(msg.c_str(), msg.size(), 0);
          }
        }
        nn_freemsg(buf);
      }
    }
  }
}

void clients() {
  struct nn_pollfd pfd[2];

  nn::socket client_s(AF_SP, NN_PULL);
  client_s.bind("tcp://127.0.0.1:8080");
  pfd[0].fd = client_s.fd();
  pfd[0].events = NN_POLLIN;

  nn::socket sub_s(AF_SP, NN_SUB);
  sub_s.connect("inproc://from_brain");
  sub_s.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::to_all_peers, 1);
  pfd[1].fd = sub_s.fd();
  pfd[1].events = NN_POLLIN;
  
  nn::socket pipeline_s(AF_SP, NN_PUSH);
  pipeline_s.connect("inproc://to_brain");
  
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
      int msgsize = client_s.recv(&buf, NN_MSG, 0);
      if(msgsize >= 0) {
        printf ("clients Message can be received from client socket! %.*s\n", msgsize, buf);
        pipeline_s.send(buf, msgsize, 0);
      }
    }
    if (pfd[1].revents & NN_POLLIN) {
      char* buf = nullptr;
      int msgsize = sub_s.recv(&buf, NN_MSG, 0);
      printf ("clients Message can be received from brain socket %.*s!\n", msgsize, buf);
      nn_freemsg(buf);
    }
  }
}

void peer(std::string address, nn::socket s) {
  struct nn_pollfd pfd[2];

  pfd[0].fd = s.fd();
  pfd[0].events = NN_POLLIN;

  nn::socket sub_s(AF_SP, NN_SUB);
  sub_s.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::to_all_peers, 1);
  sub_s.connect("inproc://from_brain");
  pfd[1].fd = sub_s.fd();
  pfd[1].events = NN_POLLIN;

  nn::socket pipeline_s(AF_SP, NN_PUSH);
  pipeline_s.connect("inproc://to_brain");
 
  std::ostringstream oss;
  oss << msg::peer_up << address;
  auto msg = oss.str();
  pipeline_s.send(msg.c_str(), msg.size(), 0);

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
      printf ("peers Message can be received from peer socket!\n");
    }
    if (pfd[1].revents & NN_POLLIN) {
      char* buf = nullptr;
      int msgsize = sub_s.recv(&buf, NN_MSG, 0);
      printf ("peers Message can be received from brain socket %.*s!\n", msgsize, buf);
      nn_freemsg(buf);
    }
  }
}
  
DEFINE_bool(init, false, "initialize peers list");
DEFINE_uint64(timeout_ms, 10000, "time between timeouts(ms)");
DEFINE_uint64(heartbeat_ms, 1000, "time between heartbeats(ms)");
DEFINE_string(peers, "", "list of peers addresses separated by a space");
DEFINE_string(peer_listen, "127.0.0.1:8000", "my address");
DEFINE_string(client_stream_listen, "127.0.0.1:8001", "address clients publish to");
DEFINE_string(publish_stream_listen, "127.0.0.1:8002", "address where updates are announced to non-participating peers");

int main(int argc, char** argv) {
  gflags::SetUsageMessage("run with -init -peers \"tcp://ip:port tcp://ip:port\"");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  auto env = lmdb::env::create();
  env.set_max_dbs(2);
  env.set_mapsize(1UL * 1024UL * 1024UL); /* 1 GiB */
  env.open("./rafty.mdb", 0, 0664);
  
  if(FLAGS_init) {
    std::stringstream ss(FLAGS_peers);
    std::istream_iterator<std::string> begin(ss);
    std::istream_iterator<std::string> end;
    std::vector<std::string> vstrings(begin, end);
     
    rafty::Config config;
    for(auto &s:vstrings) {
      config.add_peers(s);
    }
    config.set_heartbeat_ms(FLAGS_heartbeat_ms);
    config.set_timeout_ms(FLAGS_timeout_ms);
    config.set_client_stream(FLAGS_client_stream_listen);
    config.set_publisher_stream(FLAGS_publish_stream_listen);
    config.set_listen(FLAGS_peer_listen);

    std::string serialized_config;
    config.SerializeToString(&serialized_config);

    auto txn = lmdb::txn::begin(env);
    auto statedb = lmdb::dbi::open(txn, "state", MDB_CREATE);
    lmdb::val key{"config"};
    lmdb::val value{serialized_config};
    statedb.put(txn, key, value);
    txn.commit();
    std::cout << "init done" << std::endl;
    return 0;
  }

  

  std::unordered_map<std::string, std::thread> threads;
  threads.emplace("storage", std::thread{storage, std::ref(env)});
  threads.emplace("brain", std::thread{brain, std::ref(env)});
  threads.emplace("clients", std::thread{clients});

  nn::socket sub_s(AF_SP, NN_SUB);
  sub_s.connect("inproc://from_brain");
  sub_s.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::end_peer, 1);
  sub_s.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::start_peer_bind, 1);
  sub_s.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, &msg::start_peer_connect, 1);
  while(1) {
    char* buf = nullptr;
    int msgsize = sub_s.recv(&buf, NN_MSG, 0);
    if(buf[0] == msg::start_peer_bind || buf[0] == msg::start_peer_connect) {
      std::string p(&buf[1], msgsize-1);
      std::cout << "starting peer " << p << std::endl;
      nn::socket s(AF_SP, NN_PAIR);
      if(buf[0] == msg::start_peer_bind) {
        s.bind(p.c_str());
      } else {
        s.connect(p.c_str());
      }
      threads.emplace(":" + p, std::thread{peer, std::move(p), std::move(s)});
    } else {
      std::string peer(&buf[1], msgsize-1);
      std::cout << "ending peer " << peer << std::endl;
    }
    nn_freemsg(buf);
  }
  
  return 1;
}
