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

std::map<std::string, SECINFO> sections;
bool collapse = false;
bool eliminate = false;

void usage() {
	std::cout << "Usage:\n"
		<< "  ald_compiler [-O<n>] [filename [filename...]]\n"
		<< "  where:\n"
		<< "     -O0 : no optimizations\n"
		<< "     -O1 : collapse identical signals into a single signal\n"
		<< "     -O2 : eliminate unused signals\n"
		<< "     -O3 : both optimizations\n";
	exit(1);
}

void add_file(std::filesystem::path fn) {
	std::ifstream fi;
	std::string line, rest;

	fi.open(fn);
	if (!fi.is_open())
		error_exit("", "Can't open file " + fn.string() + " for reading.","");

	while (std::getline(fi, line)) {
		if ((rest = getrest(line, "SECTION")) != "")
			break;
	}
	if (rest == "")
		error_exit("", "File " + fn.string() + " does not specify a SECTION.","");
	fi.close();

	if (sections.count(rest))
		error_exit(rest, "Section " + rest + " defined in files " + sections[rest].ald_path.string() + " and " + fn.string() + ".",line);

	sections[rest];
	sections[rest].ald_path = fn;

	sections[rest].ald_file.open(fn);
	if (!sections[rest].ald_file.is_open())
		error_exit(rest, "Can't open file " + fn.string() + " for reading.", "");
	fn.replace_extension(".cpp");
	sections[rest].c_file.open(fn);
	if (!sections[rest].c_file.is_open())
		error_exit(rest, "Can't open file " + fn.string() + " for writing.", "");
	fn.replace_extension(".h");
	sections[rest].h_file.open(fn);
	if (!sections[rest].h_file.is_open())
		error_exit(rest, "Can't open file " + fn.string() + " for writing.", "");
	fn.replace_extension(".vhd");
	sections[rest].vhd_file.open(fn);
	if (!sections[rest].vhd_file.is_open())
		error_exit(rest, "Can't open file " + fn.string() + " for writing.", "");
	fn.replace_extension(".inf");
	sections[rest].inf_file.open(fn);
	if (!sections[rest].inf_file.is_open())
		error_exit(rest, "Can't open file " + fn.string() + " for writing.", "");
	fn.replace_extension(".err");
	sections[rest].log_file.open(fn);
	if (!sections[rest].log_file.is_open())
		error_exit(rest, "Can't open file " + fn.string() + " for writing.", "");
}

int main(int argc, char** argv)
{
	bool have_file = false;

	std::cout << "VAXbarn ALD Compiler version 1.00\n";
	std::cout << "Copyright (C) 2020 by Camiel Vanderhoeven\n\n";
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] != 'O' || (argv[i][2] < '0' && argv[i][2] > '3') || argv[i][3] != 0)
				usage();
			if (argv[i][2] == '1' || argv[i][2] == '3')
				collapse = true;
			if (argv[i][2] == '2' || argv[i][2] == '3')
				eliminate = true;
		}
		else {
			have_file = true;
			add_file(std::filesystem::path(argv[i]));
		}
	}

	if (!have_file) {
		for (const auto& p : std::filesystem::directory_iterator("."))
			if (p.path().extension() == ".ald" || p.path().extension() == ".ALD")
				add_file(p.path());
	}

	for (auto& s : sections) {
		ingest_signals(s.first);
	}
	xref_signals();
	for (auto& s : sections) {
		write_c_file(s.first);
	}
	for (auto& s : sections) {
		write_h_file(s.first);
	}

	write_top_h_file();

	for (auto& s : sections) {
		write_vhd_file(s.first);
	}

	write_top_vhd_file();

	for (auto& s : sections) {
		std::cout << s.first << "\t" << std::to_string(s.second.log_cnt) << " warnings\t" << std::to_string(s.second.inf_cnt) << " informationals\n";
	}
	return 0;
}


