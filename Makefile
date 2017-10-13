all:
	g++ -std=c++14 main.cpp -Iinclude objs/libgflags.a objs/libnanomsg.dylib -Wl,-rpath,./3rdparty/nanomsg/ objs/liblmdb.a rafty.pb.cc -lprotobuf
proto:
	protoc --cpp_out=. rafty.proto
	mv rafty.pb.h include
