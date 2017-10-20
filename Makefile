all: main.o brain.o peer.o clients.o rafty.pb.o storage.o resources.o
	g++ -std=c++14 main.o brain.o peer.o clients.o rafty.pb.o storage.o resources.o -Iinclude objs/libgflags.a objs/libnanomsg.dylib -Wl,-rpath,./3rdparty/nanomsg/ objs/liblmdb.a -lprotobuf

main.o: main.cpp include/rafty.pb.h
	g++ -std=c++14 -c main.cpp -Iinclude

test.o: test.cpp include/rafty.pb.h
	g++ -g -std=c++14 -c test.cpp -Iinclude

resources.o: resources.cpp include/resources.hpp include/rafty.pb.h
	g++ -std=c++14 -c resources.cpp -Iinclude

peer.o: peer.cpp include/peer.hpp include/rafty.pb.h
	g++ -std=c++14 -c peer.cpp -Iinclude

clients.o: clients.cpp include/clients.hpp include/rafty.pb.h
	g++ -std=c++14 -c clients.cpp -Iinclude

brain.o: brain.cpp include/brain.hpp include/rafty.pb.h
	g++ -std=c++14 -c brain.cpp -Iinclude

storage.o: storage.cpp include/storage.hpp include/rafty.pb.h
	g++ -g -std=c++14 -c storage.cpp -Iinclude

rafty.pb.o: rafty.pb.c include/rafty.pb.h
	gcc -c rafty.pb.cc -Iinclude

rafty.pb.c include/rafty.pb.h:  rafty.proto
	protoc --cpp_out=. rafty.proto
	mv rafty.pb.h include
