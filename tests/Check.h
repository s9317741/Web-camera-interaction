#ifndef WEBCAM_TESTS_CHECK_H
#define WEBCAM_TESTS_CHECK_H

// Minimal self-registering test harness; no external dependencies.

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace check {

struct TestCase
{
	const char* name;
	void (*body)();
};

inline std::vector<TestCase>& registry()
{
	static std::vector<TestCase> tests;
	return tests;
}

inline int& failures()
{
	static int count = 0;
	return count;
}

struct Registrar
{
	Registrar(const char* name, void (*body)())
	{
		TestCase t = { name, body };
		registry().push_back(t);
	}
};

inline void fail(const char* file, int line, const std::string& message)
{
	++failures();
	std::cerr << file << ":" << line << ": " << message << "\n";
}

} // namespace check

#define CHECK_CONCAT2(a, b) a##b
#define CHECK_CONCAT(a, b) CHECK_CONCAT2(a, b)

#define TEST(name)                                                              \
	static void name();                                                         \
	static check::Registrar CHECK_CONCAT(registrar_, name)(#name, &name);      \
	static void name()

#define CHECK(expr)                                                             \
	do                                                                          \
	{                                                                           \
		if (!(expr))                                                            \
			check::fail(__FILE__, __LINE__, "CHECK(" #expr ") failed");         \
	} while (0)

#define CHECK_EQ(expected, actual)                                              \
	do                                                                          \
	{                                                                           \
		const long long e_ = static_cast<long long>(expected);                  \
		const long long a_ = static_cast<long long>(actual);                    \
		if (e_ != a_)                                                           \
		{                                                                       \
			std::ostringstream os_;                                             \
			os_ << "CHECK_EQ(" #expected ", " #actual "): expected " << e_      \
				<< ", got " << a_;                                              \
			check::fail(__FILE__, __LINE__, os_.str());                         \
		}                                                                       \
	} while (0)

#endif
