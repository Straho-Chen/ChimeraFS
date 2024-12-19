#include <iostream>
#include <string>

using namespace std;

int main() {
  double x;
  string unit;

  cin >> x;
  cin >> unit;
  if (unit == "MiB/s") {
    cout << x;
  } else if (unit == "GiB/s") {
    x = x * 1024;
    cout << x;
  } else {
    cerr << "Unrecognized unit " << unit << endl;
    return 1;
  }

  return 0;
}