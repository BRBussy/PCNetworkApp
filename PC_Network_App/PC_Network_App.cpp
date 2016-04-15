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
#include <fstream>
#include <time.h> 
using namespace std;

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")
#pragma warning(disable : 4996)

#define DEFAULT_BUFLEN 10000
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
//Server Initialisation Subroutines
int Create_a_listening_Socket(SOCKET &ListenSocket);
int Listen_on_ListenSocket_Check_For_Client_Connect(SOCKET & ListenSocket, SOCKET & ClientSocket);

//Send Receive Protocol Subroutines
bool CoOrdinate_Sending_Data_to_Client(const SOCKET &ClientSocket);
bool CoOrdinate_Receiving_Data_from_Client(const SOCKET &ClientSocket);
bool Send_Data_to_Client(const SOCKET &ClientSocket, const char *data_to_send);
bool Receive_Data_from_Client(const SOCKET &ClientSocket, char *received_data);
bool Check_if_new_data_for_client(void);

//Data Processing Functions
template <class T> void rebuild_received_data(BYTE *Data_Payload, const int &Num_Bytes_in_Payload, T& rebuilt_variable);
void process_received_power_measurement(BYTE *Data_Payload, int &no_of_bytes_in_payload);
void process_received_command(BYTE *Data_Payload, int &no_of_bytes_in_payload);
void process_received_status(BYTE *Data_Payload, int &no_of_bytes_in_payload);



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
			 Data_to_Send = Check_if_new_data_for_client();

				if (Data_to_Send) //If there is new Data To Send to 
				{//New Data for Client? --> YES
					Client_Connected = CoOrdinate_Sending_Data_to_Client(ClientSocket);
					Sleep(2000);
				}
				else
				{//New Data for Client? --> NO
				 //Tell Client to Sent New Data to Server
					//Client_Connected = Send_Data_to_Client(ClientSocket); //Test if Client Still there?
					if (Client_Connected)
					{
						Client_Connected = CoOrdinate_Receiving_Data_from_Client(ClientSocket);
						Sleep(2000);
					}
				}
				//Check if Server Reinsitialisation is Necessary
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
//Server Initialisation Subroutines
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

//Send Receive Protocol Subroutines
bool CoOrdinate_Sending_Data_to_Client(const SOCKET &ClientSocket)
{
	char data_to_send[] = "Hello_Client";
	//Command Client to Ready itself to Receive Data
	if (!Send_Data_to_Client(ClientSocket, "|D||CM|Receive|ED|")) {
		return 0; //Client not available
	}
	//Do actual Sending of 1 piece of data.
}
bool CoOrdinate_Receiving_Data_from_Client(const SOCKET &ClientSocket)
{
	char received_data[DEFAULT_BUFLEN];
	bool Fieldfound = false;
	int Data_Start_location = 0;
	int Data_End_Location = 0;
	int no_of_bytes_in_payload = 0;
	string data_ID = "";
	BYTE *Data_Payload = NULL;
	string filename;

	//Command Client to Send Data to the Server
	if (!Send_Data_to_Client(ClientSocket, "|D||CM|Send|ED|")) {
		return 0; //Client not available
	}
	cout << "Wait for client to Send Data" << endl;
	Sleep(1500); //Give Client Some Time
	if (Receive_Data_from_Client(ClientSocket, received_data)) 
	{
		//Look for Start of Data Field
		for (int i = 0; i < (DEFAULT_BUFLEN - 3); i++) 
		{
			string DataStartField = "";
			for (int j = i; j < i + 3; j++) {
				DataStartField += received_data[j];
			}
			if (DataStartField == "|D|") {
				Fieldfound = true;
				Data_Start_location = i + 7;
				//cout << "Data Start Field Found" << endl;
				break; //Stop Looking since start of Data found
			}
		}
		if (Fieldfound)
		{
			//Look For end of Data Field
			Fieldfound = false;
			for (int i = 0; i < (DEFAULT_BUFLEN - 4); i++)
			{
				string DataEndField = "";
				for (int j = i; j < i + 4; j++) {
					DataEndField += received_data[j];
				}
				if (DataEndField == "|ED|") {
					Fieldfound = true;
					Data_End_Location = i;
					//cout << "Data End Field Found" << endl;
					break; //Stop Looking since start of Data found
				}
			}
			if (Fieldfound)
			{
				//Extract Data ID Field
				for (int i = Data_Start_location - 4; i < Data_Start_location; i++)
				{
					data_ID += received_data[i];
				}

			}
			else
			{
				cout << "Invalid Frame Received: No End of Data Found" << endl;
				return 1;
			}
		}
		else
		{
			cout << "Invalid Frame Received: No Start of Data Found" << endl;
			return 1;
		}

		no_of_bytes_in_payload = Data_End_Location - Data_Start_location; //Calculate no of bytes
		Data_Payload = (BYTE*)realloc(Data_Payload, no_of_bytes_in_payload*sizeof(BYTE)); //Create array for bytes
		
		//Extract Payload
		int j = 0;
		for (int i = Data_Start_location; i < Data_End_Location; i++) {
			Data_Payload[j] = received_data[i];
			j++;
		}
		//Get Current time to use as filename
		time_t rawtime;
		struct tm * timeinfo;
		time(&rawtime);
		string time_now_is = asctime(localtime(&rawtime));
		for (int i = 0; i < time_now_is.length(); i++) {
			if (time_now_is[i] == ' ')
			{
				time_now_is[i] = '_';
			}
			if (time_now_is[i] == ':')
			{
				time_now_is[i] = '.';
			}
		}
		
		//------Save Frame data payload to File------------------------
		//Create Unique File Name
		filename = "C:\\Users\\Bernard\\Documents\\Buffer_area\\";
		filename += time_now_is;
		if (!filename.empty() && filename[filename.length() - 1] == '\n') {
			filename.erase(filename.length() - 1);
		}
		//Chose Correct file type based on ID field
		if (data_ID == "|PR|")
		{
			filename += ".prdat";
		}
		else if (data_ID == "|CM|")
		{
			filename += ".cmdat";
		}
		else if (data_ID == "|ST|")
		{
			filename += ".stdat";
		}
		
		//Save data payload to file
		ofstream outfile;
		outfile.open(filename.c_str());
		//for (int i = Data_Start_location; i < Data_End_Location; i++) {
		//	outfile << received_data[i];
		//}
		power_measurement test_measurement;
		test_measurement.ID = 5;
		test_measurement.measurement = 22.5;
		test_measurement.when_made.dayOfMonth = 4;
		test_measurement.when_made.hour = 1;
		test_measurement.when_made.second = 55;
		test_measurement.when_made.minute = 55;
		test_measurement.when_made.year = 1994;

		BYTE *ptr_to_test_data_bytes = (BYTE*)(void*)(&test_measurement);
		for (int i = 0; i < sizeof(test_measurement); i++)
		{
			outfile << ptr_to_test_data_bytes[i];
		}
		//outfile << endl;
		outfile.close(); //close the file.

		cout << endl <<"Received Data Stats:" << endl;
		cout << "data_ID is: " << data_ID << endl;
		cout << "data start location is: " << Data_Start_location << endl;
		cout << "data end location is: " << Data_End_Location << endl;
		cout << "Number of bytes in Payload is: " << no_of_bytes_in_payload << endl;
		cout << "Data Payload contents is: " << endl <<"|";
		for (int i = 0; i < no_of_bytes_in_payload; i++) {
			cout << (int)Data_Payload[i];
			cout << "|";
		}
		cout << endl;

	}
	else 
	{
		return 0;//Receive Failed Client Disconnected
	}
	
}
bool Send_Data_to_Client(const SOCKET &ClientSocket, const char *data_to_send)
{
	//Notify Client to Wait for Data
	int iResult = send(ClientSocket, data_to_send, strlen(data_to_send), 0);
	if (iResult == SOCKET_ERROR) {//If sending Failed
		wprintf(L"send failed with error: %d\n", WSAGetLastError());
		return 0;
	}
	else
	{	//If Sending Succeeded
		Sleep(500);
		return 1;
	}
}
bool Receive_Data_from_Client(const SOCKET &ClientSocket, char *received_data)
{
	int Result;

	Result = recv(ClientSocket, received_data, strlen(received_data), 0);
	if (Result > 0) {
		return 1; //Receive was a success
	}
	else if (Result == 0)
	{
		printf("Client Disconnected!\n"); //Client seems to have closed the connection
		return 0;
	}
	else {
		printf("recv failed with error: %d\n", WSAGetLastError());
		return 0;
	}
}
bool Check_if_new_data_for_client(void)
{
	//Perform Some Checks Here and Return 1: if new Data to send. Return 0 if not;
	return 0;
}

//Data Processing Functions
template <class T> void rebuild_received_data(BYTE *Data_Payload, const int &Num_Bytes_in_Payload, T& rebuilt_variable)
{
	BYTE *ptr_to_rebuilt_variable_bytes = (BYTE*)(void*)(&rebuilt_variable);
	for (int i = 0; i < Num_Bytes_in_Payload; i++) {
		ptr_to_rebuilt_variable_bytes[i] = Data_Payload[i];
	}
}
void process_received_power_measurement(BYTE *Data_Payload, int &no_of_bytes_in_payload)
{
	cout << "Processing Received Power Measurement..." << endl;
	power_measurement first_measurement;
	rebuild_received_data(Data_Payload, no_of_bytes_in_payload, first_measurement);
	cout << "Contents of First Measurement: " << endl
		<< "ID: " << first_measurement.ID << endl
		<< "Measurement: " << (float)first_measurement.measurement << endl
		<< "Taken On: " << (int)first_measurement.when_made.year << "/"
		<< (int)first_measurement.when_made.month << "/"
		<< (int)first_measurement.when_made.dayOfMonth << " at "
		<< (int)first_measurement.when_made.hour << ":"
		<< (int)first_measurement.when_made.minute << ":"
		<< (int)first_measurement.when_made.second << endl;
}
void process_received_command(BYTE *Data_Payload, int &no_of_bytes_in_payload)
{
	cout << "Processing received Command" << endl;
}
void process_received_status(BYTE *Data_Payload, int &no_of_bytes_in_payload)
{
	cout << "Processing received status" << endl;
}






