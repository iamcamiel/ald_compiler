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


std::map<int, std::map<int, int>> bit_ranges;


void issue(std::string sec, std::pair < std::string, SIGINFO2> sig, std::string pfx) {
	std::string sn;
	sn = safename_signal(sec, sig.first);
	if (!sig.second.basics.dimensioned) {
		sections[sec].h_file << "    char " << sn << ";\n";
		sections[sec].c_file << "  oldstate." << sec << pfx << "." << sn << " = newstate." << sec << pfx << "." << sn << " = " << (sig.first.c_str()[0] == '-' ? "1" : "0") << ";\n";
	}
	else {
		if (sig.second.basics.order < 0) {
			bit_ranges[sig.second.basics.high][sig.second.basics.low] = sig.second.basics.length;
			sections[sec].h_file << "    BIT_" << sig.second.basics.high << "_" << sig.second.basics.low << " " << sn << ";\n";
		}
		else {
			bit_ranges[sig.second.basics.low][sig.second.basics.high] = sig.second.basics.length;
			sections[sec].h_file << "    BIT_" << sig.second.basics.low << "_" << sig.second.basics.high << " " << sn << ";\n";
		}
		sections[sec].c_file << "  oldstate." << sec << pfx << "." << sn << ".F = newstate." << sec << pfx << "." << sn << ".F = " << (sig.first.c_str()[0] == '-' ? "-1" : "0") << ";\n";
		if (sig.second.basics.length > 64)
			sections[sec].c_file << "  oldstate." << sec << pfx << "." << sn << ".F2 = newstate." << sec << pfx << "." << sn << ".F2 = " << (sig.first.c_str()[0] == '-' ? "-1" : "0") << ";\n";
		for (auto& bit : sig.second.refd) {
			if (!sig.second.assigned.count(bit.first)) {
				sections[sec].log_cnt++;
				sections[sec].log_file << "Signal <" << sig.second.basics.prefix << "." << sig.first << "[" << bit.first << "]> referenced but not assigned.\n";
			}
		}
	}
}

void write_h_file(std::string sec) {

	std::string sn;

	sections[sec].c_file << "void init_" << sec << "() {\n";

	sections[sec].h_file << "void process_" << sec << "();\n\n";
	sections[sec].h_file << "void process_" << sec << "_clock();\n\n";
	sections[sec].h_file << "void init_" << sec << "();\n\n";
	sections[sec].h_file << "typedef struct __" << sec << " {\n";
	for (auto& sig : sections[sec].signals) {
		if (collapse && sig.second.aliased)
			continue;
		if (eliminate && sig.second.eliminated)
			continue;
		if (!sig.second.external_ref)
			continue;
		issue(sec, sig, "");
	}
	sections[sec].h_file << "  char __placeholder_for_c__;\n";
	sections[sec].h_file << "} _" << sec << ";\n\n";
	sections[sec].h_file << "typedef struct __" << sec << "_INT {\n";
	for (auto& sig : sections[sec].signals) {
		if (collapse && sig.second.aliased)
			continue;
		if (eliminate && sig.second.eliminated)
			continue;
		if (sig.second.external_ref)
			continue;
		issue(sec, sig, "_INT");
	}
	sections[sec].h_file << "  bool __placeholder_for_c__;\n";
	sections[sec].h_file << "} _" << sec << "_INT;\n\n";
	sections[sec].c_file << "}\n";
}

void write_top_h_file() {
	std::ofstream of("ald.h");

	of << "/* ALD top include file */\n\n";
	of << "#include \"ald_types.h\"\n";
	for (auto& s : sections)
		of << "#include \"" << s.second.ald_path.replace_extension(".h").string() << "\"\n";
	of << "\ninline void process_ald() {\n";
	for (auto& s : sections)
		of << "  process_" << s.first << "();\n";
	of << "}\n\n";
	of << "\ninline void process_ald_clock() {\n";
	for (auto& s : sections)
		of << "  process_" << s.first << "_clock();\n";
	of << "}\n\n";
	of << "\ninline void init_ald() {\n";
	for (auto& s : sections)
		of << "  init_" << s.first << "();\n";
	of << "}\n\n";
	of << "typedef struct __ALD_INT {\n";
	for (auto& s : sections)
		of << "  _" << s.first << " ald_" << s.first << ";\n";
	of << "\n";
	for (auto& s : sections)
		of << "  _" << s.first << "_INT ald_" << s.first << "_INT;\n";

	of << "  struct __EXTERNAL_ {\n";
	for (auto& sig : ext_signals) {
		std::string sn = safename_signal("", sig.first);
		if (!sig.second.basics.dimensioned) {
			of << "    char " << sn << ";\n";
		}
		else {
			if (sig.second.basics.order < 0) {
				bit_ranges[sig.second.basics.high][sig.second.basics.low] = sig.second.basics.length;
				of << "    BIT_" << sig.second.basics.high << "_" << sig.second.basics.low << " " << sn << ";\n";
			}
			else {
				bit_ranges[sig.second.basics.low][sig.second.basics.high] = sig.second.basics.length;
				of << "    BIT_" << sig.second.basics.low << "_" << sig.second.basics.high << " " << sn << ";\n";
			}
		}
	}
	of << "  } ald_EXTERNAL_;\n";
	of << "} _ALD_INT;\n\n";
	of << "typedef struct __ALD {\n";
	of << "  _ALD_INT ald_int;\n";
	of << "} _ALD;\n\n";
	of << "#define EXTERNAL_ ald_int.ald_EXTERNAL_\n";
	for (auto& s : sections)
		of << "#define " << s.first << " ald_int.ald_" << s.first << "\n";
	for (auto& s : sections)
		of << "#define " << s.first << "_INT ald_int.ald_" << s.first << "_INT\n";

	std::ofstream of2("ald_types.h");

	for (auto& a : bit_ranges) {
		for (auto& b : a.second) {
			of2 << "typedef union _BIT_" << a.first << "_" << b.first << " {\n";
			if (b.second > 64) {
				of2 << "  struct {\n    long long F;\n    long long F2;\n  };\n";
			}
			else if (b.second >32) {
				of2 << "  long long F;\n";
			} else {
				of2 << "  int F;\n";
			}
  		    of2 << "  struct {\n";
			if (a.first > b.first) {
				for (int i = b.first; i <= a.first; i++)
					of2 << "    unsigned int B" << i << " : 1;\n";
			}
			else {
				for (int i = b.first; i >= a.first; i--)
					of2 << "    unsigned int B" << i << " : 1;\n";
			}
			of2 << "  };\n";
			of2 << "} BIT_" << a.first << "_" << b.first << ";\n\n";
		}
	}

}
