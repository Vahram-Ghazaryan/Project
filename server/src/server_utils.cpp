#include "server.hpp"
#include "server_utils.hpp"

int server_port(bool is_same_port) {
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

void change_port(int new_port, int& port) {
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

void print_help(std::string help) {
    std::cout << "Usage: server [options] [target]\n--help, -h\thelp message\n--change_port\tto change port" << std::endl;
}

void cmd_parse(const int argc, const char* argv[], int& port) {
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