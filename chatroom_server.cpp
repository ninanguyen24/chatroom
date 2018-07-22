/*
Nina Nguyen
June 14, 2018
CPSC 5042 Spring 2018
HW3
chatroom_server.cpp

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
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
using std::string;
using std::list;
using std::cout;
using std::endl;

const int CHAT_PORT=18888;
const unsigned int SOCKET_BUF_SIZE=256;
const unsigned int CLIENT_NAME_SIZE=16;
const unsigned int SOCKET_TIMEOUT=10;
const string TOKEN = "\\token";
const string PASS  = "\\pass";
const string MSGEND  = "\\end";
const string MSG_SEP = "\r\n";
const string WAIT_PROMPT = "[Waiting ...]";

class clientConn {
private:
    string c_name;
    int sock;
    list<string> toSend;
    char data[SOCKET_BUF_SIZE];
    
    void shutdown() {
        if (sock >= 0) {
            close(sock);
            sock = -1;
        }
    }
    
    
public:
    clientConn(int client_socket) :
      c_name(), sock(client_socket), toSend() {
          if (sock < 0) throw "Wrong client socket";
          
		  //setting timeout
          struct timeval timeout;
          timeout.tv_sec = SOCKET_TIMEOUT;
          timeout.tv_usec = 0;
          
          // Set socket timeouts
          if (setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                          sizeof(timeout)) < 0) {
              shutdown(); //close socket and mark socket as invalid
              return;
          }
          
          
          if (setsockopt (sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                          sizeof(timeout)) < 0) {
              shutdown();
          }
          
		  //trying to read the name of client
          unsigned long read_length = read(sock, data, CLIENT_NAME_SIZE);
          if (read_length <= MSG_SEP.length()) {
              shutdown();
              return;
          }
          
			//Take buffer data and build the string out of buffer data out of buffer length
          c_name.assign(data, read_length);
          if (c_name.substr(read_length - MSG_SEP.length(), MSG_SEP.length()).compare(MSG_SEP) != 0) {
              shutdown();
              return;
          }
          
          c_name=c_name.substr(0, read_length - MSG_SEP.length()); //strip the message
    }
    
	//destructor
    ~clientConn() {
        shutdown();
    }
    
	//getter for name
    const string & getName() const {
        return c_name;
    }
    
	//getter for socket state
    bool isValid() {
        return sock >=0;
    }
    
	//add datat to be sent for the client, for string
    void addData(const string & s) {
        toSend.push_back(s);
    }
    
	//add data to be sent for the client, for list
    void addData(const list<string> & ls) {
        toSend.insert(toSend.end(), ls.begin(), ls.end());
    }
    
	//looks for list to be sent one string after another
	//Every string sends using pointer to the raw string data
    void sendData() {
        while (isValid() && !toSend.empty()) {
            string sendString = toSend.front().append(MSG_SEP);
            toSend.pop_front();
            while (sendString.length() > 0) {
                long sentData=send(sock, sendString.c_str(), sendString.length(), 0);
                if (sentData < 0) {
                    shutdown();
                    return;
                }
				//if string was partially sent
                sendString = sendString.substr(sentData, sendString.length() - sentData); //remove part that was sent
            }
        }
    }
    
	//Build a list of strings that we read from client
    list<string> recieveData() {
        list<string> result;
        toSend.push_back(TOKEN); //Append token to data
        sendData(); //Send data to client
        string readBuffer;
        
		//Connection is valid
        while (isValid()) {
            long receivedLen = read(sock, data, SOCKET_BUF_SIZE); //try to read data
            if (receivedLen <= 0) {
                shutdown();
                return result;
            }
            string readString;
            readString.assign(data, receivedLen); //convert raw c style string to c++ string
            readBuffer.append(readString); //Append the new data to previous leftover string
            unsigned long sepPos;
            while ((sepPos=readBuffer.find(MSG_SEP)) != string::npos) { 
                string message = readBuffer.substr(0, sepPos);//extract message
                readBuffer=readBuffer.substr(sepPos + MSG_SEP.length(), readBuffer.length() - sepPos - MSG_SEP.length()); //remove extract part from accumulated string
                
                if (message.compare(MSGEND) == 0) { //Did get \end message
                    shutdown();
                    return result;
                } else if (message.compare(PASS) == 0) { //Did get "pass" message
                    result.clear();
                    return result;
                } else {
                    result.push_back(c_name + ": " + message); //build chat message
                }
                if (readBuffer.length()==0) { //Read entire message
                    return result;
                }
            }
        }
        return result; //list of strings
    }
};

class ChatServer {
private:
    list<clientConn*> clientSockets;
    int server_sock;
    struct sockaddr_in address;
    pthread_mutex_t cout_mutex, clientSocket_mutex;
    
    
    

    
public:
	//Close the socket
	//clean up the list of client sockets
    void server_stop() {
        if (server_sock>=0) {
            close(server_sock);
            server_sock = -1; //Indicate to the dataexhcnage thread that server is shutting down
        }
        pthread_mutex_lock(&clientSocket_mutex);
        for (auto it_conn=clientSockets.begin(); it_conn!=clientSockets.end(); it_conn++)
            delete *it_conn; //Deallocate the object that is pointed by pointer
        pthread_mutex_unlock(&clientSocket_mutex);
        
        pthread_mutex_destroy(&cout_mutex);
        pthread_mutex_destroy(&clientSocket_mutex); //destroy the mutexes
    }
    
	//Constructor
	//Inititialize the data member of the class
    ChatServer(int server_port) : clientSockets(), server_sock(-1) {
        
        if (pthread_mutex_init(&cout_mutex, NULL)!=0) { //Inititalize mutexes
            server_stop();
            return;
        }
        if (pthread_mutex_init(&clientSocket_mutex, NULL)!=0) { 
            server_stop();
            return;
        }
        //Initialization of the server socket
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock < 0) {
            server_stop();
            return;
        }
        
        memset(&address, '0', sizeof(address));
        address.sin_family=AF_INET;
        address.sin_port=htons(server_port);
        address.sin_addr.s_addr=INADDR_ANY;
        
        if (bind(server_sock, (struct sockaddr*)&address, sizeof(address)) < 0) {
            server_stop();
            return;
        }
        if (listen(server_sock, 3) < 0) {
            server_stop();
            return;
        }
        
    }
    
	//destructor
    ~ChatServer() {
        server_stop();
    }
    
	//lock the mutex for the console
	//print out string to console
    void printString(const string & s) {
        pthread_mutex_lock(&cout_mutex);
        cout << s << endl;
        pthread_mutex_unlock(&cout_mutex);
    }
    
	//Check to see if server is valid
	//valid server means valid socket
    bool isValid() {
        return server_sock>=0;
    }
    
	//creates new client socket for each client
	//Works like a fork
    void serveIncoming() {
        int client_socket;
        int addrlen = sizeof(address);
        printString(WAIT_PROMPT);
		//Keep listening while the server is valid
        while (isValid() && ((client_socket = accept(server_sock, (struct sockaddr*)&address, (socklen_t*)&addrlen))>=0)) {
            clientConn* new_client = new clientConn(client_socket); //allocate the clientConn class 
            if (new_client->isValid()) { //Check to see if client is valid
                pthread_mutex_lock(&clientSocket_mutex); //Lock client mutex socket
                clientSockets.push_back(new_client); //add new client to the client list
                string join_msg = new_client->getName() + " joined";
                pthread_mutex_unlock(&clientSocket_mutex);
                
                printString(join_msg); //Prints out client name on server console
                sendAllClients(join_msg); //Prints out to all clients
            } else {
                delete new_client; //If client is invalid, dealloacte the client
            }
            
            printString(WAIT_PROMPT); //Wait for new connection
        }
        server_stop(); //Stop the server
    }
    
	//Sends a single to all client
    void sendAllClients(const string & s) {
        pthread_mutex_lock(&clientSocket_mutex);
        for (auto it_conn=clientSockets.begin(); it_conn!=clientSockets.end(); it_conn++)
            (*it_conn)->addData(s);
        pthread_mutex_unlock(&clientSocket_mutex);
    }

	//Send a list of strings to all client
    void sendAllClients(const list<string> & ls) {
        pthread_mutex_lock(&clientSocket_mutex);
        for (auto it_conn=clientSockets.begin(); it_conn!=clientSockets.end(); it_conn++)
            (*it_conn)->addData(ls);
        pthread_mutex_unlock(&clientSocket_mutex);
    }
    
	//runs in the background for threads
	//Only launch the background thread if server is valid
    void pollConns() {
        while (isValid()) {
            pthread_mutex_lock(&clientSocket_mutex);
            auto conn_it = clientSockets.begin(); //Obtain the iterator for the client list
            bool list_end = conn_it==clientSockets.end(); //checks if new iterator is at the end of list (list empty)
            pthread_mutex_unlock(&clientSocket_mutex);
            
            if (list_end) { //iterate through client list
                sleep(1);
            } else {
            
                while (isValid() && !list_end) { //runs until the list is empty while the server is valid
                    auto curr_con = conn_it; //Store the iterator into a temp variable
                    pthread_mutex_lock(&clientSocket_mutex);
                    conn_it++; //advance iterator for the next loop of iteration (advance first)
                    list_end = conn_it==clientSockets.end(); 
                    pthread_mutex_unlock(&clientSocket_mutex);
                    
                    if ((*curr_con)->isValid()) { //Check to see if current client is valid
                        sendAllClients((*curr_con)->recieveData()); //Get data from client and send to everyone
                    } else {
                        pthread_mutex_lock(&clientSocket_mutex);
                        string left_string = (*curr_con)->getName() + " left"; 
                        delete *curr_con; //deallocate the pointer
                        clientSockets.erase(curr_con); //erase client from socket list
                        pthread_mutex_unlock(&clientSocket_mutex);
                        printString(left_string); //send leaving message
                        sendAllClients(left_string);
                    }
                }
                
            }
        }
    
    }
    
};

//Helper function
//call pollConnection function
void* pollclients(ChatServer *server_ptr)
{
    server_ptr->pollConns();
    return NULL;
}



int main() {
    ChatServer server(CHAT_PORT); //create server
    if (server.isValid()) { //check if server is listening to connection
        pthread_t dataexchange; //create thread that pass token from client to client
        if(pthread_create(&dataexchange, NULL, (void* (*)(void*))pollclients, &server)==0) {
            server.serveIncoming(); //goes into endless loop waiting for connection
            server.server_stop(); //Stops the server
            pthread_join(dataexchange, NULL); //Waits for the dataexchange thread to finish
        }
    }
    return 0;
}
