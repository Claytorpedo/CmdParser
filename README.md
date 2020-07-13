# CmdParser

Single-header c++17 parser for command line arguments. Just grab CmdParser.hpp.

## Arguments
The parser takes references to variables, assigns them a default value, and populates them if the argument is found.

Integral types, floating point types, bools, std::string/std::string_view/const char\* can be added via the `push` function. std::optional arguments are also supported which, despite the name, can be used for required arguments to verify they have been set.

`void push(Type& argRef, std::optional<char> charKey, std::optional<std::string> wordKey = std::nullopt, std::string description = "", bool verifyUnique = true);`

The first parameter is your variable, the second is a one-character key, to be used like `-c`, the third is a multi-character key to be used like `--my-word-key`, a usage description, and whether it should assert that all added keys are unique. At least one of char key or word key should be set. If a an argument is not specified, it will remain whatever default it was given externally.

So you can add options like:
```c++
int myIntegerOption = 0;
cmdParser.push(myIntegerOption, 'o', "my-option", "My description.");
```
Which could be set with any of:
`-o=10`, `-o 10`, `--my-option=10`, or `--my-option 10`
After calling `cmdParser.parse(argc, argv, ErrorHandler)`, you can now use any value that was set to `myIntegerOption`.

Over/underflowing integral arguments will set the argument to max/min value for their type, and will not report an error. The idea here is to respect the user's intent to input a "large number" or "small number", and the programmer's choice of type to contain it.

## Flags
Flags can be added with `pushFlag`. Flags are booleans that do not take a parameter -- they are set if their key is found. Their char keys can also be chained. EG: `-abcd` will set flags with char keys `a`, `b`, `c`, and `d`. If the referenced flag is set to false, then if its option is found it will be set to true, and vice versa. Setting a flag multiple times `-abca` has the same result of setting it once.

## Bools
Explicit booleans (rather than flags) expect a parameter to be set. At current:

`true`, `t`, `yes`, `y`, or `1` -> result in true.

`false`, `f`, `no`, `n`, or `0` -> result in false.

Input for these is case-insensitive.

## Example Usage
```c++
#include "CmdParser.hpp"

#include <iostream>

struct MyOptions { // Make a bundle of options to pass around.
	bool enableSpeedyMode = false;
	bool userConsent = true;
	int numberOfCakes = 0;
	float cakeFraction = 1.0f;
	std::string_view cakeName = "Cakeman";
	std::optional<int> required;
};

int main(int argc, const char* argv[]) {
	MyOptions options;
	cmd::CmdParser cmdParser;
	cmdParser.pushFlag(options.enableSpeedyMode, 's', "enable-speedy-mode", "This is a flag.");
	cmdParser.push(options.userConsent, std::nullopt, "consent", "Whether you consent.");
	cmdParser.push(options.numberOfCakes, 'n', "num-cakes", "The number of cakes.");
	cmdParser.push(options.cakeFraction, 'f', std::nullopt, "The fraction of cakes.");
	cmdParser.push(options.cakeName, std::nullopt, "cake-name", "Name your cake.");
	cmdParser.push(options.required, 'r', "required", "This one is mandatory.");

	const bool success = cmdParser.parse(argc, argv, [](auto error) { std::cout << "Error parsing: " << error.message() << "\n"; });

	constexpr std::string_view description = "My test program: Demonstrates example usage.";

	if (!success) {
		// If we get something we don't expect, print help.
		std::cerr << cmdParser.getHelpText(description);
		return 1;
	}

	if (!options.required) {
		std::cerr << "Oops. Need to specify \"--required\".\n";
		std::cerr << cmdParser.getHelpText(description);
		return 1;
	}

	// Use options here.
	std::cout << "\nresults:\n";
	std::cout << "options.enableSpeedyMode: " << (options.enableSpeedyMode ? "true" : "false") << "\n";
	std::cout << "options.userConsent:      " << (options.userConsent ? "true" : "false") << "\n";
	std::cout << "options.numberOfCakes:    " << options.numberOfCakes << "\n";
	std::cout << "options.cakeFraction:     " << options.cakeFraction << "\n";
	std::cout << "options.cakeName:         " << options.cakeName << "\n";
	std::cout << "options.required:         " << *options.required << "\n";
	std::cout << "\n";

	return 0;
}
```
Example help text:
```
$ ./a.out --num-cakes appricot
Error parsing: Unexpected format for argument "num-cakes" with parameter "appricot".
----------------------------------------
My test program: Demonstrates example usage.
Flags:
 -s --enable-speedy-mode ....[default:   false] This is a flag.
Arguments:
    --consent ...............[default:    true] Whether you consent.
 -n --num-cakes .............[default:       0] The number of cakes.
 -f .........................[default:       1] The fraction of cakes.
    --cake-name .............[default: "Cakeman"] Name your cake.
 -r --required ................................ This one is mandatory.

```
