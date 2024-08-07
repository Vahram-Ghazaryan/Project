#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <map>
#include <fstream>
#include <memory>

using boost::asio::ip::tcp;

void handle_read(std::shared_ptr<tcp::socket> socket, std::vector<char> buffer, bool start_connection) {
    socket->async_read_some(boost::asio::buffer(buffer), [socket, buffer, &start_connection] (boost::system::error_code& error) {
        if (!error) {
            if (start_connection == true) { // Write information about clients
                std::ofstream file("clients_info.txt", std::ios::app);
                if (!file) {
                    std::cerr << "Unable to open file" << std::endl;
                    return;
                }
                file << buffer.data() << " online" << " free";
                start_connection = false;
            }
        }
    });
}
void start_accept(boost::asio::io_context& io_context, tcp::acceptor& acceptor) {
    auto socket = std::make_shared<tcp::socket>(io_context);
    acceptor.async_accept(*socket, [socket, &acceptor, &io_context](boost::system::error_code& error) {
        if(!error) {
            std::vector<char> buffer;
            handle_read(socket, buffer, true);
        }
        start_accept(io_context, acceptor);
    });
}
int main(int argc, char* argv[]) {
    try {
        if(argc != 2) {
            std::cerr << "Usage: server <port>" << std::endl;
            return 1;
        }

    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), std::atoi(argv[1])));
    start_accept(io_context, acceptor);

    io_context.run();
    } catch(std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
