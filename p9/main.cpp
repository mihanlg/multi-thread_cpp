#include <iostream>
#include <cmath>
#include <omp.h>
#include <algorithm>
#include <vector>
#include <chrono>

void quickSortOMP(double *a, int start, int finish, bool par = true) {
    int i = start, j = finish;
    int len = finish - start;
    if (par and (len < 100)) par = false;
    double pivot = a[start + len / 2];
    //pivot array splitting
    do {
        while (a[i] < pivot) i++;
        while (a[j] > pivot) j--;
        if (i <= j) {
            std::swap(a[i], a[j]);
            i++; j--;
        }
    } while (i <= j);
    if (not par) {
        if (j > start) quickSortOMP(a, start, j, false);
        if (finish > i) quickSortOMP(a, i, finish, false);
        return;
    }
    #pragma omp parallel
    {
        #pragma omp sections nowait
        {
            #pragma omp section
            if (j > start) { quickSortOMP(a, start, j, true); }
            #pragma omp section
            if (finish > i) { quickSortOMP(a, i, finish, true); }
        }
    }

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Use: program <N>" << std::endl;
    }
    int N = atoi(argv[1]);
    std::vector<std::chrono::nanoseconds> diffW, diffOMP;
    double *to_sort, *same_to_sort;
    to_sort = new double[N];
    same_to_sort = new double[N];
    //do not parallel this
    auto cur_time = &std::chrono::steady_clock::now;
    double a;
    for (int t = 0; t < 10; t++) {
        for (int i = 0; i < N; i++) {
            a = (rand()%(N/10));
            to_sort[i] = same_to_sort[i] = a;
        }

        //OpenMP
        auto start_time = cur_time();
        quickSortOMP(to_sort, 0, N-1);
        diffOMP.push_back(cur_time() - start_time);

        //Without
        start_time = cur_time();
        quickSortOMP(same_to_sort, 0, N-1, false);
        diffW.push_back(cur_time() - start_time);
    }
    double diffW_mean = 0., diffOMP_mean = 0.;
    for (auto elem: diffOMP) {
        diffOMP_mean += elem.count()/1e9/diffOMP.size();
    }
    for (auto elem: diffW) {
        diffW_mean += elem.count()/1e9/diffW.size();
    }
    std::cout << "OpenMP: Mean time sorted in " << diffOMP_mean << " seconds" << std::endl;
    std::cout << "Without: Mean time sorted in " << diffW_mean << " seconds" << std::endl;

    delete[] to_sort, same_to_sort;
    //FOR N = 1000000
    //OpenMP: Mean time sorted in 0.171627 seconds
    //Without: Mean time sorted in 0.204084 seconds
    return 0;
}