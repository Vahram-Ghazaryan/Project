#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <fstream>
#include <memory>
#include <array>
#include <mutex>
#include <vector>
#include <future>
#include <algorithm>
#include <unordered_map>

using boost::asio::ip::tcp;
std::mutex mutex;

std::string username_for_end_of_connection;

void send_list(const std::shared_ptr<tcp::socket>& socket,const std::string& username, std::vector<std::string> users) {

    std::cout << "Called send_list function" << std::endl;
    std::string response = "-----------THE LIST-------------\n";
    if (!users.empty()) {
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
                if (line == username) {
                    continue;
                }   
                line = line.substr(end_of_ip_address + 1);
                response += line + "\n";
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

void change_status(const std::shared_ptr<tcp::socket>& socket, const std::string& recived_data, const int& end_of_request, std::string& username, std::vector<std::string> users) {
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
    find_end_of_ip_address = changed_line.find(username);
    changed_line = changed_line.substr(0, end_of_request + find_end_of_ip_address);
    std::string change = recived_data.substr(end_of_request + 1);
    if (change == "no free") {
        changed_line += " online no free\n";
    } else if (change == "free" ) {
        changed_line += " online free\n"; 
        send_list(socket, username, users);
    } else if(change == "wait") {
        changed_line += " waiting\n";
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
}

bool connection_request(const std::string& client_ip, const std::unordered_map<std::string, std::shared_ptr<tcp::socket>>& client_sockets, std::shared_ptr<std::string> username) {
    std::cout << "Called connection_request function" << std::endl;
    std::shared_ptr<tcp::socket> socket;
    std::lock_guard<std::mutex> lock(mutex);
    auto it = client_sockets.find(client_ip);
    if (it != client_sockets.end()) {
        socket = it -> second;
        std::cout << "Client ip is founded in map" << std::endl; 
        std::cout << "Socket is " << socket << std::endl;
    } else {
        std::cerr << "Client ip not found in map:" << client_ip<< std::endl;
        return false;
    }
    auto buffer = std::make_shared<std::array<char, 1024>>();
    std::promise<bool> promise;
    std::future<bool> future = promise.get_future();
    if (socket) {
        std::string request = "request " + *username;
        std::cout << "The request is " << request << std::endl;
        std::cout << "The client ip is " << it -> first << std::endl; 
        boost::asio::async_write(*socket, boost::asio::buffer(request), [socket, buffer, &promise, request] (const boost::system::error_code& write_error, std::size_t) {
            std::cout << "Received data" << request << std::endl;
            if (!write_error) {
                socket -> async_read_some(boost::asio::buffer(*buffer), [socket, buffer, &promise] (const boost::system::error_code& error, std::size_t length){
                    if (!error) {
                        std::string answer(buffer -> data(), length);
                        std::cout << "The answer is " << answer << std::endl;
                        if (answer == "accept") {
                            promise.set_value(true);
                        } else if (answer == "reject") {
                            promise.set_value(false);
                        } else {
                            promise.set_value(false);
                        }
                    } else {
                        std::cerr << "Error during read " << error.message() << std::endl;
                        promise.set_value(false);
                    }
                    });
            } else {
                std::cerr << "Error during write " << write_error.message() << std::endl;
                promise.set_value(false);;
            }
        });
    }
    bool accept = future.get();
    return accept;
}

void send_ip_address(const std::shared_ptr<tcp::socket>& socket, std::string recived_data, int end_of_request, const std::unordered_map<std::string, std::shared_ptr<tcp::socket>>& client_sockets, std::shared_ptr<std::string> sender_username) {
    std::cout << "Called send_ip_address function" << std::endl;
    int find_username = 0;
    std::string username = recived_data.substr(end_of_request);
    std::ifstream file("clients_info.txt");
    if (!file.is_open()) {
        std::cerr << "Unable to open file for read 2 " << std::endl;
        return;
    }
    std::cout << "File opened" << std::endl;
    std::string line;
    while(std::getline(file, line)) {
        find_username = line.find(username);
        if (find_username != std::string::npos) {
            break;
        } 
    }
    file.close();
    int end_of_ip_address = line.find(" ");
    std::string response;
    if (find_username != std::string::npos) {
        response = line.substr(0,end_of_ip_address);
    } else {
        response = "User not found\n";
    }
    std::cout << "The ip address is " << response << std::endl;
    if (!connection_request(response, client_sockets, sender_username)) {
        response = "The other client rejected the connection\n";
    }
    boost::asio::async_write(*socket, boost::asio::buffer(response), [socket](const boost::system::error_code& error, std::size_t) {
        if (error) {
            std::cerr << "Error during write: " << error.message() << std::endl;
        }
    });
}

void handle_read(std::shared_ptr<tcp::socket> socket, bool start_connection, std::shared_ptr<std::string> username_for_end_of_connection) {
    auto buffer = std::make_shared<std::array<char, 1024>>();
    std::cout << "Called handle_read function" << std::endl;
    std::string client_ip = socket -> remote_endpoint().address().to_string();                                                                                                                                                     
    static std::unordered_map<std::string, std::shared_ptr<tcp::socket>> client_sockets;
    socket->async_read_some(boost::asio::buffer(*buffer), [socket, buffer, start_connection, username_for_end_of_connection, client_ip] (const boost::system::error_code& error, std::size_t length) {
        static std::vector<std::string> users;
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
                for (int i = 0; i < users.size(); ++i) {
                    if (users[i] == username) {
                        boost::asio::async_write(*socket, boost::asio::buffer("That username is not available. Please restart program\n"), [socket](const boost::system::error_code& error, std::size_t) {
                            if (error) {
                            std::cerr << "Error during write: " << error.message() << std::endl;
                            }
                        });                       
                    socket->close();
                    return;
                    }
                }

                std::cout << "username added to vector: " << username << std::endl;
                *username_for_end_of_connection = username;
                send_list(socket, username, users);
                users.push_back(username);
                std::cout << "The vector size is " << users.size() << std::endl;
                std::ofstream file("clients_info.txt", std::ios::app);
				std::cout << "File opened for write" << std::endl;
                std::lock_guard<std::mutex> lock(mutex);
                client_sockets[client_ip] = socket;
                std::cout << username << "socket is " << client_sockets[client_ip] << std::endl;
				if (!file.is_open()) {
                    std::cerr << "Unable to open file for write" << std::endl;
                    handle_read(socket, false, username_for_end_of_connection);
                }
                std::ifstream file1("clients_info.txt");
                file << recived_data << " online" << " free\n";
                file.close();
                handle_read(socket, false, username_for_end_of_connection);
            }
            if (request == "list") {
                username = recived_data.substr(end_of_request + 1);
                send_list(socket, username, users); 
                handle_read(socket, false, username_for_end_of_connection);
            } 
            if (request == "connect") {
                send_ip_address(socket, recived_data, end_of_request, client_sockets, username_for_end_of_connection);
                handle_read(socket, false, username_for_end_of_connection);
            }
            for (int i = 0; i < users.size(); ++i) {
                if (users[i] == request) {
                    change_status(socket, recived_data, end_of_request, request, users);
                    handle_read(socket,false, username_for_end_of_connection);
                    break;
                }
            }
            handle_read(socket, false, username_for_end_of_connection);
        } else {
            std::cerr << "Error during read: " << error.message() << std::endl;
            change_status(socket, *username_for_end_of_connection + " offline", username_for_end_of_connection -> size(), *username_for_end_of_connection, users);
            auto new_end_of_vector = std::remove(users.begin(), users.end(), *username_for_end_of_connection);
            users.pop_back();
            std::cout << "The deleted username from vector is " << *username_for_end_of_connection << std::endl;
            std::cout << "vector size: " << users.size() << std::endl;
        }
    });
}

void start_accept(boost::asio::io_context& io_context, tcp::acceptor& acceptor) {	
    std::cout << "Called start_accept function" << std::endl;
	auto socket = std::make_shared<tcp::socket>(io_context);
    auto username_for_end_of_connection = std::make_shared<std::string>();
    acceptor.async_accept(*socket, [socket, &acceptor, &io_context, username_for_end_of_connection](const boost::system::error_code& error) {
        if(!error) {
            std::array<char, 1024> buffer{};
            handle_read(socket, true, username_for_end_of_connection);
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
