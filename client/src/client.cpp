#include "client.hpp"
#include "client_utils.hpp"

namespace fs = std::filesystem;
using boost::asio::ip::tcp;

// Function to connect to another client by IP address
void connect_to_client(const std::string& client_ip, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr, std::shared_ptr<std::atomic<bool>> getlineThread_ptr);

// Handler for writing data asynchronously
void handle_write(const boost::system::error_code& error, std::size_t) {
    if (error) {
        std::cerr << "Error during write: " << error.message() << "\n";
    }
}

// Function to handle reading data from the server
void handle_read(std::shared_ptr<tcp::socket> socket, 
                 std::shared_ptr<tcp::socket> server_socket, 
                 boost::asio::io_context& io_context, 
                 const std::string& username, 
                 std::shared_ptr<std::atomic<bool>> connected_ptr, 
                 std::shared_ptr<std::atomic<bool>> getlineThread_ptr) {
    
    auto buffer = std::make_shared<std::array<char, 1024>>();
	// Asynchronous read operation from the server
    socket->async_read_some(boost::asio::buffer(*buffer), 
    [socket, server_socket, buffer, &io_context, username, connected_ptr, getlineThread_ptr]
    (const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string response(buffer->data(), length);
            static std::unordered_map<std::string, bool> clients_list;
            static std::unordered_map<std::string, std::string> clients_username_ip;
            static bool print_welcome = true;
            
                if (connected_ptr -> load()) {
                   return; 
                }
                    system("clear");
                    if (print_welcome) {
                        std::cout << "𝕎 𝔼 𝕃 ℂ 𝕆 𝕄 𝔼  𝕋 𝕆  𝕃 𝕀ℕ 𝕌 𝕏  ℂ ℍ 𝔸 𝕋\n";
                        print_welcome = false;
                    } else {
                        std::cout << "𝕃 𝕀ℕ 𝕌 𝕏  ℂ ℍ 𝔸 𝕋\n";
                    }
                    if (response.find("That username is not available") != std::string::npos) {
                        std::cout << response << std::endl;
                        std::exit(0);
                    }
                    if (response.find("There is no online user") == std::string::npos && response.size() > 35) {
                        std::cout << "\n " << response.substr(0, 33);
                        std::string list = response.substr(33);
                        std::istringstream iss(list);
                        std::string line;
                        int end_of_username = 0;
                        int end_of_ip_address = 0;
                        std::string ip_address;
                        std::string find_username;
                        if (!clients_list.empty()) {
                            clients_list.clear();
                        }
                        while (std::getline(iss, line)) {
                            end_of_ip_address = line.find(" ");
                            if (end_of_ip_address != std::string::npos) {
                                
                                    ip_address = line.substr(0, end_of_ip_address);
                                    line = line.substr(end_of_ip_address + 1) + "\n";
                                    end_of_username = line.find(" ");
                                
                                std::cout << line;
                                
                                if (end_of_username != std::string::npos) {
                                    if (line.find("username of") != std::string::npos) {
                                        continue;
                                    }
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
  					
  					if (getlineThread_ptr -> load()) {
  					
                    std::thread([username, server_socket, response, socket, &io_context, connected_ptr, getlineThread_ptr]() { 
                    if (connected_ptr -> load()) {
                        return;
                    }                    
                    std::string target_username;                    
                    getlineThread_ptr -> store(false);                	
                    std::getline(std::cin, target_username);
                    if (connected_ptr -> load()) {
                        std::cout << "Write [accept] or [reject]\t";
                        return;
                    }

                    while ((clients_list.find(target_username) == clients_list.end() || clients_list[target_username] == false)) {
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
                    getlineThread_ptr -> store(false);
                    if (target_username != "") {
                                    
                    std::cout << "IP address of the selected client: " << clients_username_ip[target_username] << std::endl;
                    std::cout << "Wait for another client to accept the connection(time out 15 sec).." << std::endl;
                    connect_to_client(clients_username_ip[target_username], io_context, server_socket, username, connected_ptr, getlineThread_ptr);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    return;
                    }  
                }).detach();
                
                }
            
			if (connected_ptr -> load()) {
            	return;
            }
            handle_read(socket, server_socket, io_context, username, connected_ptr, getlineThread_ptr);
        } else {
            std::cerr << "Server problem\n";
            std::exit(1);
        }
    });
}

// Function to start a chat session between two clients
void start_chat(std::shared_ptr<tcp::socket> client_socket, std::shared_ptr<tcp::socket> server_socket, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr, std::shared_ptr<std::atomic<bool>> getlineThread_ptr, boost::asio::io_context& io_context) {
    const std::string user_color = "\033[34m";  // Blue
    const std::string reset_color = "\033[0m";  // Reset to default
    std::atomic<bool> receive(false);
    bool stop_chatting = false;
    auto buffer = std::make_shared<std::array<char, 1024>>();
    std::function<void(const boost::system::error_code&, std::size_t)> read_handler;
    std::string path;

    read_handler = [client_socket, server_socket, buffer, username, &stop_chatting, &read_handler, &receive, connected_ptr, &io_context, getlineThread_ptr, &path](const boost::system::error_code& error, std::size_t length) mutable {
        if (!error) {
            std::string encrypted_message(buffer->data(), length);
            std::string message = aes_decrypt(encrypted_message);
            if (message.find("/disconnect") == 0) {
                client_socket->cancel();
                client_socket->close();
                stop_chatting = true;
                connected_ptr->store(false);
                getlineThread_ptr->store(true);
                std::cout << "The other client has disconnected\nPress enter to continue." << std::endl;                
                return;
            } else if (message.find("/file") == 0) {
                std::string filename = message.substr(6);
                std::streamsize file_size;
                std::size_t num_threads = 0;
                parse_file_info(filename, file_size, num_threads);
                std::cout << "Receiving file: " << filename << std::endl;
                receive = true;
                receive_file_multithreaded(*client_socket, filename, file_size, receive, num_threads);
                client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);
            } else if (message.find("FILE_CREATED") != std::string::npos) {
                std::cout << "Confirmation received. Let's start sending the file..." << std::endl;
                std::size_t find_num_threads = message.find(" ");
                    if (find_num_threads != std::string::npos) {
                        std::string num_threads = message.substr(find_num_threads + 1);
                        send_file_multithreaded(path, *client_socket, std::atoi(num_threads.data()));
                        client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);
                    } else {
                        std::cerr << "Number of threads not finded\n";
                    }
            } else if (!receive) {
                print_message(username, message, false);
                client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);
            } 
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!stop_chatting) {
                client_socket->cancel();
                client_socket->close();
                connected_ptr->store(false);
                getlineThread_ptr->store(true);
                std::cout << "The other client closed the program incorrectly\nPress enter to continue." << std::endl;
                stop_chatting = true;
                return;
            }
        }
    };

    client_socket->async_read_some(boost::asio::buffer(*buffer), read_handler);
    std::string message;
    while (true) {
        std::cout << user_color << "You\n-----------" << reset_color << std::endl;
        std::getline(std::cin, message);
        if (stop_chatting) { 
            notify_server_status(server_socket, "free", username);
            handle_read(server_socket, server_socket, io_context, username, connected_ptr, getlineThread_ptr);          
            break;
        }
        if (message == "/disconnect") {
            std::cout << "Disconnecting chat." << std::endl;
            std::string encrypted_message = aes_encrypt("/disconnect");
            boost::asio::async_write(*client_socket, boost::asio::buffer(encrypted_message), handle_write);
            client_socket->cancel();
            client_socket->close();
            stop_chatting = true;
            connected_ptr->store(false);
            getlineThread_ptr->store(true);
            notify_server_status(server_socket, "free", username);
            handle_read(server_socket, server_socket, io_context, username, connected_ptr, getlineThread_ptr);          
            break;
        } else if (message == "/exit") {
            std::cout << "Exiting program." << std::endl;
            std::string encrypted_message = aes_encrypt("/disconnect");
            boost::asio::async_write(*client_socket, boost::asio::buffer(encrypted_message), handle_write);
            client_socket->cancel();
            client_socket->close();
            notify_server_status(server_socket, "offline", username);
            exit(0);
        } else if (message.find("/file") == 0) {
            path = message.substr(6);
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) {
                std::cerr << "Error to open file.\n";
            } else {
                std::streamsize file_size = file.tellg();
                file.close();
                std::string file_size_str = std::to_string(file_size);
                const std::size_t num_threads = std::thread::hardware_concurrency();
                std::string file_info = "/file " + path + ":" + file_size_str + ":" + std::to_string(num_threads);
				std::string encrypted_message = aes_encrypt(file_info);
                boost::asio::async_write(*client_socket, boost::asio::buffer(encrypted_message), handle_write);
            }
        } else if (!stop_chatting && message != "") {
            std::string full_message = username + ": " + replace_emojis(message);
            std::string encrypted_message = aes_encrypt(full_message);
            boost::asio::async_write(*client_socket, boost::asio::buffer(encrypted_message), handle_write);
            print_message("You", message, true);
        }
    }
}

// function to receive a request from another client
void accept_connections(std::shared_ptr<tcp::acceptor> acceptor, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr, std::shared_ptr<std::atomic<bool>> getlineThread_ptr) {
    auto new_socket = std::make_shared<tcp::socket>(io_context);
    acceptor->async_accept(*new_socket, [new_socket, acceptor, &io_context, server_socket, username, connected_ptr, getlineThread_ptr](const boost::system::error_code& error) {
        if (!error) {
        	std::cout << "Use /disconnect to disconnect from the other person and /exit to exit the program\nYou can also use these emojis :smile :sad :laugh :angry :wink :heart" << std::endl;
            notify_server_status(server_socket, "no free", username);
            connected_ptr -> store(true);
            auto buffer = std::make_shared<std::array<char, 1024>>();
            new_socket -> async_read_some(boost::asio::buffer(*buffer), [buffer, server_socket, username, new_socket, connected_ptr, getlineThread_ptr, &io_context](const boost::system::error_code& error, std::size_t length) {
                if (!error) {
                    std::string response(buffer -> data());
                    if (response.find("request") != std::string::npos) {
                        connected_ptr -> store(true);
                        std::string reply;
                        std::cout << "Connection request from " << response.substr(8) << std::endl;
                        std::cout << "Press Enter to continue" <<  std::endl;
                        std::getline(std::cin, reply);
                        while (!(reply == "accept" || reply == "reject")) {
                            std::cerr << "Wrong command! Input again:\t";
                            std::getline(std::cin, reply);
                        }

                        if (reply == "accept") {
                            boost::asio::async_write(*new_socket, boost::asio::buffer(reply), handle_write);
                            std::cout << "Connection accepted. You can now start chatting." << std::endl;
                            std::cout << "Use /disconnect to disconnect from the other person and /exit to exit the program\nYou can also use these emojis :smile :sad :laugh :angry :wink :heart" << std::endl;
                            std::thread chat_thread([new_socket, server_socket, username, connected_ptr, getlineThread_ptr, &io_context]() {
                        start_chat(new_socket, server_socket, username, connected_ptr, getlineThread_ptr, io_context);
                    	});
                    	chat_thread.detach();
                        } else if (reply == "reject") {
                            boost::asio::async_write(*new_socket, boost::asio::buffer(reply), handle_write);
                            std::cout << "Connection rejected." << std::endl;
                            new_socket -> cancel();
                        	new_socket -> close();
                            connected_ptr -> store(false);
                        	getlineThread_ptr -> store(true);  
                            notify_server_status(server_socket, "free", username);                          
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            handle_read(server_socket, server_socket, io_context, username, connected_ptr, getlineThread_ptr);
                            return;
                        }
                    }
                }
            });
        } else {
            notify_server_status(server_socket, "free", username);
            new_socket -> cancel();
            new_socket -> close();
            connected_ptr -> store(false);
            getlineThread_ptr -> store(true);
            std::cout << "The other client closed the program incorrectly or the request timed out\nPress enter to continue." << std::endl;
            std::cin.get();
            handle_read(server_socket, server_socket, io_context, username, connected_ptr, getlineThread_ptr);
        }

        accept_connections(acceptor, io_context, server_socket, username, connected_ptr, getlineThread_ptr);
    });
}


void connect_to_client(const std::string& client_ip, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr, std::shared_ptr<std::atomic<bool>> getlineThread_ptr) {
    auto client_socket = std::make_shared<tcp::socket>(io_context);
    auto timer = std::make_shared<boost::asio::steady_timer>(io_context, std::chrono::seconds(15));
    
    tcp::resolver resolver(io_context);
    tcp::resolver::results_type client_endpoints = resolver.resolve(client_ip, "12347");

    boost::asio::async_connect(*client_socket, client_endpoints, [client_socket, server_socket, username, &io_context, connected_ptr, getlineThread_ptr, timer](const boost::system::error_code& error, const tcp::endpoint&) {
        if (!error) {
            notify_server_status(server_socket, "no free", username);
            connected_ptr->store(true);
            boost::asio::async_write(*client_socket, boost::asio::buffer("request " + username), handle_write);

            auto buffer = std::make_shared<std::array<char, 1024>>();
            timer->async_wait([client_socket, timer, server_socket, username, connected_ptr, getlineThread_ptr, &io_context](const boost::system::error_code& ec) {
                if (!ec) {
                    std::cout << "Timeout: no response from client.\n";
                    client_socket->cancel();
                    notify_server_status(server_socket, "free", username);
                    connected_ptr->store(false);
                    getlineThread_ptr->store(true);
                    std::cout << "Press Enter to continue." << std::endl;
                    std::cin.get();
                    handle_read(server_socket, server_socket, io_context, username, connected_ptr, getlineThread_ptr);
                    return;
                }
            });
            if (!connected_ptr->load()) {
            	return;
            }
            client_socket->async_read_some(boost::asio::buffer(*buffer), [buffer, client_socket, server_socket, username, &io_context, connected_ptr, getlineThread_ptr, timer](const boost::system::error_code& error, std::size_t length) {
                if (!error) {
                    timer->cancel();

                    std::string replay(buffer->data());
                    if (replay == "accept") {
                        std::cout << "Connected to client. You can now start chatting.\nUse /disconnect to disconnect from the other person and /exit to exit the program.\n";
                        
                        std::thread chat_thread([client_socket, server_socket, username, connected_ptr, getlineThread_ptr, &io_context]() {
                            start_chat(client_socket, server_socket, username, connected_ptr, getlineThread_ptr, io_context);
                        });
                        chat_thread.detach();
                    } else if (replay == "reject") {
                        std::cout << "Connection rejected." << std::endl;
                        std::cout << "Press Enter to continue" << std::endl;
                        std::cin.get();
                        connected_ptr->store(false);
                        getlineThread_ptr->store(true);
                        client_socket->cancel();
                        client_socket->close();
                        notify_server_status(server_socket, "free", username);
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        handle_read(server_socket, server_socket, io_context, username, connected_ptr, getlineThread_ptr);
                        return;
                    }
                } else {
                    timer->cancel();
                    notify_server_status(server_socket, "free", username);
                    connected_ptr->store(false);
                    getlineThread_ptr->store(true);
                    client_socket->cancel();
                    client_socket->close();                                       
                    handle_read(server_socket, server_socket, io_context, username, connected_ptr, getlineThread_ptr);
                }
            });
        } else {
            std::cerr << "Error during client connection: " << error.message() << "\n";
            handle_read(server_socket, server_socket, io_context, username, connected_ptr, getlineThread_ptr);
        }
    });
} 
