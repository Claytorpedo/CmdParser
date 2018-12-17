#include "definitions.hpp"

SCENARIO("There are no arguments.") {
	cmd::CmdParser cmdParser;
	GIVEN("There is no input.") {
		const char* const args[] = { "" };
		CHECK(cmdParser.parse(1, args));
	}
	GIVEN("There is unexpected input.") {
		const char* const args[] = { "", "-a" };
		CHECK_FALSE(cmdParser.parse(2, args));
	}
}

SCENARIO("Taking flag arguments.") {
	cmd::CmdParser cmdParser;
	bool flagA;
	bool flagB;
	bool flagNotC;
	cmdParser.pushFlag(flagA, 'a', "flagA", "Flag A");
	cmdParser.pushFlag(flagB, 'b', "flagB", "Flag B");
	cmdParser.pushFlag(flagNotC, 'c', "flagC", "Flag C", true);
	GIVEN("Flag A is set.") {
		const char* const args[] = { "", "-a" };
		CHECK(cmdParser.parse(2, args));
		CHECK(flagA);
		CHECK_FALSE(flagB);
		CHECK(flagNotC);
	}
	GIVEN("Flag B is set.") {
		const char* const args[] = { "", "-b" };
		CHECK(cmdParser.parse(2, args));
		CHECK_FALSE(flagA);
		CHECK(flagB);
		CHECK(flagNotC);
	}
	GIVEN("Flag C is set.") {
		const char* const args[] = { "", "-c" };
		CHECK(cmdParser.parse(2, args));
		CHECK_FALSE(flagA);
		CHECK_FALSE(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("All flags are set separately.") {
		const char* const args[] = { "", "-a", "-b", "-c" };
		CHECK(cmdParser.parse(4, args));
		CHECK(flagA);
		CHECK(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("All flags are set together.") {
		const char* const args[] = { "", "-abc" };
		CHECK(cmdParser.parse(2, args));
		CHECK(flagA);
		CHECK(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("Two flags are set together, one separate.") {
		const char* const args[] = { "", "-ac", "-b" };
		CHECK(cmdParser.parse(3, args));
		CHECK(flagA);
		CHECK(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("Flag A is set using its word key, b and c by group.") {
		const char* const args[] = { "", "--flagA", "-bc" };
		CHECK(cmdParser.parse(3, args));
		CHECK(flagA);
		CHECK(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("All flags are set with word keys.") {
		const char* const args[] = { "", "--flagA", "--flagB", "--flagC" };
		CHECK(cmdParser.parse(4, args));
		CHECK(flagA);
		CHECK(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("All flags are set twice.") {
		const char* const args[] = { "", "-abc", "--flagA", "--flagB", "--flagC" };
		THEN("There is no difference from setting them once.") {
			CHECK(cmdParser.parse(5, args));
			CHECK(flagA);
			CHECK(flagB);
			CHECK_FALSE(flagNotC);
		}
	}

	GIVEN("All flags are set incorrectly.") {
		const char* const args[] = { "", "flagA", "-flagB", "--c" };
		CHECK_FALSE(cmdParser.parse(4, args));
		CHECK_FALSE(flagA);
		CHECK_FALSE(flagB);
		CHECK(flagNotC);
	}
}

SCENARIO("Taking bool arguments.") {
	cmd::CmdParser cmdParser;
	bool boolA{ false };
	bool boolB{ false };
	cmdParser.push(boolA, 'a', "boolA", "Flag A");
	cmdParser.push(boolB, 'b', "acceptB", "Flag B");
	GIVEN("Bool A is set.") {
		const char* const args[] = { "", "-a=true" };
		CHECK(cmdParser.parse(2, args));
		CHECK(boolA);
		CHECK_FALSE(boolB);
	}
	GIVEN("Bool B is set.") {
		const char* const args[] = { "", "--acceptB=yes" };
		CHECK(cmdParser.parse(2, args));
		CHECK_FALSE(boolA);
		CHECK(boolB);
	}
	GIVEN("Bool A is set to false, and b to true.") {
		const char* const args[] = { "", "--boolA=false", "-b=y" };
		CHECK(cmdParser.parse(3, args));
		CHECK_FALSE(boolA);
		CHECK(boolB);
	}
}

SCENARIO("Taking arithmetic arguments.") {
	cmd::CmdParser cmdParser;
	int32_t intArg{ 0 };
	uint32_t unsignedIntArg{ 0 };
	char charArg{ 'a' };
	float floatArg{ 0 };
	cmdParser.push(intArg, 'i', "int");
	cmdParser.push(unsignedIntArg, 'u', "uint");
	cmdParser.push(charArg, 'c', "char");
	cmdParser.push(floatArg, 'f', "float");
	GIVEN("An integer.") {
		const char* const args[] = { "", "-i", "-10" };
		CHECK(cmdParser.parse(3, args));
		CHECK(intArg == -10);
		CHECK(unsignedIntArg == 0);
		CHECK(charArg == 'a');
		CHECK(floatArg == ApproxEps(0.0f));
	}
	GIVEN("An unsigned integer.") {
		const char* const args[] = { "", "-u", "4000000000" };
		CHECK(cmdParser.parse(3, args));
		CHECK(intArg == 0);
		CHECK(unsignedIntArg == 4'000'000'000);
		CHECK(charArg == 'a');
		CHECK(floatArg == ApproxEps(0.0f));
	}
	GIVEN("A char.") {
		const char* const args[] = { "", "-c", "G" };
		CHECK(cmdParser.parse(3, args));
		CHECK(intArg == 0);
		CHECK(unsignedIntArg == 0);
		CHECK(charArg == 'G');
		CHECK(floatArg == ApproxEps(0.0f));
	}
	GIVEN("A float.") {
		const char* const args[] = { "", "-f", "14.897" };
		CHECK(cmdParser.parse(3, args));
		CHECK(intArg == 0);
		CHECK(unsignedIntArg == 0);
		CHECK(charArg == 'a');
		CHECK(floatArg == ApproxEps(14.897));
	}
	GIVEN("All are set separately.") {
		const char* const args[] = { "", "--int", "88", "--uint=12", "-c=s", "--float=-98.123" };
		CHECK(cmdParser.parse(6, args));
		CHECK(intArg == 88);
		CHECK(unsignedIntArg == 12);
		CHECK(charArg == 's');
		CHECK(floatArg == ApproxEps(-98.123));
	}
}

SCENARIO("Taking string arguments") {
	cmd::CmdParser cmdParser;
	std::string strArg;
	std::string_view strViewArg;
	cmdParser.push(strArg, 's', "str");
	cmdParser.push(strViewArg, 'v', "view", "this is used as a string view");
	GIVEN("A string.") {
		const std::string input = "this is a test string";
		const char* const args[] = { "", "-s", input.c_str() };
		CHECK(cmdParser.parse(3, args));
		CHECK(strArg == input);
		CHECK(strViewArg.empty());
	}
	GIVEN("A string view.") {
		const char* const args[] = { "", "-v=test string view" };
		CHECK(cmdParser.parse(2, args));
		CHECK(strArg.empty());
		CHECK(strViewArg == "test string view");
	}
	GIVEN("They are both set.") {
		const char* const args[] = { "", "--str=Test string", "--view", "Test string_view" };
		CHECK(cmdParser.parse(4, args));
		CHECK(strArg == "Test string");
		CHECK(strViewArg == "Test string_view");
	}
}

SCENARIO("Taking a mixture of argument types.") {
	cmd::CmdParser cmdParser;
	bool flag{ false };
	bool boolArg{ false };
	int32_t intArg{ 0 };
	size_t sizeTArg{ 0 };
	float floatArg{ 0 };
	std::string_view viewArg;
	cmdParser.pushFlag(flag, 'f');
	cmdParser.push(boolArg, std::nullopt, "bool");
	cmdParser.push(intArg, 'i', "int");
	cmdParser.push(sizeTArg, 's');
	cmdParser.push(floatArg, std::nullopt, "float");
	cmdParser.push(viewArg, 'v');
	GIVEN("No input.") {
		const char* const args[] = { "" };
		CHECK(cmdParser.parse(1, args));
		THEN("All values are their defaults.") {
			CHECK_FALSE(flag);
			CHECK_FALSE(boolArg);
			CHECK(intArg == 0);
			CHECK(sizeTArg == 0);
			CHECK(floatArg == ApproxEps(0.0f));
			CHECK(viewArg.empty());
		}
	}
	GIVEN("They are all set in a random order.") {
		const char* const args[] = { "", "-i", "100", "-v=StringView", "--float", "-0.17", "-f", "-s", "987654321", "--bool=false" };
		CHECK(cmdParser.parse(10, args));
		THEN("They are all set correctly.") {
			CHECK(flag);
			CHECK_FALSE(boolArg);
			CHECK(intArg == 100);
			CHECK(sizeTArg == 987654321);
			CHECK(floatArg == ApproxEps(-0.17));
			CHECK(viewArg == "StringView");
		}
	}
	GIVEN("A mixture are set.") {
		const char* const args[] = { "", "-f", "--int=-9", "--float", "8", "--bool=t" };
		CHECK(cmdParser.parse(6, args));
		CHECK(flag);
		CHECK(boolArg);
		CHECK(intArg == -9);
		CHECK(sizeTArg == 0);
		CHECK(floatArg == ApproxEps(8));
		CHECK(viewArg.empty());
	}
}
