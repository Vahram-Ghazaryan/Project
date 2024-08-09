#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <memory>
#include <cstring>

using boost::asio::ip::tcp;
void handle_write(const boost::system::error_code& error, std::size_t) {
    if (error) {
        std::cerr << "Error during write: " << error.message() << std::endl;
    }
}

void handle_read(std::shared_ptr<tcp::socket> socket) {
    auto buffer = std::make_shared<std::array<char, 1024>>();
    socket->async_read_some(boost::asio::buffer(*buffer), [socket, buffer](const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string response(buffer->data(), length);
            std::cout << "Server response: " << response << std::endl;
        } else {
            std::cerr << "Error during read: " << error.message() << std::endl;
        }
    });
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 5) {
            std::cerr << "Usage: client <host> <port> <ip address> <username>" << std::endl;
            return 1;
        }

        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve(argv[1], argv[2]);
        auto socket = std::make_shared<tcp::socket>(io_context);

        boost::asio::async_connect(*socket, endpoints, [socket, argv](const boost::system::error_code& error, const tcp::endpoint&) {
            if (!error) {
                std::cout << "Connected to server." << std::endl;
                std::string ip_address = argv[3];
                std::string username = argv[4];
                std::string buffer = ip_address + " " + username;
                std::cout << "The buffer: " << buffer.data() << std::endl;
                boost::asio::async_write(*socket, boost::asio::buffer(buffer), [socket](const boost::system::error_code& error, std::size_t length) {
                    handle_write(error, length);
                    handle_read(socket);
                });
            } else {
                std::cerr << "Error during connect: " << error.message() << std::endl;
            }
        });

        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
