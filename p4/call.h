//
// Created by Michael Postnikov on 26.01.17.
//

#ifndef SHELL2_CALL_H
#define SHELL2_CALL_H

#include <iostream>
#include <vector>
#include <unistd.h>
#include <fcntl.h>

class Call {
public:
    Call(std::string command) { command_.push_back(command); }

    void execute();
    void redirect(std::string direction, std::string filename);
    void add_argument(std::string arg) { command_.push_back(arg); }
    bool has_pipe_out() { return has_pipe_out_; }
    bool has_pipe_in() { return has_pipe_in_; }

    //setters
    void set_pfd_out(int pfd[2]);
    void set_pfd_in(int pfd[2]);
    void set_status(int status) { status_ = status; }
    void set_exec(bool to) { toexec_ = to; }

    //getters
    bool get_exec() { return toexec_; }
    int get_status() { return status_; }
    int* get_pfd_out() { return pfd_out_; }
    int* get_pfd_in() { return pfd_in_; }
    const char* get_command() { return const_cast<char *>(command_[0].c_str()); }
    std::vector<std::string> get_arguments() { return command_; }
    const std::string get_redirect_in() { return file_in_; }
    const std::string get_redirect_out() { return file_out_; }
private:
    int status_ = 0;
    bool toexec_ = true;
    std::string file_in_ = "", file_out_ = "";
    std::vector<std::string> command_;
    bool has_pipe_in_ = false, has_pipe_out_ = false;
    int pfd_in_[2], pfd_out_[2];
};

class Operator {
public:
    Operator(std::string op) : op_(op) {}
    const std::string getOperator() {return op_;}
private:
    std::string op_;
};

struct Command {
    Command(std::vector<Call> calls_, std::vector<Operator> ops_, bool is_background_)
            : calls(calls_), ops(ops_), is_background(is_background_) {}
    std::vector<Call> calls;
    std::vector<Operator> ops;
    bool is_background;
};


#endif //SHELL2_CALL_H
