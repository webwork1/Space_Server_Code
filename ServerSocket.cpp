#include "ServerSocket.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <ctime>
// Static constants for the ServerSocket class
const string ServerSocket::SERVER_NOT_FULL = "OK";
const string ServerSocket::SERVER_FULL = "FULL";
const string ServerSocket::SHUTDOWN_SIGNAL = "/shutdown";

using namespace std;

// ServerSocket constructor
ServerSocket::ServerSocket(unsigned int thePort, unsigned int theBufferSize, unsigned int theMaxSockets)
{
	debug = false; // Flag to control whether to output debug info
	shutdownServer = false; // Flag to control whether it's time to shut down the server

	port = thePort;                      // The port number on the server we're connecting to
	bufferSize = theBufferSize;                // The maximum size of a message
	maxSockets = theMaxSockets;                // Maximum number of sockets in our socket set
	maxClients = theMaxSockets - 1;            // Maximum number of clients who can connect to the server

	pClientSocket = new TCPsocket[maxClients]; // Create the array to the client sockets
	pSocketIsFree = new bool[maxClients];      // Create the array to the client socket free status'
	pBuffer = new char[bufferSize];            // Create the transmission buffer character array

	clientCount = 0;     // Initially we have zero clients...

	//setting initial playerList to ""
	for (int i = 0; i < maxSockets; i++) {
		playerList[i] = "";

	}

	// Create the socket set with enough space to store our desired number of connections (i.e. sockets)
	socketSet = SDLNet_AllocSocketSet(maxSockets);
	if (socketSet == NULL)
	{
		string msg = "Failed to allocate the socket set: ";
		msg += SDLNet_GetError();

		SocketException e(msg);
		throw e;
	}
	else
	{
		if (debug)
		{
			cout << "Allocated socket set size: " << maxSockets << ", of which " << maxClients << " are free." << endl;
		}
	}

	// Initialize all the client sockets (i.e. blank them ready for use!)
	for (unsigned int loop = 0; loop < maxClients; loop++)
	{
		pClientSocket[loop] = NULL;
		pSocketIsFree[loop] = true; // Set all our sockets to be free (i.e. available for use for new client connections)
	}

	// Try to resolve the provided server hostname to an IP address.
	// If successful, this places the connection details in the serverIP object and creates a listening port on the
	// provided port number.
	// Note: Passing the second parameter as "NULL" means "make a listening port". SDLNet_ResolveHost returns one of two
	// values: -1 if resolving failed, and 0 if resolving was successful
	int hostResolved = SDLNet_ResolveHost(&serverIP, NULL, port);

	if (hostResolved == -1)
	{
		string msg = "Failed to open the server socket: ";
		msg += SDLNet_GetError();

		SocketException e(msg);
		throw e;
	}
	else // If we resolved the host successfully, output the details
	{
		if (debug)
		{
			// Get our IP address in proper dot-quad format by breaking up the 32-bit unsigned
			// host address and splitting it into an array of four 8-bit unsigned numbers...
			Uint8 * dotQuad = (Uint8*)&serverIP.host;

			dotQuadString = toString((unsigned short)dotQuad[0]);
			dotQuadString += ".";
			dotQuadString += toString((unsigned short)dotQuad[1]);
			dotQuadString += ".";
			dotQuadString += toString((unsigned short)dotQuad[2]);
			dotQuadString += ".";
			dotQuadString += toString((unsigned short)dotQuad[3]);

			//... and then outputting them cast to integers. Then read the last 16 bits of the serverIP object to get the port number
			cout << "Successfully resolved server host to IP: " << dotQuadString;
			cout << ", will use port " << SDLNet_Read16(&serverIP.port) << endl;
		}
	}

	// Try to open the server socket
	serverSocket = SDLNet_TCP_Open(&serverIP);

	if (!serverSocket)
	{
		string msg = "Failed to open the server socket: ";
		msg += SDLNet_GetError();

		SocketException e(msg);
		throw e;
	}
	else
	{
		if (debug) { cout << "Sucessfully created server socket." << endl; }
	}

	// Add our server socket (i.e. the listening socket) to the socket set
	SDLNet_TCP_AddSocket(socketSet, serverSocket);

	if (debug) {
		cout << "Awaiting clients..." << endl;
	}

} // End of constructor

// ServerSocket destructor
ServerSocket::~ServerSocket()
{
	// Close all the open client sockets
	for (unsigned int loop = 0; loop < maxClients; loop++)
	{
		if (pSocketIsFree[loop] == false)
		{
			SDLNet_TCP_Close(pClientSocket[loop]);
			pSocketIsFree[loop] = true;
		}
	}

	// Close our server socket
	SDLNet_TCP_Close(serverSocket);

	// Free our socket set
	SDLNet_FreeSocketSet(socketSet);

	// Release any properties on the heap
	delete pClientSocket;
	delete pSocketIsFree;
	delete pBuffer;
}


void ServerSocket::checkForConnections()
{
	// Check for activity on the entire socket set. The second parameter is the number of milliseconds to wait for.
	// For the wait-time, 0 means do not wait (high CPU!), -1 means wait for up to 49 days (no, really), and any other
	// number is a number of milliseconds, i.e. 5000 means wait for 5 seconds, 50 will poll (1000 / 50 = 20) times per second.
	// I've used 1ms below, so we're polling 1,000 times per second, which is overkill for a small chat server, but might
	// be a good choice for a FPS server where every ms counts! Also, 1,000 polls per second produces negligable CPU load,
	// if you put it on 0 then it WILL eat all the available CPU time on one of your cores...
	int numActiveSockets = SDLNet_CheckSockets(socketSet, 1);

	if (numActiveSockets != 0)
	{
		if (debug) {
			cout << "There are currently " << numActiveSockets << " socket(s) with data to be processed." << endl;
		}
	}

	// Check if our server socket has received any data
	// Note: SocketReady can only be called on a socket which is part of a set and that has CheckSockets called on it (the set, that is)
	// SDLNet_SocketRead returns non-zero for activity, and zero is returned for no activity. Which is a bit bass-ackwards IMHO, but there you go.
	int serverSocketActivity = SDLNet_SocketReady(serverSocket);

	// If there is activity on our server socket (i.e. a client has trasmitted data to us) then...
	if (serverSocketActivity != 0)
	{
		// If we have room for more clients...
		if (clientCount < maxClients)
		{
			// Find the first free socket in our array of client sockets
			int freeSpot = -99;
			for (unsigned int loop = 0; loop < maxClients; loop++)
			{
				if (pSocketIsFree[loop] == true)
				{
					if (debug) {
						cout << "Found a free spot at element: " << loop << endl;
					}

					pSocketIsFree[loop] = false; // Set the socket to be taken
					freeSpot = loop;             // Keep the location to add the new connection at that index in the array
					break;                       // Break out of the loop straight away
				}
			}

			// ...accept the client connection and then...
			pClientSocket[freeSpot] = SDLNet_TCP_Accept(serverSocket);

			// ...add the new client socket to the socket set (i.e. the list of sockets we check for activity)
			SDLNet_TCP_AddSocket(socketSet, pClientSocket[freeSpot]);

			// Increase our client count
			clientCount++;

			// Send a message to the client saying "OK" to indicate the incoming connection has been accepted
			strcpy(pBuffer, SERVER_NOT_FULL.c_str());
			int msgLength = strlen(pBuffer) + 1;
			SDLNet_TCP_Send(pClientSocket[freeSpot], (void *)pBuffer, msgLength);

			if (debug) { cout << "Client connected. There are now " << clientCount << " client(s) connected." << endl; }
		}
		else // If we don't have room for new clients...
		{
			if (debug) { cout << "Max client count reached - rejecting client connection" << endl; }

			// Accept the client connection to clear it from the incoming connections list
			TCPsocket tempSock = SDLNet_TCP_Accept(serverSocket);

			// Send a message to the client saying "FULL" to tell the client to go away
			strcpy(pBuffer, SERVER_FULL.c_str());
			int msgLength = strlen(pBuffer) + 1;
			SDLNet_TCP_Send(tempSock, (void *)pBuffer, msgLength);

			// Shutdown, disconnect, and close the socket to the client
			SDLNet_TCP_Close(tempSock);
		}

	} // End of if server socket is has activity check

} // End of checkActivity function

// Function to do something appropriate with the detected socket activity (i.e. we received a message from a client)
// You should put whatever you want to happen when a message is sent from a client inside this function!
// In this example case, I'm going to send the message to all other connected clients except the one who originated the message!
void ServerSocket::dealWithActivity(unsigned int clientNumber)
{
	// Get the contents of the buffer as a string
	string bufferContents = pBuffer;

	// Output the message the server received to the screen
	if (debug) {
		cout << "Received: >>>> " << bufferContents << " from client number: " << clientNumber << endl;
	}

	//if message was not meant for server..

	if (bufferContents[0] != '!') {

		// Send message to all other connected clients
		for (unsigned int loop = 0; loop < maxClients; loop++)
		{
			// Send a message to the client saying "OK" to indicate the incoming connection has been accepted
			//strcpy( buffer, SERVER_NOT_FULL.c_str() );
			unsigned int msgLength = strlen(pBuffer) + 1;

			// If the message length is more than 1 (i.e. client pressed enter without entering any other text), then
			// send the message to all connected clients except the client who originated the message in the first place
			if ((msgLength > 1) && (loop != clientNumber) && (pSocketIsFree[loop] == false))
			{
				if (debug) {
					cout << "Retransmitting: " << bufferContents << " (" << msgLength << " bytes) to client " << loop << endl;
				}
				
				SDLNet_TCP_Send(pClientSocket[loop], (void *)pBuffer, msgLength);
			}
		}

	}
	//if command is meant for server...
	else {
		//delete first character of string '!'
		bufferContents.erase(bufferContents.begin());

		// if user is trying to add themselves to list of players..
		if (bufferContents[0] == 'u' && bufferContents[1] == 's' && bufferContents[2] == 'e') {

			//deleting 'use'
			bool foundPlayers = false;

			while (foundPlayers == false) {

				if (bufferContents[0] != ':') {
					bufferContents.erase(bufferContents.begin() + 0);
				}
				else {
					foundPlayers = true;
					bufferContents.erase(bufferContents.begin());
				}


			}

			//adding username to list of players
			playerList[clientNumber] = bufferContents;
			cout << bufferContents << " joined the game!" << endl;


		}

		//if client is trying to shoot
		if (bufferContents[0] == 's' && bufferContents[1] == 'h' && bufferContents[2] == 'o' &&  bufferContents[3] == 'o' && bufferContents[4] == 't') {
			
			//delete beginning
			bufferContents.erase(bufferContents.begin(),bufferContents.begin() + 6);
			//string array for passed in stuff
			vector<string> shootInfo;

			//variable to count down iteration of string
			int counting = 0;

			//getting out inputs we need
			while (bufferContents.length() > 2) {
				counting = bufferContents.length() - 1;
				bool tempBool = true;

				if (bufferContents.length() > 2) {

					bufferContents.erase(bufferContents.begin() + counting);
					counting--;

					while (tempBool) {

						if (bufferContents[counting] != '~') {
							counting--;
						}
						else {
							tempBool = false;
						}

					}

					shootInfo.push_back(bufferContents.substr(counting + 1, ((bufferContents.length() - 1) - counting + 1)));
					bufferContents.erase(counting, ((bufferContents.length() - 1) - counting));
				}

			}

			string typeOfShotServer = "";
			//getting out shot type
			for (int a = 0; a < shootInfo.size(); a++) {
				if (shootInfo[a][0] == 's' && shootInfo[a][1] == 'h' && shootInfo[a][2] == 'o' && shootInfo[a][3] == 't') {

					typeOfShotServer = shootInfo[a];

				}
			}

			///// looping through info for shot

			//////// if shot is blaster //////////////
			if (typeOfShotServer == "shot:blaster") {
				for (int i = 0; i < shootInfo.size(); i++) {
					//if info contains username
					if (shootInfo[i][0] == 'u' && shootInfo[i][1] == 's' && shootInfo[i][2] == 'e' && shootInfo[i][3] == 'r') {
						shootInfo[i].erase(shootInfo[i].begin(), shootInfo[i].begin() + 5);
						shotName.push_back(shootInfo[i]);
						shotName.push_back(shootInfo[i]);
					}
					if (shootInfo[i][0] == 's' && shootInfo[i][1] == 'h' && shootInfo[i][2] == 'o' && shootInfo[i][3] == 't') {
						shootInfo[i].erase(shootInfo[i].begin(), shootInfo[i].begin() + 5);
						shotType.push_back(shootInfo[i]);
						shotType.push_back(shootInfo[i]);
					}
					if (shootInfo[i][0] == 'x' && shootInfo[i][1] == 'c' && shootInfo[i][2] == 'o' && shootInfo[i][3] == 'r') {
						shootInfo[i].erase(shootInfo[i].begin(), shootInfo[i].begin() + 6);
						shotX.push_back(stoi(shootInfo[i]));
					}
					if (shootInfo[i][0] == 'y' && shootInfo[i][1] == 'c' && shootInfo[i][2] == 'o' && shootInfo[i][3] == 'r') {
						shootInfo[i].erase(shootInfo[i].begin(), shootInfo[i].begin() + 6);
						shotY.push_back(stoi(shootInfo[i]));
					}
					if (shootInfo[i][0] == 'r' && shootInfo[i][1] == 'o' && shootInfo[i][2] == 't' && shootInfo[i][3] == 'a') {
						shootInfo[i].erase(shootInfo[i].begin(), shootInfo[i].begin() + 6);
						shotRotation.push_back(stoi(shootInfo[i]));
						shotRotation.push_back(stoi(shootInfo[i]));
					}
					if (shootInfo[i][0] == 'x' && shootInfo[i][1] == 'v' && shootInfo[i][2] == 's' && shootInfo[i][3] == 'h' && shootInfo[i][4] == 'o' && shootInfo[i][5] == 't') {
						shootInfo[i].erase(shootInfo[i].begin(), shootInfo[i].begin() + 7);
						shotStartVelocityX.push_back(stoi(shootInfo[i]));
						shotStartVelocityX.push_back(stoi(shootInfo[i]));
					}
					if (shootInfo[i][0] == 'y' && shootInfo[i][1] == 'v' && shootInfo[i][2] == 's' && shootInfo[i][3] == 'h' && shootInfo[i][4] == 'o' && shootInfo[i][5] == 't') {
						shootInfo[i].erase(shootInfo[i].begin(), shootInfo[i].begin() + 7);
						shotStartVelocityY.push_back(stoi(shootInfo[i]));
						shotStartVelocityY.push_back(stoi(shootInfo[i]));
					}

					//setting shot time to zero
					shotTime.push_back(0);
					shotTime.push_back(0);
					////giving unique name to shot

					for (int z = 0; z <= 1; z++) {
						bool finished = false;

						while (finished == false) {
							finished = true;
							string vaf = to_string((rand() % 99999)) + "abc";

							for (int b = 0; b < uniqueName.size(); b++) {

								if (vaf == uniqueName[b]) {
									finished = false;
								}

							}

							if (finished) {
								uniqueName.push_back(vaf);
							}


						}
					}


				}

			}
				for (int i = 0; i < shootInfo.size(); i++) {
					shootInfo[i] = "";
				}
				//update shooting
				updateShooting();
		}

		// if client is requesting a chunk
		if (bufferContents[0] == 'l' && bufferContents[1] == 'o' && bufferContents[2] == 'a' && bufferContents[3] == 'd' && bufferContents[4] == 'c' && bufferContents[5] == 'h') {

			//deleting 'loadchunk:'
			bool foundPlayers = false;

			while (foundPlayers == false) {

				if (bufferContents[0] != ':') {
					bufferContents.erase(bufferContents.begin() + 0);
				}
				else {
					foundPlayers = true;
					bufferContents.erase(bufferContents.begin());
				}

			}



			//getting out x and y of chunks
			string getChunkX = bufferContents;
			string getChunkY = bufferContents;



			//getting out chunkx

			bool foundUser2 = false;
			bool holdv = false;
			int usernameVariable = 0;

			while (foundUser2 == false) {

				if (holdv) {

					if (getChunkX[usernameVariable] == '~') {
						foundUser2 = true;
					}

					getChunkX.erase(getChunkX.begin() + usernameVariable);

				}
				else {

					if (getChunkX[usernameVariable] == ',') {
						getChunkX.erase(getChunkX.begin() + usernameVariable);
						usernameVariable--;
						holdv = true;
					}
					usernameVariable++;
				}
			}

			///// getting out chunk y /////
			bool foundPassword = false;
			bool holdvPass = false;
			int passwordVariable = 0;

			while (foundPassword == false) {
				if (holdvPass == false) {

					if (getChunkY[0] != ',') {

						getChunkY.erase(getChunkY.begin() + 0);

					}
					else {
						holdvPass = true;
						getChunkY.erase(getChunkY.begin());

					}
				}
				else {

					if (getChunkY[passwordVariable] == '~') {
						foundPassword = true;
						getChunkY.erase(getChunkY.begin() + passwordVariable);
					}
					else {
						passwordVariable++;
					}

				}

			}

			//checking if chunk already exists
			std::ifstream f("data/chunks/" + bufferContents + ".txt");
			if (!f.good()) {

				//if chunk doesn't exist	


				/////////////////////   setting planet values   //////////////////


				// variables
					int numPlanets = 10;

				    vector<bool> planetInit;
					vector<int> planetX;
					vector<int> planetY;
					vector<int> planetR;
					vector<int> planetG;
					vector<int> planetB;
					vector<int> planetDiameter;
					vector<int> planetImage;

					//resetting variables
					planetInit.resize(numPlanets);
					planetX.resize(numPlanets);
					planetY.resize(numPlanets);
					planetR.resize(numPlanets);
					planetG.resize(numPlanets);
					planetB.resize(numPlanets);
					planetDiameter.resize(numPlanets);
					planetImage.resize(numPlanets);

					planetInit.shrink_to_fit();
					planetX.shrink_to_fit();
					planetY.shrink_to_fit();
					planetR.shrink_to_fit();
					planetG.shrink_to_fit();
					planetB.shrink_to_fit();
					planetDiameter.shrink_to_fit();
					planetImage.shrink_to_fit();

					//resetting planets
					for (int i = 0; i < numPlanets; i++) {
						planetInit.at(i) = false;

					}

					//(rand() % 3000) + 1;
					int randx = 0;
					int randy = 0;

					int planetCounter = 0;

					int xSide;
					int ySide;
					int sideLength;
					//looping through all planets..
					for (int i = 0; i < numPlanets; i++) {

						//setting planet Image
						planetImage[i] = (rand() % 30);

						while (planetInit[i] == false) {

							randx = ((20000*stoi(getChunkX)) ) + (rand() % 19000) + 500;
							randy = (((20000)*stoi(getChunkY))) + (rand() %19000) + 500;
							planetDiameter[i] = 300 + (rand() % 1400);

							//used to make sure new planet works with EVERY existing planet
							planetCounter = 0;

							//checking for first planet
							if (i == 0) {
								//the planet x and y is the middle of the planet
								planetX[i] = randx;
								planetY[i] = randy;
								planetR[i] = (rand() % 255);
								planetG[i] = (rand() % 255);
								planetB[i] = (rand() % 255);

								//seeing if planet has been initialized
								planetInit[i] = true;
							}
							else {
								//if not the first planet...
								for (int z = 0; z < i; z++) {

									//getting x and y sides to calculate distance between planets
									xSide = abs((randx + (planetDiameter[i] / 2)) - (planetX[z] + (planetDiameter[z] / 2)));
									ySide = abs(randy - (planetDiameter[i] / 2) - (planetY[z] - (planetDiameter[z] / 2)));

									//distance between planets
									sideLength = sqrt((xSide *xSide) + (ySide*ySide));

									//making sure random values dont intercept inited planets X
									if (sideLength > ((planetDiameter[i] * 2) + planetDiameter[z])) {
										planetCounter = planetCounter + 1;
									}
									else {
									}
									//making sure random values dont intercept inited planets Y
									if (planetCounter == i) {

										planetX[i] = randx;
										planetY[i] = randy;
										planetR[i] = (rand() % 255);
										planetG[i] = (rand() % 255);
										planetB[i] = (rand() % 255);

										//seeing if planet has been initialized
										planetInit[i] = true;
									}


								}

							}

						}


					}

					//writing variables to file
					std::ofstream out("data/chunks/" + bufferContents + ".txt");

					for (int i = 0; i < numPlanets; i++) {


						out << std::to_string(planetX[i]) + " " + std::to_string(planetY[i]) + " " + std::to_string(planetDiameter[i]) + " " + std::to_string(planetR[i]) + " " + std::to_string(planetG[i]) + " " + std::to_string(planetB[i]) + " " + std::to_string(planetImage[i]) << endl;

					}

					out.close();

			}

			//////////// returning chunk data to player ////////////
			string chunkData = "";
			string tempString = "retchunk" + getChunkX + "~" + getChunkY;

			std::ifstream planetInfo;

			planetInfo.open("data/chunks/" + bufferContents + ".txt", std::ifstream::in);

			while (planetInfo.good()) {

				chunkData += tempString + "~";
				planetInfo >> tempString;

			}

			planetInfo.close();
			//sending chunk info to player
			SDLNet_TCP_Send(pClientSocket[clientNumber], chunkData.c_str(), chunkData.length()+1);


		}

		// if client is attempting to login
		if (bufferContents[0] == 'l' && bufferContents[1] == 'o' && bufferContents[2] == 'g' && bufferContents[3] == 't') {

			//deleting 'log:'
			bool foundPlayers = false;

			while (foundPlayers == false) {

				if (bufferContents[0] != ':') {
					bufferContents.erase(bufferContents.begin() + 0);
				}
				else {
					foundPlayers = true;
					bufferContents.erase(bufferContents.begin());
				}

			}

			//getting out username

			bool foundUser2 = false;
			string attemptedUsername = bufferContents;
			bool holdv = false;
			int usernameVariable = 0;

			while (foundUser2 == false) {

				if (holdv) {

					if (attemptedUsername[usernameVariable] == '~') {
						foundUser2 = true;
					}

					attemptedUsername.erase(attemptedUsername.begin() + usernameVariable);

				}
				else {

					if (attemptedUsername[usernameVariable] == '/') {
						attemptedUsername.erase(attemptedUsername.begin() + usernameVariable);
						usernameVariable--;
						holdv = true;
					}
					usernameVariable++;
				}
			}

			///// getting out password /////
			bool foundPassword = false;
			string attemptedPassword = bufferContents;
			bool holdvPass = false;
			int passwordVariable = 0;

			while (foundPassword == false) {

				if (holdvPass == false) {

					if (attemptedPassword[0] != '/') {

						attemptedPassword.erase(attemptedPassword.begin() + 0);

					}
					else {
						holdvPass = true;
						attemptedPassword.erase(attemptedPassword.begin() + 0);

					}
				}
				else {

					if (attemptedPassword[passwordVariable] == '~') {
						foundPassword = true;
						attemptedPassword.erase(attemptedPassword.begin() + passwordVariable);
					}
					else {
						passwordVariable++;
					}

				}

			}

			//bool for is player is already logged on
			bool playerAlreadyOn = false;

			//checking if user is already logged on
			for (int i = 0; i < maxClients; i++) {
				if (attemptedUsername == playerList[i]) {
					playerAlreadyOn = true;
				}
			}

			//if player is not already on
			if (playerAlreadyOn == false) {

				///// checking username and password in database
				std::ifstream userInfo;

				userInfo.open("data/userInfo.txt", std::ifstream::in);

				string getUsername = "";
				string getPassword = "";

				//bool for seeing if user is found
				bool userFound = false;

				while (userInfo.good()) {

					userInfo >> getUsername;
					userInfo >> getPassword;

					if (getUsername == attemptedUsername && getPassword == attemptedPassword) {

						userFound = true;

						//telling user that their username and password is accepted
						SDLNet_TCP_Send(pClientSocket[clientNumber], "usracpt", 7);

					}

				}

				//sending error message if user is not found in data
				if (userFound == false) {
					SDLNet_TCP_Send(pClientSocket[clientNumber], "usrdec", 6);
				}

				userInfo.close();

			}
			//if player is already logged on
			else {

				SDLNet_TCP_Send(pClientSocket[clientNumber], "usralon", 7);

			}
		}
		///// dealing with sign up request ///////
		if (bufferContents[0] == 's' && bufferContents[1] == 'i' && bufferContents[2] == 'g' && bufferContents[3] == 'n' && bufferContents[4] == 'u' && bufferContents[5] == 'p') {
			//deleting 'signup:'
			bool foundPlayers = false;

			while (foundPlayers == false) {

				if (bufferContents[0] != ':') {
					bufferContents.erase(bufferContents.begin() + 0);
				}
				else {
					foundPlayers = true;
					bufferContents.erase(bufferContents.begin());
				}

			}


			//getting out username

			bool foundUser2 = false;
			string attemptedUsername = bufferContents;
			bool holdv = false;
			int usernameVariable = 0;

			while (foundUser2 == false) {


				if (holdv) {

					if (attemptedUsername[usernameVariable] == '~') {
						foundUser2 = true;
					}

					attemptedUsername.erase(attemptedUsername.begin() + usernameVariable);

				}
				else {

					if (attemptedUsername[usernameVariable] == '/') {
						attemptedUsername.erase(attemptedUsername.begin() + usernameVariable);
						usernameVariable--;
						holdv = true;
					}
					usernameVariable++;
				}
			}

			///// getting out password /////
			bool foundPassword = false;
			string attemptedPassword = bufferContents;
			bool holdvPass = false;
			int passwordVariable = 0;

			while (foundPassword == false) {

				if (holdvPass == false) {

					if (attemptedPassword[0] != '/') {

						attemptedPassword.erase(attemptedPassword.begin() + 0);

					}
					else {
						holdvPass = true;
						attemptedPassword.erase(attemptedPassword.begin() + 0);

					}
				}
				else {

					if (attemptedPassword[passwordVariable] == '~') {
						foundPassword = true;
						attemptedPassword.erase(attemptedPassword.begin() + passwordVariable);
					}
					else {
						passwordVariable++;
					}

				}

			}

			///// checking for username in database
			std::ifstream userInfo;

			userInfo.open("data/userInfo.txt", std::ifstream::in);

			string getUsername = "";
			string getPassword = "";

			//bool for seeing if user is found
			bool userTaken = false;

			while (userInfo.good()) {

				userInfo >> getUsername;
				userInfo >> getPassword;

				if (getUsername == attemptedUsername) {

					userTaken = true;

				}

			}

			//telling user that username is taken
			if (userTaken) {
				SDLNet_TCP_Send(pClientSocket[clientNumber], "signtaken", 9);
			}
			else {
				//telling user that their account has been created
				SDLNet_TCP_Send(pClientSocket[clientNumber], "signacpt", 8);

				//adding username and password to the end of data file
				std::ofstream out;
				out.open("data/userInfo.txt", std::ios::app);
				out << attemptedUsername + " " + attemptedPassword << endl;
				out.close();

			}

			userInfo.close();


		}

	}

	// If the client told us to shut down the server, then set the flag to get us out of the main loop and shut down
	if (bufferContents.compare(SHUTDOWN_SIGNAL) == 0)
	{
		shutdownServer = true;

		if (debug) { cout << "Disconnecting all clients and shutting down the server..." << endl; }
	}

} // End of dealWithActivity function


	//sending message to all clients
void ServerSocket::sendToClients(string s) {

	unsigned int msgLength = strlen(s.c_str()) + 1;

	// Send message to all other connected clients
	for (unsigned int loop = 0; loop < maxClients; loop++)
	{

		if (pSocketIsFree[loop] == false)
		{

			SDLNet_TCP_Send(pClientSocket[loop], s.c_str(), msgLength);
		}

	}

}


// Function to check all connected client sockets for activity
// If we find a client with activity we return its number, or if there are
// no clients with activity or we've processed all activity we return 0
int ServerSocket::checkForActivity()
{
	// Loop to check all possible client sockets for activity
	for (unsigned int clientNumber = 0; clientNumber < maxClients; clientNumber++)
	{
		// If the socket is has activity then SDLNet_SocketReady() returns non-zero
		int clientSocketActivity = SDLNet_SocketReady(pClientSocket[clientNumber]);

		// The line below produces a LOT of debug, so only uncomment if the code's seriously misbehaving!
		//cout << "Client number " << clientNumber << " has activity status: " << clientSocketActivity << endl;

		// If there is any activity on the client socket...
		if (clientSocketActivity != 0)
		{
			// Check if the client socket has transmitted any data by reading from the socket and placing it in the buffer character array
			int receivedByteCount = SDLNet_TCP_Recv(pClientSocket[clientNumber], pBuffer, bufferSize);

			// If there's activity, but we didn't read anything from the client socket, then the client has disconnected...
			if (receivedByteCount <= 0)
			{
					
					//sending to other players that client left
					playerLeaving(playerList[clientNumber]);

				//removing client from playerList
				playerList[clientNumber] = "";

				//...so output a suitable message and then...
				if (debug) { cout << "Client " << clientNumber << " disconnected." << endl; }

				//... remove the socket from the socket set, then close and reset the socket ready for re-use and finally...
				SDLNet_TCP_DelSocket(socketSet, pClientSocket[clientNumber]);
				SDLNet_TCP_Close(pClientSocket[clientNumber]);
				pClientSocket[clientNumber] = NULL;

				// ...free up their slot so it can be reused...
				pSocketIsFree[clientNumber] = true;

				// ...and decrement the count of connected clients.
				clientCount--;

				if (debug) { cout << "Server is now connected to: " << clientCount << " client(s)." << endl; }
			}
			else // If we read some data from the client socket...
			{
				// ... return the active client number to be processed by the dealWithActivity function
				return clientNumber;
			}

		} // End of if client socket is active check

	} // End of server socket check sockets loop

	// If we got here then there are no more clients with activity to process!
	return -1;

} // End of checkForActivity function

// Function to return the shutdown status of the ServerSocket object
bool ServerSocket::getShutdownStatus()
{
	return shutdownServer;
}

//when player leaves..
void ServerSocket::playerLeaving(string s) {

	if (holdPlayerLeave1 == "") {
		holdPlayerLeave1 = s;
	}
	else {
		holdPlayerLeave2 = s;
	}
		//sendToClients("usrl:" + s);

}

//setting player leave variable to ""
void ServerSocket::setplayerLeaving1() {
	holdPlayerLeave1 = "";

}
void ServerSocket::setplayerLeaving2() {
	holdPlayerLeave2 = "";
}

//updating shooting stuff
void ServerSocket::updateShooting(){

	if (shotX.size() > 0) {
		string sendShoot = "shoot:";


		//deleting vectors when shot time is excedded
		for (int i = 0; i < shotX.size(); i++) {

			//send initial shot parameters to players

			sendShoot += "/";
			sendShoot += "~uniname:" + uniqueName[i];
			sendShoot += "~user:" + shotName[i];
			sendShoot += "~shot:" + shotType[i];
			sendShoot += "~xcor:" + to_string((int)shotX[i]);
			sendShoot += "~ycor:" + to_string((int)shotY[i]);
			sendShoot += "~rotat:" + to_string((int)shotRotation[i]) + "~";
			sendShoot += "~xvshot:" + to_string(shotStartVelocityX[i]) + "~";
			sendShoot += "~yvshot:" + to_string(shotStartVelocityY[i]) + "~";
			sendShoot += "~timeshot:" + to_string(shotTime[i]) + "~";
			sendShoot += "/";

		}

		// Send message to all other connected clients
		for (unsigned int loop = 0; loop < maxClients; loop++)
		{

			if (pSocketIsFree[loop] == false)
			{

				unsigned int msgLength = strlen(sendShoot.c_str()) + 1;

				SDLNet_TCP_Send(pClientSocket[loop], sendShoot.c_str(), msgLength);
			}

		}
	}
}

//used to make sure shots are sent
void ServerSocket::updateShooting2() {
		updateShooting();

	//duplicate needed to delete first shot of vector
	for (int i = 0; i < shotX.size(); i++) {
		//delete shot on server
		shotX.erase(shotX.begin() + i);
		shotY.erase(shotY.begin() + i);
		shotName.erase(shotName.begin() + i);
		shotRotation.erase(shotRotation.begin() + i);
		shotType.erase(shotType.begin() + i);
		shotTime.erase(shotTime.begin() + i);
		uniqueName.erase(uniqueName.begin() + i);
		shotStartVelocityX.erase(shotStartVelocityX.begin() + i);
		shotStartVelocityY.erase(shotStartVelocityY.begin() + i);
	}

	for (int i = 0; i < shotX.size(); i++) {
		//delete shot on server
		shotX.erase(shotX.begin() + i);
		shotY.erase(shotY.begin() + i);
		shotName.erase(shotName.begin() + i);
		shotRotation.erase(shotRotation.begin() + i);
		shotType.erase(shotType.begin() + i);
		shotTime.erase(shotTime.begin() + i);
		uniqueName.erase(uniqueName.begin() + i);
		shotStartVelocityX.erase(shotStartVelocityX.begin() + i);
		shotStartVelocityY.erase(shotStartVelocityY.begin() + i);
	}
}