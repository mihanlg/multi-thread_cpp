#include <iostream>
#include <algorithm>
#include <set>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>

void logstr(std::string message) {
    std::cout << message << std::endl;
}

int main(int argc, char **argv) {
    int ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    std::set<int> SlaveSockets;
    SlaveSockets.insert(0);
    
    if(ClientSocket == -1) {
        std::cout << strerror(errno) << std::endl;
        return 1;
    }
    
    struct sockaddr_in SockAddr;
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(3100);
    SockAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    
    int Result = connect(ClientSocket, (struct sockaddr *)&SockAddr, sizeof(SockAddr));
    
    if(Result == -1) {
        std::cout << strerror(errno) << std::endl;
        return 1;
    }
    
    logstr("connected to the server");
    
    std::string message;
    
    while (true) {
        fd_set Set;
        FD_ZERO(&Set);
        FD_SET(ClientSocket, &Set);
        FD_SET(0, &Set);
        
        select(ClientSocket+1, &Set, NULL, NULL, NULL);
        //SEND
        if(FD_ISSET(0, &Set))
        {
            if (std::getline(std::cin, message)) {
                send(ClientSocket, (message + "\n").c_str(), message.length()+1, MSG_HAVEMORE);
            }
        }
        //RECEIVE
        if(FD_ISSET(ClientSocket, &Set))
        {
            static char Buffer[1024];
            int RecvSize = recv(ClientSocket, Buffer, 1024, MSG_HAVEMORE);
            if((RecvSize == 0) && (errno != EAGAIN)) {
                close(ClientSocket);
                break;
            }
            else if(RecvSize != 0) {
                logstr(std::string(Buffer).substr(0, RecvSize-1));
            }
        }
    }
    return 0;
}
