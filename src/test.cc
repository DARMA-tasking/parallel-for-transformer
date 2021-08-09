
#include <cstddef>

namespace {

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

template <typename Policy, typename Func>
void parallel_for(Policy i, Func j) { }

template <typename Policy, typename Func>
void parallel_scan(Policy i, Func j) { }


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
  Kokkos::parallel_for(Kokkos::RangePolicy<int, float>(1000), []{});

  Kokkos::RangePolicy<int, float> policy{1000};
  Kokkos::parallel_for(policy, []{});
}

void test() {
  //using namespace Kokkos;
  while (true) {
    int x = 10;
    if (true) x=5;
    Kokkos::parallel_for(90, []{});
    Kokkos::fence();
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

  Kokkos::parallel_for(20, []{});
  Kokkos::fence();

  X S_values;

  Kokkos::parallel_for(Kokkos::RangePolicy< PHX::Device >(0,S_values.extent(0)),
  KOKKOS_LAMBDA (const Size& n)
  {
    return n+1;
  });
  Kokkos::fence();

  using DeviceSpace = PHX::Device;

  size_t N = 0;

  Kokkos::parallel_for (Kokkos::RangePolicy<DeviceSpace,Kokkos::Schedule<Kokkos::Schedule<Kokkos::Dynamic>>>(0,N-2),
  []{});
  Kokkos::fence();
}

void test2() {
  test4<int>();
  test4<float>();
  test4<long>();

  if (true) {
    Kokkos::parallel_for("10", 20);
    Kokkos::fence();
    Kokkos::parallel_scan("10", 20);
    Kokkos::fence();
  }
  if (true) {
    Kokkos::parallel_for(40, []{});
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

}
