//Выполнено на ubuntu (написано на macOS, но там 6-й тест не работает из-за $PID, пришлось перенести)

#include <iostream>
#include <vector>
#include <set>
#include <sstream>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "call.h"

#define MAX_BUFFER 32000

pid_t currpid = getpid();
std::set<std::string> operators{">", "<", "&", "&&", "|", "||"};

/*
Задание можно (но не обязательно) выполнять в следующей последовательности:
- Обработка отдельных вызовов без операторов и перенаправлений.
- Перенаправление stdin/stdout.
- Реализация операторов "&&" и "||".
- Реализация оператора "|".
- Запуск и ожидание фоновых процессов.
- Обработка SIGINT.
 */

void exit_message(pid_t pid, int status) {
    if (WIFEXITED(status)) {
        std::cerr << "Process " << pid << " exited: " << WEXITSTATUS(status) << std::endl;
    }
}

void execute(Call &call) {
    if (not call.get_exec()) return;
    pid_t pid;
    int status;
    if ((pid = fork()) < 0) {
        std::cerr << "Forking child process failed\n";
        exit(1);
    }
    else if (pid == 0) {
        //execute parsed command
        call.execute();
    }
    else {
        currpid = pid;
        std::cerr << "Spawned child process " << pid << std::endl;
        pid_t done;
        do {
            done = wait(&status);
            exit_message(done, status);
        } while((done != pid) || not(WIFEXITED(status)));
        currpid = getpid();
        call.set_status(WEXITSTATUS(status));
        if (call.has_pipe_in()) {
            int *pfd = call.get_pfd_in();
            close(pfd[0]);
        }
        if (call.has_pipe_out()) {
            int *pfd = call.get_pfd_out();
            close(pfd[1]);
        }
    }
}

void do_pipe(Call & cmd1, Call & cmd2) {
    int pfd[2];
    pipe(pfd);
    cmd1.set_pfd_out(pfd);
    cmd2.set_pfd_in(pfd);
}

void run(Command & cmd) {
    auto & calls = cmd.calls;
    auto & ops = cmd.ops;
    for (int i = 0; i < ops.size(); i++) {
        if (ops[i].getOperator() == "|") {
            do_pipe(calls[i], calls[i+1]);
        }
        execute(calls[i]);
        if ((ops[i].getOperator() == "||") and not(calls[i].get_status())) {
            calls[i+1].set_status(calls[i].get_status());
            calls[i + 1].set_exec(false);
        }
        else if ((ops[i].getOperator() == "&&") and calls[i].get_status()) {
            calls[i+1].set_status(calls[i].get_status());
            calls[i + 1].set_exec(false);
        }
    }
    execute(calls.back());
}

std::string read_str(std::istringstream &iss) {
    std::string str = "";
    char buff = ' ';
    while (isspace(buff)) {
        if(iss.get(buff)) {
            if (buff == '\n')
                return str;
        }
        else return str;
    }
    str += buff;
    if (buff == '>' or buff == '<') {}
    else if (buff == '&') {
        if(iss.get(buff)) {
            if (buff == '&') {
                str += buff;
            } else iss.unget();
        }
    }
    else if (buff == '|') {
        if(iss.get(buff)) {
            if (buff == '|') {
                str += buff;
            } else iss.unget();
        }
    }
    else {
        while (iss.get(buff)) {
            std::string es = "";
            es += buff;
            if (not((isspace(buff)) or (operators.find(es) != operators.end()))) {
                str += buff;
            }
            else {
                iss.unget();
                break;
            }
        }
    }
    return str;
}

Command parse(std::string line) {
    std::istringstream iss(line);
    std::vector<Call> calls;
    std::vector<Operator> ops;
    std::string str;
    bool is_background = false;
    bool start = true;
    //parse commands
    unsigned long i = 0;
    while (!iss.eof()) {
        str = read_str(iss);
        if (str.length() == 0) continue;
        if (operators.find(str) != operators.end()) {
            if (str == ">" or str == "<") {
                std::string filename;
                filename = read_str(iss);
                calls.back().redirect(str, filename);
            } else if (str == "&") {
                is_background = true;
            }
            else if (str == "|" or str == "||" or str == "&&") {
                ops.push_back(Operator(str));
                start = true;
            }
        }
        else {
            if (start) {
                calls.push_back(Call(str));
                start = false;
            }
            else {
                calls.back().add_argument(str);
            }
        }
    }
    return Command(calls, ops, is_background);
}

void check_zombies() {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        exit_message(pid, status);
    }
}

void sigint_handler(int sig) {
    signal(sig, SIG_IGN);
    if (currpid == getpid()) {
        abort();
    } else {
        kill(currpid, SIGINT);
    }
    signal(sig, sigint_handler);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);
    char input[MAX_BUFFER];
    while (std::cin.getline(input, MAX_BUFFER)) {
        try {
            Command cmd = parse(input);
            if (cmd.calls.size() != 0) {
                if (cmd.is_background) {
                    pid_t pid;
                    int status;
                    if ((pid = fork()) < 0) {
                        std::cerr << "Forking child process failed\n";
                        exit(1);
                    }
                    else if (pid == 0) {
                        run(cmd);
                        exit(0);
                    }
                    else { std::cerr << "Spawned child process " << pid << std::endl; }
                }
                else run(cmd);
            }
        }
        catch (std::exception &e) {
            std::cerr << "Exception: " << e.what() << std::endl;
        }
        check_zombies();
    }
    check_zombies();
    return 0;
}
