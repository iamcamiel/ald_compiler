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

void error_exit(std::string sec, std::string msg, std::string s) {
	std::cout << "  > [" << sec << ":" << sections[sec].ald_line << "] " << s << "\n";
	std::cout << msg << "\n";
	exit(-1);
}

void error_exit(std::string sec, std::string msg, std::vector<LINE> lines) {
	for (unsigned int i = 0; i < lines.size(); i++) {
		std::cout << "  > [" << sec << ":" << (sections[sec].ald_line + i) << "] ";
		for (int i = 0; i < lines[i].ident; i++) std::cout << "  ";
		std::cout << lines[i].line << "\n";
	}
	std::cout << msg << "\n";
	exit(-1);
}

std::string getrest(std::string line, std::string match) {
	if (line.substr(0, match.length()) == match) {
		std::string r = line.substr(line.find_first_not_of(" \t", match.length()));
		r.erase(r.find_last_not_of(" \n\r\t") + 1);
		return r;
	}
	else
		return "";
}

