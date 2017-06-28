/*!***************************************************************************
*!
*! FILE NAME  : http.cc
*!
*! DESCRIPTION: HTTP, Hyper text transfer protocol.
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
#include "tcpsocket.hh"
#include "http.hh"

//#define D_HTTP
#ifdef D_HTTP
#define trace cout
#else
#define trace if(false) cout
#endif

/****************** HTTPServer DEFINITION SECTION ***************************/

//----------------------------------------------------------------------------
//

HTTPServer::HTTPServer(TCPSocket* theSocket) : mySocket(theSocket){
	
	//cout << "HTTPServer created" << endl;
}

void HTTPServer::doit()
{
	udword aLength;
	byte* aData;
	bool done = false;
	byte *responseData;
	while (!done && !mySocket->isEof())
	{

		aData = mySocket->Read(aLength);

		if (aLength > 0)
		{
		
			bool foundGET = false;
			bool foundPOST = false;
			bool foundHEAD = false;
			const static char *getString = "GET ";
			const static char *postString = "POST ";
			const static char *headString = "HEAD ";

			//check what type of HTTP header
			if (strncmp((char*)aData, getString, strlen(getString)) == 0) {
				foundGET = true;
			}
			if (strncmp((char*)aData, postString, strlen(postString)) == 0) {
				foundPOST = true;
			}
			if (strncmp((char*)aData, headString, strlen(headString)) == 0) {
				foundHEAD = true;
			}


			char *header = new char[aLength + 1];
			strncpy(header, (char *)aData, aLength);
			//cout << "header: " << header << endl;

		

			if (foundGET || foundHEAD) {

				char *filePath = findPathName(header);
				udword pathLen = strlen(filePath);

				char *fileName = findFileName(header, pathLen);
				
				responseData = FileSystem::instance().readFile(filePath, fileName, aLength);

				if (responseData == NULL) {
					//cout << "no file found" << endl;
					char *http404error = "HTTP/1.0 404 Not found\r\nContent-type: text/html\r\n\r\n<html><head><title>File not found</title></head><body><h1>404 Not found</h1></body></html>";
					mySocket->Write((byte *)http404error, (udword) strlen(http404error));
				
					delete fileName;
					delete filePath;
					//delete header;
			//		delete responseData;
				//	delete aData;
					
					break;
					
				}
				

				//Authentication
				if (strstr(filePath, "private") != NULL && strstr(fileName, "private.htm") != NULL)
				{

					//cout << "Needs authentication!" << endl;
					if (authentication(header))
					{

						char* response = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n";
						mySocket->Write((byte *)response, (udword)strlen(response));

						if (foundGET) {
							mySocket->Write((byte *)responseData, aLength);
						}

					}
					else
					{
						//cout << "Unauthorized!" << endl;
						char *http401unauthorized = "HTTP/1.0 401 Unauthorized\r\nContent-Type: text/html\r\nWWW-Authenticate: Basic realm=\"private\"\r\n\r\n<html><head><title>401 Unauthorized</title></head><body><h1>401 Unauthorized</h1></body></html>";
						mySocket->Write((byte *)http401unauthorized, (udword)strlen(http401unauthorized));
					}

					
					delete fileName;
					delete filePath;
					//delete header;
				//	delete responseData;
				//	delete aData;
					
					break;
					

				}


				else
				{
					//No authentication needed
					
					char* builtResponse = "";

					if (strstr(fileName, ".htm") != NULL)
					{
						builtResponse = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n";
					}
					else if (strstr(fileName, ".gif") != NULL)
					{
						builtResponse = "HTTP/1.0 200 OK\r\nContent-type: image/gif\r\n\r\n";
					}
					else if (strstr(fileName, ".jpg") != NULL)
					{
						builtResponse = "HTTP/1.0 200 OK\r\nContent-type: image/jpeg\r\n\r\n";
					}
					if (!mySocket->isEof()) {
						mySocket->Write((byte *)builtResponse, (udword)strlen(builtResponse));
						//cout << "writing header to port: " << mySocket->getPort() << endl;
						if (foundGET) {
							//cout << "writing data to port: " << mySocket->getPort() << endl;
							mySocket->Write((byte *)responseData, aLength);
						}
						delete fileName;
						delete filePath;
						//delete header;
					//	delete aData;
				//		delete responseData;
						
						break;
					}
					
				}
			}

			if (foundPOST)
			{
				
				clen = contentLength(header, aLength);
				udword received = 0;
				byte* postData = strstr(header, "\r\n\r\n");
				cout << "header" << header << endl;
				//postData += 4;
				bool dataInFirst = false;
				udword actContLength = strlen(postData);
				if (postData && actContLength > 4)
				{
					postData += 4;
					dataInFirst = true;
					received = actContLength - 4;
					
				}
				if (dataInFirst && clen == received)
				{
					if (parsePost((char*)postData)) {
						char *postResp = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n<html><head><title>Accepted</title></head><body><h1>The file dynamic.htm was updated successfully.</h1></body></html>";
						mySocket->Write((byte*)postResp, strlen(postResp));
						
						//delete header;
						//delete aData;
						delete postData;
						
						break;
					}
					
				}
				else {
					byte* sendData = new byte[clen];
					if (dataInFirst && received < clen)
					{
						memcpy(sendData, postData, received);
					}
					while (clen > received)
					{
						cout << "REC: " << received << " Cont: " << clen << endl;
						udword tempLen;
						byte* tempData = mySocket->Read(tempLen);
						memcpy(sendData + received, tempData, tempLen);
						received += tempLen;
						cout << "REC: " << received << " Cont: " << clen << endl;
						delete tempData;
					}
					char* toPost = extractString((char*)sendData, clen);
					if (parsePost(toPost)) {
						char *postResp = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n<html><head><title>Accepted</title></head><body><h1>The file dynamic.htm was updated successfully.</h1></body></html>";
						mySocket->Write((byte*)postResp, strlen(postResp));
						
						delete postData;
						//delete sendData;
						//delete toPost;
						//delete header;
						//delete aData;
						
						break;
					}
				
					
				}
				
			}
			
			
			}


	// done = true;	
	}
	
	mySocket->Close();
	delete responseData;
	delete aData;
}


bool HTTPServer::parsePost(char* aData)
{
	char* decoded = decodeForm(aData);
	if (FileSystem::instance().writeFile((byte*)decoded, strlen(decoded) + 1))
	{
		//delete decoded;
		return true;
	}
	return false;
}


char*
HTTPServer::findPathName(char* str)
{
	char* firstPos = strchr(str, ' ');     // First space on line 
	firstPos++;                            // Pointer to first / 
	char* lastPos = strchr(firstPos, ' '); // Last space on line 
	char* thePath = 0;                     // Result path 
	if ((lastPos - firstPos) == 1)
	{
		// Is / only 
		thePath = 0;                         // Return NULL 
	}
	else
	{
		// Is an absolute path. Skip first /. 
		thePath = extractString((char*)(firstPos + 1),
			lastPos - firstPos);

		if ((lastPos = strrchr(thePath, '/')) != 0)
		{
			// Found a path. Insert -1 as terminator. 
			*lastPos = '\xff';
			*(lastPos + 1) = '\0';
			while ((firstPos = strchr(thePath, '/')) != 0)
			{
				// Insert -1 as separator. 
				*firstPos = '\xff';
			}
		}
		else
		{
			// Is /index.html 
			delete thePath; thePath = 0;  // Return NULL 
		}
	}

	return thePath;
}


char* HTTPServer::findFileName(char* str, udword pathLen) {

	char* firstPos = strchr(str, '/') + pathLen;
	firstPos++;
	char* lastPos = strchr(firstPos, ' ');
	char* fileName;

	fileName = extractString((char*)(firstPos), lastPos - firstPos);
	if (strlen(fileName) == 0 && pathLen == 0) {
		fileName = "index.htm";
	}
	
	return fileName;
	
}


bool HTTPServer::authentication(char* header)
{
	const static char *user = "admin:123456";


	//ev ändra..
	char *indexOfAuth = strstr(header, "Authorization: Basic ");
	if (indexOfAuth != NULL)
	{
		indexOfAuth += 21;
		char *indexOfBR = strstr(indexOfAuth, "\r\n");
		char *encodedAuth = extractString(indexOfAuth, (int)indexOfBR - (int)indexOfAuth);
		char *decodedAuth = decodeBase64(encodedAuth);
		delete encodedAuth;
		bool ret = strcmp(decodedAuth, user) == 0;
		delete decodedAuth;
		return ret;



	}
	
	return false;

}



// Allocates a new null terminated string containing a copy of the data at
// 'thePosition', 'theLength' characters long. The string must be deleted by
// the caller.
//
char*
HTTPServer::extractString(char* thePosition, udword theLength)
{
  char* aString = new char[theLength + 1];
  strncpy(aString, thePosition, theLength);
  aString[theLength] = '\0';
  return aString;
}

//----------------------------------------------------------------------------
//
// Will look for the 'Content-Length' field in the request header and convert
// the length to a udword
// theData is a pointer to the request. theLength is the total length of the
// request.
//
udword
HTTPServer::contentLength(char* theData, udword theLength)
{
  udword index = 0;
  bool   lenFound = false;
  const char* aSearchString = "Content-Length: ";
  while ((index++ < theLength) && !lenFound)
  {
    lenFound = (strncmp(theData + index,
                        aSearchString,
                        strlen(aSearchString)) == 0);
  }
  if (!lenFound)
  {
    return 0;
  }
  trace << "Found Content-Length!" << endl;
  index += strlen(aSearchString) - 1;
  char* lenStart = theData + index;
  char* lenEnd = strchr(theData + index, '\r');
  char* lenString = this->extractString(lenStart, lenEnd - lenStart);
  udword contLen = atoi(lenString);
  trace << "lenString: " << lenString << " is len: " << contLen << endl;
  delete [] lenString;
  return contLen;
}

//----------------------------------------------------------------------------
//
// Decode user and password for basic authentication.
// returns a decoded string that must be deleted by the caller.
//
char*
HTTPServer::decodeBase64(char* theEncodedString)
{
  static const char* someValidCharacters =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
  
  int aCharsToDecode;
  int k = 0;
  char  aTmpStorage[4];
  int aValue;
  char* aResult = new char[80];

  // Original code by JH, found on the net years later (!).
  // Modify on your own risk. 
  
  for (unsigned int i = 0; i < strlen(theEncodedString); i += 4)
  {
    aValue = 0;
    aCharsToDecode = 3;
    if (theEncodedString[i+2] == '=')
    {
      aCharsToDecode = 1;
    }
    else if (theEncodedString[i+3] == '=')
    {
      aCharsToDecode = 2;
    }

    for (int j = 0; j <= aCharsToDecode; j++)
    {
      int aDecodedValue;
      aDecodedValue = strchr(someValidCharacters,theEncodedString[i+j])
        - someValidCharacters;
      aDecodedValue <<= ((3-j)*6);
      aValue += aDecodedValue;
    }
    for (int jj = 2; jj >= 0; jj--) 
    {
      aTmpStorage[jj] = aValue & 255;
      aValue >>= 8;
    }
    aResult[k++] = aTmpStorage[0];
    aResult[k++] = aTmpStorage[1];
    aResult[k++] = aTmpStorage[2];
  }
  aResult[k] = 0; // zero terminate string

  return aResult;  
}

//------------------------------------------------------------------------
//
// Decode the URL encoded data submitted in a POST.
//
char*
HTTPServer::decodeForm(char* theEncodedForm)
{
  char* anEncodedFile = strchr(theEncodedForm,'=');
  anEncodedFile++;
  char* aForm = new char[strlen(anEncodedFile) * 2]; 
  // Serious overkill, but what the heck, we've got plenty of memory here!
  udword aSourceIndex = 0;
  udword aDestIndex = 0;
  
  while (aSourceIndex < strlen(anEncodedFile))
  {
    char aChar = *(anEncodedFile + aSourceIndex++);
    switch (aChar)
    {
     case '&':
       *(aForm + aDestIndex++) = '\r';
       *(aForm + aDestIndex++) = '\n';
       break;
     case '+':
       *(aForm + aDestIndex++) = ' ';
       break;
     case '%':
       char aTemp[5];
       aTemp[0] = '0';
       aTemp[1] = 'x';
       aTemp[2] = *(anEncodedFile + aSourceIndex++);
       aTemp[3] = *(anEncodedFile + aSourceIndex++);
       aTemp[4] = '\0';
       udword anUdword;
       anUdword = strtoul((char*)&aTemp,0,0);
       *(aForm + aDestIndex++) = (char)anUdword;
       break;
     default:
       *(aForm + aDestIndex++) = aChar;
       break;
    }
  }
  *(aForm + aDestIndex++) = '\0';
  return aForm;
}




/************** END OF FILE http.cc *************************************/
