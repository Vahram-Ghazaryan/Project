#ifndef SERVER_HPP
#define SERVER_HPP

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

void send_list(const std::shared_ptr<tcp::socket> socket,const std::string username, std::unordered_map<std::string, std::string>& client_username_ip, bool updated = false);
void send_updated_list(const std::unordered_map<std::string, std::shared_ptr<tcp::socket>>& client_ip_sockets, std::unordered_map<std::string,std::string>& client_username_ip, std::unordered_map<std::string, bool>& changed_list, std::unordered_map<std::string, bool>& free_clinets);
void change_status(const std::shared_ptr<tcp::socket> socket, const std::string received_data, const int& end_of_request, std::string username, std::unordered_map<std::string, std::string>& client_username_ip, std::unordered_map<std::string, bool>& changed_list, std::unordered_map<std::string, bool>& free_clients, std::unordered_map<std::string, std::shared_ptr<tcp::socket>>& client_ip_sockets);
void handle_read(std::shared_ptr<tcp::socket> socket, bool start_connection, std::shared_ptr<std::string> username_for_other_scope);
void start_accept(boost::asio::io_context& io_context, tcp::acceptor& acceptor);

#endif