#include <iostream>

#include "base/callback.h"

void IncrementInteger(int& n) {
  n++;
}

int main() {
  int n = 0;
  base::OnceClosure cb1 = base::BindOnce(IncrementInteger, std::ref(n));
  cb1();
  std::cout << n << std::endl;

  base::OnceClosure cb2 = [&n](){n++;};
  cb2();
  std::cout << n << std::endl;
  return 0;
}
