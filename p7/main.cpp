#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <unistd.h>
#include <condition_variable>
#include <map>
#include <fstream>
#include <netinet/in.h>
#include <cstring>
#include <poll.h>
#include <sys/inotify.h>
#include <fcntl.h>

class Trie {
public:
    typedef std::string String;
    typedef String Word;
    typedef std::vector<Word> Words;
public:
    Trie() {}
    Words findall() const {
        Words words;
        if (final_) words.push_back("");
        for (auto const& ent : trie_) {
            char letter = ent.first;
            Words suffixes = ent.second.findall();
            for (size_t i = 0; i < suffixes.size(); i++) {
                words.push_back(std::string(1, letter) + suffixes[i]);
            }
        }
        return words;
    }
    Words find_by_prefix(String prefix) {
        if (prefix == "") return findall();
        Words words;
        char letter = prefix[0];
        if (not has_next(letter)) return words;
        words = trie_[letter].find_by_prefix(prefix.substr(1));
        for (size_t i = 0; i < words.size(); i++)
            words[i] = std::string(1, letter) + words[i];
        return words;
    }
    bool has(const Word word) {
        if (word == "") return final_;
        char letter = word[0];
        if (has_next(letter))
            return trie_[letter].has(word.substr(1));

    }
    bool has_next(const char letter) {
        return static_cast<bool>(trie_.count(letter));
    }
    void add(const Word word) {
        if (word == "") { final_ = true; return; }
        char letter = word[0];
        if (not has_next(letter)) {
            trie_[letter] = Trie();
        }
        trie_[letter].add(word.substr(1));
    }
    void construct(String filename) {
        trie_.clear();
        std::ifstream f(filename);
        if (f.is_open()) {
            Word word;
            while (f >> word) {
                add(word);
            }
        } else std::cout << "ifstream error" << std::endl;
    }
private:
    bool final_ = false;
    std::map<char, Trie> trie_;
};

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))

class Server {
public:
    Server(std::string filename) : file_(filename) {}
    ~Server() {}
    void run() {
        is_on_ = true;
        //CREATE WATCHER AND WORKERS
        enter_w();
        vocabulary_.construct(file_);
        leave_w();
        threads_.push_back(std::thread(&Server::watch, this));
        for (int i = 0; i < 10; i++) threads_.push_back(std::thread(&Server::work, this));
        //START SERVER
        start();
    }
    //WATCHER
    void watch() {
        int inotifyFd, wd, j;
        char buf[BUF_LEN] __attribute__ ((aligned(8)));
        char *p;
        struct inotify_event *event;
        ssize_t numRead;

        inotifyFd = inotify_init();
        if (inotifyFd == -1) {
            std::cerr << "inotify_init() error" << std::endl;
        }

        wd = inotify_add_watch(inotifyFd, file_.c_str(), IN_CLOSE_WRITE | IN_CREATE);
        if (wd == -1) {
            std::cerr << "inotify_add_watch() error" << std::endl;
            exit(1);
        }

        for (;;) {                                  /* Read events forever */
            if (read(inotifyFd, buf, BUF_LEN) <= 0) {
                std::cerr << "read() error!" << std::endl;
                return;
            }
            enter_w();
            vocabulary_.construct(file_);
            leave_w();
            std::cout << "Vocabulary updated" << std::endl;
        }
    }
    //WORKER
    int get_client() {
        std::unique_lock<std::mutex> lock(mutex_clients_);
        while(is_on_ && (clients_ .size() == 0))
            cond_clients_.wait(lock);
        if (clients_ .size() != 0) {
            int client = clients_ .front();
            clients_ .pop();
            return client;
        }
        else return -1;
    }
    void enter_r() {
        r_.lock();
        mutex1_.lock();
        readcount_++;
        if (readcount_ == 1)
            w_.lock();
        mutex1_.unlock();
        r_.unlock();
    }
    void leave_r() {
        mutex1_.lock();
        readcount_--;
        if (readcount_ == 0)
            w_.unlock();
        mutex1_.unlock();
    }
    void enter_w() {
        mutex2_.lock();
        writecount_++;
        if (writecount_ == 1)
            r_.lock();
        mutex2_.unlock();
    }
    void leave_w() {
        mutex2_.lock();
        writecount_--;
        if (writecount_ == 0)
            r_.unlock();
        mutex2_.unlock();
    }
    void talk(int client) {
        char buf[1];
        ssize_t bytes_read;
        while(true) {
            std::string prefix = "";
            do {
                bytes_read = read(client, buf, 1);
                prefix += buf[0];
            }
            while ((buf[0] != '\n') and (bytes_read > 0));
            if (bytes_read <= 0) break;
            prefix.pop_back();
            //block
            enter_r();
            Trie::Words words = vocabulary_.find_by_prefix(prefix);
            leave_r();
            //unblock
            unsigned long n = std::min(words.size(), (unsigned long)10);
            for (unsigned i = 0; i < n; i++) {
                send(client, (words[i]+"\n").c_str(), words[i].length()+1, 0);
            }
            send(client, "\n", 1, 0);
        }
        close(client);
        work();
    }
    void work() {
        int client;
        if ((client = get_client()) == -1) return;
        talk(client);
    }
    //SERVER
    void start() {
        int client;
        struct sockaddr_in addr;
        int on = 1;
        //Socket to receive connections
        listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket_ < 0) {
            std::cerr << "socket() error" << std::endl;
            return;
        }
        //Reuse socket
        if (setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
            std::cerr << "setsockopt() failed" << std::endl;
            close(listen_socket_);
            return;
        }
        //Bind socket
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(13666);
        if (bind(listen_socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            std::cerr << "bind() error" << std::endl;
            close(listen_socket_);
            return;
        }
        //Listen
        if (listen(listen_socket_, 5) < 0) {
            std::cerr << "listen() error" << std::endl;
            close(listen_socket_);
            return;
        }
        //Accept connections
        struct timeval timeout;
        timeout.tv_sec = 600;
        timeout.tv_usec = 0;
        fd_set set;
        FD_ZERO(&set);
        FD_SET(listen_socket_, &set);
        while(true) {
            int rv = select(listen_socket_+1, &set, NULL, NULL, &timeout);
            if (rv < 0) {
                std::cerr << "select() error" << std::endl;
                close(listen_socket_);
                return;
            }
            if (rv == 0) {
                std::cerr << "Timeout. Shutting server down..." << std::endl;
                break;
            }
            client = accept(listen_socket_, NULL, NULL);
            if (client < 0) {
                std::cerr << "accept() error" << std::endl;
                close(listen_socket_);
                return;
            }
            std::unique_lock<std::mutex> lock(mutex_clients_);
            clients_.push(client);
            cond_clients_.notify_one();
        }
        //no connections
        finish();
    }
    void finish() {
        close(listen_socket_);
        is_on_ = false;
        for (int i = 0; i < threads_.size(); i++) {
            threads_[i].join();
        }
    }
private:
    std::string file_;
    Trie vocabulary_;
    bool is_on_;
    int listen_socket_;
    std::queue<int> clients_;
    std::mutex mutex_clients_;
    std::mutex r_, w_, mutex1_, mutex2_;
    size_t readcount_ = 0, writecount_ = 0;
    std::condition_variable cond_clients_;
    std::vector<std::thread> threads_;
};



int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: program <filename>" << std::endl;
        return 1;
    }
    Server server(argv[1]);
    server.run();
    server.talk(1);
    return 0;
}