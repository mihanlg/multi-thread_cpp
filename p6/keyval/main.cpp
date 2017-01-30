#include <iostream>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <sys/socket.h>
#include <sys/mman.h>
#include <wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <fcntl.h>
#include <semaphore.h>

#define KEY_LEN 32
#define VAL_LEN 256
#define BUF_LEN (KEY_LEN + VAL_LEN)
#define N_CHILDREN 4
#define BLOCK_FOR_SEM 10

#define SEM_NAME_TL "/sem%05d%04d"
#define MEM_NAME_TL "/mem%05d"

static const int p_socket = 0;
static const int c_socket = 1;

static void sendfd(int socket, int fd) {
    struct msghdr msg = {0};
    char buf[CMSG_SPACE(sizeof(fd))];
    memset(buf, 0, sizeof(buf));
    struct iovec io;
    io.iov_base = (char *) "AC";
    io.iov_len = 2;
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    *((int *) CMSG_DATA(cmsg)) = fd;
    msg.msg_controllen = cmsg->cmsg_len;
    if (sendmsg(socket, &msg, 0) < 0) {
        std::cerr << "sendmsg() error" << std::endl;
        _exit(1);
    }
}

static int recvfd(int socket) {
    struct msghdr msg = {0};
    char m_buffer[256];
    struct iovec io = { .iov_base = m_buffer, .iov_len = sizeof(m_buffer) };
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    char c_buffer[256];
    msg.msg_control = c_buffer;
    msg.msg_controllen = sizeof(c_buffer);
    if (recvmsg(socket, &msg, 0) < 0) {
        std::cerr << "recvmsg() error" << std::endl;
        _exit(1);
    }
    struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);
    unsigned char * data = CMSG_DATA(cmsg);
    int fd = *((int*) data);
    return fd;
}

int count_sems(size_t size) {
    return std::max(0, static_cast<int>(size) - 1)/BLOCK_FOR_SEM + 1;
}

int get_sem_id(size_t pos) {
    static_cast<int>(pos/BLOCK_FOR_SEM);
}

std::vector<sem_t *> semload(int sem_n, int prefix) {
    std::vector<sem_t *> sems;
    sem_t *sem;
    char buf[14];
    for (int i = 0; i < sem_n; i++) {
        sprintf(buf, SEM_NAME_TL, prefix, i);
        if((sem = sem_open(buf, 0)) == SEM_FAILED) {
            std::cerr << "sem_open() error" << std::endl;
            _exit(1);
        }
        sems.push_back(sem);
    }
    return sems;
}

char *shmalloc(size_t size, int prefix) {
    char buf_mem[10];
    sprintf(buf_mem, MEM_NAME_TL, prefix);
    int shmid;
    if((shmid = shm_open(buf_mem, O_RDWR, 0666)) == -1) {
        std::cerr << getpid() << ": shm_open() error" << std::endl;
        _exit(1);
    }
    size_t size_mem = size*BUF_LEN;
    /*if(ftruncate(shmid, size_mem) == -1) {
        std::cerr << getpid() << ": ftruncate() error" << std::endl;
        _exit(1);
    }*/
    char *mem = (char *)mmap(NULL, size_mem, PROT_READ|PROT_WRITE, MAP_SHARED, shmid, 0);
    if(mem == MAP_FAILED) {
        std::cerr << getpid() << ": mmap() error" << std::endl;
        _exit(1);
    }
    return mem;
}

class HashTable {
public:
    HashTable(size_t size) : size_(size) {
        prefix_ = getppid();
        table_ = shmalloc(size_, prefix_);
        sems_ = semload(count_sems(size_), prefix_);
    }
    HashTable(const HashTable & ht) {
        std::cout << "HT(constHT&)" << std::endl;
        prefix_ = ht.prefix_;
        size_ = ht.size_;
        table_ = shmalloc(size_, prefix_);
        sems_ = semload(count_sems(size_), prefix_);
    }
    HashTable& operator=(const HashTable & ht) {
        if (this != &ht) {
            shm_unlink(table_);
            if (size_ != ht.size_) {
                size_ = ht.size_;
            }
            prefix_ = ht.prefix_;
            table_ = shmalloc(size_, prefix_);
            sems_ = semload(count_sems(size_), prefix_);
        }
        return *this;
    }

    std::string get(std::string key) {
        check_len(key, KEY_LEN);
        size_t pos = get_key_pos(key);
        return get_value(pos);
    }

    void set(std::string key, std::string value) {
        check_len(key, KEY_LEN);
        check_len(value, VAL_LEN);
        size_t pos = get_key_pos(key, true);
        //Create key-value buf
        char buf[BUF_LEN];
        memset(&buf[0], 0, BUF_LEN);
        memcpy(&buf[0], key.c_str(), key.length());
        memcpy(&buf[KEY_LEN], value.c_str(), value.length());
        //Place information into table
        char* line = get_line(pos);
        enter_ca(pos);
        memcpy(line, buf, BUF_LEN);
        leave_ca(pos);
    }

    std::string delete_key(std::string key) {
        //Check key
        check_len(key, KEY_LEN);
        //Find key pos
        size_t pos = get_key_pos(key);
        //Clean line
        std::string value = get_value(pos);
        char* line = get_line(pos);
        enter_ca(pos);
        memset(line, 0, BUF_LEN);
        leave_ca(pos);
        return value;
    }

    /*void print () {
        for (size_t i = 0; i < size_; i++) {
            std::cout << i << ": " << get_key(i) << ": " << get_value(i) << std::endl;
        }
    }*/

    ~HashTable() {
        shm_unlink(table_);
        for(auto sem: sems_) sem_close(sem);
    }

private:
    void enter_ca(size_t pos) {
        if (sem_wait(sems_[get_sem_id(pos)]) == -1) {
            std::cerr << "sem_wait() error" << std::endl;
            _exit(1);
        }
    }
    void leave_ca(size_t pos) {
        if (sem_post(sems_[get_sem_id(pos)]) == -1) {
            std::cerr << "sem_wait() error" << std::endl;
            _exit(1);
        }
    }
    bool is_empty(size_t pos) {
        return get_key(pos) == "";
    }
    std::string get_key(size_t pos) {
        char key[KEY_LEN];
        char *line = get_line(pos);
        enter_ca(pos);
        memcpy(key, line, KEY_LEN);
        leave_ca(pos);
        return std::string(key);
    }
    std::string get_value(size_t pos) {
        char value[VAL_LEN];
        char *line = get_line(pos);
        enter_ca(pos);
        memcpy(value, &line[KEY_LEN], VAL_LEN);
        leave_ca(pos);
        return std::string(value);
    }
    size_t get_key_pos(std::string key, bool empty_too = false) {
        size_t pos = hash(key);
        size_t i = pos;
        do {
            if ((empty_too && is_empty(i)) or (key == get_key(i))) return i;
            i = (i + 1) % size_;
        } while(i != pos);
        if (empty_too) throw std::runtime_error("Table is full and key '" + key +"' was not found!");
        else throw std::runtime_error("Key '" + key + "' was not found!");
    }
    char* get_line(size_t pos) {
        return &table_[pos*BUF_LEN];
    }
    size_t hash(std::string str) {
        return str_hash(str) % size_;
    }
    void check_len(std::string str, size_t len) {
        if (str.length() > len or str.length() == 0)
            throw std::runtime_error("Bad string size: '" + str + "'!");
    }

private:
    size_t size_;
    int prefix_;
    std::hash<std::string> str_hash;
    char* table_;
    std::vector<sem_t *> sems_;
};

class TableWriter {
public:
    TableWriter(size_t size) : ht(size) {
    }
    std::string execute(std::string command_line) {
        std::istringstream iss(command_line);
        std::string command, key, value;
        iss >> command;
        if (command == "set") {
            iss >> key;
            getline(iss, value);
            size_t npos = value.find_first_not_of(" \t\v\r");
            value.erase(0, npos);
            try { ht.set(key, value); }
            catch (std::runtime_error ex) { return "error " + std::string(ex.what()); }
            return "ok " + key + " " + value;
        }
        else if (command == "get") {
            iss >> key;
            try { value = ht.get(key); }
            catch (std::runtime_error ex) { return "error " + std::string(ex.what()); }
            return "ok " + key + " " + value;
        }
        else if (command == "delete") {
            iss >> key;
            try { value = ht.delete_key(key); }
            catch (std::runtime_error ex) { return "error " + std::string(ex.what()); }
            return "ok " + key + " " + value;
        }
        else {
            return "error unknown command";
        }
    }
private:
    HashTable ht;
};

class Handler {
public:
    Handler(size_t size, int parent) : server_(size), parent_(parent) {
    }
    ~Handler() {
        close(parent_);
    }

    void listen() {
        std::cout << getpid() << ": Ready to work!" << std::endl;
        int client = recvfd(parent_);
        std::cout << getpid() << ": Received socket from parent: " << client << std::endl;
        //Talk to client
        talk(client);
    }

    void talk(int client) {
        char buf[1];
        ssize_t bytes_read;
        while(true) {
            std::string command = "";
            do {
                bytes_read = recv(client, buf, 1, 0);
                command += buf[0];
            }
            while ((buf[0] != '\n') and (bytes_read > 0));
            if (bytes_read <= 0) break;
            std::string answer = server_.execute(command);
            send(client, answer.c_str(), answer.length(), 0);
        }
        close(client);
        listen();
    }

private:
    TableWriter server_;
    int parent_;
};

void start(int children) {
    int client, listener;
    struct sockaddr_in addr;
    int on = 1;

    //Socket to receive connections
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        std::cerr << "Socket creation error" << std::endl;
        close(children);
        exit(1);
    }
    //Reuse socket
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
        std::cerr << "setsockopt() failed" << std::endl;
        close(children);
        close(listener);
        exit(1);
    }
    //Bind socket
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(12345);
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        std::cerr << "Bind error" << std::endl;
        close(children);
        close(listener);
        exit(1);
    }
    //Listen
    if (listen(listener, 5) < 0) {
        std::cerr << "Listen error" << std::endl;
        close(children);
        close(listener);
        exit(1);
    }
    //Accept connections
    struct timeval timeout;
    timeout.tv_sec = 600;
    timeout.tv_usec = 0;
    fd_set set;
    FD_ZERO(&set);
    FD_SET(listener, &set);
    while(true) {
        int rv = select(listener+1, &set, NULL, NULL, &timeout);
        if (rv < 0) {
            std::cerr << "Select error" << std::endl;
            close(children);
            close(listener);
            exit(1);
        }
        if (rv == 0) {
            std::cerr << "Timeout. Shutting server down..." << std::endl;
            break;
        }
        client = accept(listener, NULL, NULL);
        if (client < 0) {
            std::cerr << "Accept error" << std::endl;
            close(children);
            close(listener);
            exit (1);
        }
        sendfd(children, client);
        std::cout << "Sent socket to children: " << client << std::endl;
        close(client);
    }
    close(listener);
    close(children);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: program <size>" << std::endl;
        return 1;
    }
    size_t size = static_cast<size_t>(atoi(argv[1]));
    std::vector<pid_t> children;

    //SHARED
    char buf_mem[10];
    sprintf(buf_mem, MEM_NAME_TL, getpid());
    int shmid;
    if((shmid = shm_open(buf_mem, O_CREAT|O_RDWR|O_TRUNC, 0666)) == -1) {
        std::cerr << "shm_open() error" << std::endl;
        return 1;
    }
    size_t size_mem = size*BUF_LEN;
    if(ftruncate(shmid, size_mem) == -1) {
        std::cerr << "ftruncate() error" << std::endl;
        return 1;
    }
    char *mem = (char *)mmap(NULL, size_mem, PROT_READ|PROT_WRITE, MAP_SHARED, shmid, 0);
    if(mem == MAP_FAILED) {
        std::cerr << "mmap() error" << std::endl;
        return 1;
    }
    memset(mem, 0, size_mem);
    sleep(1);

    //SEMAPHORES
    std::vector<sem_t *> sems;
    int sem_n = count_sems(size);
    sem_t *sem;
    char buf_sem[14];
    for (int i = 0; i < sem_n; i++) {
        sprintf(buf_sem, SEM_NAME_TL, getpid(), i);
        if((sem = sem_open(buf_sem, O_CREAT, 0666, 1)) == SEM_FAILED) {
            std::cerr << "sem_open() error" << std::endl;
            return 1;
        }
        sems.push_back(sem);
    }
    //CREATE SOCKET TO COMMUNICATE WITH CHILDREN
    int fd[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fd) < 0) {
        std::cerr << "socketpair() error" << std::endl;
        return 1;
    }
    //CREATE HANDLERS
    for (int i = 0; i < N_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "Fork error" << std::endl;
            close(fd[0]);
            close(fd[1]);
            return 1;
        }
        if (pid == 0) {
            //child
            close(fd[p_socket]);
            Handler h(size, fd[c_socket]);
            h.listen();
            _exit(0);
        }
        else children.push_back(pid);
    }
    //FINISH
    close(fd[c_socket]);
    start(fd[p_socket]);
    for (auto child: children) {
        kill(child, SIGINT);
        wait(NULL);
    }
    close(fd[p_socket]);
    for (auto sem: sems) sem_close(sem);
    shm_unlink(mem);
    munmap(mem, size_mem);
    return 0;
}
