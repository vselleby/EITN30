/*!***************************************************************************
*!
*! FILE NAME  : tcpsocket.cc
*!
*! DESCRIPTION: TCPSocket is the TCP protocols stacks interface to the
*!              application.
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
#include "tcpsocket.hh"

#define D_TCPSocket
#ifdef D_TCPSocket
#define trace cout
#else
#define trace if(false) cout
#endif

/****************** CLASS DEFINITION SECTION ********************************/

/*****************************************************************************
*%
*% CLASS NAME   : TCPSocket
*%
*% BASE CLASSES : None
*%
*% CLASS TYPE   : Singleton
*%
*% DESCRIPTION  : Handles the interace beetwen TCP and the application.
*%
*% SUBCLASSING  : None.
*%
*%***************************************************************************/

TCPSocket::TCPSocket(TCPConnection* theConnection) : 
	myConnection(theConnection),
	myReadSemaphore(Semaphore::createQueueSemaphore("Socket", 0)),
	myWriteSemaphore(Semaphore::createQueueSemaphore("Socket", 0))
{
	cout << "TCPSocket created" << endl;

}

byte*
TCPSocket::Read(udword& theLength)
{
	myReadSemaphore->wait(); // Wait for available data 
	theLength = myReadLength;
	byte* aData = myReadData;
	myReadLength = 0;
	myReadData = 0;
	return aData;
}

bool
TCPSocket::isEof()
{
	return eofFound;
}

void
TCPSocket::Write(byte* theData, udword theLength)
{
	myConnection->Send(theData, theLength);
	myWriteSemaphore->wait(); // Wait until the data is acknowledged 
}

void 
TCPSocket::Close()
{
	//EstablishedState::AppClose();
	myConnection->AppClose();

}

void
TCPSocket::socketDataReceived(byte* theData, udword theLength)
{
	myReadData = new byte[theLength];
	memcpy(myReadData, theData, theLength);
	myReadLength = theLength;
	myReadSemaphore->signal(); // Data is available 
}

void
TCPSocket::socketDataSent()
{
	myWriteSemaphore->signal(); // The data has been acknowledged 
}

void
TCPSocket::socketEof()
{
	eofFound = true;
	myReadSemaphore->signal();
}






/*****************************************************************************
*%
*% CLASS NAME : SimpleApplication
*%
*% BASE CLASSES : Job
*%
*% CLASS TYPE :
*%
*% DESCRIPTION : Example application
*%
*% SUBCLASSING : None
*%
*%***************************************************************************/

SimpleApplication::SimpleApplication(TCPSocket* theSocket) :
	mySocket(theSocket){}

void
SimpleApplication::doit()
{
	udword aLength;
	byte* aData;
	bool done = false;
	while (!done && !mySocket->isEof())
	{
		aData = mySocket->Read(aLength);
		if (aLength > 0)
		{
			mySocket->Write(aData, aLength);
			if ((char)*aData == 'q')
			{
				done = true;
			}
			else if ((char)*aData == 's')
			{
				udword theLen = 30 * 1460;
				writeAlot(theLen);

			}
			else if ((char)*aData == 'r')
			{
				udword theLen = 1000000;
				writeAlot(theLen);

			}
			delete aData;
		}
	}
	mySocket->Close();
}

void SimpleApplication::writeAlot(udword theLen)
{
	byte* secData;
	secData = new byte[theLen];
	byte character = 'A';
	int count = 0;

	for (int i = 0; i != (int)theLen; ++i)
	{
		secData[i] = character;
		++count;

		if (count == 1460)
		{
			character++;
			count = 0;

			if (character == 'Z' + 1)
			{
				character = 'A';
			}

		}

	}
	//(cout << "here we should answer" << endl;
	mySocket->Write(secData, theLen);
	delete secData;
}
