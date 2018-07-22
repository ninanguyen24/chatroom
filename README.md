# chatroom
This program is a server and a client chatroom using sockets in C/C++. The goal is that if N clients are connected to the server, any message that is sent by any client is broadcasted to all the other clients.

- Each client has a name that is passed to the program as an input argument.
- We need to have a multi-thread program on the server side so that the server can listen for the new connection requests
- To join the chat room, it is enough to run the client (with one input argument as the name)
- To leave the chat room, the user should type: "\end" (with out " ")
- When a client joins, all the existing clients are informed
- When a client leaves, all the existing clients are informed
- Server screen shows three types of messages: "[Waiting ...]" to indicate that the server waits, and join and leave messages
- Clients are not allowed to send anything to the server until they receive a token: "\token"
- Token is sent to clients in a round-robin way by server
- When a client receives the token there are two possibilities:
  - Client has no message: in this case the client sends this message to the server: "\pass"
  - Client has a message: in this case the client sends its message to the server
- If user of a client types more than one message between two consecutive tokens, all of these messages are concatenated and will be sent as one single message to the server. Put "\n" between messages, so that when they are printed on the screen of other clients, they shown as separate messages

**Compile Instructions:**

g++ chatroom_server.cpp -lpthread -o server
g++ chatroom_client.cpp -lpthread -o client
.a/server
.a/client client1
.a/client client2
...


**Author:**
Nina Nguyen

**Acknowledgments:**
Seattle University CPSC 5042 Systems II Homework 3
