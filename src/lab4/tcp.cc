/*!***************************************************************************
*!
*! FILE NAME  : tcp.cc
*!
*! DESCRIPTION: TCP, Transport control protocol
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
#include "timr.h"
}

#include "iostream.hh"
#include "tcp.hh"
#include "ip.hh"

#define D_TCP
#ifdef D_TCP
#define trace cout
#else
#define trace if(false) cout
#endif
/****************** TCP DEFINITION SECTION *************************/

//----------------------------------------------------------------------------
//
TCP::TCP()
{
  trace << "TCP created." << endl;
}

//----------------------------------------------------------------------------
//
TCP&
TCP::instance()
{
  static TCP myInstance;
  return myInstance;
}

//----------------------------------------------------------------------------
//
TCPConnection*
TCP::getConnection(IPAddress& theSourceAddress,
                   uword      theSourcePort,
                   uword      theDestinationPort)
{
  TCPConnection* aConnection = NULL;
  // Find among open connections
  uword queueLength = myConnectionList.Length();
  myConnectionList.ResetIterator();
  bool connectionFound = false;
  while ((queueLength-- > 0) && !connectionFound)
  {
    aConnection = myConnectionList.Next();
    connectionFound = aConnection->tryConnection(theSourceAddress,
                                                 theSourcePort,
                                                 theDestinationPort);
  }
  if (!connectionFound)
  {
    //trace << "Connection not found!" << endl;
    aConnection = NULL;
  }
  else
  {
    //trace << "Found connection in queue" << endl;
  }
  return aConnection;
}

//----------------------------------------------------------------------------
//
TCPConnection*
TCP::createConnection(IPAddress& theSourceAddress,
                      uword      theSourcePort,
                      uword      theDestinationPort,
                      InPacket*  theCreator)
{
  TCPConnection* aConnection =  new TCPConnection(theSourceAddress,
                                                  theSourcePort,
                                                  theDestinationPort,
                                                  theCreator);
  myConnectionList.Append(aConnection);
  return aConnection;
}

//----------------------------------------------------------------------------
//
void
TCP::deleteConnection(TCPConnection* theConnection)
{
  myConnectionList.Remove(theConnection);
  delete theConnection;
}

//----------------------------------------------------------------------------
//
TCPConnection::TCPConnection(IPAddress& theSourceAddress,
                             uword      theSourcePort,
                             uword      theDestinationPort,
                             InPacket*  theCreator):
        hisAddress(theSourceAddress),
        hisPort(theSourcePort),
        myPort(theDestinationPort)
{
  trace << "TCP connection created" << endl;
  myTCPSender = new TCPSender(this, theCreator),
  myState = ListenState::instance();
}

//----------------------------------------------------------------------------
//
TCPConnection::~TCPConnection()
{
  trace << "TCP connection destroyed" << endl;
  delete myTCPSender;
}

//----------------------------------------------------------------------------
//
bool
TCPConnection::tryConnection(IPAddress& theSourceAddress,
                             uword      theSourcePort,
                             uword      theDestinationPort)
{
  return (theSourcePort      == hisPort   ) &&
         (theDestinationPort == myPort    ) &&
         (theSourceAddress   == hisAddress);
}


// TCPConnection cont...


void TCPConnection::Synchronize(udword theSynchronizationNumber)
{
	// Handle an incoming SYN segment
	myState->Synchronize(this, theSynchronizationNumber);
}


void TCPConnection::NetClose()
{
	// Handle an incoming FIN segment
	myState->NetClose(this);
}

void TCPConnection::AppClose()
{
	// Handle close from application
	myState->AppClose(this);
	
}

void TCPConnection::Kill()
{
	// Handle an incoming RST segment, can also called in other error conditions
}

void TCPConnection::Receive(udword theSynchronizationNumber,
	byte*  theData,
	udword theLength)
{
	// Handle incoming data
	myState->Receive(this, theSynchronizationNumber, theData, theLength);
}

void TCPConnection::Acknowledge(udword theAcknowlegementNumber)
{
	// Handle incoming Acknowledgement
	myState->Acknowledge(this, theAcknowlegementNumber);
}

void TCPConnection::Send(byte*  theData,
	udword theLength)
{
	// Send outgoing data
	myState->Send(this, theData, theLength);
}



//----------------------------------------------------------------------------
// TCPState contains dummies for all the operations, only the interesting ones
// gets overloaded by the various sub classes.
//----------------------------------------------------------------------------

void TCPState::Kill(TCPConnection* theConnection)
{
	// Handle an incoming RST segment, can also called in other error conditions
	trace << "TCPState::Kill" << endl;
	TCP::instance().deleteConnection(theConnection);
}

//----------------------------------------------------------------------------
//
ListenState*
ListenState::instance()
{
  static ListenState myInstance;
  return &myInstance;
}

void
ListenState::Synchronize(TCPConnection* theConnection,
	udword theSynchronizationNumber)
{
	switch (theConnection->myPort)
	{
	case 7:
		//trace << "got SYN on ECHO port" << endl;
		theConnection->receiveNext = theSynchronizationNumber + 1;
		theConnection->receiveWindow = 8 * 1024;
		theConnection->sendNext = get_time();
		// Next reply to be sent. 
		theConnection->sentUnAcked = theConnection->sendNext;
		
		// Send a segment with the SYN and ACK flags set. 
		theConnection->myTCPSender->sendFlags(0x12);
		// Prepare for the next send operation. 
		
		theConnection->sendNext += 1;
		// Change state 

		theConnection->myState = SynRecvdState::instance();
		break;
	default:
		cout << (int)theConnection->myPort << endl;
		trace << "send RST..." << endl;
		theConnection->sendNext = 0;
		// Send a segment with the RST flag set. 
		theConnection->myTCPSender->sendFlags(0x04);
		TCP::instance().deleteConnection(theConnection);
		break;
	}
}




//----------------------------------------------------------------------------
//
SynRecvdState*
SynRecvdState::instance()
{
  static SynRecvdState myInstance;
  return &myInstance;
}

void SynRecvdState::Acknowledge(TCPConnection* theConnection,
	udword theAcknowledgementNumber)
{
	theConnection->sentUnAcked = theAcknowledgementNumber;

	//Perhaps do stuff with recievewindow here

	theConnection->myState = EstablishedState::instance();


}





//----------------------------------------------------------------------------
//
EstablishedState*
EstablishedState::instance()
{
  static EstablishedState myInstance;
  return &myInstance;
}
void EstablishedState::Acknowledge(TCPConnection* theConnection,
	udword theAcknowledgementNumber)
{

}
void EstablishedState::Send(TCPConnection* theConnection,
	byte*  theData,
	udword theLength)
{
	theConnection->myTCPSender->sendData(theData, theLength);
	theConnection->sendNext += theLength;
}

//----------------------------------------------------------------------------
//
void
EstablishedState::NetClose(TCPConnection* theConnection)
{
  //trace << "EstablishedState::NetClose" << endl;


  theConnection->receiveNext += 1;

  // Send a segment with the ACK flags set. 
  theConnection->myTCPSender->sendFlags(0x10);

  // Go to NetClose wait state, inform application
  theConnection->myState = CloseWaitState::instance();
  
  // Normally the application would be notified next and nothing
  // happen until the application calls appClose on the connection.
  // Since we don't have an application we simply call appClose here instead.

  // Simulate application Close...
  theConnection->AppClose();
}

//----------------------------------------------------------------------------
//
void
EstablishedState::Receive(TCPConnection* theConnection,
                          udword theSynchronizationNumber,
                          byte*  theData,
                          udword theLength)
{
  //trace << "EstablishedState::Receive" << endl;

  // Delayed ACK is not implemented, simply acknowledge the data
  // by sending an ACK segment, then echo the data using Send.
  theConnection->receiveNext = theSynchronizationNumber + theLength;
  theConnection->Send(theData, theLength);

}


//----------------------------------------------------------------------------
//
CloseWaitState*
CloseWaitState::instance()
{
  static CloseWaitState myInstance;
  return &myInstance;
}

void CloseWaitState::AppClose(TCPConnection* theConnection)
{
	//cout << "Simulating app close" << endl;
	cout << "TCP connection closed" << endl;
	
	//Fix seq and ack nbr here
	//theConnection->sendNext += 1;
	theConnection->myTCPSender->sendFlags(0x01);

	theConnection->myState = LastAckState::instance();

}


//----------------------------------------------------------------------------
//
LastAckState*
LastAckState::instance()
{
  static LastAckState myInstance;
  return &myInstance;
}

void LastAckState::Acknowledge(TCPConnection* theConnection,
	udword theAcknowledgementNumber)
{
	theConnection->sentUnAcked = theAcknowledgementNumber;
	theConnection->Kill();
}

//----------------------------------------------------------------------------
//
TCPSender::TCPSender(TCPConnection* theConnection, 
                     InPacket*      theCreator):
        myConnection(theConnection),
        myAnswerChain(theCreator->copyAnswerChain()) // Copies InPacket chain!
{
}

//----------------------------------------------------------------------------
//
TCPSender::~TCPSender()
{
  myAnswerChain->deleteAnswerChain();
}


void TCPSender::sendFlags(byte theFlags)
{
	// Decide on the value of the length totalSegmentLength. 
	// Allocate a TCP segment. 
	//cout << "1" << endl;
	uword totalSegmentLength = TCP::tcpHeaderLength;
	byte* anAnswer = new byte[totalSegmentLength];
	//cout << "2" << endl;
	// Calculate the pseudo header checksum 
	TCPPseudoHeader* aPseudoHeader =
		new TCPPseudoHeader(myConnection->hisAddress,
			totalSegmentLength);
	uword pseudosum = aPseudoHeader->checksum();
	delete aPseudoHeader;
	//cout << "3" << endl;
	// Create the TCP segment. 
	// Calculate the final checksum. 
	TCPHeader* aTCPHeader = (TCPHeader*)anAnswer;

	aTCPHeader->sourcePort = HILO(myConnection->myPort);
	aTCPHeader->destinationPort = HILO(myConnection->hisPort);
	aTCPHeader->sequenceNumber = LHILO(myConnection->sendNext);
	aTCPHeader->acknowledgementNumber =LHILO(myConnection->receiveNext);
	aTCPHeader->headerLength = 0x50; //5 words of 32 bits (headerLength = 4 bitar)
	aTCPHeader->flags = theFlags; 
	aTCPHeader->windowSize = HILO(myConnection->receiveWindow);
	aTCPHeader->checksum = 0;
	aTCPHeader->urgentPointer = 0;
	aTCPHeader->checksum = calculateChecksum(anAnswer,
		totalSegmentLength,
		pseudosum);
	
	//cout << "segmentlen: " << (int)totalSegmentLength << endl;
	
	// Send the TCP segment. 
	myAnswerChain->answer(anAnswer, totalSegmentLength);
	//cout << "5" << endl;
	// Deallocate the dynamic memory 
	delete anAnswer;

}

void TCPSender::sendData(byte* theData, udword theLength)
{
	uword totalSegmentLength = TCP::tcpHeaderLength + theLength;
	byte* anAnswer = new byte[totalSegmentLength];

	TCPPseudoHeader* aPseudoHeader =
		new TCPPseudoHeader(myConnection->hisAddress,
			totalSegmentLength);
	uword pseudosum = aPseudoHeader->checksum();
	delete aPseudoHeader;

	TCPHeader* aTCPHeader = (TCPHeader*)anAnswer;

	aTCPHeader->sourcePort = HILO(myConnection->myPort);
	aTCPHeader->destinationPort = HILO(myConnection->hisPort);
	aTCPHeader->sequenceNumber = LHILO(myConnection->sendNext);
	aTCPHeader->acknowledgementNumber = LHILO(myConnection->receiveNext);
	aTCPHeader->headerLength = 0x50; //5 words of 32 bits
	aTCPHeader->flags = 0x10; //ACK flag
	aTCPHeader->windowSize = HILO(myConnection->receiveWindow);
	aTCPHeader->checksum = 0;
	aTCPHeader->urgentPointer = 0;

	memcpy(anAnswer + TCP::tcpHeaderLength, theData, theLength);

	aTCPHeader->checksum = calculateChecksum(anAnswer,
		totalSegmentLength,
		pseudosum);

	

	myAnswerChain->answer(anAnswer, totalSegmentLength);

	delete anAnswer;


}


//----------------------------------------------------------------------------
//
TCPInPacket::TCPInPacket(byte*           theData,
                         udword          theLength,
                         InPacket*       theFrame,
                         IPAddress&      theSourceAddress):
        InPacket(theData, theLength, theFrame),
        mySourceAddress(theSourceAddress)
{
}

void TCPInPacket::decode()
{
	//cout << "New TCPInPacket" << endl;

	TCPHeader* aTCPHeader = (TCPHeader*)myData;
	mySourcePort = HILO(aTCPHeader->sourcePort);
	myDestinationPort = HILO(aTCPHeader->destinationPort);
	mySequenceNumber = LHILO(aTCPHeader->sequenceNumber);
	myAcknowledgementNumber = LHILO(aTCPHeader->acknowledgementNumber);


	// Extract the parameters from the TCP header which define the 
	// connection. 
	TCPConnection* aConnection =
		TCP::instance().getConnection(mySourceAddress,
			mySourcePort,
			myDestinationPort);

	if (!aConnection)
	{
		// Establish a new connection. 
		aConnection =
			TCP::instance().createConnection(mySourceAddress,
				mySourcePort,
				myDestinationPort,
				this);
		//SYN FLAG
		if ((aTCPHeader->flags & 0x02) != 0)
		{
			// State LISTEN. Received a SYN flag. 
			aConnection->Synchronize(mySequenceNumber);
		}
		else
		{
			// State LISTEN. No SYN flag. Impossible to continue. 
			aConnection->Kill();
		}
	}
	else
	{
		// Connection was established. Handle all states.
		//FIN FLAG
		if ((aTCPHeader->flags & 0x01) != 0 && aConnection->receiveNext == mySequenceNumber)
		{

			aConnection->NetClose();
		}


		
		//PSH FLAG
		else if (myLength > TCP::tcpHeaderLength) 
		{
			if ((aTCPHeader->flags & 0x08) != 0)
				//aConnection->Acknowledge(myAcknowledgementNumber);
				aConnection->Receive(mySequenceNumber, myData + TCP::tcpHeaderLength, myLength - TCP::tcpHeaderLength);
		}

		// ACK FLAG
		else if ((aTCPHeader->flags & 0x10) != 0)
		{
			aConnection->Acknowledge(myAcknowledgementNumber);
		}
		

		
	

	}
}



void TCPInPacket::answer(byte* theData, udword theLength)
{
	myFrame->answer(theData, theLength);
}

uword TCPInPacket::headerOffset()
{
	return /*myFrame->headerOffset() + */TCP::tcpHeaderLength;
}

//----------------------------------------------------------------------------
//
InPacket*
TCPInPacket::copyAnswerChain()
{  
  return myFrame->copyAnswerChain();
}

//----------------------------------------------------------------------------
//
TCPPseudoHeader::TCPPseudoHeader(IPAddress& theDestination,
                                 uword theLength):
        sourceIPAddress(IP::instance().myAddress()),
        destinationIPAddress(theDestination),
        zero(0),
        protocol(6)
{
  tcpLength = HILO(theLength);
}

//----------------------------------------------------------------------------
//
uword
TCPPseudoHeader::checksum()
{
  return calculateChecksum((byte*)this, 12);
}


/****************** END OF FILE tcp.cc *************************************/

