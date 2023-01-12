
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
  explicit RangePolicy(int in) {}
  explicit RangePolicy(int in, int out) {}
};

template <typename T>
class Schedule {};
class Dynamic {};

void fence(const std::string&) { }
void fence() { }

template <typename Policy, typename Func>
void parallel_for(Policy i, Func j) { }

template <typename Policy, typename Func>
void parallel_for(std::string str, Policy i, Func j) { }

template <typename Policy, typename Func>
void parallel_scan(Policy i, Func j) { }

void deep_copy(int, int) {}

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

void parallel_for_blocking(std::string str, int x) { }
void parallel_for_blocking(int x) { }
void parallel_scan_blocking(std::string str, int j) { }
void parallel_scan_blocking(int i) { }

template <typename Policy>
void parallel_for_async(std::string str, Policy rp) { }
template <typename Policy, typename X>
void parallel_for_async(std::string str, Policy rp, X) { }

void parallel_for_async(std::string str, int x) { }
void parallel_for_async(int x) { }
void parallel_scan_async(std::string str, int j) { }
void parallel_scan_async(int i) { }

void deep_copy_blocking(int, int);

void fence() { }

}

namespace PHX {
class Device {};
}

// void test3() {
//   Kokkos::parallel_for(Kokkos::RangePolicy<int, float>(1000), []{});

//   Kokkos::RangePolicy<int, float> policy{1000};
//   Kokkos::parallel_for(policy, []{});

//   int a, b;
//   Kokkos::deep_copy(a, b);
// }

// void test() {
//   //using namespace Kokkos;
//   while (true) {
//     int x = 10;
//     if (true) x=5;
//     Kokkos::parallel_for(90, []{});
//     Kokkos::fence();
//     if (false) x=6;
//     int c, b;
//     Kokkos::deep_copy(c, b);
//   }
// }

#define KOKKOS_LAMBDA []

struct X {
  int extent(int len){return 0;};
};

template <typename T>
void test4() {
  // using Size = int;

  // Kokkos::parallel_for(20, []{});
  // Kokkos::fence();

  // X S_values;

  // Kokkos::parallel_for(Kokkos::RangePolicy< PHX::Device >(0,S_values.extent(0)),
  // KOKKOS_LAMBDA (const Size& n)
  // {
  //   int d, e;
  //   Kokkos::deep_copy(d, e);
  //   return n+1;
  // });
  // Kokkos::fence();


  using DeviceSpace = PHX::Device;

  int N = 0;

  // Kokkos::parallel_for (Kokkos::RangePolicy<DeviceSpace,Kokkos::Schedule<Kokkos::Schedule<Kokkos::Dynamic>>>(0,N-2),
  // []{});
  // Kokkos::fence();


 empire::parallel_for_async(
   "parts::checkSort",
   Kokkos::RangePolicy<DeviceSpace>(0, N * N),
   KOKKOS_LAMBDA(const int i) {
   }
 );
 {
   Kokkos::fence();
 }

}

// void test2() {
//   test4<int>();
//   test4<float>();
//   test4<long>();

//   if (true) {
//     Kokkos::parallel_for("10", 20);
//     Kokkos::fence();
//     Kokkos::parallel_scan("10", 20);
//     Kokkos::fence();
//   }
//   if (true) {
//     Kokkos::parallel_for(40, []{});
//     Kokkos::fence();
//   }
//   if (true) {
//     Kokkos::parallel_for("1219", 23);
//     test();
//   }

//   if (true) {
//     Kokkos::fence();
//   }
// }

int main2() {
  // test();
  // test2();
  test4<int>();
  return 0;
}

}
