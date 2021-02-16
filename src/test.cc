
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
  explicit RangePolicy(size_t in, size_t out) {}
};

template <typename Policy>
void parallel_for(std::string str, Policy rp) { }
template <typename Policy, typename X>
void parallel_for(std::string str, Policy rp, X) { }

void parallel_for(std::string str, size_t x) { }
void parallel_for(size_t x) { }
void parallel_scan(std::string str, size_t j) { }
void parallel_scan(size_t i) { }
void fence() { }

}

namespace PHX {
class Device {};
}

void test3() {
  empire::parallel_for_async("test-123", buildKokkosPolicy<Kokkos::RangePolicy, int, float>(1000));
}

void test() {
  //using namespace Kokkos;
  while (true) {
    int x = 10;
    if (true) x=5;
    empire::parallel_for_blocking("80", 90);
    if (false) x=6;
  }
}

#define KOKKOS_LAMBDA []

struct X {
  size_t extent(int len){return 0;};
};

template <typename T>
void test4() {
  using Size = size_t;

  empire::parallel_for_blocking("10", 20);

  X S_values;

  empire::parallel_for_blocking("hello", buildKokkosPolicy<Kokkos::RangePolicy, PHX::Device>(0,S_values.extent(0)),
  KOKKOS_LAMBDA (const Size& n)
  {
    return n+1;
  });
}

void test2() {
  test4<int>();
  test4<float>();
  test4<long>();

  if (true) {
    empire::parallel_for_blocking("10", 20);
    empire::parallel_scan_blocking("10", 20);
  }
  if (true) {
    empire::parallel_for_blocking(40);
  }
  if (true) {
    empire::parallel_for_async("1219", 23);
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
