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
#include <filesystem>
#include <functional>
#include <unordered_map>

namespace fs = std::filesystem;
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
    std::string is_emoji = replace_emojis(message);
    if (is_emoji != message) {
        std::cout << "\033[A" << replace_emojis(message);
        for (int i = 0; i < is_emoji.size(); ++i) {
            std::cout << " ";
        }
        std::cout << std::endl;
    }
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
std::atomic<bool> acception(false);

void handle_read(std::shared_ptr<tcp::socket> socket, 
                 std::shared_ptr<tcp::socket> server_socket, 
                 boost::asio::io_context& io_context, 
                 const std::string& username, 
                 std::shared_ptr<std::atomic<bool>> acception) {
    
    auto buffer = std::make_shared<std::array<char, 1024>>();

    socket->async_read_some(boost::asio::buffer(*buffer), 
    [socket, server_socket, buffer, &io_context, username, acception]
    (const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string response(buffer->data(), length);

            if (response.find("request ", 0) == 0) {
                
                std::string requester = response.substr(8);
                std::cout << "\nConnection request from: " << requester << " Press ENTER to continue." << std::endl;
                acception->store(true);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string reply;
                std::getline(std::cin, reply);
                while (!(reply == "accept" || reply == "reject")) {
                    std::cerr << "Wrong command!!! Input again\t";
                    std::getline(std::cin, reply);                    
                }

                if (reply == "accept") {
                    std::string response_message = "accept from " + username + " for " + requester;
                    boost::asio::async_write(*socket, boost::asio::buffer(response_message), handle_write);
                    std::cout << "Connection accepted. You can now start chatting." << std::endl;
                    notify_server_status(server_socket, "no free", username);
                } else if (reply == "reject") {
                	acception->store(false);
                    std::string response_message = "reject from " + username + " for " + requester;
                    boost::asio::async_write(*socket, boost::asio::buffer(response_message), handle_write);
                    std::cout << "Connection rejected." << std::endl;
                    notify_server_status(server_socket, "free", username);
                }

            } else if (response == "reject") {
                std::cout << "Connection request rejected by the other client." << std::endl;
                notify_server_status(server_socket, "free", username);
            } else if (response.find("IP: ") == 0) {
                std::string client_ip = response.substr(4);
                std::cout << "IP address of the selected client: " << client_ip << std::endl;
                connect_to_client(client_ip, io_context, server_socket, username);
            } else { 
                std::thread([username, server_socket, response, socket, &io_context, acception]() {
                    std::string text;
                    if (response.find("ip not found") == 0) {
                        text = "Username not found.";
                        acception->store(false);
                    } else {
                    	acception->store(false);
                        text = "Online clients:\n";
                    }

                std::cout << text << response << std::endl;           	
                std::fstream file;
                std::unordered_map<std::string, bool> clients_list;
                file.open("online_clinets.txt", std::ios::in | std::ios::out | std::ios::trunc);
                if (file.is_open()) {
                    if (response.find("There is no online user") == std::string::npos && response.size() > 35) {
                        std::string list = response.substr(33);
                        file << list;
                        file.seekg(0);
                        std::string line;
                        int end_of_username = 0;
                        std::string find_username;
                        if (!clients_list.empty()) {
                            clients_list.clear();
                        }
                        while (std::getline(file, line)) {
                            end_of_username = line.find(" ");
                            if (end_of_username != std::string::npos) {
                                find_username = line.substr(0, end_of_username);
                                if (line.find("no free") != std::string::npos) {
                                    clients_list[find_username] = false;
                                } else {
                                    clients_list[find_username] = true;
                                }
                            }
                        }
                    } else {
                        clients_list.clear();
                    }          
                    file.close();
                } else {
                    std::cerr << "Error during open file" << std::endl;
                }
                    std::cout << "Enter the username of the client you want to connect to: ";
                    std::string target_username;
                    std::getline(std::cin, target_username);
                    while (clients_list.find(target_username) == clients_list.end() || clients_list[target_username] == false) {
                        if (acception->load()) { 
                            std::cout << "Accept or reject (accept/reject): ";
                            break;
                        }
                        if (clients_list.find(target_username) == clients_list.end()) { 
                            std::cerr << "There is no user with that name!!! Input again\t";
                        } else {
                            std::cerr << "The user is no free!!! Input other username\t";
                        }
                            std::getline(std::cin, target_username);
                    }                  
                     
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    if (!acception->load()) { 
                        std::cout << "Wait for the user to accept the connection.." << std::endl;
                        std::string connect_message = "connect " + target_username;
                        boost::asio::async_write(*socket, boost::asio::buffer(connect_message), handle_write);

                        auto buffer = std::make_shared<std::array<char, 1024>>();
                        socket->async_read_some(boost::asio::buffer(*buffer), 
                        [socket, server_socket, buffer, response, &io_context, username]
                        (const boost::system::error_code& error, std::size_t length) {
                            if (!error) {
                                std::string response(buffer->data(), length);
                                if (response.find("IP: ") == 0) {
                                    std::string client_ip = response.substr(4);
                                    std::cout << "IP address of the selected client: " << client_ip << std::endl;
                                    connect_to_client(client_ip, io_context, server_socket, username);
                                } else if (response.find("reject") == 0) {
                                    std::cout << "Connection rejected." << std::endl;
                                    notify_server_status(server_socket, "free", username);
                                } else {
                                    std::cerr << "Unexpected response: " << response << std::endl;
                                }
                            } else {
                                std::cerr << "Error during read: " << error.message() << std::endl;
                            }
                        });
                    }
                }).detach();
            }

            handle_read(socket, server_socket, io_context, username, acception);
        } else {
            std::cerr << "Error during read: " << error.message() << std::endl;
        }
    });
}




void parse_file_info(const std::string& input, std::string& filename, std::streamsize& file_size) {
 
    std::size_t space_pos = input.find(' ');
    std::size_t colon_pos = input.find(':', space_pos);

    if (space_pos != std::string::npos && colon_pos != std::string::npos) {
 
        filename = input.substr(space_pos + 1, colon_pos - space_pos - 1);

        std::string size_str = input.substr(colon_pos + 1);
        try {
            file_size = std::stoll(size_str); 
        } catch (const std::invalid_argument&) {
            std::cerr << "Invalid size value: " << size_str << std::endl;
        } catch (const std::out_of_range&) {
            std::cerr << "Size value out of range: " << size_str << std::endl;
        }
    } else {
        std::cerr << "Invalid input format." << std::endl;
    }
}

std::string extract_filename(const std::string& file_path) {
    return file_path.substr(file_path.find_last_of("/\\") + 1);
}

void send_file(const std::string& file_path, boost::asio::ip::tcp::socket& socket) {
  	std::thread([&socket, file_path]() {
        try {
            std::ifstream file(file_path, std::ios::binary | std::ios::ate);
            if (!file) {
                std::cerr << "Failed to open file.\n";
                return;
            }
		file.seekg(0, std::ios::beg);

            std::vector<char> confirmation(128); 
            size_t length = socket.read_some(boost::asio::buffer(confirmation));
            std::string confirmation_str(confirmation.data(), length);
            if (confirmation_str.find("OK") != 0) {
                std::cerr << "Failed to receive confirmation: " << confirmation_str << std::endl;
                return;
            }

            char buffer[1024];
            while (file.read(buffer, sizeof(buffer))) {
                boost::asio::write(socket, boost::asio::buffer(buffer, file.gcount()));
            }
            if (file.gcount() > 0) {
                boost::asio::write(socket, boost::asio::buffer(buffer, file.gcount()));
                
            }
            std::cout << "File sent successfully." << std::endl;

            file.close(); 
        } catch (const std::exception& e) {
            std::cerr << "Exception in send_file: " << e.what() << '\n';
        }
    }).detach();
    
}

void receive_file(boost::asio::ip::tcp::socket& socket, std::string input, std::atomic<bool>& receive) {
   std::thread([&socket, input, &receive]() {
        try {
         	
            std::string filename;         
            std::streamsize file_size;
            parse_file_info(" " + input, filename, file_size);
            
            fs::path dir = "received_files";
    		if (!fs::exists(dir)) {
        		fs::create_directory(dir);
    		}
            
            std::ofstream file(std::string(dir/filename), std::ios::binary);
            if (!file) {
                std::cerr << "Failed to create file.\n";
                receive = false;
                return;
            }

            
            boost::asio::write(socket, boost::asio::buffer("OK"));

            char buffer[1024];
            std::streamsize total_bytes_received = 0;
            while (total_bytes_received < file_size) {
                size_t bytes_received = socket.read_some(boost::asio::buffer(buffer));
                file.write(buffer, bytes_received);
                total_bytes_received += bytes_received;
            }
            std::cout << "File received successfully." << std::endl;
			receive = false;
            file.close(); 
        } catch (const std::exception& e) {
            std::cerr << "Exception in receive_file: " << e.what() << '\n';
            receive = false;

        }
    }).detach();
}

void start_chat(std::shared_ptr<tcp::socket> client_socket, std::shared_ptr<tcp::socket> server_socket, const std::string& username) {
    const std::string user_color = "\033[34m";  // Blue
    const std::string reset_color = "\033[0m";  // Reset to default
	std::atomic<bool> receive(false);
    std::atomic<bool> stop_chatting{false};
    auto buffer = std::make_shared<std::array<char, 1024>>();

    std::function<void(const boost::system::error_code&, std::size_t)> read_handler;

    read_handler = [client_socket, server_socket, buffer, username, &stop_chatting, &read_handler, &receive](const boost::system::error_code& error, std::size_t length) mutable {
        if (!error) {
            std::string message(buffer->data(), length);
            if (message.find("/disconnect") == 0) {
                client_socket->close();
                stop_chatting = true;
                std::cout << "The other client has disconnected\nPress enter to continue." << std::endl;
                return;
            } else if (message.find("/file") == 0) {
                std::string path = message.substr(6); // Extract the filename from the message
                std::cout << "Receiving file: " << path << std::endl;
                receive = true;
                receive_file(*client_socket, path, receive);
                client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);
            } else if (!receive) {
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
        	std::string path = message.substr(6);
        	std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) {
                std::cerr << "Failed to open file.\n";
                return;
            }
            std::streamsize file_size = file.tellg();
            file.seekg(0, std::ios::beg);
			file.close();
            std::string file_size_str = std::to_string(file_size);
            
        	boost::asio::async_write(*client_socket, boost::asio::buffer("/file " + path + ":" + file_size_str), handle_write);
            send_file(path, *client_socket);
            client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);
        } else if (!stop_chatting && message != "") {
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
					
					auto acception_ptr = std::make_shared<std::atomic<bool>>(false);

					std::thread handle_thread([server_socket, &io_context, username, acception_ptr]() {
    				handle_read(server_socket, server_socket, io_context, username, acception_ptr);
					});
					handle_thread.detach();



                    // Run io_context in a loop to check for pending async operations
                    while (true) {
                        io_context.run();
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

