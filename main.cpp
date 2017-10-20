#include <string>
#include <sstream>
#include <iostream>

#include <resources.hpp>
#include <storage.hpp>

#include <gflags/gflags.h>
 
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

  rafty::Storage storage;
  storage.init();

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

    storage.state_put("config", MDB_val{serialized_config.size(), &serialized_config[0]});
    storage.state_put("current_term", MDB_val{1,(void*) "0"});
    storage.state_put("voted_for", MDB_val{0, (void*)""});
    std::cout << "init done" << std::endl;
    return 0;
  }

  rafty::Resources::main(storage);
  return 0;
}
