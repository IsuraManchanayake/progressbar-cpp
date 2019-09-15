#include <cmath>
#include <cstdio>

struct WorkLoad {
  size_t tick;
  size_t work;
  static const size_t multiplier = 10000;

  WorkLoad(size_t work) : work(work), tick(0) {}

  void do_work() {
    for (size_t i = 0; i <= work * multiplier; i++) {
      tick = sqrt(work * i) / sqrt(multiplier);
    }
  }
};