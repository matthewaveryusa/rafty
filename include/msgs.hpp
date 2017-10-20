#pragma once
namespace rafty {
  namespace msg {
    constexpr char end_peer = 'E';
    constexpr char start_peer_bind = 'B';
    constexpr char start_peer_connect = 'C';
    constexpr char peer_up = 'U';
    constexpr char state = 'S';
    constexpr char republish_state = 'R';

    constexpr char append_request = 'A';
    constexpr char append_response = 'Z';

    constexpr char vote_request = 'V';
    constexpr char vote_response = 'W';

    constexpr char private_message = 'P';
  }
}
