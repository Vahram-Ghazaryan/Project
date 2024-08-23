Project name  		Terminal chat 

Team				Vahram Ghazaryan Aram Arakelyan

Start				29.07.2024

Deadline				1st week of September

Description

●	Create a Linux terminal chat
●	It should also be able to transfer files
●	It should consist of server and client programs written in c++
●	The server should be a connection to connect for the first time
●	Then the connection should be direct to the clients between serverless.
Detailed description
The project will consist of 2 programs: client and server
Server:
1.	The server must establish a connection between the 2 clients by sending the necessary data (for example, the IP address of the other client).
2.	The server should contain a list that will store the usernames of all clients, as well as which clients are online/offline, and which of the online clients are currently busy or free.
3.	And every time the list is updated, the server should send an update to all clients.
4.	The server must contain client passwords and perform client authentication. Passwords must be transmitted in encrypted form.
Client:
1.	The client must contain the server's config file, where the server's data will be specified (for example, IP address, hostname). That file will be located in /etc.
2.	The client must have options with which to change various parameters (for example, the password can be changed with the --password option)․
3.	The client must connect to the server and send the necessary data (eg IP address, username)․
4.	The client must constantly wait and accept the updates sent from the server․
5.	The client must connect to another client through the data received from the server.
6.	Then the socket with the server will be closed.
7.	The other client must either accept or reject the connection.
8.	Then the chat will open.
9.	Files must be directed in a specific format (eg /file: path).
10.	There should also be /disconnect (closes the chat, but the client remains online) and /exit (closes the chat and exits the program) commands․
11.	During /disconnect and /exit, clients connect to the server and the server updates the list and sends update to the rest of the clients.
