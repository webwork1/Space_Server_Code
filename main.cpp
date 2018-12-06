// Re-written simple SDL_net socket server example | Nov 2011 | r3dux
// Library dependencies: libSDL, libSDL_net

// IMPORTANT: This project will only build successfully in Debug mode on Windows!

#include <iostream>
#include "string"
#include "SDL_net.h"
#include "ServerSocket.h"
#include <fstream>
#include <chrono>
#include <ctime>
#include <cstdlib>

// Create a pointer to a ServerSocket object
ServerSocket *ss;

///////////////////// time stuff //////////////////


double holdTime = 0;
double currentTime = 0;
double duration = 0;

void updateTimeStuff() {

	//get time passed
	currentTime = std::clock() / (double)CLOCKS_PER_SEC;
	duration = currentTime - holdTime;
	//setting time passed back to zero every (1) second
	//0.02

	if (duration >= 0.16 ){

		holdTime = std::clock() / (double)CLOCKS_PER_SEC;

		////// every frame stuff goes here ///////
		
		//send shot and then delte shot
		ss->updateShooting2();

	}
	else if (duration > 0.02 && duration < 0.03) {
		ss->updateShooting();
	}
	else if (duration > 0.07 && duration < 0.08) {
		ss->updateShooting();
	}
	else if (duration > 0.12 && duration < 0.13) {
		ss->updateShooting();
	}

}

//////////////////// end time stuff //////////////////


int main(int argc, char *argv[])
{

	//sending players connected every once and a while
	int playerCounter = 0;

	// Initialise SDL_net
	if (SDLNet_Init() == -1)
	{
		std::cerr << "Failed to intialise SDL_net: " << SDLNet_GetError() << std::endl;
		exit(-1);
	}

	try
	{
		// Not try to instantiate the server socket
		// Parameters: port number, buffer size (i.e. max message size), max sockets
		int port;
		cout << "Enter local port:";
		std::cin >> port;


		ss = new ServerSocket(port, 512, 100);
	}
	catch (SocketException e)
	{
		std::cerr << "Something went wrong creating a SocketServer object." << std::endl;
		std::cerr << "Error is: " << e.what() << std::endl;
		std::cerr << "Terminating application." << std::endl;
		exit(-1);
	}

	try
	{
		// Specify which client is active, -1 means "no client is active"
		int activeClient = -1;

		// Main loop...
		do
		{
			//check for time sensitive activities
			if (playerCounter % 3) {
				updateTimeStuff();
			}

			// Check for any incoming connections to the server socket
			ss->checkForConnections();

			// At least once, but as many times as necessary to process all active clients...
			do
			{	

				if (playerCounter == 500) {
					if (ss->getLeavePlayer1() != "") {
						ss->sendToClients("usrl:" + ss->getLeavePlayer1());
						cout << ss->getLeavePlayer1() << " left the game." << endl;
						ss->setplayerLeaving1();
					}
				}
				if (playerCounter == 502) {
					if (ss->getLeavePlayer2() != "") {
						ss->sendToClients("usrl:" + ss->getLeavePlayer2());
						cout << ss->getLeavePlayer2() << " left the game." << endl;
						ss->setplayerLeaving2();
					}
				}
				//sending players connected to server
				if (playerCounter > 1000) {
					string playerMessage = "players:";
					playerMessage += std::to_string(ss->getClientCount());
					ss->sendToClients(playerMessage);
					playerCounter = 0;
				}
				else {
					
					playerCounter++;
				}

				// ..get the client number of any clients with unprocessed activity (returns -1 if none)
				activeClient = ss->checkForActivity();


				// If there's a client with unprocessed activity...
				if (activeClient != -1)
				{
					// ...then process that client!
					ss->dealWithActivity(activeClient);
				}

				// When there are no more clients with activity to process, continue...
			} while (activeClient != -1);

			// ...until we've been asked to shut down.
		} while (ss->getShutdownStatus() == false);

	}
	catch (SocketException e)
	{
		cerr << "Caught an exception in the main loop..." << endl;
		cerr << e.what() << endl;
		cerr << "Terminating application." << endl;
	}

	// Shutdown SDLNet - our ServerSocket will clean up after itself on destruction
	SDLNet_Quit();

	return 0;
}

