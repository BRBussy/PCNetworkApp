// PC_Network_App.cpp : Defines the entry point for the console application.
//


#include "stdafx.h"
#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "6950"
#define LOCAL_HOST "BERNARD_PC"

//Special Data Types
//Scheduling Information Structure
struct scheduling_information { //Declare scheduling_information struct type
	int ID; //Device ID
	bool hours_on_off[12][31][24]; //hours_on_off[Months, Days, Hours]
};
//Time and Date Structure
struct time_and_date { //Declare time_and_date struct type
	uint16_t year;
	uint8_t month;
	uint8_t dayOfMonth;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
};
//Power Measurement Structure
struct power_measurement { //Declare power_measurement struct type
	float measurement; //Actual Power Measurement
	time_and_date when_made; //when this reading was taken
	int ID; //The Major Appliance ID
};
//Major Appliance Status Structure
struct major_appliance_status { //Declare major_appliance_status struct type
	bool on_off; //Current on/off Status
	power_measurement latest_measurement; //Most recent power reading
};

//Function Declarations
int Create_a_listening_Socket(SOCKET &ListenSocket);
int Listen_on_ListenSocket_Check_For_Client_Connect(SOCKET & ListenSocket, SOCKET & ClientSocket);
bool Check_if_new_data_for_client(void);
//bool Send_Data_to_Client(const SOCKET &ClientSocket, const string &data_for_client);
bool Send_Data_to_Client(const SOCKET &ClientSocket);
bool Receive_Data_from_Client(const SOCKET &ClientSocket);
int Peek_at_Data_from_Client(const SOCKET &ClientSocket);

int main(void)
{
	//Variable Declaration
	WSADATA wsaData;
	SOCKET ListenSocket = SOCKET_ERROR; //Socket for Server to Listen on
	SOCKET ClientSocket = SOCKET_ERROR; //Socket to store Client Connection
	int iResult;
	bool Client_Connected = false;
	bool Data_to_Send = true;
	bool New_Data_Received = false;
	bool Server_Initialised = false;
	string DataForClient = "Hello From Server!";
	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}
	else
		printf("WSAStartup OK!\n");

	//Operating Loop
	while (true)
	{
		//Server Initialisation
		if (!Server_Initialised) {
			Server_Initialised = Create_a_listening_Socket(ListenSocket); //Create Socket for Server to Listen On
		}
		else
		{
			//Client Connected?
			if (Client_Connected)
			{//Client Connected? --> YES
			 //New Data for Client?		 
			 //Data_to_Send = Check_if_new_data_for_client();

				if (Data_to_Send) //If there is new Data To Send to 
				{//New Data for Client? --> YES
					//Command Client to Wait, New Data is Being Sent
					Client_Connected = Send_Data_to_Client(ClientSocket);
					Sleep(2000);
					Data_to_Send = false;
				}
				else
				{//New Data for Client? --> NO
				 //Tell Client to Sent New Data to Server
					//Client_Connected = Send_Data_to_Client(ClientSocket);
					if (Client_Connected)
					{
						cout << "Want to send receive new Data from Client!" << endl;
						Client_Connected = Send_Data_to_Client(ClientSocket);
						Sleep(2000);
						Data_to_Send = true;
						//Receive_Data_from_Client(ClientSocket);
					}
				}
				
				//Check if Server Reinitialisation is Necessary
				if (!Client_Connected)
				{
					Server_Initialised = false;
				}
			}
			else
			{//Client Connected? --> NO
				cout << "Client not Connected" << endl;
				closesocket(ClientSocket);
				cout << "Wait for Client to Connect..." << endl;
				while (!Client_Connected) {
					Client_Connected = Listen_on_ListenSocket_Check_For_Client_Connect(ListenSocket, ClientSocket);
				}
			}
		}
	}
			
	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();
	return 0;
}
int Create_a_listening_Socket(SOCKET &ListenSocket)
{
	struct addrinfo *ServerInfo = NULL; //Pointer to Linked List of of addrinfo Structures
	struct addrinfo hints;				//For Preparation of Socket Address Structures

	int Result;  //Contain Result for System Calls
	
	//Fill up  Relevant information for Address Structures
	ZeroMemory(&hints, sizeof(hints));	//Make Sure that Struct is empty
	hints.ai_family = AF_INET;			//Use IPv4
	hints.ai_socktype = SOCK_STREAM;	//Use TCP Stream Sockets
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;		//Assign Address of Local Host to Socket Structures

	//Resolve the server address and port
	Result = getaddrinfo(LOCAL_HOST, DEFAULT_PORT, &hints, &ServerInfo);
	if (Result != 0) {
		printf("getaddrinfo failed with error: %d\n", Result);
		WSACleanup();
		return 0;
	}
	else
		printf("getaddrinfo OK!\n");

	//Create a SOCKET for Server to Listen On
	ListenSocket = socket(ServerInfo->ai_family, ServerInfo->ai_socktype, ServerInfo->ai_protocol);
	if (ListenSocket == SOCKET_ERROR) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(ServerInfo);
		WSACleanup();
		return 0;
	}
	else
		printf("Listen Socket creation and opening OK!\n");

	//Bind ListenSocket to Local Port
	Result = bind(ListenSocket, ServerInfo->ai_addr, (int)ServerInfo->ai_addrlen);
	if (Result == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(ServerInfo);
		closesocket(ListenSocket);
		WSACleanup();
		return 0;
	}
	else
		printf("Bind OK!\n");

	freeaddrinfo(ServerInfo); //No Longer Require Sever Address Information
	return 1;
}

int Listen_on_ListenSocket_Check_For_Client_Connect(SOCKET &ListenSocket, SOCKET &ClientSocket)
{
	int Result;

	//Set Server Listening on ListenSocket for Client Connection
	Result = listen(ListenSocket, SOMAXCONN);
	if (Result == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 0;
	}
	else {
		//ListenSocket is Listening good
		ClientSocket = SOCKET_ERROR;
		ClientSocket = accept(ListenSocket, NULL, NULL);  //Look for Client Connection
		if (ClientSocket == SOCKET_ERROR) //Check if Client Connected?
		{
			closesocket(ListenSocket);
			return 0; //No client Connected
		}
		else {
			cout << "Client Connected!" << endl;
			closesocket(ListenSocket);
			return 1; //Client Connected!
		}
			
	}
}

//bool Send_Data_to_Client(const SOCKET &ClientSocket, const string &data_for_client)
bool Send_Data_to_Client(const SOCKET &ClientSocket)
{
	scheduling_information firstschedule;
	cout << "Size is: " << sizeof(firstschedule);
	firstschedule.hours_on_off[12][2][4] = true;
	firstschedule.hours_on_off[12][2][5] = false;
	firstschedule.ID = 4;

	
	const BYTE *ptr_to_bytes = (const BYTE*)(const void*)(&firstschedule); //Pointer to the bytes memory of data to send
	BYTE *Data_Payload = NULL;
	Data_Payload = (BYTE*)realloc(Data_Payload, sizeof(firstschedule)); //get piece of memory which is same num of bytes as data
	
	char frame[8943];
	frame[0] = '|';
	frame[1] = 'D';
	frame[2] = '|';
	frame[3] = '|';
	frame[4] = 'S';
	frame[5] = 'I';
	frame[6] = '|';
	frame[8939] = '|';
	frame[8940] = 'E';
	frame[8941] = 'D';
	frame[8942] = '|';
	int i;
	for (i = 0; i < sizeof(firstschedule); i++)
	{
		Data_Payload[i] = ptr_to_bytes[i];
	}
	cout << "i went to: " << i;
	for (int j = 0; j < i; j++) {
		frame[j + 7] = Data_Payload[j];
		//frame[j + 7] = 'b';
	}

	//Notify Client to Wait for Data
	int iResult = send(ClientSocket, "|D||CM|Receive|ED|", sizeof("|D||CM|Receive|ED|"), 0);
	if (iResult == SOCKET_ERROR) {//If sending Failed
		wprintf(L"send failed with error: %d\n", WSAGetLastError());
		return 0;
	}
	else
	{	//If Sending Succeeded
		Sleep(100);
	}

	iResult = send(ClientSocket, frame, sizeof(frame), 0);
	if (iResult == SOCKET_ERROR) {//If sending Failed
		wprintf(L"send failed with error: %d\n", WSAGetLastError());
		return 0;
	}
	else
	{	//If Sending Succeeded
		Sleep(10);
		return 1;
	}
}

bool Receive_Data_from_Client(const SOCKET &ClientSocket)
{
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	int Result;
	Result = recv(ClientSocket, recvbuf, recvbuflen, 0);
	if (Result > 0) {
		printf("Bytes received: %d\nData: %s\n", Result, recvbuf);
	}
	else if (Result == 0)
	{
		printf("Client Disconnected!\n");
		return 0;
	}
	else {
		printf("recv failed with error: %d\n", WSAGetLastError());
		return 0;
	}
	return 1;
}

int Peek_at_Data_from_Client(const SOCKET &ClientSocket)
{
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
	int Result;
	cout << "I am here";
	Result = recv(ClientSocket, recvbuf, recvbuflen, MSG_PEEK);
	cout << "Now I am here";
	if (Result > 0) {
		printf("No of Bytes in Buffer: %d\nData is: %s\n", Result, recvbuf);
	}
	else if (Result == 0)
	{
		printf("Client Disconnected!\n");
		return 0;
	}
	else {
		printf("Peek failed with error: %d\n", WSAGetLastError());
		return 0;
	}
	return 1;
}

bool Check_if_new_data_for_client(void)
{
	//Perform Some Checks Here and Return 1: if new Data to send. Return 0 if not;
	return 1;
}


//float data_2;
// BYTE *new_ptr_to_bytes = (BYTE*)(void*)(&data_2);
//for (int i = 0; i < sizeof(data_2); i++)
//{
//	cout << "reconstruction..." << endl;
//	new_ptr_to_bytes[i] = Data_Payload[i];
//}
//cout << "Data_2: " << data_2 << endl;
//
//Sleep(500);