#include <mpi.h>
#include <iostream>
#include <cstdlib>
#include <cmath>

double factorial(int n) {
    if (n == 0)
        return 1;
    else
        return factorial(n-1)*n;
}

int main(int argc, char *argv[]) {
    int N_exp, pid, size, rc;
    double exp_1, startTime, endTime;
    //CHECK ARGUMENTS
    if (argc != 2) {
        std::cout << "Use: program <N>" << std::endl;
        return 1;
    }
    N_exp = atoi(argv[1]);
    //MPI INIT
    if ((rc = MPI_Init(&argc, &argv))) {
        std::cout << "MPI start error, aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, rc);
        return 1;
    }
    //GET SIZE, PID
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);

    //START TIMER
    if (pid == 0) startTime = MPI_Wtime();
    //SEND N_exp TO ALL PROCESSORS
    MPI_Bcast(&N_exp, 1, MPI_INT, 0, MPI_COMM_WORLD);
    //PARALLEL CYCLE
    double partialSum = 0.;
    for (int i = pid; i <= N_exp; i += size) {
        double sum = factorial(i);
        partialSum += 1/sum;
    }
    //SUM ALL PARTIAL SUMS
    MPI_Reduce(&partialSum, &exp_1, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    //PRINT RESULT BY FIRST PROCESSOR
    if (pid == 0) {
        std::cout << "---------------------------------------------------" << std::endl;
        std::cout << "Counted exp(1): " << exp_1 << " Error: " << fabs(exp_1 - M_E) << std::endl;
        endTime = MPI_Wtime();
        std::cout << "Time used for " << N_exp << " length: " << (endTime-startTime)*1000 << std::endl;
        std::cout << "---------------------------------------------------" << std::endl;
    }
    MPI_Finalize();
    return 0;
}