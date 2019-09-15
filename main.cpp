#include "progressbar.hpp"
#include "workload.hpp"

int main() {
  WorkLoad wl(60000);
  std::thread t([&wl]() { wl.do_work(); });
  ProgressBar<size_t, DisplayComponent::All, DisplayComponent::ProgressBar,
              DisplayComponent::ElapsedTime>
      pb(wl.tick, wl.work);
  pb.init();
  t.join();
}

/*

clear && g++ main.cpp -o main -std=c++11 -lpthread && ./main

*/