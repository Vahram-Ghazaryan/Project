#ifndef SERVER_UTILS_HPP
#define SERVER_UTILS_HPP

#include <string>

int server_port(bool is_same_port = false);
void change_port(int new_port, int& port);
void print_help(std::string help);
void cmd_parse(const int argc, const char* argv[], int& port);

#endif