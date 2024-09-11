Project name  		Terminal chat 

Team				Vahram Ghazaryan Aram Arakelyan

Start				29.07.2024

Deadline		09.11.2024

Detailed description.

The project consists of 2 projects - client and server (which are located in separate directories that contain a CMakeLists.txt file)
Server:
1. The server establishes a connection between 2 clients by sending the necessary data (for example, the IP address of the other client).
2. The server contains a list that stores the usernames of all customers, as well as which of the online customers are currently busy or free.
3. And every time the list is updated, the server sends an update to all clients.
Client:
1. The client contains the server configuration file that specifies the server information (eg IP address, hostname).
2. The client has options with which it is possible to change various parameters (for example, the IP address of the server).
3. The client connects to the server and sends the necessary data (eg IP address, username).
4. The client continuously waits for and accepts updates sent from the server.
5. The client connects using data received from another client's server.

To run the programs, you must:
   1. Download the repository,
   2. Download the packages needed for boost/asio.hpp and openssl:
      sudo apt install libboost-all-dev
      sudo apt install libssl-dev
   3. Move to the build/ directory and execute the following commands:     
      rm -rf *   
      cmake..   
      make   
   4. Run the program.

   The links to the slides about the project are in the links.txt file.

