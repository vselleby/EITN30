/*!***************************************************************************
*!
*! FILE NAME  : http.hh
*!
*! DESCRIPTION:
*!
*!***************************************************************************/

#ifndef http_hh
#define http_hh


#include "tcpsocket.hh"
#include "threads.hh"
#include "fs.hh"
#include "job.hh"



class HTTPServer : public Job 
{
public:
	HTTPServer(TCPSocket* theSocket);

	void doit();

	char* findPathName(char* str);


	char* extractString(char* thePosition, udword theLength);

	udword contentLength(char* theData, udword theLength);

	char* decodeBase64(char* theEncodedString);

	char* decodeForm(char* theEncodedForm);

	char* findFileName(char* str, udword pathLen);

	bool authentication(char* header);

	bool parsePost(char* aData);

private:
	TCPSocket* mySocket;
	udword clen;
};




#endif
