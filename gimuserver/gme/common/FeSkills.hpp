#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gme
{

/*!
* A unit's acquired SP enhancements, as UserUnitInfo.fe_skill_info (Fnxab5CN)
* carries them.  The format is the client's own parser,
* UserUnitInfoBase::setFeSkillInfo @0x12B7B14:
*
*   ""  or "0"                      none
*   "<cat>@<skill>:<skill>/<cat>@<skill>"
*
* groups split on '/', each split on '@' -- a group with fewer than two fields
* is skipped; the first field goes to the category list (getFeCategory, only
* helper-list icons read it), the second is split on ':' and every id other
* than "" / "0" goes to the skill list isFeSkill and getFeSkillInfoList read.
* Categories are UnitFeSkillMst.category_id, skills FeSkillMst ids.
*
* parse() keeps what the client would keep; str() writes groups in ascending
* category order and each group's skills in the order they were acquired.
*/
struct FeSkillSet
{
	std::vector<std::pair<std::string, std::vector<std::string>>> groups;

	static std::vector<std::string> split(std::string_view text, char sep)
	{
		std::vector<std::string> out;
		size_t start = 0;
		while (true)
		{
			const auto at = text.find(sep, start);
			out.emplace_back(text.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start));
			if (at == std::string_view::npos)
				return out;
			start = at + 1;
		}
	}

	static FeSkillSet parse(std::string_view text)
	{
		FeSkillSet set;
		if (text.empty() || text == "0")
			return set;
		for (const auto& group : split(text, '/'))
		{
			const auto fields = split(group, '@');
			if (fields.size() < 2)
				continue;
			std::vector<std::string> skills;
			for (auto& skill : split(fields[1], ':'))
				if (!skill.empty() && skill != "0")
					skills.push_back(std::move(skill));
			set.groups.emplace_back(fields[0], std::move(skills));
		}
		return set;
	}

	bool has(std::string_view skill) const
	{
		return std::any_of(groups.begin(), groups.end(), [&](const auto& g) {
			return std::find(g.second.begin(), g.second.end(), skill) != g.second.end();
		});
	}

	size_t size() const
	{
		size_t n = 0;
		for (const auto& g : groups)
			n += g.second.size();
		return n;
	}

	void add(const std::string& category, const std::string& skill)
	{
		for (auto& g : groups)
			if (g.first == category)
			{
				g.second.push_back(skill);
				return;
			}
		const auto key = number(category);
		auto at = std::find_if(groups.begin(), groups.end(), [&](const auto& g) {
			return number(g.first) > key;
		});
		groups.insert(at, { category, { skill } });
	}

	std::string str() const
	{
		std::string out;
		for (const auto& [category, skills] : groups)
		{
			if (skills.empty())
				continue;
			if (!out.empty())
				out += '/';
			out += category;
			out += '@';
			for (size_t i = 0; i < skills.size(); ++i)
			{
				if (i)
					out += ':';
				out += skills[i];
			}
		}
		return out;
	}

	/*! A decimal id, or nullopt for anything else (the client's StrToInt would read 0). */
	static std::optional<int64_t> id(std::string_view text)
	{
		int64_t value = 0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		if (error != std::errc{} || end != text.data() + text.size())
			return std::nullopt;
		return value;
	}

private:
	static int64_t number(std::string_view text)
	{
		return id(text).value_or(INT64_MAX);
	}
};

} // namespace gme
