#ifndef CLIENT_UTILS_HPP
#define CLIENT_UTILS_HPP

#include <string>

using boost::asio::ip::tcp;

std::string replace_emojis(const std::string& message);
void print_message(const std::string& sender, const std::string& message, bool is_current_user);
void parse_file_info(std::string& filename, std::streamsize& file_size, std::size_t& num_threads);
void send_file_part(boost::asio::ip::tcp::socket& socket, const std::string& file_path, std::streamsize offset, std::streamsize part_size);
void send_file_multithreaded(const std::string& file_path, boost::asio::ip::tcp::socket& socket, std::size_t num_threads);
void receive_file_part(boost::asio::ip::tcp::socket& socket, std::ofstream& file, std::streamsize offset, std::streamsize part_size);
void receive_file_multithreaded(boost::asio::ip::tcp::socket& socket, const std::string& filename, std::streamsize file_size, std::atomic<bool>& receive, std::size_t& num_threads_of_other_client);
void notify_server_status(std::shared_ptr<tcp::socket> server_socket, const std::string& status, const std::string& username);
void request_list(std::shared_ptr<tcp::socket> server_socket, const std::string& username);
void change_host_ip(std::string ip);
void change_port(int port);
bool cmd_parse(const int argc, const char* argv[], std::string& username);
std::pair<std::string, std::string> read_config(const std::string& filename);
bool is_file_empty(const std::string& filename);
bool file_exists(const std::string& filename);
std::string aes_encrypt(const std::string& plaintext);
std::string aes_decrypt(const std::string& ciphertext);

#endif // CLIENT_UTILS_HPP
