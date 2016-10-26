#include <iostream>
#include <algorithm>
#include <set>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <sys/event.h>

#include <errno.h>
#include <string.h>

#include <set>
#include <map>

int set_nonblock(int fd) {
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}

void logstr(std::string line) {
    std::cout << line << std::endl;
}

int main(int argc, char **argv) {
    int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if(MasterSocket == -1) {
        std::cout << strerror(errno) << std::endl;
        return 1;
    }
    
    int enable = 1;
    if (setsockopt(MasterSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        std::cout << "setsockopt(SO_REUSEADDR) failed" << std::endl;
    
    struct sockaddr_in SockAddr;
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(3100);
    SockAddr.sin_addr.s_addr = INADDR_ANY;

    
    int Result = bind(MasterSocket, (struct sockaddr *)&SockAddr, sizeof(SockAddr));
    
    if(Result == -1) {
        std::cout << strerror(errno) << std::endl;
        return 1;
    }
    
    set_nonblock(MasterSocket);
    
    Result = listen(MasterSocket, SOMAXCONN);
    
    if(Result == -1) {
        std::cout << strerror(errno) << std::endl;
        return 1;
    }
    
    
    int KQueue = kqueue();
    
    struct kevent KEvent;
    bzero(&KEvent, sizeof(KEvent));
    
    EV_SET(&KEvent, MasterSocket, EVFILT_READ, EV_ADD, 0, 0, 0);
    kevent(KQueue, &KEvent, 1, NULL, 0, NULL);
    
    auto clients = std::set<int>();
    auto messages = std::map<int, std::string>();
    
    while(true) {
        bzero(&KEvent, sizeof(KEvent));
        kevent(KQueue, NULL, 0, &KEvent, 1, NULL);
        
        if(KEvent.filter == EVFILT_READ) {
            if(KEvent.ident == MasterSocket) {
                int SlaveSocket = accept(MasterSocket, 0, 0);
                set_nonblock(SlaveSocket);
                bzero(&KEvent, sizeof(KEvent));
                EV_SET(&KEvent, SlaveSocket, EVFILT_READ, EV_ADD, 0, 0, 0);
                kevent(KQueue, &KEvent, 1, NULL, 0, NULL);
                clients.insert(SlaveSocket);
                send(SlaveSocket, "Welcome\n", 8, MSG_HAVEMORE);
                logstr("accepted connection");
            }
            else {
                static char Buffer[1024];
                int RecvSize = recv(KEvent.ident, Buffer, 1024, MSG_HAVEMORE);
                if(RecvSize <= 0) {
                    close(KEvent.ident);
                    messages[KEvent.ident] = "";
                    logstr("connection terminated");
                }
                else {
                    messages[KEvent.ident] += std::string(Buffer).substr(0, RecvSize);
                    if (Buffer[RecvSize-1] == '\n') {
                        auto message = messages[KEvent.ident];
                        logstr(message.substr(0, message.length()-1));
                        for(auto client : clients) {
                            send(client, message.c_str(), message.length(), MSG_HAVEMORE);
                        }
                        messages[KEvent.ident] = "";
                    }
                }
            }
        }
    }
    
    return 0;
}
