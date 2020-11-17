
namespace Kokkos {

void parallel_for(int i, int j) { }
void parallel_scan(int i, int j) { }

}


void test() {
  using namespace Kokkos;
  while (true) {
    parallel_for(10, 20);
  }
}

void test2() {
  if (true) {
    Kokkos::parallel_for(10, 20);
    Kokkos::parallel_scan(10, 20);
  }
}

int main2() {
  test();
  test2();
  return 0;
}
