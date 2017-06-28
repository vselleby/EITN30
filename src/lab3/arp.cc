
#include "compiler.h"
#include "arp.hh"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
extern "C"
{
#include "system.h"
}
#include "iostream.hh"




ARPInPacket::ARPInPacket(byte* theData,
						udword theLength,
						InPacket* theFrame):
	InPacket(theData, theLength, theFrame)
{

}

void
ARPInPacket::decode()
{
	IPAddress myIPAddress = IP::instance().myAddress();
	ARPHeader* arphead = (ARPHeader*)myData;
	//ARPHeader* targethead = new ARPHeader();

	if (arphead->targetIPAddress == myIPAddress) {
		EthernetAddress ethsource = Ethernet::instance().myAddress();
		EthernetAddress ethtarget = arphead->senderEthAddress;
		IPAddress ipsource = arphead->targetIPAddress;
		IPAddress iptarget = arphead->senderIPAddress;
		arphead->op = HILO(0x0002);
		arphead->senderEthAddress = ethsource;
		arphead->senderIPAddress = ipsource;
		arphead->targetEthAddress = ethtarget;
		arphead->targetIPAddress = iptarget;
		
		uword hoffs = headerOffset();
		byte* temp = new byte[myLength];
		byte* aReply = temp;
		memcpy(aReply, arphead , hoffs);
		memcpy(aReply + hoffs, myData + hoffs, myLength - hoffs);

		answer(aReply, myLength);
		cout << "ARP sent!" << endl;

		delete myData;
		delete arphead;

	}	
}

void
ARPInPacket::answer(byte* theData, udword theLength)
{
	myFrame->answer(theData, theLength);
	
}

uword
ARPInPacket::headerOffset() 
{
	return ARPHeaderLength;
}

/*
ARPHeader::ARPHeader()
{
}
*/
