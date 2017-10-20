#include <storage.hpp>

int main() 
{
 rafty::Storage s;
 s.init();
 rafty::Value v;
 v.set_index(1);
 v.set_term(1);
 v.set_value("hello");
 s.put(v);
 v.set_index(2);
 s.put(v);
 v.set_index(3);
 s.put(v);
 v.set_index(4);
 s.put(v);
 s.delete_since(3);
 v.set_index(4);
 s.put(v);
 v.set_index(5);
 s.put(v);
 s.delete_since(3);
}
