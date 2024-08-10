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

void notify_server_status(std::shared_ptr<tcp::socket> server_socket, const std::string& status, const std::string& username) {
    std::string message = '"' + username + " " + status + '"';
    boost::asio::async_write(*server_socket, boost::asio::buffer(message), handle_write);
}

void start_chat(std::shared_ptr<tcp::socket> client_socket, std::shared_ptr<tcp::socket> server_socket, const std::string& username) {
    auto buffer = std::make_shared<std::array<char, 1024>>();

    client_socket->async_read_some(boost::asio::buffer(*buffer), [client_socket, server_socket, buffer, username](const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string message(buffer->data(), length);
            std::cout << "Received: " << message << std::endl;
            start_chat(client_socket, server_socket, username);
        } else {
            std::cerr << "Error during read: " << error.message() << std::endl;
            notify_server_status(server_socket, "free", username);
        }
    });

    while (true) {
        std::string message;
        std::cout << "Enter message: ";
        std::getline(std::cin, message);

        if (message == "exit") {
            std::cout << "Exiting chat." << std::endl;
            client_socket->close();
            notify_server_status(server_socket, "free", username);
            break;
        }

        boost::asio::async_write(*client_socket, boost::asio::buffer(message), handle_write);
    }
}

void handle_read(std::shared_ptr<tcp::socket> socket, std::shared_ptr<tcp::socket> server_socket, boost::asio::io_context& io_context, const std::string& username) {
    auto buffer = std::make_shared<std::array<char, 1024>>();
    socket->async_read_some(boost::asio::buffer(*buffer), [socket, server_socket, buffer, &io_context, username](const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string response(buffer->data(), length);
            std::cout << "Online clients:\n" << response << std::endl;

            std::string target_username;
            std::cout << "Enter the username of the client you want to connect to: ";
            std::cin >> target_username;

            boost::asio::async_write(*socket, boost::asio::buffer(target_username), [socket, server_socket, &io_context, username](const boost::system::error_code& error, std::size_t length) {
                handle_write(error, length);

                auto buffer = std::make_shared<std::array<char, 1024>>();
                socket->async_read_some(boost::asio::buffer(*buffer), [socket, server_socket, buffer, &io_context, username](const boost::system::error_code& error, std::size_t length) {
                    if (!error) {
                        std::string client_ip(buffer->data(), length);
                        std::cout << "IP address of the selected client: " << client_ip << std::endl;

                        auto client_socket = std::make_shared<tcp::socket>(io_context);
                        tcp::resolver resolver(io_context);
                        tcp::resolver::results_type client_endpoints = resolver.resolve(client_ip, "12345");
                        boost::asio::async_connect(*client_socket, client_endpoints, [client_socket, server_socket, username](const boost::system::error_code& error, const tcp::endpoint&) {
                            if (!error) {
                                std::cout << "Connected to client. You can now start chatting." << std::endl;
                                
                                notify_server_status(server_socket, "no free", username);
                                
                                start_chat(client_socket, server_socket, username);
                            } else {
                                std::cerr << "Error during client connect: " << error.message() << std::endl;
                            }
                        });
                    } else {
                        std::cerr << "Error during read: " << error.message() << std::endl;
                    }
                });
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
        auto server_socket = std::make_shared<tcp::socket>(io_context);

        std::string username = argv[3];

        boost::asio::async_connect(*server_socket, endpoints, [server_socket, &io_context, username](const boost::system::error_code& error, const tcp::endpoint&) {
            if (!error) {
                std::cout << "Connected to server." << std::endl;
                std::string ip_address = server_socket->local_endpoint().address().to_string();
                std::string buffer = ip_address + " " + username;

                boost::asio::async_write(*server_socket, boost::asio::buffer(buffer), [server_socket, &io_context, username](const boost::system::error_code& error, std::size_t length) {
                    handle_write(error, length);
                    handle_read(server_socket, server_socket, io_context, username);
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

