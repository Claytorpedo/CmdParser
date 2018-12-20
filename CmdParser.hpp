#pragma once
#ifndef INCLUDE_CMD_PARSER_HPP
#define INCLUDE_CMD_PARSER_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <sstream>
#include <type_traits>
#include <vector>

namespace cmd {
	class CmdParser {
	public:
		static constexpr char             s_charArgDelim = '-';
		static constexpr std::string_view s_wordArgDelim = "--";

		static constexpr char             s_paramDelim = '=';

		static constexpr std::array<std::string_view, 5> s_trueStrings = { "true", "t", "yes", "y", "1" };
		static constexpr std::array<std::string_view, 5> s_falseStrings = { "false", "f", "no", "n", "0" };
	public:
		CmdParser() = default;
		CmdParser(std::ostream& errorLogger) : error_logger_(errorLogger) {}

		// Set to the opposite of defaultVal if present.
		void pushFlag(bool& flagRef, std::optional<char> charKey, std::optional<std::string> wordKey = std::nullopt, bool defaultVal = false, std::string description = "", bool verifyUnique = true) {
			flagRef = defaultVal;
			checkValid(charKey, wordKey, verifyUnique);
			flags_.emplace_back(std::move(charKey), std::move(wordKey), defaultVal ? "true" : "false", std::move(description), flagRef, defaultVal);
		}

		// Bool type where the user needs to specify true or false. 
		void push(bool& boolRef, std::optional<char> charKey, std::optional<std::string> wordKey = std::nullopt, bool defaultVal = false, std::string description = "", bool verifyUnique = true) {
			boolRef = defaultVal;
			checkValid(charKey, wordKey, verifyUnique);
			args_.emplace_back(std::make_unique<BoolArg>(std::move(charKey), std::move(wordKey), defaultVal ? "true" : "false", std::move(description), boolRef));
		}

		// Any non-bool integral or floating-point type.
		template <class ArithmeticType>
		typename std::enable_if<std::is_arithmetic_v<ArithmeticType> && !std::is_same_v<ArithmeticType, bool>>::
		type push(ArithmeticType& argRef, std::optional<char> charKey, std::optional<std::string> wordKey = std::nullopt, ArithmeticType defaultVal = 0, std::string description = "", bool verifyUnique = true) {
			argRef = defaultVal;
			checkValid(charKey, wordKey, verifyUnique);
			std::ostringstream stream;
			stream << defaultVal;
			args_.emplace_back(std::make_unique<NumericArg<ArithmeticType>>(std::move(charKey), std::move(wordKey), stream.str(), std::move(description), argRef));
		}

		// String or string_view.
		template <class StringType>
		typename std::enable_if<std::is_same_v<StringType, std::string> || std::is_same_v<StringType, std::string_view>>::
		type push(StringType& stringRef, std::optional<char> charKey, std::optional<std::string> wordKey = std::nullopt, std::string_view defaultVal = "", std::string description = "", bool verifyUnique = true) {
			stringRef = defaultVal;
			checkValid(charKey, wordKey, verifyUnique);
			std::string defaultValStr;
			defaultValStr.reserve(defaultVal.size() + 2);
			defaultValStr = "\"";
			defaultValStr += defaultVal;
			defaultValStr += "\"";
			args_.emplace_back(std::make_unique<StringArg<StringType>>(std::move(charKey), std::move(wordKey), std::move(defaultValStr), std::move(description), stringRef));
		}

		// Returns false if an error is encountered.
		bool parse(int argc, const char* const argv[]) {
			invoke_name_ = argv[0];
			bool success = true;
			for (int i = 1; i < argc; ++i) {
				const std::string_view input = argv[i];
				if (input.size() > s_wordArgDelim.size() && input.substr(0, s_wordArgDelim.size()) == s_wordArgDelim) { // Word keys.
					std::string_view cmd, param;
					if (auto optParam = splitCmd(input.substr(s_wordArgDelim.size()), cmd)) {
						param = *optParam;
					} else if (auto it = std::find_if(flags_.begin(), flags_.end(), [cmd](const auto& flag) { return flag.hasWordKey(cmd); }); it != flags_.end()) { // No param found: might be flag.
						it->set(std::string_view{});
						continue;
					} else { // Not a flag: should have a parameter in the next input.
						++i;
						if (i >= argc) {
							error_logger_ << "CmdParser Error: Unexpected termination, expected parameter.\n";
							return false;
						}
						param = argv[i];
					}
					if (auto it = std::find_if(args_.begin(), args_.end(), [cmd](const auto& arg) { return arg->hasWordKey(cmd); }); it != args_.end()) {
						if (!it->get()->set(param)) {
							error_logger_ << "CmdParser Error: Unexpected format for argument \"" << cmd << "\" with parameter \"" << param << "\"\n";
							success = false;
						}
					} else {
						error_logger_ << "CmdParser Error: Unrecognized command \"" << cmd << "\"\n";
						success = false;
					}
				} else if (input.size() > 1 && input[0] == s_charArgDelim) { // Char keys.
					// Look for flags first. There may be multiple chained together.
					bool isFlag = false;
					for (size_t k = 1; k < input.size(); ++k) {
						if (auto it = std::find_if(flags_.begin(), flags_.end(), [c = input[k]](const auto& flag){ return flag.hasCharKey(c); }); it != flags_.end()) {
							it->set(std::string_view{});
							isFlag = true;
						} else {
							if (isFlag) {
								error_logger_ << "CmdParser Error: Unrecognized flag \"" << input[k] << "\"\n";
								success = false;
							}
							break;
						}
					}
					if (isFlag)
						continue;

					// If it wasn't a flag, then it should be a single character command with a parameter.
					std::string_view cmd, param;
					// Get the command, and see if the parameter is attached to it.
					if (auto optParam = splitCmd(input.substr(1), cmd)) {
						param = *optParam;
					} else { // Find the parameter in the next input, if it exists.
						++i;
						if (i >= argc) {
							error_logger_ << "CmdParser Error: Unexpected termination, expected parameter.\n";
							return false;
						}
						param = argv[i];
					}

					if (cmd.size() != 1) {
						error_logger_ << "CmdParser Error: Command \"" << input << "\" has an unexpected format.\n";
						success = false;
					} else if (auto it = std::find_if(args_.begin(), args_.end(), [c = cmd.at(0)](const auto& arg) { return arg->hasCharKey(c); }); it != args_.end()) {
						if (!it->get()->set(param)) {
							error_logger_ << "CmdParser Error: Unexpected format for argument \"" << cmd << "\" with parameter \"" << param << "\"\n";
							success = false;
						}
					} else {
						error_logger_ << "CmdParser Error: Unrecognized command \"" << cmd << "\"\n";
						success = false;
					}
				} else {
					error_logger_ << "CmdParser Error: Unrecognized command format \"" << input << "\"\n";
					success = false;
				}
			}
			return success;
		}

		void printHelp(std::optional<std::string> programDescription = std::nullopt, std::ostream& outstream = std::cout) {
			outstream << "----------------------------------------\n";
			if (programDescription)
				outstream << *programDescription << "\n";

			auto printArg = [&outstream](const auto& arg) {
				outstream << " "; // Indent the section.
				if (arg.charKey) {
					outstream << s_charArgDelim << *arg.charKey;
				} else {
					outstream << "  ";
				}

				constexpr std::string_view spacer = " .........................";
				if (arg.wordKey) {
					outstream << std::setfill('.') << std::left << std::setw(spacer.size()) << (" " + std::string(s_wordArgDelim) + *arg.wordKey + " ");
				} else {
					outstream << spacer;
				}
				outstream << "[default: " << std::setfill(' ') << std::right << std::setw(8) << arg.defaultValStr << "] " << arg.description << "\n";
			};

			if (!flags_.empty()) {
				outstream << "Flags:\n";
				for (const auto& flag : flags_) {
					printArg(flag);
				}
			}

			if (!args_.empty()) {
				outstream << "Arguments:\n";
				for (const auto& arg : args_) {
					printArg(*arg);
				}
			}
			outstream << std::endl;
		}

		// Must be called after parsing.
		std::string_view getInvokeName() { return invoke_name_ ? *invoke_name_ : "Invoke name not parsed!"; }

	private:
		// Returns an attached parameter if found.
		static constexpr std::optional<std::string_view> splitCmd(std::string_view input, std::string_view& out_cmd) {
			const auto splitPos = input.find(s_paramDelim);
			out_cmd = input.substr(0, splitPos);
			if (splitPos >= input.size() + 1)
				return std::nullopt;
			return input.substr(splitPos + 1);
		}

	private:
		void checkUnique(std::optional<char> charKey, std::optional<std::string> wordKey) {
			if (charKey) {
				for (const auto& flag : flags_) {
					assert(!flag.charKey || *flag.charKey != *charKey);
				}
				for (const auto& arg : args_) {
					assert(!arg->charKey || *arg->charKey != *charKey);
				}
			}
			if (wordKey) {
				for (const auto& flag : flags_) {
					assert(!flag.wordKey || *flag.wordKey != *wordKey);
				}
				for (const auto& arg : args_) {
					assert(!arg->wordKey || *arg->wordKey != *wordKey);
				}
			}
		}

		void checkValid(std::optional<char> charKey, std::optional<std::string> wordKey, bool verifyUnique) {
			assert(charKey || wordKey); // At least one should be set.
			if (wordKey)
				assert(!wordKey->empty() && wordKey->at(0) != s_charArgDelim);
			if (verifyUnique)
				checkUnique(charKey, wordKey);
		}

		struct Arg {
			std::optional<char> charKey = std::nullopt;
			std::optional<std::string> wordKey = std::nullopt;
			std::string defaultValStr;
			std::string description;
			Arg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& defaultValStr, std::string&& description)
				: charKey(std::move(charKey)), wordKey(std::move(wordKey)), defaultValStr(std::move(defaultValStr)), description(std::move(description)) {}

			virtual bool set(std::string_view input) = 0;
			bool hasCharKey(char key)             const { return charKey && *charKey == key; }
			bool hasWordKey(std::string_view key) const { return wordKey && *wordKey == key; }
		};

		struct FlagArg : public Arg {
			FlagArg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& defaultValStr, std::string&& desc, bool& arg, bool defaultVal)
				: Arg(std::move(charKey), std::move(wordKey), std::move(defaultValStr), std::move(desc)), flagRef(arg), defaultVal(defaultVal) {}
			bool set(std::string_view) override {
				flagRef = !defaultVal;
				return true;
			}
			bool& flagRef;
			bool defaultVal;
		};

		struct BoolArg : public Arg {
			BoolArg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& defaultValStr, std::string&& desc, bool& arg)
				: Arg(std::move(charKey), std::move(wordKey), std::move(defaultValStr), std::move(desc)), boolRef(arg) {}
			bool set(std::string_view input) override {
				std::string lower(input);
				std::transform(lower.begin(), lower.end(), lower.begin(), [](auto c) { return ::isupper(c) ? static_cast<char>(::tolower(c)) : c; });
				if (std::find_if(s_trueStrings.cbegin(), s_trueStrings.cend(), [&lower](std::string_view o) { return lower == o; }) != s_trueStrings.cend()) {
					boolRef = true;
				} else if (std::find_if(s_falseStrings.cbegin(), s_falseStrings.cend(), [&lower](std::string_view o) { return lower == o; }) != s_falseStrings.cend()) {
					boolRef = false;
				} else {
					return false;
				}
				return true;
			}
			bool& boolRef;
		};

		template <class ArgType>
		struct NumericArg : public Arg {
			NumericArg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& defaultValStr, std::string&& desc, ArgType& arg)
				: Arg(std::move(charKey), std::move(wordKey), std::move(defaultValStr), std::move(desc)), argRef(arg) {}
			bool set(std::string_view input) override {
				std::istringstream stream{ std::string(input) };
				// istringstream will treat all 8-bit types like chars, giving unexpected results with signed and unsigned chars.
				// In these cases, input into an integer-type and cast the value back.
				if constexpr(std::is_same_v<ArgType, uint8_t>) {
					uint32_t in;
					const bool success = static_cast<bool>(stream >> in);
					if (success)
						argRef = static_cast<uint8_t>(in);
					return success;
				} else if constexpr(std::is_same_v<ArgType, int8_t>) {
					int32_t in;
					const bool success = static_cast<bool>(stream >> in);
					if (success)
						argRef = static_cast<int8_t>(in);
					return success;
				}
				else {
					return static_cast<bool>(stream >> argRef);
				}
			}
			ArgType& argRef;
		};

		template <class ArgType>
		struct StringArg : public Arg {
			StringArg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& defaultValStr, std::string&& desc, ArgType& arg)
				: Arg(std::move(charKey), std::move(wordKey), std::move(defaultValStr), std::move(desc)), stringRef(arg) {}
			bool set(std::string_view input) override {
				stringRef = input;
				return true;
			}
			ArgType& stringRef;
		};

		std::vector<std::unique_ptr<Arg>> args_;
		std::vector<FlagArg>              flags_;
		std::optional<std::string>        invoke_name_ = std::nullopt;
		std::ostream&                     error_logger_ = std::cerr;
	};
}

#endif // INCLUDE_CMD_PARSER_HPP
