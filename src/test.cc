
namespace Kokkos {

void parallel_for(int i, int j) { }
void parallel_scan(int i, int j) { }

}


void test() {
  using namespace Kokkos;
  while (true) {
    int x = 10;
    if (true) x=5;
    parallel_for(10, 20);
    if (false) x=6;
  }
}

void test2() {
  if (true) {
    Kokkos::parallel_for(10, 20);
    Kokkos::parallel_scan(10, 20);
  }
  if (true) {
    Kokkos::parallel_for(30, 40);
  }
}

int main2() {
  test();
  test2();
  return 0;
}
