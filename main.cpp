#include "workload.hpp"
#include "progressbar.hpp"

int main() {
    WorkLoad wl(100000);
    std::thread t([&wl]() { wl.do_work(); });
    ProgressBar<size_t, 1, DisplayComponent::All> pb(wl.tick, wl.work);
    pb.init();
    t.join();
}

/*

clear && g++ main.cpp -o main -std=c++11 -lpthread && ./main

*/