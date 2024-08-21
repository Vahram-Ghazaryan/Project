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

using boost::asio::ip::tcp;

std::mutex mutex;
std::string username_for_other_scope;

void send_list(const std::shared_ptr<tcp::socket> socket,const std::string username, std::unordered_map<std::string, std::string>& client_username_ip, bool updated = false) {
    std::cout << "Called send_list function" << std::endl;
    std::string response;
    if (updated) {
        response = "-----------LIST UPDATED----------\n";
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

void send_updated_list(const std::unordered_map<std::string, std::shared_ptr<tcp::socket>>& client_ip_sockets, std::unordered_map<std::string,std::string>& client_username_ip, std::unordered_map<std::string, bool>& changed_list, std::unordered_map<std::string, bool>& waiting_clinets) {
    for (auto& element: client_username_ip) {
        auto changed = changed_list.find(element.first);
        if (changed != changed_list.end()) {
            if (changed -> second) {
                auto waiting = waiting_clinets.find(element.first);
                if (waiting != waiting_clinets.end()) {
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

void change_status(const std::shared_ptr<tcp::socket> socket, const std::string received_data, const int& end_of_request, std::string username, std::unordered_map<std::string, std::string>& client_username_ip, std::unordered_map<std::string, bool>& changed_list, std::unordered_map<std::string, bool>& waiting_clients, std::unordered_map<std::string, std::shared_ptr<tcp::socket>>& client_ip_sockets) {
    std::cout << "Called change_status" << std::endl;
    std::ifstream file("clients_info.txt");
    if (!file.is_open()) {
        std::cerr << "Unable to open file for change" << std::endl;
        return;
    }
    std::lock_guard<std::mutex> lock(mutex);
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
            std::cout << "Finded username is " << username << std::endl;
            target_line = line_number;
            changed_line = line;
            std::cout << "Line to changed " << line <<  std::endl;
        }
    }
    file.close();
    username = username.substr(0, username.size() - 1);
    find_end_of_ip_address = changed_line.find(username);
    changed_line = changed_line.substr(0, end_of_request + find_end_of_ip_address);
    std::string change = received_data.substr(end_of_request + 1);
    if (change == "no free") {
        changed_line += " online no free\n";
    } else if (change == "free" ) {
        changed_line += " online free\n"; 
    } else if(change == "wait") {
        changed_line += " waiting\n";
        waiting_clients[username] = true;
    } else if (change == "offline") {
        changed_line = "";
    }
    std::ofstream file_for_change("clients_info.txt");
    for(int i = 0; i < lines.size(); ++i) {
        if (i == target_line - 1) {
            std::cout << "The line is changed" << std::endl;
            file_for_change << changed_line;
            std::cout << "Changed line: " << changed_line << std::endl;
            std::cout << "target line: " << target_line << std::endl;
            continue;
        }
        file_for_change << lines[i] << std::endl;
    }
    file_for_change.close();
    if (change == "free") {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        send_list(socket, username, client_username_ip);
    }
    for (auto& element: changed_list) {
        if (element.first == username.substr(0, username.size() - 1)) {
            continue;
        }
        element.second = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    send_updated_list(client_ip_sockets, client_username_ip, changed_list, waiting_clients);
}

void connection_request(const std::string& client_ip, const std::unordered_map<std::string, std::shared_ptr<tcp::socket>>& client_sockets, std::shared_ptr<std::string> username) {
    std::cout << "Called connection_request function" << std::endl;
    if (client_ip == "") {
        std::cerr << "Ip address not found" << std::endl;
        return;
    }
    std::shared_ptr<tcp::socket> socket;
    std::lock_guard<std::mutex> lock(mutex);
    auto it = client_sockets.find(client_ip);
    if (it != client_sockets.end()) {
        socket = it -> second;
        std::cout << "Client ip is founded in map" << std::endl; 
        std::cout << "Socket is " << socket << std::endl;
    } else {
        std::cerr << "Client ip not found in map:" << client_ip<< std::endl;
        return;
    }
    if (socket) {
        std::string request = "request " + *username;
        std::cout << "The request is " << request << std::endl;
        std::cout << "The client ip is " << it -> first << std::endl; 
        boost::asio::async_write(*socket, boost::asio::buffer(request), [socket, request] (const boost::system::error_code& write_error, std::size_t) {
        std::cout << "Received data" << request << std::endl;
            if (write_error) {
                std::cerr << "Error during write " << write_error.message() << std::endl;
            }
        });
    }
}

std::string find_ip_address(std::string received_data, int end_of_request, const std::unordered_map<std::string, std::string>& client_username_ip) {
    std::cout << "Called find_ip_address function" << std::endl;
    int find_username = 0;
    std::cout << "The recived data is " << received_data << std::endl;
    std::string username = received_data.substr(end_of_request + 1);
    std::cout << "The username is " << username << std::endl;
    auto it = client_username_ip.find(username);
    std::string ip_address;
    if (it != client_username_ip.end()) {
        ip_address = it -> second;
        return ip_address;
    } else {
        return "";
    }
}

void send_answer_of_connection(std::string& received_data, const std::unordered_map<std::string, std::string>& client_username_ip, const std::unordered_map<std::string, std::shared_ptr<tcp::socket>>& client_ip_sockets) {
    std::cout << "Called send_answer_of_connection" << std::endl;
    std::lock_guard<std::mutex> lock(mutex);
    std::string request = received_data.substr(0, 6);
    received_data = received_data.substr(12);
    int find_end_of_username = received_data.find(" ");
    std::string answer;
    std::shared_ptr<tcp::socket> requester_socket;
    std::string requester_ip;
    std::string requester = received_data.substr(find_end_of_username + 5);
    std::cout << "The requester is " << requester << std::endl;       
    auto find_requester_ip = client_username_ip.find(requester);
    if (find_requester_ip != client_username_ip.end()) {
        requester_ip = find_requester_ip -> second;
    } else {
        std::cerr << "Requester ip not found" << std::endl;
        return;
    }
    auto find_requester_socket = client_ip_sockets.find(requester_ip);
    if (find_requester_socket != client_ip_sockets.end()) {
        requester_socket = find_requester_socket -> second;
    }
    if (request == "accept") {
        std::string receiver_username = received_data.substr(0, find_end_of_username);
        std::string receiver_ip;
        std::cout << "The receiver username is " << receiver_username << std::endl;
        auto find_receiver_ip = client_username_ip.find(receiver_username);
        if (find_receiver_ip != client_username_ip.end()) {
            receiver_ip = find_receiver_ip -> second;
            answer = "IP: " + receiver_ip;
        } else {
            std::cerr << "Receiver ip not found" << std::endl;
            answer = "ip not found";
        }     
    } else {
        answer = "reject";
    }
    boost::asio::async_write(*requester_socket, boost::asio::buffer(answer), [requester_socket] (const boost::system::error_code& error, std::size_t) {
        if (error) {
            std::cerr << "Error during write " << error.message() << std::endl;
        }
    });
}

void handle_read(std::shared_ptr<tcp::socket> socket, bool start_connection, std::shared_ptr<std::string> username_for_other_scope) {
    static bool open_file_first_time = true;
    auto buffer = std::make_shared<std::array<char, 1024>>();
    std::cout << "Called handle_read function" << std::endl;
    std::string client_ip = socket -> remote_endpoint().address().to_string();                                                                                                                                                     
    static std::unordered_map<std::string, std::string> client_username_ip;
    static std::unordered_map<std::string, bool> changed_list;
    static std::unordered_map<std::string, std::shared_ptr<tcp::socket>> client_ip_sockets;
    static std::unordered_map<std::string, bool> waiting_clients;
    socket->async_read_some(boost::asio::buffer(*buffer), [socket, buffer, start_connection, username_for_other_scope, client_ip] (const boost::system::error_code& error, std::size_t length) {
    if (!error) {
        std::string recived_data(buffer -> data(),length);
		std::cout << "Received data: " << recived_data << std::endl;
        int end_of_request = recived_data.find(" ");
        std::string request = recived_data.substr(0, end_of_request);
        std::cout << request << std::endl;
        std::string username;
        if (end_of_request != std::string::npos) {
            std::string request = recived_data.substr(0, end_of_request);
        }
		if (start_connection) { // Write information about clients
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
            if (open_file_first_time) {
                std::ofstream file("clients_info.txt");
                if((!file.is_open())) {
                    std::cerr << "Unable to open file for write" << std::endl;
                }
                file << "";
                file.close();
                open_file_first_time = false;
            }
            std::ofstream file("clients_info.txt", std::ios::app);
			std::cout << "File opened for write" << std::endl;
            client_ip_sockets[client_ip] = socket;
            client_username_ip[username] = client_ip;
            changed_list[username] = false;
            waiting_clients[username] = false;
            std::cout << username << " socket is " << client_ip_sockets[client_ip] << std::endl;
			if (!file.is_open()) {
                std::cerr << "Unable to open file for write" << std::endl;
                handle_read(socket, false, username_for_other_scope);
            }
            std::ifstream file1("clients_info.txt");
            file << recived_data << " online" << " free\n";
            file.close();
            handle_read(socket, false, username_for_other_scope);
        }
        if (request == "list") {
            username = recived_data.substr(end_of_request + 1);
            send_list(socket, username, client_username_ip); 
            handle_read(socket, false, username_for_other_scope);
        } 
        if (request == "connect") {
            connection_request(find_ip_address(recived_data, end_of_request, client_username_ip), client_ip_sockets, username_for_other_scope);
            handle_read(socket, false, username_for_other_scope);
        }
        if (request == "accept") {
            send_answer_of_connection(recived_data, client_username_ip, client_ip_sockets);
            handle_read(socket, false, username_for_other_scope);
        }
        if (request == "reject") {
            send_answer_of_connection(recived_data, client_username_ip, client_ip_sockets);
            handle_read(socket, false, username_for_other_scope);
        }
        auto it = client_username_ip.find(request);
        if (it !=  client_username_ip.end()) {
            change_status(socket, recived_data, end_of_request, request, client_username_ip, changed_list, waiting_clients, client_ip_sockets);
        }
        handle_read(socket, false, username_for_other_scope);
    } else {
        std::cerr << "Error during read: " << error.message() << std::endl;
        change_status(socket, *username_for_other_scope + " offline", username_for_other_scope -> size(), *username_for_other_scope, client_username_ip, changed_list, waiting_clients, client_ip_sockets);
        auto it = client_username_ip.find(*username_for_other_scope);
        std::string ip_address_for_delete_socket;
        if (it != client_username_ip.end()) {
            ip_address_for_delete_socket = it -> second;
            client_ip_sockets.erase(ip_address_for_delete_socket);
        }
        client_username_ip.erase(*username_for_other_scope);
        changed_list.erase(*username_for_other_scope);
        waiting_clients.erase(*username_for_other_scope);
        }
    });
}

void start_accept(boost::asio::io_context& io_context, tcp::acceptor& acceptor) {	
    std::cout << "Called start_accept function" << std::endl;
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

int main(int argc, char* argv[]) {
    try {
        if(argc != 2) {
            std::cerr << "Usage: server <port>" << std::endl;
            return 1;
        }
	std::cout << "Server started" << std::endl;
    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), std::atoi(argv[1])));
    start_accept(io_context, acceptor);

    io_context.run();
    } catch(std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}

