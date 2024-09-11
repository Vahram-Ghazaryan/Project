#include "client.hpp"
#include "client_utils.hpp"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>

namespace fs = std::filesystem;
using boost::asio::ip::tcp;

// Dictionary to map text commands to corresponding emojis
std::unordered_map<std::string, std::string> emoji_map = {
    {":smile", "😊"},
    {":sad", "😢"},
    {":laugh", "😂"},
    {":angry", "😡"},
    {":wink", "😉"},
    {":heart", "❤️"}
};

// Function to replace text commands with emojis
std::string replace_emojis(const std::string& message) {
    std::string result = message;
    for (const auto& pair : emoji_map) {
        std::string command = pair.first;
        std::string emoji = pair.second;

        size_t pos = 0;
        while ((pos = result.find(command, pos)) != std::string::npos) {
            result.replace(pos, command.length(), emoji);
            pos += emoji.length();
        }
    }
    return result;
}

std::string current_time() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S");
    return ss.str();
}

void print_message(const std::string& sender, const std::string& message, bool is_current_user) {
    std::string time_str = "[" + current_time() + "]";
    std::string formatted_message = time_str + " " + message;
    
    // ANSI escape codes for colors
    const std::string user_color = "\033[34m";  // Blue
    const std::string other_color = "\033[32m"; // Green
    const std::string reset_color = "\033[0m";  // Reset to default
    std::string is_emoji = replace_emojis(message);
    if (is_emoji != message) {
        std::cout << "\033[A" << replace_emojis(message);
        for (int i = 0; i < is_emoji.size(); ++i) {
            std::cout << " ";
        }
        std::cout << std::endl;
    }
    if (is_current_user) {
        std::cout << user_color << std::setw(55) << std::left << time_str << reset_color << std::endl;
    } else {
        std::cout << other_color << std::setw(55) << std::right << formatted_message << "\n-----------" << reset_color << std::endl;
    }
}

void notify_server_status(std::shared_ptr<tcp::socket> server_socket, const std::string& status, const std::string& username) {
    std::string message = username + " " + status;
    boost::asio::async_write(*server_socket, boost::asio::buffer(message), handle_write);
}

void request_list(std::shared_ptr<tcp::socket> server_socket, const std::string& username) {
    std::string request = "list " + username;
    boost::asio::async_write(*server_socket, boost::asio::buffer(request), [](const boost::system::error_code& error, std::size_t) {
        if (error) {
            std::cerr << "Error during list request: " << error.message() << "\n";
        }
    });

    auto buffer = std::make_shared<std::array<char, 1024>>();
    server_socket->async_read_some(boost::asio::buffer(*buffer), [server_socket, buffer](const boost::system::error_code& error, std::size_t length) {
        if (!error) {
            std::string response(buffer->data(), length);
            std::cout << "Online clients:\n" << response << std::endl;
        } else {
            std::cerr << "Error during list read: " << error.message() << "\n";
        }
    });
}

void parse_file_info(std::string& filename, std::streamsize& file_size, size_t& num_threads) {
	std::string input = filename;
    std::size_t colon_pos = input.find(':');
    if (colon_pos != std::string::npos) {
        std::string full_path = input.substr(0, colon_pos);
        filename = std::filesystem::path(full_path).filename().string();
        std::string num_threads_of_other_client = input.substr(colon_pos + 1);
        std::size_t find_num_threads = num_threads_of_other_client.find(':');
        if (find_num_threads != std::string::npos) {
            std::string size_str = num_threads_of_other_client.substr(0, find_num_threads);
            num_threads_of_other_client = num_threads_of_other_client.substr(find_num_threads + 1);
            num_threads = std::atoi(num_threads_of_other_client.data());
            try {
                file_size = std::stoll(size_str); 
            } catch (const std::invalid_argument&) {
                std::cerr << "Invalid size value: " << size_str << "\n";
            } catch (const std::out_of_range&) {
                std::cerr << "Size value out of range: " << size_str << "\n";
            }
        } else {
            std::cerr << "Invalid input format.\n";
        }
    } else {
        std::cerr << "Invalid input format.\n";
    }
}



std::string extract_filename(const std::string& file_path) {
    return file_path.substr(file_path.find_last_of("/\\") + 1);
}

void send_file_part(boost::asio::ip::tcp::socket& socket, const std::string& file_path, std::streamsize offset, std::streamsize part_size) {
    try {
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open file part.\n";
            return;
        }

        file.seekg(offset);
        const std::size_t buffer_size = 4096; 
        char buffer[buffer_size];

        std::streamsize bytes_to_send = part_size;
        while (bytes_to_send > 0) {
            std::streamsize chunk_size = std::min(bytes_to_send, static_cast<std::streamsize>(buffer_size));
            file.read(buffer, chunk_size);
            boost::asio::write(socket, boost::asio::buffer(buffer, file.gcount()));
            bytes_to_send -= file.gcount();
        }

        file.close();
    } catch (const std::exception& e) {
        std::cerr << "Exception in send_file_part: " << e.what() << '\n';
    }
}

void send_file_multithreaded(const std::string& file_path, boost::asio::ip::tcp::socket& socket, std::size_t num_threads) {
    try {
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file) {
            std::cerr << "Failed to open file.\n";
            return;
        }

        std::streamsize file_size = file.tellg();
        std::streamsize part_size = file_size / num_threads;
        std::vector<std::future<void>> futures;

        for (std::size_t i = 0; i < num_threads; ++i) {
            std::streamsize offset = i * part_size;
            std::streamsize current_part_size = (i == num_threads - 1) ? (file_size - offset) : part_size;

            futures.emplace_back(std::async(std::launch::async, send_file_part, std::ref(socket), file_path, offset, current_part_size));
        }

        for (auto& future : futures) {
            future.get();
        }

        std::cout << "File sent successfully using multiple threads." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception in send_file_multithreaded: " << e.what() << '\n';
    }
}

void receive_file_part(boost::asio::ip::tcp::socket& socket, std::ofstream& file, std::streamsize offset, std::streamsize part_size) {
    try {
        const std::size_t buffer_size = 4096; 
        char buffer[buffer_size];

        file.seekp(offset);
        std::streamsize bytes_to_receive = part_size;
        while (bytes_to_receive > 0) {
            std::streamsize chunk_size = std::min(bytes_to_receive, static_cast<std::streamsize>(buffer_size));
            std::streamsize bytes_received = socket.read_some(boost::asio::buffer(buffer, chunk_size));
            file.write(buffer, bytes_received);
            bytes_to_receive -= bytes_received;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in receive_file_part: " << e.what() << '\n';
    }
}

void receive_file_multithreaded(boost::asio::ip::tcp::socket& socket, const std::string& filename, std::streamsize file_size, std::atomic<bool>& receive, std::size_t& num_threads_of_other_client) {
    try {
    	std::filesystem::create_directories("received_files");
        std::ofstream file("received_files/" + filename, std::ios::binary);
        if (!file) {
        	std::string answer = aes_encrypt("FAILED");


        	boost::asio::write(socket, boost::asio::buffer(answer));
            std::cerr << "Failed to create file.\n";
            receive = false;
            return;
        }
        std::size_t num_threads = std::thread::hardware_concurrency();
        if (num_threads > num_threads_of_other_client) {
            num_threads = num_threads_of_other_client;
        }
        std::string answer = aes_encrypt("FILE_CREATED "+ std::to_string(num_threads));
		boost::asio::write(socket, boost::asio::buffer(answer));
        std::streamsize part_size = file_size / num_threads;

        std::vector<std::thread> threads;

        for (std::size_t i = 0; i < num_threads; ++i) {
            std::streamsize offset = i * part_size;
            std::streamsize current_part_size = (i == num_threads - 1) ? (file_size - offset) : part_size;

            threads.emplace_back(receive_file_part, std::ref(socket), std::ref(file), offset, current_part_size);
        }

        for (auto& thread : threads) {
            thread.join();
        }

        std::cout << "File received successfully using multiple threads.\n";
        receive = false;
        file.close();
    } catch (const std::exception& e) {
        std::cerr << "Exception in receive_file_multithreaded: " << e.what() << '\n';
        receive = false;
    }
}

bool is_octet(std::string octet) {
    if (octet == "0") {
        return true;
    }
    short check_octet = std::atoi(octet.data());
    if (check_octet && check_octet > 0 && check_octet <256) {
        return true;
    }
    return false;
}
void change_host_ip(std::string ip) {
    std::string octet;
    int find_octet = 0;
    std::string ip_for_change = ip;
    for (int i = 0; i < 3; ++i) {
        find_octet = ip.find(".");
        octet = ip.substr(0, find_octet);
        if (!is_octet(octet)) {
            std::cerr << "Wrong ip address\n";
            return;
        }
        ip = ip.substr(find_octet + 1);
    }
    if (!is_octet(ip)) {
        std::cerr << "Wrong ip address\n";
        return;
    }
    std::fstream file("chat.conf", std::ios::in);
    std::string line;
    std::vector<std::string> lines;
    while(std::getline(file, line)) {
        if (line.find("hostIp") != std::string::npos) {
            line = "hostIp " + ip_for_change;
            lines.push_back(line);
            continue;
        }
        lines.push_back(line);
    }
    file.close();
    file.open("chat.conf", std::ios::out | std::ios::trunc);
    for (int i = 0; i < lines.size(); ++i) {
        file << lines[i] << "\n";
    }
    std::cout << "Host ip changed" << std::endl;
    file.close();
}

bool file_exists(const std::string& filename) {
    std::ifstream file(filename);
    return !file.good();
}

bool is_file_empty(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file\n";
        return false;
    }
    file.seekg(0, std::ios::end); 
    std::streamsize size = file.tellg();
    file.close();
    return size == 0;
}

void change_port(int port) {
    if (port > 1023 && port < 65535) {
        std::fstream file("chat.conf", std::ios::in);
        std::string line;
        std::vector<std::string> lines;
        while(std::getline(file, line)) {
            if (line.find("port") != std::string::npos) {
                line = "port " + std::to_string(port);
                lines.push_back(line);
                continue;
            }
            lines.push_back(line);
        }
        file.close();
        file.open("chat.conf", std::ios::out | std::ios::trunc);
        for (int i = 0; i < lines.size(); ++i) {
            file << lines[i] << "\n";
        }
        std::cout << "Port changed" << std::endl;
        file.close();
    } else {
        std::cerr << "Wrong port\n";
    }
}

void print_help() {
    std::cout << "Usage: chat_client [options] [target]\n--help, -h\thelp message\n--change_port\tto change port\n--change_host_ip\tto change ip address of server\n--username, -u\t to enter username" << std::endl;
}

bool cmd_parse(const int argc, const char* argv[], std::string& username) {
    for(int i = 1; i < argc; i++) {
        std::string param_name = argv[i];

        if(param_name == "-h" || param_name == "--help") {
            print_help();
            continue;
        }
        
        if (param_name == "--change_port") {
            if (i + 1 == argc ) {
                std::cerr << "Usage: --change_port <port>\n";
                return false;
            }
            change_port(std::atoi(argv[i + 1]));
            ++i;
            continue;
        }
        if (param_name == "--change_host_ip") {
            if (i + 1 == argc ) {
                std::cerr << "Usage: --change_host_ip <host_ip>\n";
                return false;
            }
            change_host_ip(argv[i + 1]);
            ++i;
            continue;
        }
        if (param_name == "--username" || param_name == "-u") {
            if (i + 1 == argc ) {
                std::cerr << "Usage: --username/-u <username>\n";
                return false;
            }
            username = argv[i + 1];
            return true;
        }
        if (i + 1 == argc) {
            return false;
        }
        std::cerr << "Wrong argument\n";
        return false;
    }
    return false;
}

std::pair<std::string, std::string> read_config(const std::string& filename) {
    std::ifstream config_file(filename);
    std::string line, host, port;

    while (std::getline(config_file, line)) {
        std::istringstream iss(line);
        std::string key, value;
        if (iss >> key >> value) {
            if (key == "hostIp") {
                host = value;
            } else if (key == "port") {
                port = value;
            }
        }
    }

    return {host, port};
}
const std::string aes_key = "0123456789abcdef0123456789abcdef"; 
const std::string aes_iv = "abcdef9876543210";

// Function to encrypt a message using AES encryption with OpenSSL
std::string aes_encrypt(const std::string& plaintext) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    std::vector<unsigned char> ciphertext(plaintext.size() + AES_BLOCK_SIZE);
    int len = 0;
	
	// Initialize the decryption operation using AES-256-CBC mode
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (unsigned char*)aes_key.data(), (unsigned char*)aes_iv.data());
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, (unsigned char*)plaintext.data(), plaintext.size());
    int ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return std::string(ciphertext.begin(), ciphertext.begin() + ciphertext_len);
}

std::string aes_decrypt(const std::string& ciphertext) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    std::vector<unsigned char> plaintext(ciphertext.size());
    int len = 0;

    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (unsigned char*)aes_key.data(), (unsigned char*)aes_iv.data());
    EVP_DecryptUpdate(ctx, plaintext.data(), &len, (unsigned char*)ciphertext.data(), ciphertext.size());
    int plaintext_len = len;
    EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return std::string(plaintext.begin(), plaintext.begin() + plaintext_len);
}
