#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <fstream>
#include <memory>
#include <array>
#include <mutex>

using boost::asio::ip::tcp;
std::mutex mutex;
void send_list(const std::shared_ptr<tcp::socket>& socket) {
    std::ifstream file("clients_info.txt");
    if(!file.is_open()) {
        std::cerr << "Unable to open file for read" << std::endl;
        return;
    }
    std::string line;
    std::string response = "-----------THE LIST-------------\n";
    int end_of_ip_address = 0;

    while (std::getline(file, line)) {
        end_of_ip_address = line.find(" ");
        if (end_of_ip_address != std::string::npos) {
            line = line.substr(end_of_ip_address + 1);
            response += line + "\n";
        }        
    }
    file.close();
    boost::asio::async_write(*socket, boost::asio::buffer(response), [socket](const boost::system::error_code& error, std::size_t) {
        if (error) {
            std::cerr << "Error during write: " << error.message() << std::endl;
        }
    });
}

void handle_read(std::shared_ptr<tcp::socket> socket, bool start_connection) {
    auto buffer = std::make_shared<std::array<char, 1024>>();
    std::cout << "Called handle_read function" << std::endl;
    socket->async_read_some(boost::asio::buffer(*buffer), [socket, buffer, start_connection] (const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string recived_data(buffer -> data(),length);
			std::cout << "Received data: " << recived_data << std::endl;
			if (start_connection) { // Write information about clients
                std::lock_guard<std::mutex> lock(mutex);
                std::ofstream file("clients_info.txt", std::ios::app);
				std::cout << "File opened for write" << std::endl;
				if (!file.is_open()) {
                    std::cerr << "Unable to open file for write" << std::endl;
                   return;
                }
                std::ifstream file1("clients_info.txt");
                send_list(socket);
                file << recived_data << " online" << " free\n";
                file.close();
                handle_read(socket, false);
            }
            if (recived_data == "list") {
                send_list(socket);
            } else {
                std::ifstream file("clients_info.txt");
                if (!file.is_open()) {
                    std::cerr << "Unable to open file for read 2 " << std::endl;
                }
                int username = 0;
                std::string line;
                while(std::getline(file, line)) {
                    username = line.find(recived_data);
                    if (username > 0) {
                        break;
                    } 
                }
                file.close();
                if (username == -1 ) {
                    std::cerr << "User not found" << std::endl;
                    return;
                } else {
                    int end_of_ip_address = 0;
                    std::string response = line.substr(0,end_of_ip_address);
                    boost::asio::async_write(*socket, boost::asio::buffer(response), [socket](const boost::system::error_code& error, std::size_t) {
                     if (error) {
                        std::cerr << "Error during write: " << error.message() << std::endl;
                    }
                });
                }
            }
            handle_read(socket, false);   
        } else {
            std::cerr << "Error during read: " << error.message() << std::endl;
        }
    });
}   

void start_accept(boost::asio::io_context& io_context, tcp::acceptor& acceptor) {	std::cout << "Called start_accept function" << std::endl;
	auto socket = std::make_shared<tcp::socket>(io_context);
    acceptor.async_accept(*socket, [socket, &acceptor, &io_context](const boost::system::error_code& error) {
        if(!error) {
            std::array<char, 1024> buffer{};
            handle_read(socket, true);
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
