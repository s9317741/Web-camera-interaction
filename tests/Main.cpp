#include "Check.h"

int main()
{
	const std::vector<check::TestCase>& tests = check::registry();
	for (std::size_t i = 0; i < tests.size(); ++i)
	{
		const int before = check::failures();
		tests[i].body();
		std::cout << (check::failures() == before ? "ok   " : "FAIL ") << tests[i].name << "\n";
	}
	std::cout << tests.size() << " tests, " << check::failures() << " failed checks\n";
	return check::failures() == 0 ? 0 : 1;
}
