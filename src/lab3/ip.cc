#include "compiler.h"
#include "ip.hh"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
extern "C"
{
#include "system.h"
}
#include "iostream.hh"


IP::IP() {
	
	myIPAddress = new IPAddress(0x82, 0xEB, 0xC8, 0x74);
}

IP&
IP::instance()
{
	static IP myInstance;
	return myInstance;
}

const IPAddress&
IP::myAddress() {
	return *myIPAddress;
}

IPInPacket::IPInPacket(byte* theData, udword theLength, InPacket* theFrame):
	InPacket(theData, theLength, theFrame)
{
	sequence_number = 0;
}

void 
IPInPacket::decode()
{
	 
	IPHeader* ipin = (IPHeader*)myData;
	myProtocol = ipin->protocol;
	mySourceIPAddress = ipin->sourceIPAddress;
	uword frag = HILO(ipin->fragmentFlagsNOffset) & 0x3FFF;
	if (ipin->versionNHeaderLength == 0x45 &&
		frag == 0 &&
		ipin->destinationIPAddress == IP::instance().myAddress())
	{
		
		//IT IS A ICMPInPacket
		if ((myLength > 28) && (*(myData + 20) == 8))
		{

			ICMPInPacket icmpIn(myData + headerOffset(), HILO(ipin->totalLength) - headerOffset(), this);
			icmpIn.decode();

		}

		//IT IS A TCPInPacket
		else {

			//TODO in Lab4

		}

	}

}

void 
IPInPacket::answer(byte* theData, udword theLength)
{
	IPHeader* iphead = new IPHeader();
	iphead->versionNHeaderLength = 0x45;
	iphead->TypeOfService = 0;
	iphead->totalLength = HILO(theLength + headerOffset());
	sequence_number = sequence_number % 65536;
	iphead->identification = sequence_number;
	iphead->fragmentFlagsNOffset = 0;
	iphead->timeToLive = 64;
	iphead->protocol = myProtocol;
	iphead->headerChecksum = 0;
	iphead->sourceIPAddress = IP::instance().myAddress();
	iphead->destinationIPAddress = mySourceIPAddress;
	iphead->headerChecksum = calculateChecksum((byte*)iphead,
		headerOffset(), 0);
	
	++sequence_number;
	byte* sendPack = new byte[theLength + headerOffset()];
	memcpy(sendPack, iphead, headerOffset());
	memcpy(sendPack + headerOffset(), theData, theLength);

	myFrame->answer(sendPack, theLength + headerOffset());

	delete theData;
	delete iphead;
}

uword
IPInPacket::headerOffset()
{
	return IP::ipHeaderLength;
}

