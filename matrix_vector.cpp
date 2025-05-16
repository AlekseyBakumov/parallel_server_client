#include <iostream>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <vector>
#include <fstream>
#include <thread>

double cpuSecond()
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ((double)ts.tv_sec + (double)ts.tv_nsec * 1.e-9);
}

void matrix_vector_product(std::shared_ptr<double[]>a, 
                           std::shared_ptr<double[]>b, 
                           std::shared_ptr<double[]>c, 
                           size_t m, size_t n)
{
    for (int i = 0; i < m; i++)
    {
        c[i] = 0.0;
        for (int j = 0; j < n; j++)
            c[i] += a[i * n + j] * b[j];
    }
}

void matrix_vector_product_omp(std::shared_ptr<double[]>a, 
                               std::shared_ptr<double[]>b, 
                               std::shared_ptr<double[]>c, 
                               size_t m, size_t n,
                               int nthreads, int threadid)
{
    //int nthreads = omp_get_num_threads();
    //int threadid = omp_get_thread_num();
    int items_per_thread = m / nthreads;
    int lb = threadid * items_per_thread;
    int ub = (threadid == nthreads - 1) ? (m - 1) : (lb + items_per_thread - 1);
    //std::cout << "Thread " << threadid << " working on " 
    //          << "(" << lb << ", " << ub << ")" << std::endl;
    for (int i = lb; i <= ub; i++)
    {
        c[i] = 0.0;
        for (int j = 0; j < n; j++)
            c[i] += a[i * n + j] * b[j];
    }
}

double run_parallel(size_t n, size_t m, int k)
{
    std::shared_ptr<double[]> a(new double[m * n]);
    std::shared_ptr<double[]> b(new double[n]);
    std::shared_ptr<double[]> c(new double[m]);

    if (a == NULL || b == NULL || c == NULL)
    {
        std::cout << "Error allocate memory!\n" << std::endl;
        exit(1);
    }

    for (size_t i = 0; i < m; i++)
    {
        for (size_t j = 0; j < n; j++)
            a[i * n + j] = i + j;
    }

    for (size_t j = 0; j < n; j++)
        b[j] = j;

    std::vector<std::thread> threads;

    double t = cpuSecond();
    std:: cout << "starting" << std::endl;
    for (int i = 0; i < k; i++)
    {
        threads.emplace_back(
            std::thread(matrix_vector_product_omp,a,b,c,m,n,k,i)
        );
    }

    for (int i = 0; i < k; i++)
    {
        threads[i].join();
    }

    //matrix_vector_product_omp(a, b, c, m, n);
    t = cpuSecond() - t;

    printf("Elapsed time (parallel %d threads): %.6f sec.\n", k, t);

    return t;
}

double avg_time_parallel(size_t n, size_t m, int k, int runs)
{
    double time = 0;
    for (int i = 0; i < runs; i++)
    {
        time += run_parallel(n,m,k);
    }

    return time / runs;
}

/*
class AddTask
{
private:
    std::shared_ptr<double[]> a,b,c;
    size_t m, n;
    int k;
    int n = 1;
    int id = 0;
public:
    AddTask(std::shared_ptr<double[]>a, 
            std::shared_ptr<double[]>b, 
            std::shared_ptr<double[]>c, 
            size_t m, size_t n
            int nthreads, int threadid)
    {
        this->a = a;
        this->b = b;
        this->c = c;
        this->m = m;
        this->n = n;
        this->n = nthreads;
        this->i = threadid;
    }
    void operator()()
    {
        matrix_vector_product_omp(a,b,c,m,n,n,id);
    }
};
*/

int main(int argc, char *argv[])
{
    int runs = 10;

    size_t M = 20000;
    size_t N = 20000;
    if (argc > 1)
        M = atoi(argv[1]);
    if (argc > 2)
        N = atoi(argv[2]);

    int threads[] = {2,4,7,8,16,20,40};
    double single_thread_time = avg_time_parallel(M, N, 1, runs);
    double time;

    std::ofstream out_file;
    out_file.open("results.csv");

    out_file << "threads" << "," << "time" << "," << "speedup" << std::endl;
    out_file << 1 << "," << single_thread_time << "," << 1 << std::endl;

    for (int tr : threads)
    {
        time = avg_time_parallel(M, N, tr, runs);
        out_file << tr << "," << time << "," << single_thread_time / time << std::endl;
    }

    return 0;
}
