#include <libserpent/funcs.h>
#include <iostream>

using namespace std;

int main() {
	cout << printAST(compileToLLL(get_file_contents("examples/namecoin.se"))) << "\n";
}
