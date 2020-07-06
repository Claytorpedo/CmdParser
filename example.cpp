#include "CmdParser.hpp"

#include <iostream>

struct MyOptions { // Make a bundle of options to pass around.
	bool enableSpeedyMode = false;
	bool userConsent = true;
	int numberOfCakes = 0;
	float cakeFraction = 1.0f;
	std::string_view cakeName = "Cakeman";
};

int main(int argc, const char* argv[]) {
	MyOptions options;
	cmd::CmdParser cmdParser;
	cmdParser.pushFlag(options.enableSpeedyMode, 's', "enable-speedy-mode", "This is a flag.");
	cmdParser.push(options.userConsent, std::nullopt, "consent", "Whether you consent.");
	cmdParser.push(options.numberOfCakes, 'n', "num-cakes", "The number of cakes.");
	cmdParser.push(options.cakeFraction, 'f', std::nullopt, "The fraction of cakes.");
	cmdParser.push(options.cakeName, std::nullopt, "cake-name", "Name your cake.");

	if (!cmdParser.parse(argc, argv, [](auto error) { std::cout << "Error parsing: " << error.message() << "\n"; })) {
		// If we get something we don't expect, print help.
		std::cout << cmdParser.getHelpText("My test program: Demonstrates example usage.");
		return 1;
	}

	// Use options here.
	std::cout << "\nresults:\n";
	std::cout << "options.enableSpeedyMode: " << (options.enableSpeedyMode ? "true" : "false") << "\n";
	std::cout << "options.userConsent:      " << (options.userConsent ? "true" : "false") << "\n";
	std::cout << "options.numberOfCakes:    " << options.numberOfCakes << "\n";
	std::cout << "options.cakeFraction:     " << options.cakeFraction << "\n";
	std::cout << "options.cakeName:         " << options.cakeName << "\n";
	std::cout << "\n";

	return 0;
}
