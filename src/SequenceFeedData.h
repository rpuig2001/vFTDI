#pragma once

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace SequenceFeedData {
	struct Entry {
		std::string Callsign;
		int RunwaySequence;
	};

	inline bool ReplaceSequence(std::vector<std::string>& sequence, const std::vector<Entry>& entries)
	{
		std::vector<std::string> updated;
		updated.reserve(entries.size());
		for (const Entry& entry : entries)
			updated.push_back(entry.Callsign);
		if (sequence == updated)
			return false;
		sequence.swap(updated);
		return true;
	}

	class Parser {
	public:
		explicit Parser(const std::string& json) : Json(json) {}

		bool Parse(std::vector<Entry>& entries)
		{
			SkipWhitespace();
			if (!Consume('{'))
				return false;
			bool hasSequence = false;
			SkipWhitespace();
			if (Consume('}'))
				return false;
			while (true) {
				std::string key;
				if (!ParseString(key))
					return false;
				SkipWhitespace();
				if (!Consume(':'))
					return false;
				SkipWhitespace();
				if (key == "sequence") {
					if (hasSequence || !ParseSequence(entries))
						return false;
					hasSequence = true;
				}
				else if (!SkipValue(0)) {
					return false;
				}
				SkipWhitespace();
				if (Consume('}'))
					break;
				if (!Consume(','))
					return false;
				SkipWhitespace();
			}
			SkipWhitespace();
			if (!hasSequence || Position != Json.size())
				return false;
			std::stable_sort(entries.begin(), entries.end(), [](const Entry& first, const Entry& second) {
				return first.RunwaySequence < second.RunwaySequence;
			});
			std::set<std::string> seen;
			entries.erase(std::remove_if(entries.begin(), entries.end(), [&seen](const Entry& entry) {
				return !seen.insert(entry.Callsign).second;
			}), entries.end());
			return true;
		}

	private:
		const std::string& Json;
		size_t Position = 0;
		unsigned int Depth = 0;

		void SkipWhitespace()
		{
			while (Position < Json.size() && (Json[Position] == ' ' || Json[Position] == '\t' ||
				Json[Position] == '\r' || Json[Position] == '\n'))
				++Position;
		}

		bool Consume(char character)
		{
			if (Position >= Json.size() || Json[Position] != character)
				return false;
			++Position;
			return true;
		}

		bool ParseString(std::string& value)
		{
			SkipWhitespace();
			if (!Consume('"'))
				return false;
			value.clear();
			while (Position < Json.size()) {
				const unsigned char character = static_cast<unsigned char>(Json[Position++]);
				if (character == '"')
					return true;
				if (character < 0x20)
					return false;
				if (character != '\\') {
					value.push_back(static_cast<char>(character));
					continue;
				}
				if (Position >= Json.size())
					return false;
				const char escape = Json[Position++];
				switch (escape) {
				case '"': case '\\': case '/': value.push_back(escape); break;
				case 'b': value.push_back('\b'); break;
				case 'f': value.push_back('\f'); break;
				case 'n': value.push_back('\n'); break;
				case 'r': value.push_back('\r'); break;
				case 't': value.push_back('\t'); break;
				case 'u':
					if (Json.size() - Position < 4)
						return false;
					for (int digit = 0; digit < 4; ++digit) {
						const char hex = Json[Position++];
						if (!((hex >= '0' && hex <= '9') || (hex >= 'a' && hex <= 'f') ||
							(hex >= 'A' && hex <= 'F')))
							return false;
					}
					value.push_back('?');
					break;
				default: return false;
				}
			}
			return false;
		}

		bool ParseInteger(int& value)
		{
			SkipWhitespace();
			const size_t start = Position;
			if (Position < Json.size() && Json[Position] == '-')
				++Position;
			const size_t digits = Position;
			while (Position < Json.size() && Json[Position] >= '0' && Json[Position] <= '9')
				++Position;
			if (Position == digits || (Position < Json.size() &&
				(Json[Position] == '.' || Json[Position] == 'e' || Json[Position] == 'E')))
				return false;
			char* end = nullptr;
			const long parsed = std::strtol(Json.c_str() + start, &end, 10);
			if (end != Json.c_str() + Position || parsed < 1 || parsed > INT_MAX)
				return false;
			value = static_cast<int>(parsed);
			return true;
		}

		bool ParseSequence(std::vector<Entry>& entries)
		{
			if (!Consume('['))
				return false;
			SkipWhitespace();
			if (Consume(']'))
				return true;
			while (true) {
				Entry entry{};
				if (!ParseEntry(entry))
					return false;
				if (!entry.Callsign.empty() && entry.RunwaySequence > 0)
					entries.push_back(std::move(entry));
				SkipWhitespace();
				if (Consume(']'))
					return true;
				if (!Consume(','))
					return false;
				SkipWhitespace();
			}
		}

		bool ParseEntry(Entry& entry)
		{
			if (!Consume('{'))
				return false;
			SkipWhitespace();
			if (Consume('}'))
				return true;
			while (true) {
				std::string key;
				if (!ParseString(key))
					return false;
				SkipWhitespace();
				if (!Consume(':'))
					return false;
				SkipWhitespace();
				if (key == "callsign") {
					if (!ParseString(entry.Callsign))
						return false;
				}
				else if (key == "rwy_seq") {
					if (!ParseInteger(entry.RunwaySequence))
						return false;
				}
				else if (!SkipValue(0)) {
					return false;
				}
				SkipWhitespace();
				if (Consume('}'))
					return true;
				if (!Consume(','))
					return false;
				SkipWhitespace();
			}
		}

		bool SkipValue(unsigned int depth)
		{
			if (depth > 64 || Position >= Json.size())
				return false;
			SkipWhitespace();
			if (Position >= Json.size())
				return false;
			if (Json[Position] == '"') {
				std::string ignored;
				return ParseString(ignored);
			}
			if (Json[Position] == '{' || Json[Position] == '[') {
				const char open = Json[Position++];
				const char close = open == '{' ? '}' : ']';
				SkipWhitespace();
				if (Consume(close))
					return true;
				while (true) {
					if (open == '{') {
						std::string ignoredKey;
						if (!ParseString(ignoredKey))
							return false;
						SkipWhitespace();
						if (!Consume(':'))
							return false;
					}
					if (!SkipValue(depth + 1))
						return false;
					SkipWhitespace();
					if (Consume(close))
						return true;
					if (!Consume(','))
						return false;
					SkipWhitespace();
				}
			}
			const size_t start = Position;
			while (Position < Json.size() && Json[Position] != ',' && Json[Position] != ']' &&
				Json[Position] != '}' && Json[Position] != ' ' && Json[Position] != '\t' &&
				Json[Position] != '\r' && Json[Position] != '\n')
				++Position;
			return Position > start;
		}
	};

	inline bool Parse(const std::string& json, std::vector<Entry>& entries)
	{
		entries.clear();
		return Parser(json).Parse(entries);
	}
}
