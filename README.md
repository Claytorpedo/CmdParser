# CmdParser

Single-header c++17 parser for command line arguments. Just grab CmdParser.hpp.

## Arguments
The parser takes references to variables and populates them if the argument is found.

Integral types, floating point types, bools, and std::string/std::string_view can be added via the `push` function.

`void push(ArithmeticType& argRef, std::optional<char> charKey, std::optional<std::string> wordKey = std::nullopt, std::string description = "", bool verifyUnique = true);`

The first parameter is your variable, the second is a one-character key, to be used like `-c`, the third is a multi-character key to be used like `--my-word-key`, a usage description, and whether it should assert that all added keys are unique (ToDo for the future: it may be possible to turn these into static_asserts once there are constexpr vectors). At least one of char key or word key must be set.

So you can add options like:
```c++
int myIntegerOption{ 0 };
cmdParser.push(myIntegerOption, 'o', "my-option", "My description.");
```
Which could be set with any of:
`-o=10`, `-o 10`, `--my-option=10`, or `--my-option 10`
After calling `cmdParser.parse(argc, argv)`, you can now use any value that was set to `myIntegerOption`.

## Flags
Flags can be added with `pushFlag`. Flags are booleans that do not take a parameter -- they are set if their key is found. Their char keys can also be chained. EG: `-abcd` will set flags with char keys `a`, `b`, `c`, and `d`. They have an additional parameter for default value, which can be set to true if you want activating the flag to result in a false bool.

## Bools
Explicit booleans (rather than flags) expect a parameter to be set. At current:

`true`, `t`, `yes`, `y`, or `1` -> result in true.

`false`, `f`, `no`, `n`, or `0` -> result in false.

Input for these is case-insensitive.

## Example Usage
```c++
#include "CmdParser.hpp"

struct MyOptions { // Make a bundle of options to pass around.
    bool enableSpeedyMode{ true };
    bool userConsent{ false };
    int numberOfCakes{ 0 };
    float cakeFraction{ 13.4f };
    std::string_view cakeName;
};

int main(int argc, const char* argv[]) {
    MyOptions options;
    cmd::CmdParser cmdParser;
    cmdParser.pushFlag(options.enableSpeedyMode, 's', "enable-speedy-mode", "This is a flag.", true);
    cmdParser.push(options.userConsent, std::nullopt, "consent", "Whether you consent.");
    cmdParser.push(options.numberOfCakes, 'n', "num-cakes", "The number of cakes.");
    cmdParser.push(options.cakeFraction, 'f', std::nullopt, "The fraction of cakes.");
    cmdParser.push(options.cakeName, std::nullopt, "cake-name", "Name your cake.");

    if (!cmdParser.parse(argc, argv)) { // If we get something we don't expect, print help.
        cmdParser.printHelp("My test program: Demonstrates example usage.");
        return 1;
    }
    
    // Use options here.

    return 0;
}

```
Example output for bad input:
```
@:/test$ ./a.out these-aren't-my-flags
CmdParser Error: Unrecognized command format "these-aren't-my-flags"
----------------------------------------
My test program: Demonstrates example usage.
Flags:
    -s --enable-speedy-mode ......... This is a flag.
Arguments:
       --consent .................... Whether you consent.
    -n --num-cakes .................. The number of cakes.
    -f .............................. The fraction of cakes.
       --cake-name .................. Name your cake.
```