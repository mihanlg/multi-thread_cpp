//
// Created by Michael Postnikov on 26.01.17.
//

#include "call.h"

void Call::execute() {
    std::vector<char*> argv;
    for (auto const& a : get_arguments())
        argv.emplace_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    if (file_in_ != "") {
        int in = open(file_in_.c_str(), O_RDONLY, 0666);
        if (in == -1) {
            std::cerr << "Can't open a file " << file_in_ << std::endl;
            exit(1);
        }
        auto FN = fileno(stdin);
        close(FN);
        if (dup2(in, FN) == -1) {
            std::cerr << "Cannot redirect stdin" << std::endl;
            exit(1);
        }
        fflush(stdin);
        close(in);
    }

    if (file_out_ != "") {
        int out = open(file_out_.c_str(), O_WRONLY|O_CREAT|O_APPEND|O_TRUNC, 0666);
        if (out == -1) {
            std::cerr << "Can't open a file " << file_out_ << std::endl;
            exit(1);
        }
        auto FN = fileno(stdout);
        close(FN);
        if (dup2(out, FN) == -1) {
            std::cerr << "Cannot redirect stdout" << std::endl;
            exit(1);
        }
        fflush(stdout);
        close(out);
    }

    if (has_pipe_in_) {
        auto FN = fileno(stdin);
        close(FN);
        dup2(pfd_in_[0], FN);
        close(pfd_in_[0]);
        close(pfd_in_[1]);
    }

    if (has_pipe_out_) {
        auto FN = fileno(stdout);
        close(FN);
        dup2(get_pfd_out()[1], FN);
        close(get_pfd_out()[0]);
        close(get_pfd_out()[1]);
    }

    if (execvp(get_command(), argv.data()) < 0) {
        std::cerr << "Exec failed\n";
        exit(1);
    }
}

void Call::redirect(std::string direction, std::string filename) {
    if (direction == "<") {
        file_in_ = filename;
    }
    else if (direction == ">") {
        file_out_ = filename;
    }
}

void Call::set_pfd_out(int pfd[2]) {
    has_pipe_out_ = true;
    pfd_out_[0] = pfd[0];
    pfd_out_[1] = pfd[1];
}

void Call::set_pfd_in(int pfd[2]) {
    has_pipe_in_ = true;
    pfd_in_[0] = pfd[0];
    pfd_in_[1] = pfd[1];
}