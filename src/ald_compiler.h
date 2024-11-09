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

#pragma once

#include <stdio.h>
#include <tchar.h>

#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <utility>
#include <sstream>
#include <filesystem>

typedef struct _SIGINFO1 {
	bool dimensioned;
	int order;
	int length;
	int low;
	int high;
	std::string prefix;
	bool external;
	bool special;
} SIGINFO1;

typedef std::pair <std::string, SIGINFO1> SIG1;

typedef struct _SIGINFO2 {
	SIGINFO1 basics;
	bool aliased;
	SIG1 alias;
	bool referenced;
	bool external_ref;
	std::map<int, bool> assigned;
	std::map<int, bool> refd;
	bool eliminated;
} SIGINFO2;

typedef struct _SECINFO {
	std::filesystem::path ald_path;
	std::ifstream ald_file;
	int ald_line;
	std::ofstream h_file;
	std::ofstream c_file;
	std::ofstream vhd_file;
	std::ofstream inf_file;
	std::ofstream log_file;
	std::map<std::string, SIGINFO2> signals;
	std::map<std::string, bool> unknown_sections;
	std::map<std::string, std::map<std::string, bool>> unknown_signals;
	std::map<std::string, std::map<std::string, bool>> foreign_signals;
	int log_cnt;
	int inf_cnt;
} SECINFO;

extern std::map<std::string, SIGINFO2> ext_signals;

typedef struct _LINE {
	std::string line;
	int ident;
	bool is_signal;
	SIG1 signal;
} LINE;

extern std::map<std::string, SECINFO> sections;
extern bool collapse;
extern bool eliminate;

//
// == prototypes ==
//

void error_exit(std::string sec, std::string msg, std::string s);
void error_exit(std::string sec, std::string msg, std::vector<LINE> lines);
std::string getrest(std::string line, std::string match);

void process_lines(std::string sec, void(*func)(std::string, std::vector<LINE>), int ck = 0);

std::string safename_signal(std::string sec, std::string s, bool vhdl = false);
SIG1 get_signal(std::string sec, std::string s, bool output);
SIG1 get_final_signal(std::string sec, std::string s);
void ingest_signals(std::string sec);
void xref_signals();
void add_ext_signal(std::string sec, SIG1 s, std::vector<LINE> lines);

void write_c_file(std::string sec);
void write_h_file(std::string sec);
void write_top_h_file();
void write_vhd_file(std::string sec);
void write_top_vhd_file();


inline bool is_known_word(std::string s) {
	return s == "AND"
		|| s == "NAND"
		|| s == "ANDNOT"
		|| s == "OR"
		|| s == "NOR"
		|| s == "ORNOT"
		|| s == "NOT"
		|| s == "XOR"
		|| s == "0"
		|| s == "1";
}

inline bool is_known_operator(std::string s) {
	return s == "AND"
		|| s == "NAND"
		|| s == "ANDNOT"
		|| s == "OR"
		|| s == "NOR"
		|| s == "ORNOT"
		|| s == "NOT"
		|| s == "XOR";
}

inline bool is_known_multi_operator(std::string s) {
	return s == "AND"
		|| s == "NAND"
		|| s == "ANDNOT"
		|| s == "OR"
		|| s == "NOR"
		|| s == "ORNOT"
		|| s == "XOR";
}

extern std::vector<std::vector<LINE>> temporaries;
