/*!***************************************************************************
*!
*! FILE NAME  : llc.cc
*!
*! DESCRIPTION: LLC dummy
*!
*!***************************************************************************/

/****************** INCLUDE FILES SECTION ***********************************/

#include "compiler.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
extern "C"
{
#include "system.h"
}

#include "iostream.hh"
#include "ethernet.hh"
#include "llc.hh"

//#define D_LLC
#ifdef D_LLC
#define trace cout
#else
#define trace if(false) cout
#endif
/****************** LLC DEFINITION SECTION *************************/

//----------------------------------------------------------------------------
//
LLCInPacket::LLCInPacket(byte*           theData,
                         udword          theLength,
						 InPacket*       theFrame,
                         EthernetAddress theDestinationAddress,
                         EthernetAddress theSourceAddress,
                         uword           theTypeLen):
InPacket(theData, theLength, theFrame),
myDestinationAddress(theDestinationAddress),
mySourceAddress(theSourceAddress),
myTypeLen(theTypeLen)
{

}

//----------------------------------------------------------------------------
//
void
LLCInPacket::decode()
{
	//ARP HERE
	if (myDestinationAddress == EthernetAddress(0xff, 0xff, 0xff, 0xff, 0xff, 0xff))
	{
		if ((myTypeLen == 0x0806) /*&& (myLength > 28)*/)
		{
			ARPInPacket arpIn(myData, myLength, myFrame);
			arpIn.decode();
			
		}
	}

	//IP HERE
	else if (myDestinationAddress == Ethernet::instance().myAddress()) {
		
		if ((myTypeLen == 0x0800) /*&& (myLength > 28)*/)
		{
			IPInPacket ipIn(myData, myLength, myFrame);
			ipIn.decode();

		}
		
	}

}

//----------------------------------------------------------------------------
//
void
LLCInPacket::answer(byte *theData, udword theLength)
{
  myFrame->answer(theData, theLength);
}

uword
LLCInPacket::headerOffset()
{
  return myFrame->headerOffset() + 0;
}


InPacket*
LLCInPacket::copyAnswerChain()
{
	LLCInPacket* anAnswerPacket = new LLCInPacket(*this);
	anAnswerPacket->setNewFrame(myFrame->copyAnswerChain());
	return anAnswerPacket;
}

/****************** END OF FILE Ethernet.cc *************************************/

