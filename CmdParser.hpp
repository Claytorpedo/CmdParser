#ifndef INCLUDE_CMD_PARSER_HPP
#define INCLUDE_CMD_PARSER_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Both gcc and clang do not supporrt floating point charconv operations yet.
// Use a stringstream as a workaround for now.
#ifndef __cpp_lib_to_chars
#include <sstream>
#endif

namespace cmd {

struct ignore_unknown_args_policy {};
struct error_on_unknown_arg_policy {};

struct Error {
	std::string error;
	const std::string& message() const { return error; }
};

// Can optionally be returned from an error handler to signify early termination cases.
enum class ErrorResult {
	Terminate,
	Continue
};

namespace Detail {

template <typename S>
struct is_string_type : std::disjunction<
	std::is_same<S, std::string>,
	std::is_same<S, std::string_view>,
	std::is_same<S, const char*>>
{};
template <typename S> inline constexpr bool is_string_type_v = is_string_type<S>::value;

template <typename... Args>
auto MakeError(Args&&... args) {
	Error error;
	error.error.reserve((std::size(args) + ...));
	(error.error.append(std::forward<Args>(args)), ...);
	return error;
}

#ifndef __cpp_lib_to_chars
template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
std::from_chars_result fromCharsStringStream(const char* first, const char* last, T& val, [[maybe_unused]] bool isHexFormat) {
	// Can't seem to get floating point working properly with gcc, and I doubt I will ever actually need it.
	std::istringstream stream{std::string(first, last)};
	const bool success = static_cast<bool>(stream >> val);
	const auto read = stream.gcount();
	return {first + read, success ? std::errc{} : std::errc::invalid_argument};
}

template <typename T>
std::string toString(const T val) {
	std::ostringstream stream;
	stream << val;
	return std::move(stream).str();
}
#else
template <typename T>
std::string toString(const T val) {
	constexpr std::size_t BufferSize = 64;
	char buffer[BufferSize];
	const auto [ptr, errc] = std::to_chars(buffer, buffer + BufferSize, val);
	assert(errc == std::errc{}); // Buffer should be large enough.
	return {buffer, static_cast<std::size_t>(std::distance(buffer, ptr))};
}
#endif // !__cpp_lib_to_chars

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
	void pushFlag(bool& flagRef, std::optional<char> charKey, std::optional<std::string> wordKey = std::nullopt, std::string description = "", bool verifyUnique = true) {
		checkValid(charKey, wordKey, verifyUnique);
		flags_.emplace_back(std::move(charKey), std::move(wordKey), flagRef ? "true" : "false", std::move(description), flagRef, flagRef);
	}

	// Bool type where the user needs to specify true or false.
	void push(bool& boolRef, std::optional<char> charKey, std::optional<std::string> wordKey = std::nullopt, std::string description = "", bool verifyUnique = true) {
		checkValid(charKey, wordKey, verifyUnique);
		args_.emplace_back(std::make_unique<BoolArg>(std::move(charKey), std::move(wordKey), boolRef ? "true" : "false", std::move(description), boolRef));
	}

	// Any non-bool integral or floating-point type.
	template <class ArithmeticType, std::enable_if_t<std::is_arithmetic_v<ArithmeticType> && !std::is_same_v<ArithmeticType, bool>, int> = 0>
	void push(ArithmeticType& argRef, std::optional<char> charKey, std::optional<std::string> wordKey = std::nullopt, std::string description = "", bool verifyUnique = true) {
		checkValid(charKey, wordKey, verifyUnique);
		args_.emplace_back(std::make_unique<NumericArg<ArithmeticType>>(std::move(charKey), std::move(wordKey), Detail::toString(argRef), std::move(description), argRef));
	}

	// std::string or std::string_view.
	template <class StringType, std::enable_if_t<Detail::is_string_type_v<StringType>, int> = 0>
	void push(StringType& stringRef, std::optional<char> charKey, std::optional<std::string> wordKey = std::nullopt, std::string description = "", bool verifyUnique = true) {
		checkValid(charKey, wordKey, verifyUnique);
		std::string defaultValStr;

		std::string_view refView;
		if constexpr (std::is_same_v<StringType, const char*>) {
			if (stringRef)
				refView = stringRef;
		} else {
			refView = stringRef;
		}

		if (!refView.empty()) {
			defaultValStr.reserve(refView.size() + 2);
			defaultValStr = '\"';
			defaultValStr += refView;
			defaultValStr += '\"';
		}
		args_.emplace_back(std::make_unique<StringArg<StringType>>(std::move(charKey), std::move(wordKey), std::move(defaultValStr), std::move(description), stringRef));
	}

	// std::optional can be used for required arguments, as clients can ensure they were set after parsing.
	template <class RequiredType>
	void push(std::optional<RequiredType>& ref, std::optional<char> charKey, std::optional<std::string> wordKey = std::nullopt, std::string description = "", bool verifyUnique = true) {
		checkValid(charKey, wordKey, verifyUnique);
		args_.emplace_back(std::make_unique<OptionalArg<RequiredType>>(std::move(charKey), std::move(wordKey), std::move(description), ref));
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

	std::string getHelpText(std::string_view programDescription = {}) {
		std::string helpText;
		helpText.reserve(512);
		helpText = "----------------------------------------\n";
		if (!programDescription.empty()) {
			helpText.append(programDescription);
			helpText += '\n';
		}

		auto writeArg = [&helpText](const auto& arg) {
			helpText += ' '; // Indent the section.
			// Char argument.
			{
				if (arg.charKey) {
					helpText += CharArgDelim;
					helpText += *arg.charKey;
				} else {
					helpText += ' ';
					helpText += ' ';
				}
			}
			// Word argument.
			{
				helpText += ' ';
				constexpr std::string_view Spacer = ".........................";
				if (arg.wordKey) {
					helpText.append(WordArgDelim);
					helpText.append(*arg.wordKey);
					helpText += ' ';
					const std::size_t argSize = WordArgDelim.size() + arg.wordKey->size() + 1;
					if (argSize < Spacer.size())
						helpText.append(Spacer.begin(), std::next(Spacer.begin(), Spacer.size() - argSize));
				} else {
					helpText.append(Spacer);
				}
			}
			// Default value.
			if (!arg.defaultValStr.empty()) {
				constexpr std::string_view DefaultHeader = "[default: ";
				constexpr std::string_view Spacer = "       ";
				constexpr std::string_view DefaultEnd = "] ";
				helpText.append(DefaultHeader);
				if (const auto size = arg.defaultValStr.size(); size < Spacer.size())
					helpText.append(Spacer.begin(), std::next(Spacer.begin(), Spacer.size() - size));
				helpText.append(arg.defaultValStr);
				helpText.append(DefaultEnd);
			} else {
				constexpr std::string_view Spacer = ".................. ";
				helpText.append(Spacer);
			}

			helpText.append(arg.description);
			helpText += '\n';
		};

		if (!flags_.empty()) {
			constexpr std::string_view Flags = "Flags:\n";
			helpText.append(Flags);
			for (const auto& flag : flags_) {
				writeArg(flag);
			}
		}

		if (!args_.empty()) {
			constexpr std::string_view Arguments = "Arguments:\n";
			helpText.append(Arguments);
			for (const auto& arg : args_) {
				writeArg(*arg);
			}
		}
		helpText += '\n';
		return helpText;
	}

	// Must be called after parsing.
	std::string_view getInvokeName() const noexcept { return invoke_name_; }

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

		Arg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& defaultValStr, std::string&& description) noexcept
			: charKey(std::move(charKey)), wordKey(std::move(wordKey)), defaultValStr(std::move(defaultValStr)), description(std::move(description)) {}

		Arg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& description) noexcept
			: charKey(std::move(charKey)), wordKey(std::move(wordKey)), description(std::move(description)) {}

		virtual bool set(std::string_view input) noexcept = 0;
		bool hasCharKey(char key) const noexcept { return charKey && *charKey == key; }
		bool hasWordKey(std::string_view key) const noexcept { return wordKey && *wordKey == key; }
	};

	struct FlagArg : public Arg {
		FlagArg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& defaultValStr, std::string&& desc, bool& arg, bool defaultVal) noexcept
			: Arg{std::move(charKey), std::move(wordKey), std::move(desc), std::move(defaultValStr)}, flagRef(arg), defaultVal(defaultVal) {}

		bool set(std::string_view) noexcept override {
			flagRef = !defaultVal;
			return true;
		}
		bool& flagRef;
		bool defaultVal;
	};

	template <typename RefType>
	static bool trySetBool(RefType& ref, const std::string_view input) noexcept {
		std::string lower(input);
		std::transform(lower.begin(), lower.end(), lower.begin(), [](auto c) { return ::isupper(c) ? static_cast<char>(::tolower(c)) : c; });
		if (std::find_if(TrueStrings.cbegin(), TrueStrings.cend(), [&lower](std::string_view o) { return lower == o; }) != TrueStrings.cend()) {
			ref = true;
		} else if (std::find_if(FalseStrings.cbegin(), FalseStrings.cend(), [&lower](std::string_view o) { return lower == o; }) != FalseStrings.cend()) {
			ref = false;
		} else {
			return false;
		}
		return true;
	}

	template <typename T, typename V = void> struct ref_type { using value_type = typename T::value_type; };
	template <typename T> struct ref_type<T, typename std::enable_if_t<std::is_pod_v<T>>> { using value_type = T; };
	template <typename T> using ref_type_t = typename ref_type<T>::value_type;

	template <typename RefType, typename ResultType>
	static bool trySetArithmeticResult(RefType& ref, ResultType val, std::errc errc, bool isPositive) noexcept {
		using ArgType = ref_type_t<RefType>;
		if (errc == std::errc{}) {
			if (val > (std::numeric_limits<ArgType>::max)()) {
				ref = (std::numeric_limits<ArgType>::max)();
			} else if (val < (std::numeric_limits<ArgType>::lowest)()) {
				ref = (std::numeric_limits<ArgType>::lowest)();
			} else {
				ref = static_cast<ArgType>(val);
			}
			return true;
		} else if (errc == std::errc::result_out_of_range) {
			ref = isPositive ? (std::numeric_limits<ArgType>::max)() : (std::numeric_limits<ArgType>::lowest)();
			return true;
		} else if (errc == std::errc::invalid_argument) {
			// report?
		}
		return false;
	}

	static constexpr std::string_view Hex = "0x";
	static constexpr std::string_view NegativeHex = "-0x";
	static constexpr std::size_t BufferSize = 64;
	static_assert(BufferSize > 3);

	static std::string_view getNegativeHexView(char* buffer, std::string_view input) noexcept {
		buffer[0] = '-';
		const auto begin = std::next(input.begin(), 3);
		const auto end = std::next(begin, std::min(input.size() - 3, BufferSize - 2));
		std::copy(begin, end, buffer + 1);
		return {buffer, std::min(input.size() - 2, BufferSize)};
	};

	enum class NumericStringType {
		General,
		Hex,
		NegativeHex
	};
	static NumericStringType getNumericStringType(std::string_view input) noexcept {
		const auto caseInsensitiveComp = [](char lhs, char rhs) {
			return lhs == rhs || std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
		};
		if (input.size() > 2) {
			if (std::equal(Hex.begin(), Hex.end(), input.begin(), caseInsensitiveComp))
				return NumericStringType::Hex;
			else if (std::equal(NegativeHex.begin(), NegativeHex.end(), input.begin(), caseInsensitiveComp))
				return NumericStringType::NegativeHex;
		}
		return NumericStringType::General;
	}

	template <typename RefType>
	static bool trySetSignedInteger(RefType& ref, std::string_view input) noexcept {
		int base = 10;
		char buffer[BufferSize];
		switch (getNumericStringType(input)) {
			case NumericStringType::General:
				break;
			case NumericStringType::Hex:
				input.remove_prefix(Hex.size());
				if (input.empty())
					return false;
				base = 16;
				break;
			case NumericStringType::NegativeHex:
				input = getNegativeHexView(buffer, input);
				base = 16;
				break;
		}

		std::int64_t val = 0;
		const auto [p, errc] = std::from_chars(input.data(), input.data() + input.size(), val, base);
		return trySetArithmeticResult(ref, val, errc, input[0] == '-');
	}

	template <typename RefType>
	static bool trySetUnsignedInteger(RefType& ref, std::string_view input) noexcept {
		int base = 10;
		switch (getNumericStringType(input)) {
			case NumericStringType::General:
				break;
			case NumericStringType::Hex:
				input.remove_prefix(Hex.size());
				if (input.empty())
					return false;
				base = 16;
				break;
			case NumericStringType::NegativeHex:
				return false;
		}

		std::uint64_t val = 0;
		const auto [p, errc] = std::from_chars(input.data(), input.data() + input.size(), val, base);
		return trySetArithmeticResult(ref, val, errc, input[0] == '-');
	}

	template <typename RefType>
	static bool trySetFloat(RefType& ref, std::string_view input) noexcept {
		bool isHexFormat = false;
		char buffer[BufferSize];
		switch (getNumericStringType(input)) {
			case NumericStringType::General:
				break;
			case NumericStringType::Hex:
				input.remove_prefix(Hex.size());
				if (input.empty())
					return false;
				isHexFormat = true;
				break;
			case NumericStringType::NegativeHex:
				input = getNegativeHexView(buffer, input);
				isHexFormat = true;
				break;
		}

		ref_type_t<RefType> val = 0;
#ifndef __cpp_lib_to_chars
		const auto [p, errc] = Detail::fromCharsStringStream(input.data(), input.data() + input.size(), val, isHexFormat);
#else
		const auto format = isHexFormat ? std::chars_format::hex : std::chars_format::general;
		const auto [p, errc] = from_chars(input.data(), input.data() + input.size(), val, format);
#endif
		return trySetArithmeticResult(ref, val, errc, input[0] == '-');
	}

	template <typename RefType>
	static bool trySetArithmetic(RefType& ref, const std::string_view input) noexcept {
		using ArgType = ref_type_t<RefType>;

		if (input.empty())
			return false;

		// In cases where integer arguments would overflow, prefer setting the min/max value instead of failing.
		if constexpr (!std::is_same_v<ArgType, char> && std::is_integral_v<ArgType>) {
			if constexpr (std::is_unsigned_v<ArgType>) {
				return trySetUnsignedInteger(ref, input);
			} else {
				return trySetSignedInteger(ref, input);
			}
		} else if constexpr (std::is_floating_point_v<ArgType>) {
			return trySetFloat(ref, input);
		} else if constexpr (std::is_same_v<ArgType, char>) {
			if (input.size() != 1)
				return false;
			ref = input[0];
			return true;
		}
	}

	template <typename RefType>
	static bool setStringType(RefType& ref, const std::string_view input) noexcept {
		if constexpr (std::is_same_v<ref_type_t<RefType>, const char*>) {
			ref = input.data();
		} else {
			ref = input;
		}
		return true;
	}

	struct BoolArg : public Arg {
		BoolArg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& defaultValStr, std::string&& desc, bool& arg) noexcept
			: Arg{std::move(charKey), std::move(wordKey), std::move(desc), std::move(defaultValStr)}, boolRef(arg) {}

		bool set(std::string_view input) noexcept final { return trySetBool(boolRef, input); }
		bool& boolRef;
	};

	template <class ArgType>
	struct NumericArg : public Arg {
		NumericArg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& defaultValStr, std::string&& desc, ArgType& arg) noexcept
			: Arg{std::move(charKey), std::move(wordKey), std::move(desc), std::move(defaultValStr)}, argRef(arg) {}

		bool set(std::string_view input) noexcept final { return trySetArithmetic(argRef, input); }
		ArgType& argRef;
	};

	template <class ArgType>
	struct StringArg : public Arg {
		StringArg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& defaultValStr, std::string&& desc, ArgType& arg) noexcept
			: Arg{std::move(charKey), std::move(wordKey), std::move(desc), std::move(defaultValStr)}, stringRef(arg) {}

		bool set(std::string_view input) noexcept final { return setStringType(stringRef, input); }
		ArgType& stringRef;
	};

	template <class ArgType>
	struct OptionalArg : public Arg {
		OptionalArg(std::optional<char>&& charKey, std::optional<std::string>&& wordKey, std::string&& desc, std::optional<ArgType>& arg) noexcept
			: Arg{std::move(charKey), std::move(wordKey), std::move(desc)}, optionalRef(arg) {}

		bool set(std::string_view input) noexcept final {
			if constexpr (std::is_same_v<ArgType, bool>)
				return trySetBool(optionalRef, input);
			else if constexpr (std::is_arithmetic_v<ArgType>)
				return trySetArithmetic(optionalRef, input);
			else if constexpr (Detail::is_string_type_v<ArgType>)
				return setStringType(optionalRef, input);
		}
		std::optional<ArgType>& optionalRef;
	};

	std::vector<std::unique_ptr<Arg>> args_;
	std::vector<FlagArg> flags_;
	std::string invoke_name_;
};

} // cmd

#endif // INCLUDE_CMD_PARSER_HPP
