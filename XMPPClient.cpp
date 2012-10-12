#include "XMPPClient.h"

/****************/
/* XMPP STANZAS */
/***************/
static const prog_char PROGMEM open_stream_template[] = "<stream:stream " 
"xmlns='jabber:client' " 
"xmlns:stream='http://etherx.jabber.org/streams' " 
"to='%s' " 
"version='1.0'>";

static const prog_char PROGMEM plain_auth_template[] = "<auth " 
"xmlns='urn:ietf:params:xml:ns:xmpp-sasl' " 
"mechanism='PLAIN'>" 
"%s" 
"</auth>";

static const prog_char PROGMEM bind_template[] = "<iq " 
"type='set' " 
"id='bind_1'>" 
"<bind " 
"xmlns='urn:ietf:params:xml:ns:xmpp-bind'>" 
"<resource>%s</resource>" 
"</bind>" 
"</iq>";

static const prog_char PROGMEM session_request_template[] = "<iq " 
"to='%s' " 
"type='set' " 
"id='ard_sess'>" 
"<session " 
"xmlns='urn:ietf:params:xml:ns:xmpp-session' />" 
"</iq>";

static const prog_char PROGMEM presence_template[] = "<presence>" 
"<show/>" 
"</presence>";

static const prog_char PROGMEM message_template[] = "<message " 
"to='%s' " 
"xmlns='jabber:client' " 
"type='chat' " 
"id='msg' " 
"xml:lang='en'>" 
"<body>%s</body>" 
"</message>";
static const prog_char PROGMEM close_template[] = "<presence type='unavailable'/>"
"</stream:stream>";

/********************/
/* TRANSITION TABLE */
/********************/
struct XMPPTransitionTableEntry {
  XMPPState currentState;
  XMPPState nextState;
  char *keyword;
};

int connTableSize = 6;
XMPPTransitionTableEntry connTable[] = {{INIT, AUTH, "PLAIN"},
{AUTH, AUTH_STREAM, "success"},
{AUTH_STREAM, BIND, "bind"},
{BIND, SESS, "jid"},
{SESS, READY, "result"},
{READY, WAIT, ""},
{WAIT, WAIT, ""}};


/************************/
/* STRING COPY FUNCTION */
/************************/
void strcpyuntil(char *dest, char *src, char *end) {
	while(src != end) 
		*dest++ = *src++;
	*dest = '\0';
}


/*****************/
/* CLASS METHODS */
/*****************/

int XMPPClient::xmppLogin(char *server, char *username, char *password, char *resource, byte *macAddress) {
  int ret;
    // todo, make mac address optional
  boolean connected = false, error = false;
  this->username = username;
  this->server = server;
  this->resource = resource;
  this->password = password;


  // start the Ethernet connection
  if(Ethernet.begin(macAddress) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    return 0;
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);

  if(connect(server, 5222)) {
    Serial.print("Conneted to ");
    Serial.println(server);
    state = INIT;
  } else {
    Serial.print("Failed to connect to ");
    Serial.println(server);
  }

  while(!connected) {
    ret = stateAction();

    if (state == READY) {
      connected = true;
      continue;
    }

    processInput();
  }

  return 1;
}


int XMPPClient::openStream(char *server) {
  sendTemplate(open_stream_template, strlen(server), server);
}

int XMPPClient::authenticate(char *username, char *password) {
  int plainStringLen = strlen(username) + strlen(password) + 2;
  int encStringLen = base64_enc_len(plainStringLen);
  char plainString[plainStringLen];
  char encString[encStringLen];
  
  /* Set up our plain auth string. It's in the form:
   * "\0username\0password"
   * where \0 is the null character
   */
   memset(plainString, '\0', plainStringLen);
   memcpy(plainString + 1, username, strlen(username));
   memcpy(plainString + 2 + strlen(username), password, strlen(password));

  /* Encode to base64 */
   base64_encode(encString, plainString, plainStringLen);
   sendTemplate(plain_auth_template, encStringLen, encString);
 }


int XMPPClient::bindResource(char *resource) {
  sendTemplate(bind_template, strlen(resource), resource);
}

int XMPPClient::openSession(char *server) {
  // Serial.print("Trying to open session to: ");
  // Serial.println(server);
  sendTemplate(session_request_template, strlen(server), server);
}

int XMPPClient::sendMessage(char *recipientJid, char *message) {
  sendTemplate(message_template, strlen(recipientJid) + strlen(message), recipientJid, message);
}

int XMPPClient::sendPresence() {
  sendTemplate(presence_template, 0);
}


char * XMPPClient::receiveMessage() {
	int bufLen = 600;
	// int msgLen = 100;
	// char buffer[bufLen];
	//char recMsg[100] = "";
  int nChar = available();
  *recMsg = NULL;
  *recBuffer = NULL;

  if (nChar > 0 && nChar < bufLen) {
    // Serial.println("receiveMessage called");
		for(int i = 0 ; i < nChar; i++) {
			recBuffer[i] = read();
    }

		// Terminate the string
		recBuffer[nChar - 1] = '\0';

    if(!strlen(recBuffer)) {
 			//Ignore what we've read if it's an empty string
    }
    else {
 			// Check that what we received is a message
      char tag1[] = "<message";
      char buf2[9];
      strncpy(buf2, recBuffer, 8);
      buf2[8] = '\0';

      if (strcmp(tag1, buf2) != 0) {
 				// Ignore what we received
        Serial.println("Received something that is not a message");
        *recBuffer = NULL;
        *recMsg = NULL;
      }
      else {
        // Serial.print("The buffer:");
        // Serial.println(recBuffer);
 				// Parse the message
        char *startIndex = strstr(recBuffer, "<body>");
        if (startIndex != NULL) {
          Serial.println("Received a message");
          startIndex = startIndex + 6;
          char *ptrMsg;
          ptrMsg = recMsg;

          while (*startIndex != '<') {
            *ptrMsg = *startIndex;
            ptrMsg++;
            startIndex++;
          }

          *ptrMsg = '\0';
          //Serial.println(recMsg);
          *recBuffer = NULL;
          return recMsg;
        }
        else {
          *recBuffer = NULL;
          *recMsg = NULL;
          return NULL;
        }
      }
    }
  }
  return recMsg;
}

int XMPPClient::close() {
  sendTemplate(close_template, 0);
}

int XMPPClient::sendTemplate(const prog_char *temp_P, int fillLen, ...) {
  int tempLen = strlen_P(temp_P);
  char temp[tempLen];
  char buffer[tempLen + fillLen];
  va_list args;

  strcpy_P(temp, temp_P);

  va_start(args, fillLen);
  vsprintf(buffer, temp, args);
  write(buffer);

  return 1;
}

int XMPPClient::stateAction() {
 
  Serial.print("State = ");
  Serial.println(state);

  switch(state) {
    case INIT:
      openStream(server);
      break;
    case AUTH:
      authenticate(username, password);
      break;
    case AUTH_STREAM:
      openStream(server);
      break;
    case BIND:
      bindResource(resource);
      break;
    case SESS:
      openSession(server);
      break;
    case READY:
      return 1;
      break;
    case WAIT:
      break;
    default:
      return -1;
      break;
  }
  return 0;
}

void XMPPClient::processInput() {
  int bufLen = 8;
  char buffer[bufLen];
  int i = 0;
  memset(buffer, '\0', bufLen);
  boolean stateChanged = false;

  if(!connected()) {
    state = WAIT;
    Serial.println("Lost connection in processInput");
    return;
  }

  // TODO: This process is pretty inefficient and naively implemented
  // It might be an idea to rewrite it cleverer
  while(!stateChanged) {
    if(available()) {
      /* Push a character from the ethernet interface into the buffer */
      for(i = 0 ; i < bufLen; i++) {
        buffer[i] = buffer[i+1];
      }
      buffer[i] = read();
      
      
      /* Ignore what we've read if it's an empty string */
      if(!strlen(buffer)) {
        continue;
      } else {
        //Serial.println(buffer);
      }
      
      for(int i = 0; i < connTableSize; i++) {
        if(state == connTable[i].currentState && strstr(buffer,connTable[i].keyword)) {

          /*Serial.println(buffer);
          Serial.println(connTable[i].keyword);
          Serial.println((int)strstr(buffer, connTable[i].keyword)); 
          
          Serial.print(connTable[i].keyword);
          Serial.println(" seen, transitioning");*/
	  
          
          state = connTable[i].nextState;
          flush();
          stateChanged = true;
          break;
        }
      }
    } else {
      delay(10);
    }
  }
}
