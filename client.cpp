#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <mutex>
#include <fstream>
#include <functional>
#include <unordered_map>

using boost::asio::ip::tcp;

std::unordered_map<std::string, std::string> emoji_map = {
    {":smile", "😊"},
    {":sad", "😢"},
    {":laugh", "😂"},
    {":angry", "😡"},
    {":wink", "😉"},
    {":heart", "❤️"}
};

std::string replace_emojis(const std::string& message) {
    std::string result = message;
    for (const auto& pair : emoji_map) {
        std::string command = pair.first;
        std::string emoji = pair.second;

        size_t pos = 0;
        while ((pos = result.find(command, pos)) != std::string::npos) {
            result.replace(pos, command.length(), emoji);
            pos += emoji.length();
        }
    }
    return result;
}

void connect_to_client(const std::string& client_ip, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username);

std::string current_time() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S");
    return ss.str();
}

void print_message(const std::string& sender, const std::string& message, bool is_current_user) {
    std::string time_str = "[" + current_time() + "]";
    std::string formatted_message = time_str + " " + message;
    
    // ANSI escape codes for colors
    const std::string user_color = "\033[34m";  // Blue
    const std::string other_color = "\033[32m"; // Green
    const std::string reset_color = "\033[0m";  // Reset to default

    if (is_current_user) {
        std::cout << user_color << std::setw(55) << std::left << time_str << reset_color << std::endl;
    } else {
        std::cout << other_color << std::setw(55) << std::right << formatted_message << "\n-----------" << reset_color << std::endl;
    }
}

void handle_write(const boost::system::error_code& error, std::size_t) {
    if (error) {
        std::cerr << "Error during write: " << error.message() << std::endl;
    }
}

void notify_server_status(std::shared_ptr<tcp::socket> server_socket, const std::string& status, const std::string& username) {
    std::string message = username + " " + status;
    boost::asio::async_write(*server_socket, boost::asio::buffer(message), handle_write);
}

void request_list(std::shared_ptr<tcp::socket> server_socket, const std::string& username) {
    std::string request = "list " + username;
    boost::asio::async_write(*server_socket, boost::asio::buffer(request), [](const boost::system::error_code& error, std::size_t) {
        if (error) {
            std::cerr << "Error during list request: " << error.message() << std::endl;
        }
    });

    auto buffer = std::make_shared<std::array<char, 1024>>();
    server_socket->async_read_some(boost::asio::buffer(*buffer), [server_socket, buffer](const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string response(buffer->data(), length);
            std::cout << "Online clients:\n" << response << std::endl;
        } else {
            std::cerr << "Error during list read: " << error.message() << std::endl;
        }
    });
}

void handle_read(std::shared_ptr<tcp::socket> socket, std::shared_ptr<tcp::socket> server_socket, boost::asio::io_context& io_context, const std::string& username) {
    auto buffer = std::make_shared<std::array<char, 1024>>();
    socket->async_read_some(boost::asio::buffer(*buffer), [socket, server_socket, buffer, &io_context, username](const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string response(buffer->data(), length);
            if (response.rfind("request ", 0) == 0) {
                std::string requester = response.substr(8);
                std::cout << "Connection request from: " << requester << std::endl;

                std::string reply;
                std::cout << "Accept or reject (accept/reject): ";
                std::getline(std::cin, reply);

                if (reply == "accept") {
                    std::string response_message = "accept from " + username + " for " + requester;
                    boost::asio::async_write(*socket, boost::asio::buffer(response_message), handle_write);
                    std::cout << "Connection accepted. You can now start chatting." << std::endl;
                    notify_server_status(server_socket, "no free", username);
                } else if (reply == "reject") {
                    std::string response_message = "reject from " + username + " for " + requester;
                    boost::asio::async_write(*socket, boost::asio::buffer(response_message), handle_write);
                    std::cout << "Connection rejected." << std::endl;
                    notify_server_status(server_socket, "waiting", username);
                }
            } else if (response == "reject") {
                std::cout << "Connection request rejected by the other client." << std::endl;
                notify_server_status(server_socket, "waiting", username);
            } else if (response.find("IP: ") == 0) {
                std::string client_ip = response.substr(4);
                std::cout << "IP address of the selected client: " << client_ip << std::endl;
                connect_to_client(client_ip, io_context, server_socket, username);
            } else {
            	std::string text;
            	if (response.find("ip not found") == 0){
            		text = "Username not found.";
            	} else text = "Online clients:\n";
            	
                std::cout << text << response << std::endl;

                std::string target_username;
                std::cout << "Enter the username of the client you want to connect to (or 'wait' to wait for incoming connections): ";
                std::getline(std::cin, target_username);

                if (target_username == "wait") {
                    std::cout << "To return to the previous menu you can restart the program\nSwitching to standby mode.." << std::endl;
                    notify_server_status(server_socket, "waiting", username);
                } else {
                	std::cout << "Wait for the user to accept the connection.." << std::endl;
                    std::string connect_message = "connect " + target_username;
                    boost::asio::async_write(*socket, boost::asio::buffer(connect_message), handle_write);

                    auto buffer = std::make_shared<std::array<char, 1024>>();
                    socket->async_read_some(boost::asio::buffer(*buffer), [socket, server_socket, buffer, &io_context, username](const boost::system::error_code& error, std::size_t length) {
                        if (!error) {
                            std::string response(buffer->data(), length);
                            if (response.find("IP: ") == 0) {
                                std::string client_ip = response.substr(4);
                                std::cout << "IP address of the selected client: " << client_ip << std::endl;
                                connect_to_client(client_ip, io_context, server_socket, username);
                            } else {
                                std::cerr << "Unexpected response: " << response << std::endl;
                            }
                        } else {
                            std::cerr << "Error during read: " << error.message() << std::endl;
                        }
                    });
                }
            }

            handle_read(socket, server_socket, io_context, username);
        } else {
            std::cerr << "Error during read: " << error.message() << std::endl;
        }
    });
}

void send_file(const std::string& path, std::shared_ptr<tcp::socket> client_socket) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << path << std::endl;
        return;
    }
    
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    auto buffer = std::make_shared<std::vector<char>>(1024);

    std::function<void(boost::system::error_code, std::size_t)> write_handler;
    write_handler = [buffer, client_socket, &file, write_handler](boost::system::error_code ec, std::size_t /*length*/) mutable {
        if (ec) {
            std::cerr << "Error during file transfer: " << ec.message() << std::endl;
            return;
        }
        if (file) {
            if (file.read(buffer->data(), buffer->size()) || file.gcount() > 0) {
                boost::asio::async_write(*client_socket, boost::asio::buffer(buffer->data(), file.gcount()), write_handler);
            } else {
                std::cerr << "Error reading from file " << std::endl;
            }
        } else {
            std::cout << "File transfer completed successfully." << std::endl;
        }
    };

    if (file.read(buffer->data(), buffer->size()) || file.gcount() > 0) {
        boost::asio::async_write(*client_socket, boost::asio::buffer(buffer->data(), file.gcount()), write_handler);
    } else {
        std::cerr << "Error reading from file " << std::endl;
    }
}


void receive_file(const std::string& path, std::shared_ptr<tcp::socket> client_socket) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cout << "Error creating file: " << path << std::endl;
        return;
    }

    auto buffer = std::make_shared<std::vector<char>>(1024);

    std::function<void(boost::system::error_code, std::size_t)> read_handler;
    read_handler = [buffer, client_socket, &file, read_handler](boost::system::error_code ec, std::size_t length) mutable {
        if (!ec && length > 0) {
            file.write(buffer->data(), length);
            client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);
        } else if (ec) {
            std::cout << "File reception failed: " << ec.message() << std::endl;
        } else {
            std::cout << "File reception completed." << std::endl;
            file.close();
        }
    };

    client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);
}

void start_chat(std::shared_ptr<tcp::socket> client_socket, std::shared_ptr<tcp::socket> server_socket, const std::string& username) {
    const std::string user_color = "\033[34m";  // Blue
    const std::string reset_color = "\033[0m";  // Reset to default

    std::atomic<bool> stop_chatting{false};
    auto buffer = std::make_shared<std::array<char, 1024>>();

    std::function<void(const boost::system::error_code&, std::size_t)> read_handler;

    read_handler = [client_socket, server_socket, buffer, username, &stop_chatting, &read_handler](const boost::system::error_code& error, std::size_t length) mutable {
        if (!error) {
            std::string message(buffer->data(), length);
            if (message.find("/disconnect") == 0) {
                client_socket->close();
                stop_chatting = true;
                std::cout << "The other client has disconnected\nPress enter to continue." << std::endl;
                return;
            } else if (message.find("/file") == 0) {
                std::string path = "received_" + message.substr(6); // Extract the filename from the message
                std::cout << "Receiving file: " << path << std::endl;
                receive_file(path, client_socket);
            } else {
                print_message(username, message, false);
                client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!stop_chatting) {
                std::cout << "The other client closed the program incorrectly\nPress enter to continue." << std::endl;
                stop_chatting = true;
                return;
            }
        }
    };

    client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);

    while (true) {
        std::string message;
        std::cout << user_color << "You\n-----------" << reset_color << std::endl;
        std::getline(std::cin, message);
        if (stop_chatting) {
            notify_server_status(server_socket, "free", username);
            break;
        }
        if (message == "/disconnect") {
            std::cout << "Disconnecting chat." << std::endl;
            boost::asio::async_write(*client_socket, boost::asio::buffer("/disconnect"), handle_write);
            client_socket->close();
            stop_chatting = true;
            notify_server_status(server_socket, "free", username);
            break;
        } else if (message == "/exit") {
            std::cout << "Exiting program." << std::endl;
            boost::asio::async_write(*client_socket, boost::asio::buffer("/disconnect"), handle_write);
            client_socket->close();
            notify_server_status(server_socket, "offline", username);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            exit(0);
        } else if (message.find("/file") == 0) {
            std::string path = message.substr(6); // Extract the path from the command
            send_file(path, client_socket);
        } else if (!stop_chatting) {
            std::string full_message = username + ": " + replace_emojis(message);
            boost::asio::async_write(*client_socket, boost::asio::buffer(full_message), handle_write);
            print_message("You", message, true);
        }
    }
}




void accept_connections(std::shared_ptr<tcp::acceptor> acceptor, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username) {
    auto new_socket = std::make_shared<tcp::socket>(io_context);
    acceptor->async_accept(*new_socket, [new_socket, acceptor, &io_context, server_socket, username](const boost::system::error_code& error) {
        if (!error) {
        	std::cout << "Use /disconnect to disconnect from the other person and /exit to exit the program\nYou can also use these emojis :smile :sad :laugh :angry :wink :heart" << std::endl;
            notify_server_status(server_socket, "no free", username);
            std::thread chat_thread(start_chat, new_socket, server_socket, username);
            chat_thread.detach();
        } else {
            std::cerr << "Error during accept: " << error.message() << std::endl;
        }

        accept_connections(acceptor, io_context, server_socket, username);
    });
}

void connect_to_client(const std::string& client_ip, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username) {
    auto client_socket = std::make_shared<tcp::socket>(io_context);
    tcp::resolver resolver(io_context);
    tcp::resolver::results_type client_endpoints = resolver.resolve(client_ip, "12347");

    boost::asio::async_connect(*client_socket, client_endpoints, [client_socket, server_socket, username](const boost::system::error_code& error, const tcp::endpoint&) {
        if (!error) {
            std::cout << "Connected to client. You can now start chatting.\nUse /disconnect to disconnect from the other person and /exit to exit the program\nYou can also use these emojis :smile :sad :laugh :angry :wink :heart" << std::endl;
            notify_server_status(server_socket, "no free", username);
            std::thread chat_thread(start_chat, client_socket, server_socket, username);
            chat_thread.detach();
        } else {
            std::cerr << "Error during client connection: " << error.message() << std::endl;
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
					
					std::thread handle_thread(handle_read, server_socket, server_socket, std::ref(io_context), username);
                    handle_thread.detach();
                    

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
