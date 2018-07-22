/*
Nina Nguyen
June 14, 2018
CPSC 5042 Spring 2018
HW3
chatroom_client.cpp

In this assignment your job is to build a chat room. For this purpose, you should write a server and a client using sockets in C/C++. 
The goal is that if N clients are connected to the server, any message that is sent by any client is broadcasted to all the other clients.
1)	Each client has a name that is passed to the program as an input argument.
2)	We need to have a multi-thread program on the server side so that the server can listen for the new connection requests
3)	To join the chat room, it is enough to run the client (with one input argument as the name)
4)	To leave the chat room, the user should type: "\end" (with out " ")
5)	When a client joins, all the existing clients are informed
6)	When a client leaves, all the existing clients are informed
7)	Server screen shows three types of messages: "[Waiting ...]" to indicate that the server waits, and join and leave messages
8)	Clients are not allowed to send anything to the server until they receive a token: "\token"
9)	Token is sent to clients in a round-robin way by server
10)	When a client receives the token there are two possibilities:
a.	Client has no message: in this case the client sends this message to the server: "\pass"
b.	Client has a message: in this case the client sends its message to the server
11)	If user of a client types more than one message between two consecutive tokens, all of these messages are concatenated and will be 
sent as one single message to the server. Put "\n" between messages, so that when they are printed on the screen of other clients, 
they shown as separate messages

*/

#include <string>
#include <iostream>
#include <list>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
using std::string;
using std::list;
using std::cout;
using std::cin;
using std::getline;
using std::endl;


const string CHAT_HOST="127.0.0.1";
const int CHAT_PORT=18888;
const unsigned int SOCKET_BUF_SIZE=256;
const unsigned int SOCKET_TIMEOUT=10;
const string TOKEN = "\\token";
const string PASS  = "\\pass";
const string MSGEND  = "\\end";
const string MSG_SEP = "\r\n";


class ChatClient {
private:
    int sock;
    list<string> toSend;
    pthread_mutex_t cout_mutex, toSend_mutex;
    char data[SOCKET_BUF_SIZE];
    struct sockaddr_in serv_addr;
public:
    ChatClient(const string &host, int port, const string &c_name) :
    sock(-1), toSend() {
        if (pthread_mutex_init(&cout_mutex, NULL) !=0) {
            shutdown();
            return;
        }
        if (pthread_mutex_init(&toSend_mutex, NULL) !=0) {
            shutdown();
            return;
        }
        
        // create socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            shutdown();
            return;
        }
        
        // sets timeout for sockets
        struct timeval timeout;
        timeout.tv_sec = SOCKET_TIMEOUT;
        timeout.tv_usec = 0;
        
        // set socket timeouts,
        if (setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                        sizeof(timeout)) < 0) {
            shutdown();
            return;
        }
        
        // timeout for send
        if (setsockopt (sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                        sizeof(timeout)) < 0) {
            shutdown();
            return;
        }
        
        // sets up structure for conversion
        memset(&serv_addr, '0', sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        
        // converts address from text form to numerical form
        if (inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
            shutdown();
            return;
        }
        
        // connects with the server
        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            shutdown();
            return;
        }
        
        // send client name to server
        std::string tmp(c_name);
        tmp.append(MSG_SEP);
        if (send(sock, tmp.c_str(), tmp.length(), 0) != (ssize_t)tmp.length()) {
            shutdown();
            return;
        }

        
    }

    //Destructor
    ~ChatClient() {
        shutdown();
    }
    
	//close socket and set to -1 and destroy mutexes
    void shutdown() {
        if (sock >=0) {
            close(sock);
            sock=-1;
        }
        pthread_mutex_destroy(&cout_mutex);
        pthread_mutex_destroy(&toSend_mutex);
    }
    
	//check to see if socket is valid
    bool isValid() {
        return sock >=0;
    }
    
	//thread safe console out
    void printString(const string & s) {
        pthread_mutex_lock(&cout_mutex);
        cout << s << endl;
        pthread_mutex_unlock(&cout_mutex);
    }
    
	//thread safe function to append data to the output list
    void addToSend(const string & s) {
        pthread_mutex_lock(&toSend_mutex);
        toSend.push_back(s);
        pthread_mutex_unlock(&toSend_mutex);
    }
    
	//while valid, receive data
    void exchange() {
        while (isValid()) {
            recieveData();
        }
    }
    
    //using toSend buffer by creating a copy, cleans up the data
    void sendData() {
        pthread_mutex_lock(&toSend_mutex);
        list<string> toSendCopy(toSend);
        toSend.clear();
        pthread_mutex_unlock(&toSend_mutex);
        if (toSendCopy.empty()) //If there is no message
            toSendCopy.push_back(PASS);
        
		//trying to push string to server
        while (isValid() && !toSendCopy.empty()) {
            string sendString = toSendCopy.front().append(MSG_SEP);
            toSendCopy.pop_front();
            while (sendString.length() > 0) {
                long sentData=send(sock, sendString.c_str(), sendString.length(), 0);
                if (sentData < 0) {
                    shutdown();
                    return;
                }
                sendString = sendString.substr(sentData, sendString.length() - sentData);
            }
        }
    }
    
	//read data from server
    void recieveData() {
        list<string> result;
        string readBuffer;
        
        while (isValid()) {
            long receivedLen = read(sock, data, SOCKET_BUF_SIZE);
            
            if (receivedLen <= 0) {
                shutdown();
                return;
            }
            
            string readString;
            readString.assign(data, receivedLen);
            readBuffer.append(readString);
            unsigned long sepPos;
            while ((sepPos=readBuffer.find(MSG_SEP)) != string::npos) {
                string message = readBuffer.substr(0, sepPos);
                readBuffer=readBuffer.substr(sepPos + MSG_SEP.length(), readBuffer.length() - sepPos - MSG_SEP.length());
                
                if (message.compare(TOKEN) == 0) { //if token, send data
                    sendData();
                    return;
                } else {
                    printString(message); //print message to screen
                }
                if (readBuffer.length()==0) {
                    return;
                }
            }
        }
    }
    
    
};


void* dataexchange(ChatClient *client_ptr)
{
    client_ptr->exchange();
    return NULL;
}



int main(int argc, const char * argv[]) {
    if (argc!=2) return 0;
    string c_name(argv[1]); //client name
    ChatClient client(CHAT_HOST, CHAT_PORT, c_name); //initialize chat client
    if (client.isValid()) {
        pthread_t datathread;
        if(pthread_create(&datathread, NULL, (void* (*)(void*))dataexchange, &client)==0) {
            while (client.isValid() && !cin.eof()) { //read message
                string input_message;
                getline(cin, input_message);
                client.addToSend(input_message); //append message to be sent
            }
            client.shutdown();
            pthread_join(datathread, NULL);
        }
        
    }
}
