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

int colla;

void xref(std::string sec, std::vector<LINE> lines) {
	SIG1 s;
	unsigned int ln;
	int sig_kind = 1;

	if (lines.size() == 1) return;

	if (lines[0].line == "SPECIAL") {
		ln = 1;
	} else {
		if (lines[0].line == "DECODE") {
			s = get_signal(sec, lines[1].line,true);
			ln = 2; // skip line 1 (output)
		}
		else {
			if (!lines[0].is_signal)
				return;
			s = get_signal(sec, lines[0].line,true);
			ln = 1; // skip line 1 (output)
		}

		if (sections[s.second.prefix].signals[s.first].eliminated)
			return;
	}
	for (; ln < lines.size(); ln++) {
		if (lines[0].line == "SPECIAL") {
			if (lines[ln].line == "IN")
				sig_kind = 1;
			if (lines[ln].line == "OUT")
				sig_kind = 2;
			if (lines[ln].line == "INOUT")
				sig_kind = 3;
		}
		if (!lines[ln].is_signal)
			continue;
		if (sig_kind > 1)
			continue;
		s = get_final_signal(sec, lines[ln].line);
		if (s.first.substr(1) == "temp")
			continue;

		if (s.second.prefix == "*") {
			add_ext_signal(sec,s,lines);
			continue;
		}
		if (!sections.count(s.second.prefix) || !sections[s.second.prefix].signals.count(s.first))
			continue;

		if (s.second.low < sections[s.second.prefix].signals[s.first].basics.low || s.second.high > sections[s.second.prefix].signals[s.first].basics.high) {
			sections[sec].log_cnt++;
			sections[sec].log_file << "Signal <" << s.second.prefix << "." << s.first << "[" << std::to_string(s.second.low) << ".." << std::to_string(s.second.high) << "]> falls outside of expected ["
				<< std::to_string(sections[s.second.prefix].signals[s.first].basics.low) << ".." << std::to_string(sections[s.second.prefix].signals[s.first].basics.high) << "]\n";
		}
		sections[s.second.prefix].signals[s.first].referenced = true;
		if (s.second.prefix != sec)
			sections[s.second.prefix].signals[s.first].external_ref = true;
		for (int i = s.second.low; i <= s.second.high; i++) {
			sections[s.second.prefix].signals[s.first].refd[i] = true;
		}
	}
}

void clps(std::string sec, std::vector<LINE> lines) {
	SIG1 s1, s2;

	if (!lines[0].is_signal)
		return;

	if (lines.size() != 2 || !lines[0].is_signal || !lines[1].is_signal)
		return;
	
	s1 = get_signal(sec, lines[0].line,true);
	if (s1.second.external)
		return;
	s2 = get_signal(sec, lines[1].line, false);

	SIGINFO2& si1 = sections[sec].signals[s1.first];

	if (s1.second.low != si1.basics.low || s1.second.high != si1.basics.high)
		return;
	si1.aliased = true;
	si1.alias = s2;
	colla++;
}

void xref_signals() {
	int elim;
	if (collapse) {
		colla = 0;
		for (auto& sec : sections) {
			process_lines(sec.first, clps);
		}
		std::cout << "Collapsed " << std::to_string(colla) << " signals.\n";
	}
	if (eliminate) {
		do {
			elim = 0;
			for (auto& sec : sections) {
				for (auto& sig : sec.second.signals)
					if (sig.second.basics.external)
						sig.second.external_ref = sig.second.referenced = true;
				process_lines(sec.first, xref);
			}
			for (auto& sec : sections) {
				for (auto& ss : sec.second.signals) {
					if ((!ss.second.referenced) && (!ss.second.eliminated)) {
						ss.second.eliminated = true;
						sec.second.inf_cnt++;
						sec.second.inf_file << "Signal " << ss.first << " eliminated.\n";
						elim++;
					}
					ss.second.referenced = false;
				}
			}
			std::cout << "Eliminated " << std::to_string(elim) << " signals.\n";
		} while (elim);
	}
	for (auto& sec : sections) {
		for (auto& sig : sec.second.signals)
			if (sig.second.basics.external)
				sig.second.external_ref = sig.second.referenced = true;
		process_lines(sec.first, xref);
	}
}