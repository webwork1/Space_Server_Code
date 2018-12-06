#ifndef SOCKET_H
#define SOCKET_H

#include <iostream>
#include <string>
#include <sstream>
#include "SDL_net.h"
#include <vector>
#include "SocketException.h" // Include our custom exception header which defines an inline class

using std::string;
using std::cout;
using std::cerr;
using std::endl;

class ServerSocket
{
private:
	bool debug = false;                 // Flag to control whether the ServerSocket should display debug info

	unsigned int port;          // The port our server will listen for incoming connecions on
	unsigned int bufferSize;    // Size of our message buffer
	unsigned int maxSockets;    // Max number of sockets
	unsigned int maxClients;    // Max number of clients in our socket set (defined as maxSockets - 1 as the server socket itself take 1 port)

	IPaddress serverIP;         // The IP of the socket server (this will end up being 0.0.0.0 - which means roughly "any IP address")
	TCPsocket serverSocket;     // The server socket that clients will connect to
	string    dotQuadString;    // The IP address of the server as a dot-quad string i.e. "127.0.0.1"

	// These must be pointers because we don't know the array size until the SocketServer object is created!
	// Note: These pointers are made into arrays in the ServerSocket constructor!
	TCPsocket *pClientSocket;   // A pointer to (what will be) an array of sockets for the clients
	bool *pSocketIsFree;        // A pointer to (what will be) an array of flags indicating which client sockets are free
	char *pBuffer;              // A pointer to (what will be) an array of characters used to store the messages we receive

	SDLNet_SocketSet socketSet; // Our entire set of sockets

	unsigned int clientCount;   // Count of how many clients are currently connected to the server

	bool shutdownServer;        // Flag to control when to shut down the server

	string playerList[100]; //list of players


	//vectors for shooting
	std::vector<double> shotX;
	std::vector<double> shotY;
	std::vector<string> shotName;
	std::vector<int> shotRotation;
	std::vector<string> shotType;
	std::vector<double> shotTime;
	std::vector<string> uniqueName;
	std::vector<int> shotStartVelocityX;
	std::vector<int> shotStartVelocityY;
public:

	void updateShooting();

	string holdPlayerLeave1 = "";
	string holdPlayerLeave2 = "";

	//get client count
	int getClientCount() { return clientCount; }

	//get playerLeaving 1
	string getLeavePlayer1() { return holdPlayerLeave1; }
	//get playerLeaving 1
	string getLeavePlayer2() { return holdPlayerLeave2; }

	//reseting player leaveing to ""
	void setplayerLeaving1();
	void setplayerLeaving2();

	static const string SERVER_NOT_FULL;
	static const string SERVER_FULL;
	static const string SHUTDOWN_SIGNAL;

	ServerSocket(unsigned int port, unsigned int bufferSize, unsigned int maxSockets);

	~ServerSocket();

	// Function to poll for clients connecting
	void checkForConnections();

	//used to make sure shots are recieved
	void updateShooting2();

	// Function to poll for client activity (i.e. message sent or dropped connection)
	// Returns either the number of an active client, or -1 if no clients with activity to process
	int checkForActivity();

	// Function to actually do something when client activity is detected!
	void dealWithActivity(unsigned int clientNumber);

	// Function to return the shutdown status, used to control when to terminate
	bool getShutdownStatus();

	//sending data to every client
	void sendToClients(string s);

	//player left
	void playerLeaving(string s);

};

// Template function to convert anything to a string
template<class T>
std::string toString(const T& t)
{
	std::ostringstream stream;
	stream << t;
	return stream.str();
}

#endif