#include "definitions.hpp"

namespace {
auto ErrorSink(cmd::Error) { return cmd::ErrorResult::Continue; }
}

SCENARIO("There are no arguments.") {
	cmd::CmdParser cmdParser;
	GIVEN("There is no input.") {
		const char* const args[] = { "" };
		CHECK(cmdParser.parse(1, args, ErrorSink));
	}
	GIVEN("There is no input with a void-returning error handler.") {
		const char* const args[] = {""};
		CHECK(cmdParser.parse(1, args, [](auto) {}));
	}
	GIVEN("There is unexpected input.") {
		const char* const args[] = { "", "-a" };
		CHECK_FALSE(cmdParser.parse(2, args, ErrorSink));
	}
}

SCENARIO("Taking flag arguments.") {
	cmd::CmdParser cmdParser;
	bool flagA = false;
	bool flagB = false;
	bool flagNotC = true;
	cmdParser.pushFlag(flagA, 'a', "flagA", "Flag A");
	cmdParser.pushFlag(flagB, 'b', "flagB", "Flag B");
	cmdParser.pushFlag(flagNotC, 'c', "flagC", "Flag not C");
	GIVEN("Flag A is set.") {
		const char* const args[] = { "", "-a" };
		CHECK(cmdParser.parse(2, args, ErrorSink));
		CHECK(flagA);
		CHECK_FALSE(flagB);
		CHECK(flagNotC);
	}
	GIVEN("Flag B is set.") {
		const char* const args[] = { "", "-b" };
		CHECK(cmdParser.parse(2, args, ErrorSink));
		CHECK_FALSE(flagA);
		CHECK(flagB);
		CHECK(flagNotC);
	}
	GIVEN("Flag C is set.") {
		const char* const args[] = { "", "-c" };
		CHECK(cmdParser.parse(2, args, ErrorSink));
		CHECK_FALSE(flagA);
		CHECK_FALSE(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("All flags are set separately.") {
		const char* const args[] = { "", "-a", "-b", "-c" };
		CHECK(cmdParser.parse(4, args, ErrorSink));
		CHECK(flagA);
		CHECK(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("All flags are set together.") {
		const char* const args[] = { "", "-abc" };
		CHECK(cmdParser.parse(2, args, ErrorSink));
		CHECK(flagA);
		CHECK(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("Two flags are set together, one separate.") {
		const char* const args[] = { "", "-ac", "-b" };
		CHECK(cmdParser.parse(3, args, ErrorSink));
		CHECK(flagA);
		CHECK(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("Flag A is set using its word key, b and c by group.") {
		const char* const args[] = { "", "--flagA", "-bc" };
		CHECK(cmdParser.parse(3, args, ErrorSink));
		CHECK(flagA);
		CHECK(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("All flags are set with word keys.") {
		const char* const args[] = { "", "--flagA", "--flagB", "--flagC" };
		CHECK(cmdParser.parse(4, args, ErrorSink));
		CHECK(flagA);
		CHECK(flagB);
		CHECK_FALSE(flagNotC);
	}
	GIVEN("All flags are set twice.") {
		const char* const args[] = { "", "-abc", "--flagA", "--flagB", "--flagC" };
		THEN("There is no difference from setting them once.") {
			CHECK(cmdParser.parse(5, args, ErrorSink));
			CHECK(flagA);
			CHECK(flagB);
			CHECK_FALSE(flagNotC);
		}
	}

	GIVEN("All flags are set incorrectly.") {
		const char* const args[] = { "", "flagA", "-flagB", "--c" };
		CHECK_FALSE(cmdParser.parse(4, args, ErrorSink));
		CHECK_FALSE(flagA);
		CHECK_FALSE(flagB);
		CHECK(flagNotC);
	}
}

SCENARIO("Taking bool arguments.") {
	cmd::CmdParser cmdParser;
	bool boolA = false;
	bool boolB = false;
	cmdParser.push(boolA, 'a', "boolA", "Bool A");
	cmdParser.push(boolB, 'b', "acceptB", "Bool B");
	GIVEN("Bool A is set.") {
		const char* const args[] = { "", "-a=true" };
		CHECK(cmdParser.parse(2, args, ErrorSink));
		CHECK(boolA);
		CHECK_FALSE(boolB);
	}
	GIVEN("Bool B is set.") {
		const char* const args[] = { "", "--acceptB=yes" };
		CHECK(cmdParser.parse(2, args, ErrorSink));
		CHECK_FALSE(boolA);
		CHECK(boolB);
	}
	GIVEN("Bool A is set to false, and b to true.") {
		const char* const args[] = { "", "--boolA=false", "-b=y" };
		CHECK(cmdParser.parse(3, args, ErrorSink));
		CHECK_FALSE(boolA);
		CHECK(boolB);
	}
}

SCENARIO("Taking arithmetic arguments.") {
	cmd::CmdParser cmdParser;
	int32_t intArg = 0;
	uint32_t unsignedIntArg = 1;
	char charArg = 'a';
	float floatArg = 1.0f;
	int8_t smallIntArg = 0;
	uint8_t smallUintArg = 0;
	int16_t int16 = 0;
	cmdParser.push(intArg, 'i', "int");
	cmdParser.push(unsignedIntArg, 'u', "uint");
	cmdParser.push(charArg, 'c', "char");
	cmdParser.push(floatArg, 'f', "float");
	cmdParser.push(smallIntArg, 's', "small-int");
	cmdParser.push(smallUintArg, std::nullopt, "small-uint");
	cmdParser.push(int16, 't');
	GIVEN("An integer.") {
		const char* const args[] = { "", "-i", "-10" };
		CHECK(cmdParser.parse(3, args, ErrorSink));
		CHECK(intArg == -10);
		CHECK(unsignedIntArg == 1);
		CHECK(charArg == 'a');
		CHECK(floatArg == 1.0f);
		CHECK(smallIntArg == 0);
		CHECK(smallUintArg == 0);
		CHECK(int16 == 0);
	}
	GIVEN("An unsigned integer.") {
		const char* const args[] = { "", "-u", "4000000000" };
		CHECK(cmdParser.parse(3, args, ErrorSink));
		CHECK(intArg == 0);
		CHECK(unsignedIntArg == 4'000'000'000);
		CHECK(charArg == 'a');
		CHECK(floatArg == 1.0f);
		CHECK(smallIntArg == 0);
		CHECK(smallUintArg == 0);
		CHECK(int16 == 0);
	}
	GIVEN("A char.") {
		const char* const args[] = { "", "-c", "G" };
		CHECK(cmdParser.parse(3, args, ErrorSink));
		CHECK(intArg == 0);
		CHECK(unsignedIntArg == 1);
		CHECK(charArg == 'G');
		CHECK(floatArg == 1.0f);
		CHECK(smallIntArg == 0);
		CHECK(smallUintArg == 0);
		CHECK(int16 == 0);
	}
	GIVEN("A float.") {
		const char* const args[] = { "", "-f", "14.897" };
		CHECK(cmdParser.parse(3, args, ErrorSink));
		CHECK(intArg == 0);
		CHECK(unsignedIntArg == 1);
		CHECK(charArg == 'a');
		CHECK(floatArg == ApproxEps(14.897));
		CHECK(smallIntArg == 0);
		CHECK(smallUintArg == 0);
		CHECK(int16 == 0);
	}
	GIVEN("An int8_t.") {
		const char* const args[] = { "", "-s", "-40" };
		CHECK(cmdParser.parse(3, args, ErrorSink));
		CHECK(intArg == 0);
		CHECK(unsignedIntArg == 1);
		CHECK(charArg == 'a');
		CHECK(floatArg == 1.0f);
		CHECK(smallIntArg == -40);
		CHECK(smallUintArg == 0);
		CHECK(int16 == 0);
	}
	GIVEN("A uint8_t.") {
		const char* const args[] = { "", "--small-uint=1" };
		CHECK(cmdParser.parse(2, args, ErrorSink));
		CHECK(intArg == 0);
		CHECK(unsignedIntArg == 1);
		CHECK(charArg == 'a');
		CHECK(floatArg == 1.0f);
		CHECK(smallIntArg == 0);
		CHECK(smallUintArg == 1);
		CHECK(int16 == 0);
	}
	GIVEN("An int16_t.") {
		const char* const args[] = { "", "-t=-10000" };
		CHECK(cmdParser.parse(2, args, ErrorSink));
		CHECK(intArg == 0);
		CHECK(unsignedIntArg == 1);
		CHECK(charArg == 'a');
		CHECK(floatArg == 1.0f);
		CHECK(smallIntArg == 0);
		CHECK(smallUintArg ==  0);
		CHECK(int16 == -10000);
	}
	GIVEN("All are set separately.") {
		const char* const args[] = { "", "--int", "88", "--uint=12", "-c=s", "--float=-98.123", "-s=10", "--small-uint", "40", "-t=100" };
		CHECK(cmdParser.parse(10, args, ErrorSink));
		CHECK(intArg == 88);
		CHECK(unsignedIntArg == 12);
		CHECK(charArg == 's');
		CHECK(floatArg == ApproxEps(-98.123));
		CHECK(smallIntArg == 10);
		CHECK(smallUintArg == 40);
		CHECK(int16 == 100);
	}
	GIVEN("Values in hex format.") {
#ifdef __cpp_lib_to_chars
		const char* const args[] = {"", "-i", "-0xA0", "--uint=0x0F", "--float=0xFF", "--small-int", "0x10", "--small-uint", "0xFF", "-t", "0x8000"};
#else
		const char* const args[] = {"", "-i", "-0xA0", "--uint=0x0F", "--small-int", "0x10", "--small-uint", "0xFF", "-t", "0x8000"};
#endif
		CHECK(cmdParser.parse(sizeof(args) / sizeof(args[0]), args, ErrorSink));
		CHECK(intArg == -160);
		CHECK(unsignedIntArg == 15);
		CHECK(charArg == 'a');
#ifdef __cpp_lib_to_chars
		CHECK(floatArg == 255.0f);
#endif
		CHECK(smallIntArg == 16);
		CHECK(smallUintArg == 255);
		CHECK(int16 == 32767);
	}
	GIVEN("A char when we expect an integer.") {
		const char* const args[] = { "", "--small-int", "a" };
		THEN("Parsing should fail.") {
			CHECK_FALSE(cmdParser.parse(3, args, ErrorSink));
			CHECK(smallIntArg == 0);
		}
	}
}

SCENARIO("Giving overflow values for integer arguments.") {
	cmd::CmdParser cmdParser;
	int32_t intArg = 0;
	int8_t smallIntArg = 0;
	uint8_t smallUintArg = 0;
	int16_t int16 = 0;
	cmdParser.push(intArg, 'i');
	cmdParser.push(smallIntArg, 's');
	cmdParser.push(smallUintArg, 'u');
	cmdParser.push(int16, 't');
	GIVEN("A value larger than an int32_t can hold.") {
		const char* const args[] = { "", "-i", "10000000000000" };
		THEN("The value should default to the max it can hold.") {
			CHECK(cmdParser.parse(3, args, ErrorSink));
			CHECK(intArg == std::numeric_limits<int32_t>::max());
		}
	}
	GIVEN("A value smaller than an int32_t can hold.") {
		const char* const args[] = { "", "-i", "-10000000000000" };
		THEN("The value should default to the min it can hold.") {
			CHECK(cmdParser.parse(3, args, ErrorSink));
			CHECK(intArg == std::numeric_limits<int32_t>::min());
		}
	}
	GIVEN("A value larger than an int8_t can hold.") {
		const char* const args[] = { "", "-s", "200" };
		THEN("The value should default to the max it can hold.") {
			CHECK(cmdParser.parse(3, args, ErrorSink));
			CHECK(smallIntArg == std::numeric_limits<int8_t>::max());
		}
	}
	GIVEN("A value smaller than an int8_t can hold.") {
		const char* const args[] = { "", "-s", "-200" };
		THEN("The value should default to the min it can hold.") {
			CHECK(cmdParser.parse(3, args, ErrorSink));
			CHECK(smallIntArg == std::numeric_limits<int8_t>::min());
		}
	}
	GIVEN("A value larger than a unt8_t can hold.") {
		const char* const args[] = { "", "-u", "256" };
		THEN("The value should default to the max it can hold.") {
			CHECK(cmdParser.parse(3, args, ErrorSink));
			CHECK(smallUintArg == std::numeric_limits<uint8_t>::max());
		}
	}
	GIVEN("A value larger than an int16_t can hold.") {
		const char* const args[] = { "", "-t", "33000" };
		THEN("The value should default to the max it can hold.") {
			CHECK(cmdParser.parse(3, args, ErrorSink));
			CHECK(int16 == std::numeric_limits<int16_t>::max());
		}
	}
	GIVEN("A value smaller than an int16_t can hold.") {
		const char* const args[] = { "", "-t", "-33000" };
		THEN("The value should default to the min it can hold.") {
			CHECK(cmdParser.parse(3, args, ErrorSink));
			CHECK(int16 == std::numeric_limits<int16_t>::min());
		}
	}
}

SCENARIO("Taking string arguments") {
	cmd::CmdParser cmdParser;
	std::string strArg;
	std::string_view strViewArg = "default";
	cmdParser.push(strArg, 's', "str");
	cmdParser.push(strViewArg, 'v', "view", "this is used as a string view");
	GIVEN("A string.") {
		const std::string input = "this is a test string";
		const char* const args[] = { "", "-s", input.c_str() };
		CHECK(cmdParser.parse(3, args, ErrorSink));
		CHECK(strArg == input);
		CHECK(strViewArg == "default");
	}
	GIVEN("A string view.") {
		const char* const args[] = { "", "-v=test string view" };
		CHECK(cmdParser.parse(2, args, ErrorSink));
		CHECK(strArg.empty());
		CHECK(strViewArg == "test string view");
	}
	GIVEN("They are both set.") {
		const char* const args[] = { "", "--str=Test string", "--view", "Test string_view" };
		CHECK(cmdParser.parse(4, args, ErrorSink));
		CHECK(strArg == "Test string");
		CHECK(strViewArg == "Test string_view");
	}
}

SCENARIO("Taking a mixture of argument types.") {
	cmd::CmdParser cmdParser;
	bool flag = false;
	bool boolArg = false;
	int32_t intArg = 0;
	size_t sizeTArg = 0;
	float floatArg = 0;
	std::string_view viewArg;
	cmdParser.pushFlag(flag, 'f');
	cmdParser.push(boolArg, std::nullopt, "bool");
	cmdParser.push(intArg, 'i', "int");
	cmdParser.push(sizeTArg, 's');
	cmdParser.push(floatArg, std::nullopt, "float");
	cmdParser.push(viewArg, 'v');
	GIVEN("No input.") {
		const char* const args[] = { "" };
		CHECK(cmdParser.parse(1, args, ErrorSink));
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
		CHECK(cmdParser.parse(10, args, ErrorSink));
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
		CHECK(cmdParser.parse(6, args, ErrorSink));
		CHECK(flag);
		CHECK(boolArg);
		CHECK(intArg == -9);
		CHECK(sizeTArg == 0);
		CHECK(floatArg == ApproxEps(8));
		CHECK(viewArg.empty());
	}
}
