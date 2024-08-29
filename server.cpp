#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <fstream>
#include <memory>
#include <array>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <thread>

#define DEFAULT_PORT 12345

using boost::asio::ip::tcp;

std::mutex mutex;
std::string username_for_other_scope;

void send_list(const std::shared_ptr<tcp::socket> socket,const std::string username, std::unordered_map<std::string, std::string>& client_username_ip, bool updated = false) {
    std::string response;
    if (updated) {
        response = "-----------LIST UPDATED---------\n";
    } else {
        response = "-----------THE LIST-------------\n";
    }
    if (!client_username_ip.empty()) {
        std::ifstream file("clients_info.txt");
        if(!file.is_open()) {
            std::cerr << "Unable to open file for read" << std::endl;
            return;
        }
        std::string line;
        int end_of_ip_address = 0;
        while (std::getline(file, line)) {
            end_of_ip_address = line.find(" ");
            if (end_of_ip_address != std::string::npos) {
                if (line.substr(end_of_ip_address + 1, username.size()) == username) {
                    continue;
                }
                response += line.substr(end_of_ip_address + 1) + "\n";
            }        
        }
        file.close();
    } else {
        response += "There is no online user\n";
    }
    boost::asio::async_write(*socket, boost::asio::buffer(response), [socket](const boost::system::error_code& error, std::size_t) {
        if (error) {
            std::cerr << "Error during write: " << error.message() << std::endl;
        }
    });
}

void send_updated_list(const std::unordered_map<std::string, std::shared_ptr<tcp::socket>>& client_ip_sockets, std::unordered_map<std::string,std::string>& client_username_ip, std::unordered_map<std::string, bool>& changed_list, std::unordered_map<std::string, bool>& free_clinets) {
    for (auto& element: client_username_ip) {
        auto changed = changed_list.find(element.first);
        if (changed != changed_list.end()) {
            if (changed -> second) {
                auto waiting = free_clinets.find(element.first);
                if (waiting != free_clinets.end()) {
                    if (waiting -> second) {
                        auto find_socket = client_ip_sockets.find(element.second);
                        if (find_socket != client_ip_sockets.end()) {
                            send_list(find_socket -> second, element.first, client_username_ip, true);
                            changed_list[element.first] = false;
                            return;    
                        }
                    }
                }
            }
        }
    } 
}

void change_status(const std::shared_ptr<tcp::socket> socket, const std::string received_data, const int& end_of_request, std::string username, std::unordered_map<std::string, std::string>& client_username_ip, std::unordered_map<std::string, bool>& changed_list, std::unordered_map<std::string, bool>& free_clients, std::unordered_map<std::string, std::shared_ptr<tcp::socket>>& client_ip_sockets) {
    std::ifstream file("clients_info.txt");
    if (!file.is_open()) {
        std::cerr << "Unable to open file for change" << std::endl;
        return;
    }
    std::string line;
    std::string changed_line;
    int line_number = 0;
    int target_line = -1;
    std::vector<std::string> lines;
    int find_end_of_ip_address = 0;
    username += " ";
    while(std::getline(file, line)) {
        ++line_number;
        lines.push_back(line);
        if (line.find(username) != std::string::npos) {
            target_line = line_number;
            changed_line = line;
        }
    }
    file.close();
    username = username.substr(0, username.size() - 1);
    find_end_of_ip_address = changed_line.find(username);
    changed_line = changed_line.substr(0, end_of_request + find_end_of_ip_address);
    std::string change = received_data.substr(end_of_request + 1);
    if (change == "no free") {
        free_clients[username] = false;
        changed_line += " online no free\n";
    } else if (change == "free" ) {
        free_clients[username] = true;
        changed_line += " online free\n"; 
    } else if (change == "offline") {
        changed_line = "";
        free_clients[username] = false;  
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::ofstream file_for_change("clients_info.txt");
        for(int i = 0; i < lines.size(); ++i) {
            if (i == target_line - 1) {
                file_for_change << changed_line;
                continue;
            }
            file_for_change << lines[i] << std::endl;
        }
        file_for_change.close();
    }
   if (change == "free") {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        changed_list[username] = false;
        send_list(socket, username, client_username_ip);
    }
    for (auto& element: changed_list) {
        if (element.first == username) {
            continue;
        }
        element.second = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    send_updated_list(client_ip_sockets, client_username_ip, changed_list, free_clients);
}

void handle_read(std::shared_ptr<tcp::socket> socket, bool start_connection, std::shared_ptr<std::string> username_for_other_scope) {
    static bool open_file_first_time = true;
        if (open_file_first_time) {
            std::lock_guard<std::mutex> lock(mutex);
            std::ofstream file("clients_info.txt");
            if((!file.is_open())) {
                    std::cerr << "Unable to open file for write\n";
            }
            file << "";
            file.close();
            open_file_first_time = false;
        }
    auto buffer = std::make_shared<std::array<char, 1024>>();
    std::string client_ip = socket -> remote_endpoint().address().to_string();                                                                                                                                                     
    static std::unordered_map<std::string, std::string> client_username_ip;
    static std::unordered_map<std::string, bool> changed_list;
    static std::unordered_map<std::string, std::shared_ptr<tcp::socket>> client_ip_sockets;
    static std::unordered_map<std::string, bool> free_clients;
    socket->async_read_some(boost::asio::buffer(*buffer), [socket, buffer, start_connection, username_for_other_scope, client_ip] (const boost::system::error_code& error, std::size_t length) {
    if (!error) {
        std::string recived_data(buffer -> data(),length);
        int end_of_request = recived_data.find(" ");
        std::string request = recived_data.substr(0, end_of_request);
        std::string username;
        if (end_of_request != std::string::npos) {
            std::string request = recived_data.substr(0, end_of_request);
        }
		if (start_connection) {
            username = recived_data.substr(end_of_request + 1);
            auto it = client_username_ip.find(username);
            if (it != client_username_ip.end()) {
                boost::asio::async_write(*socket, boost::asio::buffer("That username is not available. Please restart program\n"), [socket](const boost::system::error_code& error, std::size_t) {
                if (error) {
                    std::cerr << "Error during write: " << error.message() << std::endl;
                    }
                });                       
                socket->close();
                return;
            }
            *username_for_other_scope = username;
            send_list(socket, username, client_username_ip);
            std::lock_guard<std::mutex> lock(mutex);
            std::ofstream file("clients_info.txt", std::ios::app);
            client_ip_sockets[client_ip] = socket;
            client_username_ip[username] = client_ip;
            changed_list[username] = false;
            free_clients[username] = true;
			if (!file.is_open()) {
                std::cerr << "Unable to open file for write" << std::endl;
                handle_read(socket, false, username_for_other_scope);
            }
            std::ifstream file1("clients_info.txt");
            file << recived_data << " online" << " free\n";
            file.close();
            for (auto& element: changed_list) {
                if (element.first == username) {
                    continue;
                }
            element.second = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            send_updated_list(client_ip_sockets, client_username_ip, changed_list, free_clients);
            handle_read(socket, false, username_for_other_scope);
        }
        if (request == "list") {
            username = recived_data.substr(end_of_request + 1);
            send_list(socket, username, client_username_ip); 
            handle_read(socket, false, username_for_other_scope);
        }
        auto it = client_username_ip.find(request);
        if (it !=  client_username_ip.end()) {
            change_status(socket, recived_data, end_of_request, request, client_username_ip, changed_list, free_clients, client_ip_sockets);
        }
        handle_read(socket, false, username_for_other_scope);
    } else {
        std::cerr << "Error during read: " << error.message() << std::endl;
        change_status(socket, *username_for_other_scope + " offline", username_for_other_scope -> size(), *username_for_other_scope, client_username_ip, changed_list, free_clients, client_ip_sockets);
        auto it = client_username_ip.find(*username_for_other_scope);
        std::string ip_address_for_delete_socket;
        if (it != client_username_ip.end()) {
            ip_address_for_delete_socket = it -> second;
            client_ip_sockets.erase(ip_address_for_delete_socket);
        }
        client_username_ip.erase(*username_for_other_scope);
        changed_list.erase(*username_for_other_scope);
        free_clients.erase(*username_for_other_scope);
        }
    });
}

void start_accept(boost::asio::io_context& io_context, tcp::acceptor& acceptor) {	
	auto socket = std::make_shared<tcp::socket>(io_context);
    auto username_for_other_scope = std::make_shared<std::string>();
    acceptor.async_accept(*socket, [socket, &acceptor, &io_context, username_for_other_scope](const boost::system::error_code& error) {
        if(!error) {
            std::array<char, 1024> buffer{};
            handle_read(socket, true, username_for_other_scope);
        }
        start_accept(io_context, acceptor);
    });
}
int server_port(bool is_same_port = false) {
    std::ifstream file("server.conf");
    if (file.good()) {
    std::string line;
        if (std::getline(file, line)) {
            int find_server_port = line.find("server port ");
            if (find_server_port != std::string::npos) {
                line = line.substr(12);
                find_server_port = line.find(" ");
                if (find_server_port != std::string::npos) {
                    line = line.substr(0, find_server_port);
                    int port = std::atoi(line.data());
                    if (port && port > 1023 && port < 65535) {
                        return port;
                    }
                }                        
            }             
        }
    }
    if (!is_same_port) {
        std::cerr << "\"server.conf\" file problem, used " << DEFAULT_PORT << " port\n";
        std::cerr << "Please write server port using --change_port\n";
        return DEFAULT_PORT;
    }
    return 0;
}

static void change_port(int new_port, int& port) {
    if (new_port && new_port > 1023 && new_port < 65535) {
        if (new_port == server_port(true)) {
            std::cerr << "it is the port written in the same \"server.conf\" file\n";
            return;
        }
        port = new_port;
        std::ofstream file("server.conf");
        file << "server port " << port << " ";
        std::cout << "Server port changed" << std::endl;
    } else {
        std::cerr << "Wrong port\n";
    } 
}

static void print_help(std::string help) {
    std::cout << "help_message" << std::endl;
}

static void cmd_parse(const int argc, const char* argv[], int& port) {
    for(int i = 1; i < argc; i++) {
        std::string param_name = argv[i];

        if(param_name == "-h" || param_name == "--help") {
            print_help(argv[0]);
            continue;
        }
        
        if (param_name == "--change_port") {
            if (i + 1 == argc ) {
                std::cerr << "Usage: --change_port <port>\n";
                return;
            }
            change_port(std::atoi(argv[i + 1]), port);
            ++i;
        }
    }
}

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
