
#include "My_barrier.h"

My_barrier::My_barrier(int max)
{
    this->max=max;
    this->counter=0;
    this->waiting=0;

}
My_barrier::~My_barrier(){

}

void My_barrier::my_wait()
{
    std::unique_lock<std::mutex> lck(this->mx);
    this->counter++;
    this->waiting++;

    this->cv.wait(lck,[&]{ return counter>=max;});
    this->cv.notify_one();

    this->waiting--;

    if(this->waiting==0)
        this->counter=0;
    
    lck.unlock();

}