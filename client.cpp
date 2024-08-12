#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <memory>
#include <thread>
#include <vector>

using boost::asio::ip::tcp;

void handle_write(const boost::system::error_code& error, std::size_t) {
    if (error) {
        std::cerr << "Error during write: " << error.message() << std::endl;
    }
}

void notify_server_status(std::shared_ptr<tcp::socket> server_socket, const std::string& status, const std::string& username) {
    std::string message = username + " " + status;
    boost::asio::async_write(*server_socket, boost::asio::buffer(message), handle_write);
}

void start_chat(std::shared_ptr<tcp::socket> client_socket, std::shared_ptr<tcp::socket> server_socket, const std::string& username) {
    auto buffer = std::make_shared<std::array<char, 1024>>();
    
    auto read_handler = [client_socket, server_socket, buffer, username](const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string message(buffer->data(), length);
            std::cout << "\n" << username <<": " << message << std::endl;
            start_chat(client_socket, server_socket, username); 
        } else {
            std::cerr << "Error during read: " << error.message() << std::endl;
            notify_server_status(server_socket, "free", username);
        }
    };
    client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);

    std::thread([client_socket, server_socket, username]() {
        while (true) {
            std::string message;
            std::cout << "You: ";
            std::getline(std::cin, message);

            if (message == "exit") {
                std::cout << "Exiting chat." << std::endl;
                client_socket->close();
                notify_server_status(server_socket, "free", username);
                break;
            } else if (message == "wait") {
                std::cout << "Waiting for incoming connections." << std::endl;
                notify_server_status(server_socket, "waiting", username);
            } else {
                boost::asio::async_write(*client_socket, boost::asio::buffer(message), handle_write);
            }
        }
    }).detach();
}

void accept_connections(std::shared_ptr<tcp::acceptor> acceptor, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username) {
    auto new_socket = std::make_shared<tcp::socket>(io_context);
    acceptor->async_accept(*new_socket, [new_socket, acceptor, &io_context, server_socket, username](const boost::system::error_code& error) {
        if (!error) {
            std::cout << "New connection accepted. Enter message or 'wait' to wait for incoming connections: " << std::endl;

            notify_server_status(server_socket, "no free", username);
            std::thread chat_thread(start_chat, new_socket, server_socket, username);
            chat_thread.detach();
        } else {
            std::cerr << "Error during accept: " << error.message() << std::endl;
        }

        accept_connections(acceptor, io_context, server_socket, username);
    });
}

void handle_read(std::shared_ptr<tcp::socket> socket, std::shared_ptr<tcp::socket> server_socket, boost::asio::io_context& io_context, const std::string& username) {
    auto buffer = std::make_shared<std::array<char, 1024>>();
    socket->async_read_some(boost::asio::buffer(*buffer), [socket, server_socket, buffer, &io_context, username](const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string response(buffer->data(), length);
            std::cout << "Online clients:\n" << response << std::endl;

            std::string target_username;
            std::cout << "Enter the username of the client you want to connect to (or 'wait' to wait for incoming connections): ";
            std::getline(std::cin, target_username);

            if (target_username == "wait") {
            	std::cout << "Switch to standby mode.." << std::endl;
                boost::asio::async_write(*socket, boost::asio::buffer("wait"), [socket, server_socket, &io_context, username](const boost::system::error_code& error, std::size_t length) {
                    handle_write(error, length);
                });
            } else {
                boost::asio::async_write(*socket, boost::asio::buffer("connect " + target_username), [socket, server_socket, &io_context, username](const boost::system::error_code& error, std::size_t length) {
                    handle_write(error, length);

                    auto buffer = std::make_shared<std::array<char, 1024>>();
                    socket->async_read_some(boost::asio::buffer(*buffer), [socket, server_socket, buffer, &io_context, username](const boost::system::error_code& error, std::size_t length) {
                        if (!error) {
                            std::string client_ip(buffer->data(), length);
                            std::cout << "IP address of the selected client: " << client_ip << std::endl;

                            auto client_socket = std::make_shared<tcp::socket>(io_context);
                            tcp::resolver resolver(io_context);
                            tcp::resolver::results_type client_endpoints = resolver.resolve(client_ip, "12347");
                            boost::asio::async_connect(*client_socket, client_endpoints, [client_socket, server_socket, username](const boost::system::error_code& error, const tcp::endpoint&) {
                                if (!error) {
                                    std::cout << "Connected to client. You can now start chatting." << std::endl;

                                    notify_server_status(server_socket, "no free", username);

                                    std::thread chat_thread(start_chat, client_socket, server_socket, username);
                                    chat_thread.detach();
                                } else {
                                    std::cerr << "Error during client connection: " << error.message() << std::endl;
                                }
                            });
                        } else {
                            std::cerr << "Error during read: " << error.message() << std::endl;
                        }
                    });
                });
            }
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

                    auto acceptor = std::make_shared<tcp::acceptor>(io_context, tcp::endpoint(tcp::v4(), 12347)); // Port for accepting connections
                    std::thread accept_thread(accept_connections, acceptor, std::ref(io_context), server_socket, username);
                    accept_thread.detach();

                    handle_read(server_socket, server_socket, io_context, username);

                    // Run io_context in a loop to check for pending async operations
                    while (true) {
                        io_context.poll();
                        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Adjust sleep duration as needed
                    }
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

