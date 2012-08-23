#ifndef _H_XMPP_CLIENT
#define _H_XMPP_CLIENT

#include <Base64.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <string.h>
#include <stdarg.h>
#include <avr/pgmspace.h>

enum XMPPState {
  INIT,
  AUTH,
  AUTH_STREAM,
  BIND,
  SESS,
  READY,
  WAIT
};

class XMPPClient : public EthernetClient {
    private:
	char *username;
	char *server;
	char *password;
	char *resource;
	XMPPState state;

	// variables to recive messages
	char recBuffer[600];
	char recMsg[100];


	int sendTemplate(const prog_char *strTemplate, int fillLen, ...);

	int openStream(char *server);
	int authenticate(char *username, char *password);
	int bindResource(char *resource);
	int openSession(char *server);

	void processInput();
	int stateAction();


    public:

    bool tryConnect;

	int xmppLogin(char *server, char *username, char *password, char *resource, byte *macAddress);

	int sendMessage(char *recipientJid, char *message);
	int sendPresence();
	
	char * receiveMessage();

	int close();

};

#endif /* _H_XMPP_CLIENT */
