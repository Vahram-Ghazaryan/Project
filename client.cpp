#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <memory>

using boost::asio::ip::tcp;

void handle_write(const boost::system::error_code& error, std::size_t) {
    if (error) {
        std::cerr << "Error during write: " << error.message() << std::endl;
    }
}

void handle_read(std::shared_ptr<tcp::socket> socket, boost::asio::io_context& io_context, const std::string& port) {
    auto buffer = std::make_shared<std::array<char, 1024>>();
    socket->async_read_some(boost::asio::buffer(*buffer), [socket, buffer, &io_context, port](const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string response(buffer->data(), length);
            std::cout << "Online clients: " << response << std::endl;

            std::string selected_client;
            std::cout << "Enter the IP address of the client you want to connect to: ";
            std::cin >> selected_client;

            socket->close();

            auto client_socket = std::make_shared<tcp::socket>(io_context);
            tcp::resolver resolver(io_context);
            tcp::resolver::results_type client_endpoints = resolver.resolve(selected_client, port);
            boost::asio::async_connect(*client_socket, client_endpoints, [client_socket](const boost::system::error_code& error, const tcp::endpoint&) {
                if (!error) {
                    std::cout << "Connected to client." << std::endl;
                } else {
                    std::cerr << "Error during client connect: " << error.message() << std::endl;
                }
            });
        } else {
            std::cerr << "Error during read: " << error.message() << std::endl;
        }
    });
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 4) {
            std::cerr << "Usage: client <host> <port> <username>" << std::endl;
            return 1;
        }

        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve(argv[1], argv[2]);
        auto socket = std::make_shared<tcp::socket>(io_context);

        boost::asio::async_connect(*socket, endpoints, [socket, argv, &io_context](const boost::system::error_code& error, const tcp::endpoint&) {
            if (!error) {
                std::cout << "Connected to server." << std::endl;
                std::string ip_address = socket->local_endpoint().address().to_string();
                std::string username = argv[3];
                std::string buffer = ip_address + " " + username;

                boost::asio::async_write(*socket, boost::asio::buffer(buffer), [socket, &io_context, argv](const boost::system::error_code& error, std::size_t length) {
                    handle_write(error, length);
                    handle_read(socket, io_context, argv[2]);
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

