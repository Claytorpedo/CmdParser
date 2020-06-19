#include "CmdParser.hpp"

struct MyOptions { // Make a bundle of options to pass around.
	bool enableSpeedyMode = true;
	bool userConsent = false;
	int numberOfCakes = 0;
	float cakeFraction = 13.4f;
	std::string_view cakeName;
};

int main(int argc, const char* argv[]) {
	MyOptions options;
	cmd::CmdParser cmdParser;
	cmdParser.pushFlag(options.enableSpeedyMode, 's', "enable-speedy-mode", false, "This is a flag.");
	cmdParser.push(options.userConsent, std::nullopt, "consent", true, "Whether you consent.");
	cmdParser.push(options.numberOfCakes, 'n', "num-cakes", 0, "The number of cakes.");
	cmdParser.push(options.cakeFraction, 'f', std::nullopt, 1.0f, "The fraction of cakes.");
	cmdParser.push(options.cakeName, std::nullopt, "cake-name", "Cakeman", "Name your cake.");

	if (!cmdParser.parse(argc, argv, [](auto error) { std::cout << "Error parsing: " << error.message() << "\n"; return cmd::ErrorResult::Continue; })) {
		// If we get something we don't expect, print help.
		cmdParser.printHelp("My test program: Demonstrates example usage.");
		return 1;
	}

	// Use options here.

	return 0;
}
