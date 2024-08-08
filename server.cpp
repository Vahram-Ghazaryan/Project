#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <fstream>
#include <memory>

using boost::asio::ip::tcp;

void handle_read(std::shared_ptr<tcp::socket> socket, std::string buffer, bool start_connection) {
    socket->async_read_some(boost::asio::buffer(buffer), [socket, &buffer, &start_connection] (const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            if (start_connection == true) { // Write information about clients
                std::ofstream file("clients_info.txt", std::ios::app);
                if (!file.is_open()) {
                    std::cerr << "Unable to open file for write" << std::endl;
                    return;
                }
                file << buffer << " online" << " free";
                start_connection = false;
            }
            if (buffer == "list") {
                std::ifstream file("clients_info.txt");
                if (!file.is_open()) {
                        std::cerr << "Unable to open file for read" << std::endl;
                        return;
                }
                std::string line;
                int end_of_ip_address = 0;
                buffer = "-----------THE LIST-------------\n";
                while(std::getline(file, line)) {
                    end_of_ip_address = line.find(" ");
                    line = line.substr(end_of_ip_address);
                    buffer += line + "\n";
                } 
                boost::asio::async_write(*socket, boost::asio::buffer(buffer), [socket](const boost::system::error_code& error, std::size_t lenght) {});
            } else {
                std::ifstream file("clients_info.txt");
                if (!file.is_open()) {
                    std::cerr << "Unable to open file for read" << std::endl;
                }
                int username = 0;
                std::string line;
                while(std::getline(file, line)) {
                    username = line.find(buffer);
                    if (username > 0) {
                        break;
                    } 
                }
                if (username == -1 ) {
                    std::cerr << "User not found" << std::endl;
                    return;
                } else {
                    int end_of_ip_address = 0;
                    buffer = line.substr(0,end_of_ip_address);
                }
                boost::asio::async_write(*socket, boost::asio::buffer(buffer), [socket](const boost::system::error_code& error, std::size_t lenght) {});
            }
            buffer.resize(1024);
            handle_read(socket, buffer, false);   
        } else {
            std::cerr << "Error during read" << error.message() << std::endl;
        }
    });
}   
void start_accept(boost::asio::io_context& io_context, tcp::acceptor& acceptor) {
    auto socket = std::make_shared<tcp::socket>(io_context);
    acceptor.async_accept(*socket, [socket, &acceptor, &io_context](const boost::system::error_code& error) {
        if(!error) {
            std::string buffer(1024, '\0');
            handle_read(socket, buffer, true);
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

    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), std::atoi(argv[1])));
    start_accept(io_context, acceptor);

    io_context.run();
    } catch(std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
