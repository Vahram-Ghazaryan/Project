#include <iostream>
#include "client.hpp"
#include "client_utils.hpp"

int main(int argc, char* argv[]) {	
    try {
        std::string username;
        if (argc >= 2) { 
            if(!cmd_parse(argc, const_cast<const char**> (argv), username)) {
                return 0;
            };
        } else {
            std::cerr << "Usage: client --username <username> or -u <username>\n";
            return 1;
        }
        if (file_exists("chat.conf") || is_file_empty("chat.conf")) {
            std::ofstream file("chat.conf");
            file << "hostIp \n" << "port\n";
            file.close();
            std::cerr << "Wirte hostIP and(or) port in chat.conf file or use --change_host_ip and --change_port\n";
        }
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
                    auto getlineThread_ptr = std::make_shared<std::atomic<bool>>(true);
                    
                    std::thread accept_thread(accept_connections, acceptor, std::ref(io_context), server_socket, username, connected_ptr, getlineThread_ptr);
                    accept_thread.detach();
                                       
                    std::thread handle_thread([server_socket, &io_context, username, connected_ptr, getlineThread_ptr]() {
                        handle_read(server_socket, server_socket, io_context, username, connected_ptr, getlineThread_ptr);
                    });
                   // handle_thread.detach();

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
