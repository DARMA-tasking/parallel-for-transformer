
#include <cstddef>

namespace std {
struct string {
  string(char const* str) {}
};
};

namespace Kokkos {

template <typename... A>
class RangePolicy {
public:
  explicit RangePolicy(size_t in) {}
};

template <typename Policy>
void parallel_for(std::string str, Policy rp) { }

void parallel_for(std::string str, size_t x) { }
void parallel_for(size_t x) { }
void parallel_scan(std::string str, size_t j) { }
void parallel_scan(size_t i) { }
void fence() { }

}

void test3() {
  Kokkos::parallel_for("test-123", Kokkos::RangePolicy<int, float>(1000));
}

void test() {
  //using namespace Kokkos;
  while (true) {
    int x = 10;
    if (true) x=5;
    Kokkos::parallel_for("80", 90);
    Kokkos::fence();
    if (false) x=6;
  }
}

void test2() {
  if (true) {
    Kokkos::parallel_for("10", 20);
    Kokkos::fence();
    Kokkos::parallel_scan("10", 20);
    Kokkos::fence();
  }
  if (true) {
    Kokkos::parallel_for(40);
    Kokkos::fence();
  }
  if (true) {
    Kokkos::parallel_for("1219", 23);
    test();
  }

  if (true) {
    Kokkos::fence();
  }
}

int main2() {
  test();
  test2();
  return 0;
}
