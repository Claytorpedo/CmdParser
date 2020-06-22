#ifndef INCLUDE_CMD_PARSER_HPP
#define INCLUDE_CMD_PARSER_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
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

struct ignore_unknown_args_policy {};
struct error_on_unknown_arg_policy {};

struct Error {
	std::string error;
	const std::string& message() const { return error; }
};

enum class ErrorResult {
	Terminate,
	Continue
};

namespace Detail {
template <typename... Args>
auto MakeError(Args&&... args) {
	Error error;
	error.error.reserve((std::size(args) + ...));
	(error.error.append(std::forward<Args>(args)), ...);
	return error;
}
} // namespace Detail

class CmdParser {
public:
	static constexpr char             CharArgDelim = '-';
	static constexpr std::string_view WordArgDelim = "--";

	static constexpr char ParamDelim = '=';

	static constexpr std::array<std::string_view, 5> TrueStrings = {"true", "t", "yes", "y", "1"};
	static constexpr std::array<std::string_view, 5> FalseStrings = {"false", "f", "no", "n", "0"};
public:
	CmdParser() = default;

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

	// std::string or std::string_view.
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
	template <typename ErrorHandler,
	          typename UnknownArgPolicy = ignore_unknown_args_policy,
	          std::enable_if_t<std::is_invocable_v<ErrorHandler, Error>, int> = 0>
	bool parse(int argc, const char* const argv[], ErrorHandler errorHandler) {
		using namespace std::literals;
		constexpr bool ReportUnknownArgs = std::is_same_v<UnknownArgPolicy, error_on_unknown_arg_policy>;
		static_assert(std::is_invocable_r_v<void, ErrorHandler, Error> || std::is_invocable_r_v<ErrorResult, ErrorHandler, Error>);

		invoke_name_ = argv[0];
		bool success = true;

		// Returns true if parsing should continue.
		const auto reportErrorAndContinue = [&success, &errorHandler] (auto&&... args) {
			success = false;
			auto error = Detail::MakeError(std::forward<decltype(args)>(args)...);
			if constexpr (std::is_invocable_r_v<void, ErrorHandler, Error>) {
				errorHandler(std::move(error));
				return true; // Continue by default.
			} else {
				return errorHandler(std::move(error)) == ErrorResult::Continue;
			}
		};

		for (int i = 1; i < argc; ++i) {
			const std::string_view input = argv[i];
			if (input.size() > WordArgDelim.size() && input.substr(0, WordArgDelim.size()) == WordArgDelim) { // Word keys.
				std::string_view cmd;
				std::string_view param;
				if (auto optParam = splitCmd(input.substr(WordArgDelim.size()), cmd)) {
					param = *optParam;
				} else if (auto it = std::find_if(flags_.begin(), flags_.end(), [cmd](const auto& flag) { return flag.hasWordKey(cmd); }); it != flags_.end()) {
					// It's a flag.
					it->set(std::string_view{});
					continue;
				} else { // Not a flag: should have a parameter in the next input.
					++i;
					if (i >= argc) {
						reportErrorAndContinue("Unexpected termination. Expected parameter for command \""sv, input, "\"."sv);
						return false;
					}
					param = argv[i];
				}
				if (auto it = std::find_if(args_.begin(), args_.end(), [cmd](const auto& arg) { return arg->hasWordKey(cmd); }); it != args_.end()) {
					if (!it->get()->set(param) && !reportErrorAndContinue("Unexpected format for argument \""sv, cmd, "\" with parameter \""sv, param, "\"."sv))
						return false;
				} else {
					if constexpr (ReportUnknownArgs) {
						if (!reportErrorAndContinue("Unrecognized command \""sv, cmd, "\"."sv))
							return false;
					}
				}
			} else if (input.size() > 1 && input[0] == CharArgDelim) { // Char keys.
				// Look for flags first. There may be multiple chained together.
				bool isFlag = false;
				for (size_t k = 1; k < input.size(); ++k) {
					if (auto it = std::find_if(flags_.begin(), flags_.end(), [c = input[k]](const auto& flag){ return flag.hasCharKey(c); }); it != flags_.end()) {
						it->set(std::string_view{});
						isFlag = true;
					} else {
						if constexpr (ReportUnknownArgs) {
							if (isFlag && !reportErrorAndContinue("Unrecognized flag \""sv, std::string_view{&input[k], 1}, "\"."sv))
								return false;
						}
						break;
					}
				}
				if (isFlag)
					continue;

				// If it wasn't a flag, then it should be a single character command with a parameter.
				std::string_view cmd;
				std::string_view param;
				// Get the command, and see if the parameter is attached to it.
				if (auto optParam = splitCmd(input.substr(1), cmd)) {
					param = *optParam;
				} else { // Find the parameter in the next input, if it exists.
					++i;
					if (i >= argc) {
						reportErrorAndContinue("Unexpected termination. Expected parameter for command \""sv, input, "\"."sv);
						return false;
					}
					param = argv[i];
				}

				if (cmd.size() != 1) {
					if (!reportErrorAndContinue("Command \""sv, input, "\" has an unexpected format."sv))
						return false;
				} else if (auto it = std::find_if(args_.begin(), args_.end(), [c = cmd[0]](const auto& arg) { return arg->hasCharKey(c); }); it != args_.end()) {
					if (!it->get()->set(param)) {
						if (!reportErrorAndContinue("Unexpected format for argument \""sv, cmd, "\" with parameter \""sv, param, "\"."))
							return false;
					}
				} else {
					if constexpr (ReportUnknownArgs) {
						if (!reportErrorAndContinue("Unrecognized command \""sv, cmd, "\"."sv))
							return false;
					}
				}
			} else if (!reportErrorAndContinue("Unrecognized command format \""sv, input, "\"."sv)) {
				return false;
			}
		}
		return success;
	}

	void printHelp(std::string_view programDescription = {}, std::ostream& outstream = std::cout) {
		outstream << "----------------------------------------\n";
		if (!programDescription.empty())
			outstream << programDescription << "\n";

		auto printArg = [&outstream](const auto& arg) {
			outstream << " "; // Indent the section.
			if (arg.charKey) {
				outstream << CharArgDelim << *arg.charKey;
			} else {
				outstream << "  ";
			}

			constexpr std::string_view spacer = " .........................";
			if (arg.wordKey) {
				outstream << std::setfill('.') << std::left << std::setw(spacer.size()) << (" " + std::string(WordArgDelim) + *arg.wordKey + " ");
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
		const auto splitPos = input.find(ParamDelim);
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
			assert(!wordKey->empty() && wordKey->at(0) != CharArgDelim);
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
			if (std::find_if(TrueStrings.cbegin(), TrueStrings.cend(), [&lower](std::string_view o) { return lower == o; }) != TrueStrings.cend()) {
				boolRef = true;
			} else if (std::find_if(FalseStrings.cbegin(), FalseStrings.cend(), [&lower](std::string_view o) { return lower == o; }) != FalseStrings.cend()) {
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

		bool set(const std::string_view input) override {
			// In cases where integer arguments would overflow, prefer setting the min/max value instead of failing.
			if constexpr (!std::is_same_v<ArgType, char> && std::is_integral_v<ArgType>) {
				if constexpr (std::is_unsigned_v<ArgType>) {
					std::uint64_t val = 0;
					const auto [p, errc] = std::from_chars(input.data(), input.data() + input.size(), val);
					if (errc == std::errc{}) {
						if (val > std::numeric_limits<ArgType>::max()) {
							argRef = std::numeric_limits<ArgType>::max();
						} else {
							argRef = static_cast<ArgType>(val);
						}
						return true;
					}
					return false;
				} else {
					std::int64_t val = 0;
					const auto [p, errc] = std::from_chars(input.data(), input.data() + input.size(), val);
					if (errc == std::errc{}) {
						if (val > std::numeric_limits<ArgType>::max()) {
							argRef = std::numeric_limits<ArgType>::max();
						} else if (val < std::numeric_limits<ArgType>::min()) {
							argRef = std::numeric_limits<ArgType>::min();
						} else {
							argRef = static_cast<ArgType>(val);
						}
						return true;
					}
					return false;
				}
			} else if constexpr (std::is_floating_point_v<ArgType>) {
				ArgType val = 0;
				const auto [p, errc] = std::from_chars(input.data(), input.data() + input.size(), val);
				if (errc == std::errc{}) {
					argRef = val;
					return true;
				}
				return false;
			}
			else if constexpr (std::is_same_v<ArgType, char>) {
				if (input.size() != 1)
					return false;
				argRef = input[0];
				return true;
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
};
} // cmd

#endif // INCLUDE_CMD_PARSER_HPP
