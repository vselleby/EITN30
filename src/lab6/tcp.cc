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


// -------- TCP -------- //
TCP::TCP()
{
  //trace << "TCP created." << endl;
}

bool
TCP::acceptConnection(uword portNo)
{
	return portNo == 80 || portNo == 7;
}

void
TCP::connectionEstablished(TCPConnection* theConnection)
{

	TCPSocket* aSocket = new TCPSocket(theConnection);
	// Create a new TCPSocket. 

	theConnection->registerSocket(aSocket);
	// Register the socket in the TCPConnection. 
	if (theConnection->serverPortNumber() == 7)
	{

		Job::schedule(new SimpleApplication(aSocket));
		// Create and start an application for the connection. 

	}
	else if (theConnection->serverPortNumber() == 80)
	{

		Job::schedule(new HTTPServer(aSocket));
		// Create and start an application for the connection. 
	}
	else {

		TCP::instance().deleteConnection(theConnection);
	}
}

TCP&
TCP::instance()
{
  static TCP myInstance;
  return myInstance;
}


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

void
TCP::deleteConnection(TCPConnection* theConnection)
{
  myConnectionList.Remove(theConnection);
  delete theConnection;
}

// -------- TCPConnection -------- //
TCPConnection::TCPConnection(IPAddress& theSourceAddress,
                             uword      theSourcePort,
                             uword      theDestinationPort,
                             InPacket*  theCreator):
        hisAddress(theSourceAddress),
        hisPort(theSourcePort),
        myPort(theDestinationPort)
{
 // trace << "TCP connection created" << endl;
  myTCPSender = new TCPSender(this, theCreator);
  myState = ListenState::instance();
  timer = new RetransmitTimer(this, Clock::seconds * 1);
  //myRST = new RSTTimer(this, Clock::seconds * 5);
  resetFlag = false;
}


TCPConnection::~TCPConnection()
{
  //trace << "TCP connection destroyed" << endl;
  delete mySocket;
  delete myTCPSender;
  delete timer;
 // delete myRST;
}


bool
TCPConnection::tryConnection(IPAddress& theSourceAddress,
                             uword      theSourcePort,
                             uword      theDestinationPort)
{
  return (theSourcePort      == hisPort   ) &&
         (theDestinationPort == myPort    ) &&
         (theSourceAddress   == hisAddress);
}

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
	if (mySocket)
	{
		mySocket->socketEof();
		mySocket->socketDataSent();
	}
	//if (this)
	myState->Kill(this);
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
	
	//myRST->cancel();
	myState->Acknowledge(this, theAcknowlegementNumber);
	//if (myPort == 80) myRST->start();
}

void TCPConnection::Send(byte*  theData,
	udword theLength)
{
	// Send outgoing data
	myState->Send(this, theData, theLength);

}


uword
TCPConnection::serverPortNumber()
{
	return myPort;
}

void
TCPConnection::registerSocket(TCPSocket* theSocket)
{
	mySocket = theSocket;
}



// -------- TCPState -------- //
void TCPState::Kill(TCPConnection* theConnection)
{
	// Handle an incoming RST segment, can also called in other error conditions
	//trace << "TCPState::Kill" << endl;
	
	TCP::instance().deleteConnection(theConnection);
}

//----------------------------------------------------------------------------
// TCPState contains dummies for all the operations, only the interesting ones
// gets overloaded by the various sub classes.
//----------------------------------------------------------------------------


// -------- ListenState -------- //
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
	if (TCP::instance().acceptConnection(theConnection->myPort))
	{
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

	}
	else
	{
		//cout << (int)theConnection->myPort << endl;
		trace << "send RST..." << endl;
		theConnection->sendNext = 0;
		// Send a segment with the RST flag set. 
		theConnection->myTCPSender->sendFlags(0x04);
		TCP::instance().deleteConnection(theConnection);
	}
}

// -------- SynRecvdSate -------- //
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

	TCP::instance().connectionEstablished(theConnection);
}

// -------- EstablishedState -------- //
EstablishedState*
EstablishedState::instance()
{
  static EstablishedState myInstance;
  return &myInstance;
}
void EstablishedState::Acknowledge(TCPConnection* theConnection,
	udword theAcknowledgementNumber)
{
	// should only be invoked from the state machine 
	//when all data are sent and acknowledged by the recipient
	theConnection->sentUnAcked = theAcknowledgementNumber;
	
	if (theAcknowledgementNumber == (theConnection->firstSeq + theConnection->queueLength)) /*Could be something else*/
	{
		theConnection->mySocket->TCPSocket::socketDataSent();
	}
	else if(theAcknowledgementNumber == theConnection->sentMaxSeq)
	{
		theConnection->timer->cancel();
		theConnection->myTCPSender->sendFromQueue();
	}
	else if(theConnection->sentMaxSeq != (theConnection->firstSeq + theConnection->queueLength))
	{
		theConnection->myTCPSender->sendFromQueue();
	}
}

void EstablishedState::Send(TCPConnection* theConnection,
	byte*  theData,
	udword theLength)
{
	//theConnection->myTCPSender->sendData(theData, theLength);
	//theConnection->sendNext += theLength;

	theConnection->transmitQueue = theData;
	theConnection->queueLength = theLength;
	//cout << "the KEBNGD: " << theLength << endl;
	theConnection->firstSeq = theConnection->sendNext;
	theConnection->myTCPSender->resetOffset();

	theConnection->myTCPSender->sendFromQueue();

}

void
EstablishedState::NetClose(TCPConnection* theConnection)
{
  theConnection->receiveNext += 1;

  // Send a segment with the ACK flags set. 
  theConnection->myTCPSender->sendFlags(0x10);
  theConnection->sendNext += 1; //Is this needed?
  // Go to NetClose wait state, inform application
  theConnection->myState = CloseWaitState::instance();


  // Normally the application would be notified next and nothing
  // happen until the application calls appClose on the connection.
  // Since we don't have an application we simply call appClose here instead.

  
}

void
EstablishedState::Receive(TCPConnection* theConnection,
                          udword theSynchronizationNumber,
                          byte*  theData,
                          udword theLength)
{
  

  if(theSynchronizationNumber + theLength >= theConnection->receiveNext)
  theConnection->receiveNext = theSynchronizationNumber + theLength;
  //cout << "syncnbr: " << theSynchronizationNumber << endl;
  //cout << "theLen: " << theLength << endl;
  //cout << "receiveNext: " << theConnection->receiveNext << endl;
 
  theConnection->mySocket->TCPSocket::socketDataReceived(theData, theLength); 
 
  
}

void
EstablishedState::AppClose(TCPConnection* theConnection)
{
	//cout << "Closing socket on portNo: " << theConnection->hisPort << endl;
	theConnection->sentUnAcked = theConnection->sendNext;

	theConnection->myTCPSender->sendFlags(0x11);
	theConnection->sendNext += 1;

	theConnection->myState = FinWait1State::instance();
	//Send FIN flag + ACK????????????????????
	theConnection->receiveNext += 1;

}

// -------- CloseWatiState -------- //
CloseWaitState*
CloseWaitState::instance()
{
  static CloseWaitState myInstance;
  return &myInstance;
}

void CloseWaitState::AppClose(TCPConnection* theConnection)
{
	//cout << "Simulating app close" << endl;
	//cout << "TCP connection closed" << endl;
	
	//Fix seq and ack nbr here
	//theConnection->sendNext += 1;
	theConnection->sentUnAcked = theConnection->sendNext;
	theConnection->myTCPSender->sendFlags(0x11);
	theConnection->sendNext += 1;
	theConnection->myState = LastAckState::instance();
	if (theConnection->mySocket) theConnection->mySocket->socketEof();

}

// -------- LastAckState -------- //
LastAckState*
LastAckState::instance()
{
  static LastAckState myInstance;
  return &myInstance;
}

void LastAckState::Acknowledge(TCPConnection* theConnection,
	udword theAcknowledgementNumber)
{
	//theConnection->sendNext = theAcknowledgementNumber;
	theConnection->sentUnAcked = theAcknowledgementNumber;
	
	//theConnection->mySocket->socketEof();

	theConnection->Kill();

}

// -------- FinWait1State -------- //
FinWait1State*
FinWait1State::instance()
{
	static FinWait1State myInstance;
	return &myInstance;
}

void FinWait1State::Acknowledge(TCPConnection* theConnection,
	udword theAcknowledgementNumber)
{
	
	
	theConnection->sendNext = theAcknowledgementNumber;
	theConnection->myState = FinWait2State::instance();
}

// -------- FinWait2State -------- //
FinWait2State*
FinWait2State::instance()
{
	static FinWait2State myInstance;
	return &myInstance;
}

void FinWait2State::NetClose(TCPConnection* theConnection)
{
	//theConnection->receiveNext += 1;
	// Send a segment with the ACK flags set. 
	//theConnection->sendNext = sentUnAcked;
	theConnection->myTCPSender->sendFlags(0x10);
	theConnection->sendNext += 1;
	//Time wait here?
	if(theConnection->mySocket) theConnection->mySocket->socketEof();


	theConnection->Kill();
}



// -------- TCPSender -------- //
TCPSender::TCPSender(TCPConnection* theConnection, 
                     InPacket*      theCreator):
        myConnection(theConnection),
        myAnswerChain(theCreator->copyAnswerChain()) // Copies InPacket chain!
{
	count = 0;
}


TCPSender::~TCPSender()
{
  myAnswerChain->deleteAnswerChain();
}

void TCPSender::sendFlags(byte theFlags)
{


	 if(!myConnection->resetFlag)
	{

		// Decide on the value of the length totalSegmentLength. 
		// Allocate a TCP segment. 
		uword totalSegmentLength = TCP::tcpHeaderLength;
		byte* anAnswer = new byte[totalSegmentLength];

		// Calculate the pseudo header checksum 
		TCPPseudoHeader* aPseudoHeader =
			new TCPPseudoHeader(myConnection->hisAddress,
				totalSegmentLength);
		uword pseudosum = aPseudoHeader->checksum();
		delete aPseudoHeader;

		// Create the TCP segment. 
		// Calculate the final checksum. 
		TCPHeader* aTCPHeader = (TCPHeader*)anAnswer;

		aTCPHeader->sourcePort = HILO(myConnection->myPort);
		aTCPHeader->destinationPort = HILO(myConnection->hisPort);
		aTCPHeader->sequenceNumber = LHILO(myConnection->sendNext);
		aTCPHeader->acknowledgementNumber = LHILO(myConnection->receiveNext);
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
		//if (theFlags == 0x11) cout << "Sending bullshit fin+ack to port: " << myConnection->hisPort << endl;
		myAnswerChain->answer(anAnswer, totalSegmentLength);

		// Deallocate the dynamic memory 
		delete anAnswer;
	}
}

void TCPSender::sendData(byte* theData, udword theLength)
{
	if (!myConnection->resetFlag)
	{
		uword totalSegmentLength = TCP::tcpHeaderLength + theLength;
		byte* anAnswer = new byte[totalSegmentLength];

		TCPPseudoHeader* aPseudoHeader = new TCPPseudoHeader(myConnection->hisAddress, totalSegmentLength);
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

		aTCPHeader->checksum = calculateChecksum(anAnswer, totalSegmentLength, pseudosum);

		//cout << "Sending packet with data to port: " << myConnection->hisPort << endl;

		myAnswerChain->answer(anAnswer, totalSegmentLength);

		delete anAnswer;

	}
}


void TCPSender::sendFromQueue()
{
    /* is this really needed?
	udword theWindowSize = myConnection->myWindowSize -
		(myConnection->sendNext - myConnection->sentUnAcked);
		*/
	while (myConnection->myWindowSize -
		(myConnection->sentMaxSeq - myConnection->sentUnAcked) >= 1460 && 
		(myConnection->sendNext < (myConnection->firstSeq + myConnection->queueLength)))
	{
		theOffset = myConnection->sendNext - myConnection->firstSeq;
		//cout << "sendNext: " << (int)myConnection->sendNext << endl;
		//cout << "should be the same at end: " << (int)myConnection->firstSeq + myConnection->queueLength << endl;
		if (myConnection->queueLength < 1460)
		{
			sendData(myConnection->transmitQueue, myConnection->queueLength);
			myConnection->sendNext = myConnection->firstSeq + myConnection->queueLength;
			myConnection->timer->start();
		}
		else
		{
			theFirst = myConnection->transmitQueue + theOffset;
			if ((myConnection->queueLength - theOffset) > 1460) {
				theSendLength = 1460;
				//cout << "theLen: " << (myConnection->queueLength - theOffset) << endl;
			}
			else
			{
				theSendLength = (myConnection->queueLength - theOffset);
				//cout << "sendLEN2: " << theSendLength << endl;
			}

			//theOffset += theSendLength;
			sendData(theFirst, theSendLength);
			myConnection->timer->start();
			myConnection->sendNext += theSendLength;
			
		}

		if (myConnection->sendNext > myConnection->sentMaxSeq) {
			myConnection->sentMaxSeq = myConnection->sendNext;
		}
	}


}

void TCPSender::resetOffset()
{
	theOffset = 0;
}


// -------- TCPInPacket -------- //
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
	// Extract the parameters from the TCP header which define the 
	// connection. 
	TCPHeader* aTCPHeader = (TCPHeader*)myData;
	mySourcePort = HILO(aTCPHeader->sourcePort);
	myDestinationPort = HILO(aTCPHeader->destinationPort);
	mySequenceNumber = LHILO(aTCPHeader->sequenceNumber);
	myAcknowledgementNumber = LHILO(aTCPHeader->acknowledgementNumber);

	TCPConnection* aConnection =
		TCP::instance().getConnection(mySourceAddress,
			mySourcePort,
			myDestinationPort);

	//Set myWindowSize to that suggested in the TCP header.
	aConnection->myWindowSize = HILO(aTCPHeader->windowSize);

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

		//FIN FLAG RECEIVED
		if ((aTCPHeader->flags & 0x01) != 0 && aConnection->myState == EstablishedState::instance())
		{
			aConnection->NetClose();

		}
		//FIN + ACK RECEIVED
		else if (aTCPHeader->flags == 0x11)
		{
			aConnection->Acknowledge(myAcknowledgementNumber);
			aConnection->NetClose();
		}
		else if (aTCPHeader->flags == 0x18)
		{
			aConnection->Receive(mySequenceNumber, myData + TCP::tcpHeaderLength, myLength - TCP::tcpHeaderLength);
		}
		else if (aTCPHeader->flags == 0x14)
		{
			aConnection->resetFlag = true;
			aConnection->mySocket->socketEof();
			aConnection->mySocket->socketDataSent();
			//aConnection->Kill();
		}
		else if (aTCPHeader->flags == 0x10)
		{
			if (myLength - TCP::tcpHeaderLength != 0)
			{
				aConnection->Receive(mySequenceNumber, myData + TCP::tcpHeaderLength, myLength - TCP::tcpHeaderLength);
			}
			else
			{
				aConnection->Acknowledge(myAcknowledgementNumber);
			}
		}
	}
}


void TCPInPacket::answer(byte* theData, udword theLength)
{
	myFrame->answer(theData, theLength);
}

uword TCPInPacket::headerOffset()
{
	return TCP::tcpHeaderLength;
}


InPacket*
TCPInPacket::copyAnswerChain()
{  
  return myFrame->copyAnswerChain();
}

// -------- TCPPseudoHeader -------- //
TCPPseudoHeader::TCPPseudoHeader(IPAddress& theDestination,
                                 uword theLength):
        sourceIPAddress(IP::instance().myAddress()),
        destinationIPAddress(theDestination),
        zero(0),
        protocol(6)
{
  tcpLength = HILO(theLength);
}


uword
TCPPseudoHeader::checksum()
{
  return calculateChecksum((byte*)this, 12);
}


// -------- RetransmitTimer -------- //
RetransmitTimer::RetransmitTimer(TCPConnection* theConnection,
	Duration retransmitTime) : myConnection(theConnection), myRetransmitTime(retransmitTime){}

void
RetransmitTimer::start()
{
	this->timeOutAfter(myRetransmitTime);
}

void 
RetransmitTimer::cancel()
{
	this->resetTimeOut();
}

void
RetransmitTimer::timeOut()
{
	myConnection->sendNext = myConnection->sentUnAcked; 
	myConnection->myTCPSender->sendFromQueue();
}



//SEND RST INSTEAD OF CRASHING
/*
RSTTimer::RSTTimer(TCPConnection* theConnection,
	Duration time) : myConnection(theConnection), myRSTTime(time)
{}

void
RSTTimer::start()
{
	this->timeOutAfter(myRSTTime);
}

void
RSTTimer::cancel()
{
	this->resetTimeOut();
}

void
RSTTimer::timeOut()
{
	myConnection->myTCPSender->sendFlags(0x14);
	myConnection->Kill();
}

*/

/****************** END OF FILE tcp.cc *************************************/

