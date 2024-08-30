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
#include <cstdlib>
#include <future>

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

void connect_to_client(const std::string& client_ip, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr);

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
        std::cerr << "Error during write: " << error.message() << "\n";
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
            std::cerr << "Error during list request: " << error.message() << "\n";
        }
    });

    auto buffer = std::make_shared<std::array<char, 1024>>();
    server_socket->async_read_some(boost::asio::buffer(*buffer), [server_socket, buffer](const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string response(buffer->data(), length);
            std::cout << "Online clients:\n" << response << std::endl;
        } else {
            std::cerr << "Error during list read: " << error.message() << "\n";
        }
    });
}

void handle_read(std::shared_ptr<tcp::socket> socket, 
                 std::shared_ptr<tcp::socket> server_socket, 
                 boost::asio::io_context& io_context, 
                 const std::string& username, 
                 std::shared_ptr<std::atomic<bool>> connected_ptr) {
    
    auto buffer = std::make_shared<std::array<char, 1024>>();

    socket->async_read_some(boost::asio::buffer(*buffer), 
    [socket, server_socket, buffer, &io_context, username, connected_ptr]
    (const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            static std::unordered_map<std::string, bool> clients_list;
            static std::unordered_map<std::string, std::string> clients_username_ip;
                std::string response(buffer->data(), length);

        	
                if (connected_ptr -> load()) {
                   return; 
                }
                std::fstream file;
                file.open("online_clinets.txt", std::ios::in | std::ios::out | std::ios::trunc);
                if (file.is_open()) {
                    if (response.find("There is no online user") == std::string::npos && response.size() > 35) {
                        std::cout << "\n " << response.substr(0, 33);
                        std::string list = response.substr(33);
                        file << list << std::endl;;
                        file.seekg(0);
                        std::string line;
                        int end_of_username = 0;
                        int end_of_ip_address = 0;
                        std::string ip_address;
                        std::string find_username;
                        if (!clients_list.empty()) {
                            clients_list.clear();
                        }
                        while (std::getline(file, line)) {
                            end_of_ip_address = line.find(" ");
                            if (end_of_ip_address != std::string::npos) {
                                ip_address = line.substr(0, end_of_ip_address);
                                line = line.substr(end_of_ip_address + 1);
                                end_of_username = line.find(" ");
                                std::cout << line << std::endl;
                                if (end_of_username != std::string::npos) {
                                    find_username = line.substr(0, end_of_username);
                                    if (line.find("no free") != std::string::npos) {
                                        clients_list[find_username] = false;
                                    } else {
                                        clients_list[find_username] = true;
                                    }
                                    clients_username_ip[find_username] = ip_address;
                                }
                            }
                        }
                    } else {
                        clients_list.clear();
                        std::cout << "\n" << response << std::endl;
                    }          
                    file.close();
                } else {
                    std::cerr << "Error during open file\n";
                }
                    std::thread([username, server_socket, response, socket, &io_context, connected_ptr]() { 
                    if (connected_ptr -> load()) {
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    std::cout << "Enter the username of the client(only free) you want to connect to: ";
                    std::string target_username;
                    std::getline(std::cin, target_username);
                    while (clients_list.find(target_username) == clients_list.end() || clients_list[target_username] == false) {
                        if (connected_ptr -> load()) {
                            return;
                        }
                        if (clients_list.find(target_username) == clients_list.end()) { 
                            std::cerr << "There is no user with that name! Input again:\t";
                        } else {
                            std::cerr << "The user is no free! Input other username:\t";
                        }
                            std::getline(std::cin, target_username);
                    }                  
                    std::cout << "IP address of the selected client: " << clients_username_ip[target_username] << std::endl;
                    connect_to_client(clients_username_ip[target_username], io_context, server_socket, username, connected_ptr);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    return;  
                }).detach();
                
            

            handle_read(socket, server_socket, io_context, username, connected_ptr);
        } else {
            std::cerr << "Error during read: " << error.message() << "\n";
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
            std::cerr << "Invalid size value: " << size_str << "\n";
        } catch (const std::out_of_range&) {
            std::cerr << "Size value out of range: " << size_str << "\n";
        }
    } else {
        std::cerr << "Invalid input format.\n";
    }
}

std::string extract_filename(const std::string& file_path) {
    return file_path.substr(file_path.find_last_of("/\\") + 1);
}

void send_file_part(boost::asio::ip::tcp::socket& socket, const std::string& file_path, std::streamsize offset, std::streamsize part_size) {
    try {
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open file part.\n";
            return;
        }

        file.seekg(offset);
        const std::size_t buffer_size = 4096; 
        char buffer[buffer_size];

        std::streamsize bytes_to_send = part_size;
        while (bytes_to_send > 0) {
            std::streamsize chunk_size = std::min(bytes_to_send, static_cast<std::streamsize>(buffer_size));
            file.read(buffer, chunk_size);
            boost::asio::write(socket, boost::asio::buffer(buffer, file.gcount()));
            bytes_to_send -= file.gcount();
        }

        file.close();
    } catch (const std::exception& e) {
        std::cerr << "Exception in send_file_part: " << e.what() << '\n';
    }
}


void send_file_multithreaded(const std::string& file_path, boost::asio::ip::tcp::socket& socket) {
    try {
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file) {
            std::cerr << "Failed to open file.\n";
            return;
        }

        std::streamsize file_size = file.tellg();
        const std::size_t num_threads = std::thread::hardware_concurrency();
        std::streamsize part_size = file_size / num_threads;

        std::vector<std::future<void>> futures;

        for (std::size_t i = 0; i < num_threads; ++i) {
            std::streamsize offset = i * part_size;
            std::streamsize current_part_size = (i == num_threads - 1) ? (file_size - offset) : part_size;

            futures.emplace_back(std::async(std::launch::async, send_file_part, std::ref(socket), file_path, offset, current_part_size));
        }

        for (auto& future : futures) {
            future.get();
        }

        std::cout << "File sent successfully using multiple threads." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception in send_file_multithreaded: " << e.what() << '\n';
    }
}

void receive_file_part(boost::asio::ip::tcp::socket& socket, std::ofstream& file, std::streamsize offset, std::streamsize part_size) {
    try {
        const std::size_t buffer_size = 4096; 
        char buffer[buffer_size];

        file.seekp(offset);
        std::streamsize bytes_to_receive = part_size;
        while (bytes_to_receive > 0) {
            std::streamsize chunk_size = std::min(bytes_to_receive, static_cast<std::streamsize>(buffer_size));
            std::streamsize bytes_received = socket.read_some(boost::asio::buffer(buffer, chunk_size));
            file.write(buffer, bytes_received);
            bytes_to_receive -= bytes_received;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in receive_file_part: " << e.what() << '\n';
    }
}

void receive_file_multithreaded(boost::asio::ip::tcp::socket& socket, const std::string& filename, std::streamsize file_size, std::atomic<bool>& receive) {
    try {
        std::ofstream file("received_files/" + filename, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to create file.\n";
            receive = false;
            return;
        }

        const std::size_t num_threads = std::thread::hardware_concurrency();
        std::streamsize part_size = file_size / num_threads;

        std::vector<std::thread> threads;

        for (std::size_t i = 0; i < num_threads; ++i) {
            std::streamsize offset = i * part_size;
            std::streamsize current_part_size = (i == num_threads - 1) ? (file_size - offset) : part_size;

            threads.emplace_back(receive_file_part, std::ref(socket), std::ref(file), offset, current_part_size);
        }

        for (auto& thread : threads) {
            thread.join();
        }

        std::cout << "File received successfully using multiple threads.\n";
        receive = false;
        file.close();
    } catch (const std::exception& e) {
        std::cerr << "Exception in receive_file_multithreaded: " << e.what() << '\n';
        receive = false;
    }
}

void start_chat(std::shared_ptr<tcp::socket> client_socket, std::shared_ptr<tcp::socket> server_socket, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr) {
    const std::string user_color = "\033[34m";  // Blue
    const std::string reset_color = "\033[0m";  // Reset to default
    std::atomic<bool> receive(false);
    std::atomic<bool> stop_chatting{false};
    auto buffer = std::make_shared<std::array<char, 1024>>();
	boost::asio::io_context io_context;
    std::function<void(const boost::system::error_code&, std::size_t)> read_handler;

    read_handler = [client_socket, server_socket, buffer, username, &stop_chatting, &read_handler, &receive, connected_ptr, &io_context](const boost::system::error_code& error, std::size_t length) mutable {
        if (!error) {
            std::string message(buffer->data(), length);
            if (message.find("/disconnect") == 0) {
                client_socket->close();
                stop_chatting = true;
                connected_ptr -> store(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                notify_server_status(server_socket, "free", username);
                handle_read(server_socket, server_socket, io_context, username, connected_ptr);
                std::cout << "The other client has disconnected\nPress enter to continue." << std::endl;
                return;
            } else if (message.find("/file") == 0) {
                std::string input = message.substr(6); 
                std::string filename;
                std::streamsize file_size;
                parse_file_info(" " + input, filename, file_size);
                std::cout << "Receiving file: " << filename << std::endl;
                receive = true;
                receive_file_multithreaded(*client_socket, filename, file_size, receive);
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
            client_socket->cancel();
            client_socket->close();
            stop_chatting = true;
            connected_ptr -> store(false);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            notify_server_status(server_socket, "free", username);
            handle_read(server_socket, server_socket, io_context, username, connected_ptr);          
            break;
        } else if (message == "/exit") {
            std::cout << "Exiting program." << std::endl;
            boost::asio::async_write(*client_socket, boost::asio::buffer("/disconnect"), handle_write);
            client_socket->cancel();
            client_socket->close();
            notify_server_status(server_socket, "offline", username);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            exit(0);
        } else if (message.find("/file") == 0) {
            std::string path = message.substr(6);
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) {
                std::cerr << "Failed to open file.\n";
            } else {
                std::streamsize file_size = file.tellg();
                file.seekg(0, std::ios::beg);
                file.close();
                std::string file_size_str = std::to_string(file_size);
                boost::asio::async_write(*client_socket, boost::asio::buffer("/file " + path + ":" + file_size_str), handle_write);
                send_file_multithreaded(path, *client_socket);
                client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);
            }
        } else if (!stop_chatting && message != "") {
            std::string full_message = username + ": " + replace_emojis(message);
            boost::asio::async_write(*client_socket, boost::asio::buffer(full_message), handle_write);
            print_message("You", message, true);
        }
    }
}




void accept_connections(std::shared_ptr<tcp::acceptor> acceptor, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr) {
    auto new_socket = std::make_shared<tcp::socket>(io_context);
    acceptor->async_accept(*new_socket, [new_socket, acceptor, &io_context, server_socket, username, connected_ptr](const boost::system::error_code& error) {
        if (!error) {
            notify_server_status(server_socket, "no free", username);
            connected_ptr -> store(true);
            auto buffer = std::make_shared<std::array<char, 1024>>();
            new_socket -> async_read_some(boost::asio::buffer(*buffer), [buffer, server_socket, username, new_socket, connected_ptr](const boost::system::error_code& error, std::size_t length) {
                if (!error) {
                    std::string response(buffer -> data());
                    if (response.find("request") != std::string::npos) {
                        connected_ptr -> store(true);
                        std::string reply;
                        std::cout << "Connection request from " << response.substr(8) << std::endl;
                        std::cout << "Write [accept] or [reject]\t"; 
                        std::getline(std::cin, reply);
                        while (!(reply == "accept" || reply == "reject")) {
                            std::cerr << "Wrong command! Input again:\t";
                            std::getline(std::cin, reply);
                        }

                        if (reply == "accept") {
                            boost::asio::async_write(*new_socket, boost::asio::buffer(reply), handle_write);
                            std::cout << "Connection accepted. You can now start chatting." << std::endl;
                            std::cout << "Use /disconnect to disconnect from the other person and /exit to exit the program\nYou can also use these emojis :smile :sad :laugh :angry :wink :heart" << std::endl;
                            std::thread chat_thread(start_chat, new_socket, server_socket, username, connected_ptr);
                            chat_thread.detach();
                        } else if (reply == "reject") {
                            boost::asio::async_write(*new_socket, boost::asio::buffer(reply), handle_write);
                            std::cout << "Connection rejected." << std::endl;
                            notify_server_status(server_socket, "free", username);
                        }
                    }
                }
            });
        } else {
            std::cerr << "Error during accept: " << error.message() << "\n";
            new_socket -> close();
            handle_read(server_socket, server_socket, io_context, username, connected_ptr);
        }

        accept_connections(acceptor, io_context, server_socket, username, connected_ptr);
    });
}

void connect_to_client(const std::string& client_ip, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr) {
    auto client_socket = std::make_shared<tcp::socket>(io_context);
    tcp::resolver resolver(io_context);
    tcp::resolver::results_type client_endpoints = resolver.resolve(client_ip, "12347");

    boost::asio::async_connect(*client_socket, client_endpoints, [client_socket, server_socket, username, &io_context, connected_ptr](const boost::system::error_code& error, const tcp::endpoint&) {
        if (!error) {
            notify_server_status(server_socket, "no free", username);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            connected_ptr -> store(true);
            boost::asio::async_write(*client_socket, boost::asio::buffer("request " + username), handle_write);
            auto buffer = std::make_shared<std::array<char, 1024>>();	
    	    client_socket->async_read_some(boost::asio::buffer(*buffer), [buffer, client_socket, server_socket, username, connected_ptr] (const boost::system::error_code& error, std::size_t length) {
                if (!error) {
                    std::string replay(buffer -> data());
                    if (replay == "accept") {
                        std::cout << "Connected to client. You can now start chatting.\nUse /disconnect to disconnect from the other person and /exit to exit the program\nYou can also use these emojis :smile :sad :laugh :angry :wink :heart" << std::endl;
                        std::thread chat_thread(start_chat, client_socket, server_socket, username, connected_ptr);
                        chat_thread.detach();
                    } else if (replay == "reject") {
                        std::cout << "Connection rejected." << std::endl;
                		notify_server_status(server_socket, "free", username);
                        client_socket -> close();
                    }
                } else {
                    std::cerr << "Erroor during read from new client: " << error.message() << "\n";
                }
            });
        } else {
            std::cerr << "Error during client connection: " << error.message() << "\n";            
            client_socket -> close();
            handle_read(server_socket, server_socket, io_context, username, connected_ptr);
        }
    });
}


std::pair<std::string, std::string> read_config(const std::string& filename) {
    std::ifstream config_file(filename);
    std::string line, host, port;

    while (std::getline(config_file, line)) {
        std::istringstream iss(line);
        std::string key, value;
        if (iss >> key >> value) {
            if (key == "hostIp") {
                host = value;
            } else if (key == "port") {
                port = value;
            }
        }
    }

    return {host, port};
}
	

int main(int argc, char* argv[]) {	
    try {
        if (argc != 2) { 
            std::cerr << "Usage: client <username>\n";
            return 1;
        }

        std::string username = argv[1];
        auto [host, port] = read_config("chat.conf");

        if (host.empty() || port.empty()) {
        	std::string example = "\n-----\nhostIp 192.168.0.109\nport 12345\n-----\n";
            std::cerr << "Error: hostIp or port not specified in chat.conf\nExample chat.conf:\n" << example;            
            return 1;
        }
		std::cout << "𝕎 𝔼 𝕃 ℂ 𝕆 𝕄 𝔼  𝕋 𝕆  𝕃 𝕀ℕ 𝕌 𝕏  ℂ ℍ 𝔸 𝕋\n";
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve(host, port);
        auto server_socket = std::make_shared<tcp::socket>(io_context);

        boost::asio::async_connect(*server_socket, endpoints, [server_socket, &io_context, username](const boost::system::error_code& error, const tcp::endpoint&) {
            if (!error) {
                std::cout << "Connected to server." << std::endl;
                std::string ip_address = server_socket->local_endpoint().address().to_string();
                std::string buffer = ip_address + " " + username;

                boost::asio::async_write(*server_socket, boost::asio::buffer(buffer), [server_socket, &io_context, username](const boost::system::error_code& error, std::size_t length) {
                    handle_write(error, length);

                    auto acceptor = std::make_shared<tcp::acceptor>(io_context, tcp::endpoint(tcp::v4(), 12347)); // Port for accepting connections                    
                    auto connected_ptr = std::make_shared<std::atomic<bool>>(false);
                    
                    std::thread accept_thread(accept_connections, acceptor, std::ref(io_context), server_socket, username, connected_ptr);
                    accept_thread.detach();
                                       
                    std::thread handle_thread([server_socket, &io_context, username, connected_ptr]() {
                        handle_read(server_socket, server_socket, io_context, username, connected_ptr);
                    });
                    handle_thread.detach();

                    // Run io_context in a loop to check for pending async operations
                    while (true) {
                        io_context.run();
                        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Adjust sleep duration as needed
                    }
                });
            } else {
                std::cerr << "Error during connect: " << error.message() << "\n";
            }
        });

        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
