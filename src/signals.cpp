// ALD Compiler
// Copyright(C) 2024 Camiel Vanderhoeven
//
// This program is free software : you can redistribute it and /or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.If not, see < http://www.gnu.org/licenses/>.

#include "ald_compiler.h"

std::map<std::string, SIGINFO2> ext_signals;

std::string safename_signal(std::string sec, std::string s, bool vhdl) {
	std::ostringstream oss;
	if (s.substr(0, 1) == "+") {
		if (vhdl)
			oss << "P_";
		else
			oss << "";
	}
	else if (s.substr(0, 1) == "-") {
		if (vhdl)
			oss << "M_";
		else
			oss << "_";
	}
	else
		error_exit(sec, "signal name must start with a + or a -.", s);
	for (unsigned int i = 1; i < s.length(); i++) {
		if ((s.c_str()[i] >= 'a' && s.c_str()[i] <= 'z') || ((s.c_str()[i] >= '0' && s.c_str()[i] <= '9') && i > 1) || s.c_str()[i] == 'O' || s.c_str()[i] == 'X')
			oss << s.substr(i, 1);
		else if (s.c_str()[i] == ' ') {
			if (i == 1)
				error_exit(sec, "signal name can't start with a space", s);
			oss << "_";
		}
		else if (s.c_str()[i] == '-')
			oss << "M";
		else if (s.c_str()[i] == '+') {
			oss << "P";
		}
		else
			error_exit(sec, "Character <" + s.substr(i, 1) + "> is illegal in a signal name", s);
	}
	return oss.str();
}

SIG1 get_signal(std::string sec, std::string s, bool output) {
	std::string n;
	std::string name;
	SIGINFO1 dim;

	if (s.find_first_of("(") == std::string::npos) {
		dim.prefix = sec;
	}
	else {
		dim.prefix = s.substr(s.find_first_of("(") + 1);
		dim.prefix.erase(dim.prefix.find_first_of(")"));
		s.erase(s.find_first_of("("));
		s.erase(s.find_last_not_of(" ") + 1);
	}
	if (s.find_first_of("[") == std::string::npos) {
		n = s;
		dim.dimensioned = false;
		dim.length = 1;
		dim.order = 0;
		dim.low = 0;
		dim.high = 0;
	}
	else {
		n = s.substr(0, s.find_first_of("["));
		n.erase(n.find_last_not_of(" \n\r\t") + 1);
		std::string d;
		d = s.substr(s.find_first_of("["));
		int c, f, l;
		int i = 1;
		c = d.c_str()[i] - '0';
		if (c < 0 || c > 9) error_exit(sec, "dimension syntax error", s);
		f = c;
		for (;;) {
			i++;
			if (d.c_str()[i] == ']') {
				l = f;
				break;
			}
			if (d.c_str()[i] == '.' && d.c_str()[i + 1] == '.')
				break;
			c = d.c_str()[i] - '0';
			if (c < 0 || c > 9) error_exit(sec, "dimension syntax error", s);
			f = f * 10 + c;
		}
		if (d.c_str()[i] != ']') {
			i += 2;
			c = d.c_str()[i] - '0';
			if (c < 0 || c > 9) error_exit(sec, "dimension syntax error", s);
			l = c;
			for (;;) {
				i++;
				if (d.c_str()[i] == ']') {
					break;
				}
				c = d.c_str()[i] - '0';
				if (c < 0 || c > 9) error_exit(sec, "dimension syntax error", s);
				l = l * 10 + c;
			}
		}
		dim.dimensioned = true;
		if (f == l) {
			dim.order = 0;
			dim.low = f;
			dim.high = f;
		}
		else if (f < l) {
			dim.order = 1;
			dim.low = f;
			dim.high = l;
		}
		else {
			dim.order = -1;
			dim.low = l;
			dim.high = f;
		}
		dim.length = dim.high - dim.low + 1;
	}

	if (dim.prefix == "*" && output) {
		dim.external = true;
		dim.prefix = sec;
	}
	else
		dim.external = false;
	dim.special = false;
	return std::make_pair(n, dim);
}

SIG1 get_final_signal(std::string sec, std::string sig) {
	SIG1 s = get_signal(sec, sig, false);
	if (s.first.substr(1) == "temp")
		return s;
	if (s.second.prefix == "") s.second.prefix = sec;
	if (!sections.count(s.second.prefix)) {
		if (!sections[sec].unknown_sections.count(s.second.prefix)) {
			sections[sec].unknown_sections[s.second.prefix] = true;
			sections[sec].log_cnt++;
			sections[sec].log_file << "Signal(s) in unknown section <" << s.second.prefix << "> encountered.\n";
		}
		return s;
	}
	if (!sections[s.second.prefix].signals.count(s.first)) {
		if (s.second.prefix != sec || s.first.substr(1, 4) != "temp" || s.first.find_first_not_of("0123456789", 5) != std::string::npos) {
			if (!sections[sec].unknown_signals[s.second.prefix].count(s.first)) {
				sections[sec].unknown_signals[s.second.prefix][s.first] = true;
				sections[sec].log_cnt++;
				sections[sec].log_file << "Unknown signal <" << s.second.prefix << "." << s.first << "> encountered.\n";
			}
		}
		return s;
	}
	SIG1 orig_s = s;
	while (sections[s.second.prefix].signals[s.first].aliased) {
		SIGINFO1 a = sections[s.second.prefix].signals[s.first].basics;
		SIG1 b = sections[s.second.prefix].signals[s.first].alias;
		s.second.low += b.second.low - a.low;
		s.second.high += b.second.high - a.high;
		s.second.dimensioned = b.second.dimensioned;
		s.second.order = b.second.order;
		if (b.first == orig_s.first && b.second.prefix == orig_s.second.prefix) {
			error_exit(sec, "Signal <" + orig_s.second.prefix + "." + orig_s.first + "> and <" + b.second.prefix + "." + b.first + "> are involved in a circular reference.", sig);
		}
		s.first = b.first;
		s.second.prefix = b.second.prefix;
		if (s.second.prefix == "") s.second.prefix = sec;
		if (!sections.count(s.second.prefix)) {
			if (!sections[sec].unknown_sections.count(s.second.prefix)) {
				sections[sec].unknown_sections[s.second.prefix] = true;
				sections[sec].log_cnt++;
				sections[sec].log_file << "Signal(s) in unknown section <" << s.second.prefix << "> encountered.\n";
			}
			return s;
		}
		if (!sections[s.second.prefix].signals.count(s.first)) {
			if (s.second.prefix != sec || s.first.substr(1, 4) != "temp" || s.first.find_first_not_of("0123456789", 5) != std::string::npos) {
				if (!sections[sec].unknown_signals[s.second.prefix].count(s.first)) {
					sections[sec].unknown_signals[s.second.prefix][s.first] = true;
					sections[sec].log_cnt++;
					sections[sec].log_file << "Unknown signal <" << s.second.prefix << "." << s.first << "> encountered.\n";
				}
			}
			return s;
		}
	}
	return s;
}

void add_signal(std::string sec, SIG1 s, std::vector<LINE> lines) {
	SIGINFO1 mine;
	SIGINFO1 all;

	mine = s.second;
	if (mine.prefix == "")
		mine.prefix = sec;
	if (mine.prefix == sec) {
		if (sections[sec].signals.count(s.first)) {
			all = sections[sec].signals[s.first].basics;
			if (all.dimensioned != mine.dimensioned)
				error_exit(sec, "Signal <" + s.first + "> appears both dimensioned and undimensioned.", lines);
			if (mine.dimensioned) {
				if (mine.order != 0) {
					if (all.order == 0)
						all.order = mine.order;
					if (all.order != mine.order)
						error_exit(sec, "Signal <" + s.first + "> appears with different orderings.", lines);
				}
				if (mine.low < all.low) all.low = mine.low;
				if (mine.high > all.high) all.high = mine.high;
				all.length = all.high - all.low + 1;
			}
		}
		else
		{
			all = mine;
			sections[sec].signals[s.first];
			sections[sec].signals[s.first].aliased = false;
			sections[sec].signals[s.first].referenced = false;
			sections[sec].signals[s.first].external_ref = false;
		}
		sections[sec].signals[s.first].basics = all;
		for (int i = 0; i < mine.length; i++) {
			if (sections[sec].signals[s.first].assigned.count(i + mine.low))
				error_exit(sec, "Signal <" + s.first + "[" + std::to_string(i + mine.low) + "]> assigned multiple times.", lines);
			sections[sec].signals[s.first].assigned[i + mine.low] = true;
		}
	}
	else {
		error_exit(sec, "Can't have a section name in an output signal name.", lines);
	}
	safename_signal(sec, s.first);
}

void add_ext_signal(std::string sec, SIG1 s, std::vector<LINE> lines) {
	SIGINFO1 mine;
	SIGINFO1 all;

	mine = s.second;
	if (ext_signals.count(s.first)) {
		all = ext_signals[s.first].basics;
		if (all.dimensioned != mine.dimensioned)
			error_exit(sec, "Signal <" + s.first + "> appears both dimensioned and undimensioned.", lines);
		if (mine.dimensioned) {
			if (mine.order != 0) {
				if (all.order == 0)
					all.order = mine.order;
				if (all.order != mine.order)
					error_exit(sec, "Signal <" + s.first + "> appears with different orderings.", lines);
			}
			if (mine.low < all.low) all.low = mine.low;
			if (mine.high > all.high) all.high = mine.high;
			all.length = all.high - all.low + 1;
		}
	}
	else
	{
		all = mine;
		ext_signals[s.first];
		ext_signals[s.first].aliased = false;
		ext_signals[s.first].referenced = false;
		ext_signals[s.first].external_ref = false;
	}
	ext_signals[s.first].basics = all;
	safename_signal(sec, s.first);
}

void ingest(std::string sec, std::vector<LINE> lines) {
	SIG1 s;

	if (lines.size() == 1) return;
	if (lines[0].line == "SPECIAL") return;
	if (lines[0].line == "DECODE")
		s = get_signal(sec, lines[1].line,true);
	else
		s = get_signal(sec, lines[0].line, true);

	add_signal(sec, s, lines);
}

void ingest_special(std::string sec, std::vector<LINE> lines) {
	SIG1 s;
	int sig_kind = 1;

	if (lines.size() == 1) return;
	if (lines[0].line != "SPECIAL") return;

	for (auto& l : lines) {
		if (l.line == "IN") { sig_kind = 1; continue; }
		if (l.line == "OUT") { sig_kind = 2; continue; }
		if (l.line == "INOUT") { sig_kind = 3; continue; }
		if (!l.is_signal) continue;
		if (sig_kind == 1) continue;
		s = get_signal(sec, l.line, sig_kind>1);
		if (sig_kind > 1)
			s.second.special = true;
		add_signal(sec, s, lines);
	}
}

void ingest_signals(std::string sec) {

	process_lines(sec, ingest);
	process_lines(sec, ingest_special);
}