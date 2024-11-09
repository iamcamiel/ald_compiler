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

static int numtemps = 0;
static int issuedtemps = 0;

std::vector<std::vector<LINE>> temporaries;

static std::string put_signal(std::string sec, SIG1 s) {
	std::ostringstream oss;
	SIGINFO2 si;
	if (s.second.prefix == "")
		s.second.prefix = sec;
	if (s.second.prefix == sec)
		si.external_ref = false;
	else
		si.external_ref = true;
	if (sections.count(s.second.prefix)) {
		if (sections[s.second.prefix].signals.count(s.first)) {
			si = sections[s.second.prefix].signals[s.first];
		}
	}
	if (s.second.prefix != sec) {
		sections[sec].foreign_signals[s.second.prefix][s.first] = true;
	}
	if (s.second.prefix == "*")
		oss << "EXTERNAL_";
	else
		oss << s.second.prefix;

	if (si.external_ref)
		oss << ".";
	else
		oss << "_INT.";

	oss << safename_signal(sec, s.first);

	return oss.str();
}

static std::string put_signal(std::string sec, SIG1 s, int i) {
	std::ostringstream oss;
	oss << put_signal(sec, s);

	if (s.second.dimensioned) {
		if (s.second.length > 1) {
			if (s.second.order < 0)
				oss << ".B" << s.second.high - i;
			else
				oss << ".B" << s.second.low + i;
		}
		else
			oss << ".B" << s.second.low;
	}
	return oss.str();
}

static void process(std::string sec, std::vector<LINE> lines) {
	bool in_temp = false;
	int temp_ident = 0;
	std::string rest;
	int sig_kind = 1;
	int all_temps = 0;
	int cur_temps = 0;

	if ((rest = getrest(lines[0].line, "SECTION")) != "")
		return;
	if ((rest = getrest(lines[0].line, ";")) != "") {
		if (lines.size() > 1) error_exit(sec, "No indented lines after comment.", lines);
		sections[sec].c_file << "// " << rest << "\n";
	}
	else if ((rest = getrest(lines[0].line, "#")) != "") {
		if (lines.size() > 1) error_exit(sec, "No indented lines after comment.", lines);
		sections[sec].c_file << "// " << rest << "\n";
		sections[sec].h_file << "// " << rest << "\n";
	}
	else if (lines[0].line == "") {
		if (lines.size() > 1) error_exit(sec, "No indented lines after blank line.", lines);
		sections[sec].c_file << "\n";
	}
	else {
		if (lines.size() == 1) error_exit(sec, "Multi-line statement expected.", lines);
		if (lines[0].line == "DECODE") {
			if (lines.size() < 3 || lines.size() > 4) error_exit(sec, "Invalid number of arguments to decoder.", lines);
			SIG1 put = get_signal(sec, lines[1].line, true);

			if (sections[sec].signals.count(put.first) && sections[sec].signals[put.first].eliminated)
				return;
			SIG1 pos = get_signal(sec, lines[2].line, false);
			SIG1 gate = std::make_pair(std::string(""), SIGINFO1());
			if (lines.size() > 3) {
				gate = get_signal(sec, lines[3].line, false);
				if (gate.second.length != 1) error_exit(sec, "Gate dimension error.", lines);
			}
			if (put.second.length != 1 << pos.second.length) error_exit(sec, "Decoder dimension error.", lines);
			if (pos.second.order != 1 || put.second.order != 1) error_exit(sec, "Decoder order error.", lines);
			if (put.second.low != 0) error_exit(sec, "Decoder offset error.", lines);
			for (int i = 0; i < put.second.length; i++) {
				std::vector<LINE> lvec;
				LINE ll;
				ll.ident = 0;
				ll.line = lines[1].line.substr(0, lines[1].line.find_first_of("[")) + "[" + std::to_string(i) + "]";
				ll.is_signal = true;
				ll.signal = get_signal(sec, ll.line, true);
				lvec.push_back(ll);
				ll.ident = 1;
				ll.line = "AND";
				ll.is_signal = false;
				lvec.push_back(ll);
				ll.ident = 2;
				ll.is_signal = true;
				if (gate.first != "") {
					ll.line = lines[3].line;
					ll.signal = get_signal(sec, ll.line, false);
					lvec.push_back(ll);
				}
				for (int j = 0; j < pos.second.length; j++) {
					if ((i >> j) & 1) {
						ll.ident = 2;
					}
					else {
						ll.ident = 2;
						ll.line = "NOT";
						ll.is_signal = false;
						lvec.push_back(ll);
						ll.ident = 3;
					}
					ll.is_signal = true;
					ll.line = lines[2].line.substr(0, lines[2].line.find_first_of("[")) + "[" + std::to_string(pos.second.high - j) + "](" + pos.second.prefix + ")";
					ll.signal = get_signal(sec, ll.line, false);
					lvec.push_back(ll);
				}
				process(sec, lvec);
			}
			return;
		}
		SIG1 output;
		if (lines[0].line == "SPECIAL") {
			output.second.length = 1;
		} else {
			output = get_signal(sec, lines[0].line, true);
			if (output.second.prefix != sec) error_exit(sec, "Can't have a section name in an output signal name.", lines);
			if (sections[sec].signals.count(output.first)) {
				if (sections[sec].signals[output.first].eliminated  || sections[sec].signals[output.first].aliased)
					return;
				SIGINFO1 existing = sections[sec].signals[output.first].basics;
				if (existing.dimensioned != output.second.dimensioned)
					error_exit(sec, "Signal <" + output.first + "> appears both dimensioned and undimensioned.", lines);
				if (existing.dimensioned) {
					if (existing.order != 0) {
						if (output.second.order == 0)
							output.second.order = existing.order;
						if (output.second.order != existing.order)
							error_exit(sec, "Signal <" + output.first + "> appears with different orderings.", lines);
					}
					if (existing.low < output.second.low) output.second.low = existing.low;
					if (existing.high > output.second.high) output.second.high = existing.high;
					output.second.length = output.second.high - output.second.low + 1;
					sections[sec].signals[output.first].basics = output.second;
					output = get_signal(sec, lines[0].line, true);
				}
			}
			else {
				sections[sec].signals[output.first];
				sections[sec].signals[output.first].basics = output.second;
			}
			for (LINE l : lines) {
				if (l.is_signal) {
					if (l.signal.second.length != output.second.length && l.signal.second.length != 1 && output.second.length != 1)
						error_exit(sec, "dimension mismatch in <" + l.line + ">.", lines);
				}
			}
		}
		std::map<int, std::string> stack;
		for (int i = 0; i < output.second.length; i++) {
			int last_ident = 0;
			int prev_ident = 0;
			cur_temps = 0;
			for (LINE l : lines) {
				if (in_temp) {
					if (l.ident > temp_ident) {
						l.ident -= temp_ident;
						if (i == 0) temporaries[numtemps - 1].push_back(l);
						continue;
					}
					else {
						in_temp = false;
					}
				}
				if (l.ident == 0) {
					if (l.is_signal)
						sections[sec].c_file << "  newstate." + put_signal(sec, output, i) + " = ";
					continue;
				}
				if (lines[0].line == "SPECIAL") {
					if (l.line == "IN") {
						sig_kind = 1;
						continue;
					}
					if (l.line == "OUT") {
						sig_kind = 2;
						continue;
					}
					if (l.line == "INOUT") {
						sig_kind = 3;
						continue;
					}
				}
				last_ident = prev_ident;
				while (l.ident <= last_ident) {
					if (stack[last_ident] != "NOT" && stack[last_ident] != "" && stack[last_ident] != "TEMP") {
						sections[sec].c_file << ")";
					}
					stack[last_ident] = "";
					last_ident--;
				}
				if (l.ident <= prev_ident) {
					if (stack[last_ident] == "AND" || stack[last_ident] == "NAND")
						sections[sec].c_file << " && ";
					else if (stack[last_ident] == "ANDNOT")
						sections[sec].c_file << " && !";
					else if (stack[last_ident] == "OR" || stack[last_ident] == "NOR")
						sections[sec].c_file << " || ";
					else if (stack[last_ident] == "ORNOT")
						sections[sec].c_file << " || !";
					else if (stack[last_ident] == "XOR")
						sections[sec].c_file << " != ";
					else if (is_known_word(stack[last_ident]))
						error_exit(sec, "syntax error in logic expression; no second term expected", lines);
					else
						sections[sec].c_file << " , ";
				}
				if (l.is_signal) {
					if (l.signal.first.substr(1) == "temp") {
						stack[l.ident] = "TEMP";
						in_temp = true;
						temp_ident = l.ident;
						if (i == 0) {
							numtemps++;
							all_temps++;
						}
						cur_temps++;
						l.line = l.line.substr(0, 5) + std::to_string(numtemps-all_temps+cur_temps) + l.line.substr(5);
						l.signal.first += std::to_string(numtemps-all_temps+cur_temps);
						if (i == 0) {
							LINE ll = l;
							ll.ident = 0;
							std::vector<LINE> lll;
							lll.push_back(ll);
							temporaries.push_back(lll);
						}
					}
					else {
						l.signal = get_final_signal(sec, l.line);
					}
					if (l.signal.second.length > 1 && output.second.length == 1) {
						if (!is_known_word(stack[last_ident])) {
							if (lines[0].line == "SPECIAL") {
								if (sig_kind & 1)
									sections[sec].c_file << "oldstate." << put_signal(sec, l.signal);
								if (sig_kind == 3)
									sections[sec].c_file << ", ";
								if (sig_kind & 2)
									sections[sec].c_file << "newstate." << put_signal(sec, l.signal);
							}
							else
								sections[sec].c_file << "oldstate." << put_signal(sec, l.signal);
						}
						else {
							sections[sec].c_file << "oldstate." << put_signal(sec, l.signal, 0);
							for (int ii = 1; ii < l.signal.second.length; ii++) {
								if (stack[last_ident] == "AND" || stack[last_ident] == "NAND")
									sections[sec].c_file << " && ";
								else if (stack[last_ident] == "ANDNOT")
									sections[sec].c_file << " && !";
								else if (stack[last_ident] == "OR" || stack[last_ident] == "NOR")
									sections[sec].c_file << " || ";
								else if (stack[last_ident] == "ORNOT")
									sections[sec].c_file << " || !";
								else if (stack[last_ident] == "XOR")
									sections[sec].c_file << " != ";
								else
									error_exit(sec, "syntax error in logic expression; no second term expected", lines);
								sections[sec].c_file << "oldstate." << put_signal(sec, l.signal, ii);
							}
						}
					}
					else {
						if (lines[0].line == "SPECIAL") {
							if (sig_kind & 1)
								sections[sec].c_file << "oldstate." << put_signal(sec, l.signal, 1);
							if (sig_kind == 3)
								sections[sec].c_file << ", ";
							if (sig_kind & 2)
								sections[sec].c_file << "newstate." << put_signal(sec, l.signal, i);
						}
						else
							sections[sec].c_file << "oldstate." << put_signal(sec, l.signal, i);
					}
				}
				else {
					stack[l.ident] = l.line;
					if (l.line == "AND" || l.line == "OR" || l.line == "XOR")
						sections[sec].c_file << "(";
					else if (l.line == "ANDNOT" || l.line == "ORNOT")
						sections[sec].c_file << "(!";
					else if (l.line == "NAND" || l.line == "NOR")
						sections[sec].c_file << "!(";
					else if (l.line == "NOT")
						sections[sec].c_file << "!";
					else if (l.line == "0")
						sections[sec].c_file << "(false";
					else if (l.line == "1")
						sections[sec].c_file << "(true";
					else {
						if (l.line.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos)
							error_exit(sec, "Illegal function name <" + l.line + ">.", lines);
						sections[sec].c_file << l.line << "(";
					}
				}
				prev_ident = l.ident;
			}
			last_ident = prev_ident;
			while (0 <= last_ident) {
				if (stack[last_ident] != "NOT" && stack[last_ident] != "" && stack[last_ident] != "TEMP") {
					sections[sec].c_file << ")";
				}
				stack[last_ident] = "";
				last_ident--;
			}
			sections[sec].c_file << ";\n";
		}
	}
	if (numtemps > issuedtemps) {
		issuedtemps++;
		process(sec, temporaries[issuedtemps - 1]);
	}
}

void write_c_file(std::string sec) {

	sections[sec].c_file << "#include \"360_struc.h\"\n\n";

	sections[sec].c_file << "void process_" << sec << "() {\n";
	process_lines(sec, process, 1);
	sections[sec].c_file << "}\n\n";

	sections[sec].c_file << "void process_" << sec << "_clock() {\n";
	process_lines(sec, process, 2);
	sections[sec].c_file << "}\n\n";
}
