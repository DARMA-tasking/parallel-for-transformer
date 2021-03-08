
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

template <typename T>
class Schedule {};
class Dynamic {};

void fence() { }

}

namespace empire {

template <typename Policy>
void parallel_for_blocking(std::string str, Policy rp) { }
template <typename Policy, typename X>
void parallel_for_blocking(std::string str, Policy rp, X) { }

template <typename Policy>
void parallel_for_blocking(Policy rp) { }
template <typename Policy, typename X>
void parallel_for_blocking(Policy rp, X) { }

void parallel_for_blocking(std::string str, size_t x) { }
void parallel_for_blocking(size_t x) { }
void parallel_scan_blocking(std::string str, size_t j) { }
void parallel_scan_blocking(size_t i) { }

template <typename Policy>
void parallel_for_async(std::string str, Policy rp) { }
template <typename Policy, typename X>
void parallel_for_async(std::string str, Policy rp, X) { }

void parallel_for_async(std::string str, size_t x) { }
void parallel_for_async(size_t x) { }
void parallel_scan_async(std::string str, size_t j) { }
void parallel_scan_async(size_t i) { }

void fence() { }

}

namespace PHX {
class Device {};
}

void test3() {
  empire::parallel_for_async("test-123", Kokkos::RangePolicy<int, float>(1000));

  Kokkos::RangePolicy<int, float> policy{1000};
  empire::parallel_for_async("test-123", policy);
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

  empire::parallel_for_blocking(Kokkos::RangePolicy< PHX::Device >(0,S_values.extent(0)),
  KOKKOS_LAMBDA (const Size& n)
  {
    return n+1;
  });

  using DeviceSpace = PHX::Device;

  size_t N = 0;

  empire::parallel_for_blocking ("", Kokkos::RangePolicy<DeviceSpace,Kokkos::Schedule<Kokkos::Schedule<Kokkos::Dynamic>>>(0,N-2),
  []{});
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
