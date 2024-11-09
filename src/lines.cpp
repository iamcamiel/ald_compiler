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

void process_lines(std::string sec, void(*func)(std::string, std::vector<LINE>), int ck) {
	std::string line;
	std::vector<LINE> lines;
	int clk = 1;

	sections[sec].ald_file.clear();
	sections[sec].ald_file.seekg(0);
	
	int lineno = 0;
	while (std::getline(sections[sec].ald_file, line))
	{
		lineno++;
		while (line.find("\t") != std::string::npos) {
			line.replace(line.find("\t"), 1, "    ");
		}
		line.erase(line.find_last_not_of(" \t\r\n") + 1);
		LINE l;
		if (line == "") {
			l.line = "";
			l.ident = 0;
		}
		else {
			l.line = line.substr(line.find_first_not_of(" "));
			l.ident = line.find_first_not_of(" ");
		}
		if (l.ident % 2) error_exit(sec, "identation must be in steps of 2.", lines);
		l.ident /= 2;
		l.is_signal = (l.line.substr(0, 1) == "-" || l.line.substr(0, 1) == "+");
		if (l.is_signal) {
			l.signal = get_signal(sec, l.line, false);
		}

		if (l.ident == 0) {
			if (lines.size()) {
				if (!ck || (ck == clk))
					func(sec, lines);
				lines.clear();
				sections[sec].ald_line = lineno;
			}

			if (l.line == "CLOCK") {
				clk = 2;
				continue;
			}
			if (l.line == "NOCLOCK") {
				clk = 1;
				continue;
			}
		}
		lines.push_back(l);
	}
	if (lines.size()) {
		if (!ck || (ck == clk))
			func(sec, lines);
	}
}
