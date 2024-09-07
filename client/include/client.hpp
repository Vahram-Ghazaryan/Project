#ifndef CLIENT_HPP
#define CLIENT_HPP

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

using boost::asio::ip::tcp;

void connect_to_client(const std::string& client_ip, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr, std::shared_ptr<std::atomic<bool>> getlineThread_ptr);
void handle_write(const boost::system::error_code& error, std::size_t);
void handle_read(std::shared_ptr<tcp::socket> socket, std::shared_ptr<tcp::socket> server_socket, boost::asio::io_context& io_context, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr, std::shared_ptr<std::atomic<bool>> getlineThread_ptr);
void start_chat(std::shared_ptr<tcp::socket> client_socket, std::shared_ptr<tcp::socket> server_socket, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr, std::shared_ptr<std::atomic<bool>> getlineThread_ptr, boost::asio::io_context& io_context);
void accept_connections(std::shared_ptr<tcp::acceptor> acceptor, boost::asio::io_context& io_context, std::shared_ptr<tcp::socket> server_socket, const std::string& username, std::shared_ptr<std::atomic<bool>> connected_ptr, std::shared_ptr<std::atomic<bool>> getlineThread_ptr);

#endif // CLIENT_HPP

