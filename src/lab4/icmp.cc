#include "compiler.h"
#include "icmp.hh"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
extern "C"
{
#include "system.h"
}
#include "iostream.hh"


ICMPInPacket::ICMPInPacket(byte* theData, udword theLength, InPacket* theFrame)
	:InPacket(theData, theLength, theFrame)
{

}

void 
ICMPInPacket::decode()
{
	ICMPHeader* icmphead = (ICMPHeader*)myData;
	icmphead->type = 0;

	uword oldSum = *(uword*)(myData + 2);
	uword newSum = oldSum + 0x8;
	icmphead->checksum = 0;
	icmphead->checksum = newSum;

	uword hoffs = headerOffset();
	byte* temp = new byte[myLength];
	byte* aReply = temp;
	memcpy(aReply, icmphead, hoffs);
	memcpy(aReply + hoffs, myData + hoffs, myLength - hoffs);

	answer(aReply, myLength);
	cout << "ICMP sent!" << endl;

	//delete myData;
	//delete icmphead;
	//delete temp;
}

void 
ICMPInPacket::answer(byte* theData, udword theLength)
{
	
	myFrame->answer(theData, theLength);
}

uword
ICMPInPacket::headerOffset()
{
	return icmpHeaderLen + icmpEchoHeaderLen;
}

