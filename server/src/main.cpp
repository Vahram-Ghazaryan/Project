#include "server_utils.hpp"
#include "server.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    try {
    int port = 0;    
        if(argc > 1) {
            cmd_parse(argc, const_cast<const char**> (argv), port);
            return 0;
        }
    port = server_port();
	std::cout << "Server started" << std::endl;
    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    start_accept(io_context, acceptor);

    io_context.run();
    } catch(std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}