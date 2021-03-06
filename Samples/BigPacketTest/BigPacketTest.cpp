/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant 
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include <iostream>
#include <fstream>
#include <sstream>

#include "RakPeerInterface.h"
#include "BitStream.h"
#include <stdlib.h> // For atoi
#include <cstring> // For strlen
#include "RakNetStatistics.h"
#include "GetTime.h"
#include "MessageIdentifiers.h"
#include "MTUSize.h"
#include <stdio.h>
#include "Kbhit.h"
#include "RakSleep.h"
#include "Gets.h"

bool quit;
bool sentPacket=false;

#define BIG_PACKET_SIZE 83886080
const int GB = 1073741824;
//83886080
//167772160
//41943040

using namespace RakNet;

RakPeerInterface *client, *server;
char *text;
int totalDatarecived = 0;
int clientPacketLoss = 0;
float serverPacketLoss = 0;


int main(void)
{
	std::ofstream file;

	client=server=0;

	text= new char [BIG_PACKET_SIZE];
	quit=false;
	char ch;

	printf("This is a test I use to test the packet splitting capabilities of RakNet\n");
	printf("All it does is send a large block of data to the feedback loop\n");
	printf("Difficulty: Beginner\n\n");

	printf("Enter 's' to run as server, 'c' to run as client, space to run local.\n");
	ch=' ';
	Gets(text,BIG_PACKET_SIZE);
	ch=text[0];

	if (ch=='c')
	{
		client=RakNet::RakPeerInterface::GetInstance();
		printf("Working as client\n");
		printf("Enter remote IP: ");
		Gets(text,BIG_PACKET_SIZE);
		if (text[0]==0)
			strcpy(text, "natpunch.jenkinssoftware.com"); // dx in Europe
	}
	else if (ch=='s')
	{
		server=RakNet::RakPeerInterface::GetInstance();
		printf("Working as server\n");
	}
	else
	{
		client=RakNet::RakPeerInterface::GetInstance();
		server=RakNet::RakPeerInterface::GetInstance();;
		strcpy(text, "127.0.0.1");
	}

	// Test IPV6
	int socketFamily;
	socketFamily=AF_INET;
	//socketFamily=AF_INET6;

	if (server)
	{
		server->SetTimeoutTime(5000,RakNet::UNASSIGNED_SYSTEM_ADDRESS);
		RakNet::SocketDescriptor socketDescriptor(3000,0);
		socketDescriptor.socketFamily=socketFamily;
		server->SetMaximumIncomingConnections(4);
		StartupResult sr;
		sr=server->Startup(4, &socketDescriptor, 1);
		if (sr!=RAKNET_STARTED)
		{
			printf("Server failed to start. Error=%i\n", sr);
			return 1;
		}
		// server->SetPerConnectionOutgoingBandwidthLimit(40000);

		printf("Started server on %s\n", server->GetMyBoundAddress().ToString(true));
	}
	if (client)
	{
		client->SetTimeoutTime(5000,RakNet::UNASSIGNED_SYSTEM_ADDRESS);
		RakNet::SocketDescriptor socketDescriptor(0,0);
		socketDescriptor.socketFamily=socketFamily;
		StartupResult sr;
		sr=client->Startup(4, &socketDescriptor, 1);
		if (sr!=RAKNET_STARTED)
		{
			printf("Client failed to start. Error=%i\n", sr);
			return 1;
		}
		client->SetSplitMessageProgressInterval(10000); // Get ID_DOWNLOAD_PROGRESS notifications
		//	client->SetPerConnectionOutgoingBandwidthLimit(28800);

		printf("Started client on %s\n", client->GetMyBoundAddress().ToString(true));

		client->Connect(text, 3000, 0, 0);
	}
	RakSleep(500);

	printf("My IP addresses:\n");
	RakPeerInterface *rakPeer;
	if (server)
		rakPeer=server;
	else
		rakPeer=client;
	unsigned int i;
	for (i=0; i < rakPeer->GetNumberOfAddresses(); i++)
	{
		printf("%i. %s\n", i+1, rakPeer->GetLocalIP(i));
	}


	// Always apply the network simulator on two systems, never just one, with half the values on each.
	// Otherwise the flow control gets confused.
// 	if (client)
// 		client->ApplyNetworkSimulator(.01, 0, 0);
// 	if (server)
// 		server->ApplyNetworkSimulator(.01, 0, 0);

	RakNet::TimeMS start,stop;

	RakNet::TimeMS nextStatTime = RakNet::GetTimeMS() + 1000;
	RakNet::Packet *packet;
	start=RakNet::GetTimeMS();
	while (!quit)
	{
		if (server)
		{
			for (packet = server->Receive(); packet; server->DeallocatePacket(packet), packet=server->Receive())
			{
				if (packet->data[0]==ID_NEW_INCOMING_CONNECTION || packet->data[0]==253)
				{
					printf("Starting send\n");
					start=RakNet::GetTimeMS();
					
					/*
					if (BIG_PACKET_SIZE<=100000)
					{
						for (int i=0; i < BIG_PACKET_SIZE; i++)
							text[i]=255-(i&255);
					}
					else
						text[0]=(unsigned char) 255;
					*/

					text[0] = (unsigned char)255;
					int nrofpackets = (GB / BIG_PACKET_SIZE) + 1;
					for (int i = 0; i < nrofpackets; i++)
					{
						server->Send(text, BIG_PACKET_SIZE, HIGH_PRIORITY, RELIABLE_ORDERED_WITH_ACK_RECEIPT, 0, packet->systemAddress, false);
					}
					
					// Keep the stat from updating until the messages move to the thread or it quits right away
					nextStatTime=RakNet::GetTimeMS()+1000;
				}

				if (packet->data[0] == 252)
				{
					//Recived message that all data has arrived to the client
					//Send over total data resent
					printf("Recived 252 Packet\n");
					char * message = new char[5];	//int + 1
					message[0] = (unsigned char)251;
					message[1] = (unsigned char)serverPacketLoss;
					server->Send(message, sizeof(message), LOW_PRIORITY, RELIABLE_ORDERED_WITH_ACK_RECEIPT, 0, packet->systemAddress, false);
					printf("Sent 251 Packet\n");
				}

				if (packet->data[0]==ID_CONNECTION_LOST)
					printf("ID_CONNECTION_LOST from %s\n", packet->systemAddress.ToString());
				else if (packet->data[0]==ID_DISCONNECTION_NOTIFICATION)
					printf("ID_DISCONNECTION_NOTIFICATION from %s\n", packet->systemAddress.ToString());
				else if (packet->data[0]==ID_NEW_INCOMING_CONNECTION)
					printf("ID_NEW_INCOMING_CONNECTION from %s\n", packet->systemAddress.ToString());
				else if (packet->data[0]==ID_CONNECTION_REQUEST_ACCEPTED)
					printf("ID_CONNECTION_REQUEST_ACCEPTED from %s\n", packet->systemAddress.ToString());
			}

			if (kbhit())
			{
				char ch=getch();
				if (ch==' ')
				{
					printf("Sending medium priority message\n");
					char t[1];
					t[0]=(unsigned char) 254;
					server->Send(t, 1, MEDIUM_PRIORITY, RELIABLE_ORDERED, 1, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
				}
				if (ch=='q')
					quit=true;
			}
		}
		if (client)
		{
			packet = client->Receive();
			while (packet)
			{
				if (packet->data[0]==ID_DOWNLOAD_PROGRESS)	//For each packet that are sent
				{
					//RakNet::BitStream progressBS(packet->data, packet->length, false);
					//progressBS.IgnoreBits(8); // ID_DOWNLOAD_PROGRESS
					//unsigned int progress;
					//unsigned int total;
					//unsigned int partLength;

					//// Disable endian swapping on reading this, as it's generated locally in ReliabilityLayer.cpp
					//progressBS.ReadBits( (unsigned char* ) &progress, BYTES_TO_BITS(sizeof(progress)), true );
					//progressBS.ReadBits( (unsigned char* ) &total, BYTES_TO_BITS(sizeof(total)), true );
					//progressBS.ReadBits( (unsigned char* ) &partLength, BYTES_TO_BITS(sizeof(partLength)), true );
					//
					//
					//printf("Progress: msgID=%i Progress %i/%i Partsize=%i TotalData:%d\n",
					//	(unsigned char) packet->data[0],
					//	progress,
					//	total,
					//	partLength,
					//	totalDatarecived);
					
				}
				else if (packet->data[0]==255)
				{
					if (packet->length!=BIG_PACKET_SIZE)
					{
						printf("Test failed. %i bytes (wrong number of bytes).\n", packet->length);
						quit=true;
						break;
					}

					/*
					if (BIG_PACKET_SIZE<=100000)
					{
						for (int i=0; i < BIG_PACKET_SIZE; i++)
						{
							if  (packet->data[i]!=255-(i&255))
							{
								printf("Test failed. %i bytes (bad data).\n", packet->length);
								quit=true;
								break;
							}
						}
					}
					*/

					if (!quit)
					{
						printf("Test succeeded. %i bytes.\n", packet->length);
						totalDatarecived += (int)packet->length;
						bool repeat=false;
						if (repeat)
						{
							printf("Rerequesting send.\n");
							unsigned char ch=(unsigned char) 253;
							client->Send((const char*) &ch, 1, MEDIUM_PRIORITY, RELIABLE_ORDERED, 1, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
						}
						else
						{
							if (totalDatarecived >= GB)
							{
								unsigned char ch = (unsigned char)252;	//Transfer Completed
								client->Send((const char*)&ch, 1, MEDIUM_PRIORITY, RELIABLE_ORDERED, 1, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
								stop = RakNet::GetTimeMS();

							}
							
						}
					}

				}
				else if (packet->data[0]==254)
				{
 					printf("Got high priority message.\n");
				}
				else if (packet->data[0] == 251)
				{
					printf("Recived 251 Packet\n");
					memcpy(&serverPacketLoss, &packet->data[1], sizeof(float));
					quit = true;
					break;
				}
				else if (packet->data[0]==ID_CONNECTION_LOST)
					printf("ID_CONNECTION_LOST from %s\n", packet->systemAddress.ToString());
				else if (packet->data[0]==ID_DISCONNECTION_NOTIFICATION)
					printf("ID_DISCONNECTION_NOTIFICATION from %s\n", packet->systemAddress.ToString());
				else if (packet->data[0]==ID_NEW_INCOMING_CONNECTION)
					printf("ID_NEW_INCOMING_CONNECTION from %s\n", packet->systemAddress.ToString());
				else if (packet->data[0]==ID_CONNECTION_REQUEST_ACCEPTED)
				{
					start=RakNet::GetTimeMS();
					printf("ID_CONNECTION_REQUEST_ACCEPTED from %s\n", packet->systemAddress.ToString());
				}
				else if (packet->data[0]==ID_CONNECTION_ATTEMPT_FAILED)
					printf("ID_CONNECTION_ATTEMPT_FAILED from %s\n", packet->systemAddress.ToString());

				client->DeallocatePacket(packet);
				packet = client->Receive();
			}
		}

		//if (RakNet::GetTimeMS() > nextStatTime)
		//{
		//	nextStatTime=RakNet::GetTimeMS()+1000;
		//	RakNetStatistics rssSender;
		//	RakNetStatistics rssReceiver;
		//	if (server)
		//	{
		//		unsigned int i;
		//		unsigned short numSystems;
		//		server->GetConnectionList(0,&numSystems);
		//		if (numSystems>0)
		//		{
		//			for (i=0; i < numSystems; i++)
		//			{
		//				server->GetStatistics(server->GetSystemAddressFromIndex(i), &rssSender);
		//				StatisticsToString(&rssSender, text,2);
		//				printf("==== System %i ====\n", i+1);
		//				printf("%s\n\n", text);

		//			}
		//		}
		//	}
		//	if (client && server==0 && client->GetGUIDFromIndex(0)!=UNASSIGNED_RAKNET_GUID)
		//	{
		//		client->GetStatistics(client->GetSystemAddressFromIndex(0), &rssReceiver);
		//		StatisticsToString(&rssReceiver, text,2);
		//		printf("%s\n\n", text);
		//	}
		//}

		RakSleep(100);
	}
	
	double ms = (double)(stop-start);

	if (server)
	{
		RakNetStatistics *rssSender2=server->GetStatistics(server->GetSystemAddressFromIndex(0));
		StatisticsToString(rssSender2, text, 1);
		printf("%s", text);
	}

	std::string filename = "RaknetLog";
	//filename.append((char*)BIG_PACKET_SIZE);

	file.open(filename + ".tsv");
	file << filename << "\n";
	file << "Packet Size: " << BIG_PACKET_SIZE << " B\n";
	file << "Time (ms)	Loss\n";
	file << ms << "	" << serverPacketLoss << "\n";
	file.close();

	printf("%i bytes per ms (%.2f ms), Average Packet Loss: %d. Press enter to quit\n", (int)((double)(BIG_PACKET_SIZE) / ms), ms, serverPacketLoss) ;
	Gets(text,BIG_PACKET_SIZE);



	delete []text;
	RakNet::RakPeerInterface::DestroyInstance(client);
	RakNet::RakPeerInterface::DestroyInstance(server);

	return 0;
}
