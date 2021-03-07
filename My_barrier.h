#pragma once
#include<thread>
#include<condition_variable>

class My_barrier{
private:
    int counter;
    int waiting;
    int max;
    std::mutex mx;
    std::condition_variable cv;

public:
    My_barrier(int max);
    ~My_barrier();
    void my_wait();

};