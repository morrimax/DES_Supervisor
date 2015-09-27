///////////////////////////////////////////////////////////////////////////////
//////////////DES Supervisor Application for BSCOPNBMAX and MPO////////////////
///////////////////////////////////////////////////////////////////////////////

#include <sstream>
#include <getopt.h>
#include <cstring>
#include <algorithm>
#include <cctype>
#include "../include/UBTS.h"
#include "../include/Utilities.h"
using namespace std;

#ifdef DEBUG
ostream& out = cout;
#else
ostringstream oss;
ostream& out = oss;
#endif

Mode MODE_FLAG = BSCOPNBMAX;
bool MPO_CONDITION_FLAG = false;
bool VERBOSE_FLAG = false;
bool FILE_OUT_FLAG = false;

const char* const INITIAL_CLEAN_UP = "rm -f ./results/*";
const char* const FSM_FSM_FILE = "./results/FSM.fsm";
const char* const FSM_TXT_FILE = "./results/FSM.txt";
const char* const A_UxG_FILE = "./results/A_UxG.fsm";
const char* const A_UxG_REDUCED_FILE = "./results/A_UxG_reduced.fsm";
const char* const NBAIC_FILE = "./results/NBAIC.fsm";
const char* const ICS_FILE = "./results/ICS.fsm";
const char* const UBTS_FILE = "./results/UBTS.fsm";
const char* const EBTS_FILE = "./results/EBTS.fsm";
const char* const MPO_FILE = "./results/MPO.fsm";
const char* const BDO_FILE = "./results/BDO.fsm";

void do_BSCOPNBMAX(const string& FSM_file, const string& property,
				   const string& ISP_file);
void do_MPO(const string& FSM_file, const string& property,
		    const string& ISP_file);
void convert_fsm(const string& FSM_file);
void generate_supervisor(NBAIC* nbaic, FSM* fsm);
void generate_activation_policy(NBAIC* nbaic, FSM* fsm);
void print_help();
void print_size_info();
void print_size_info(int NBAIC_size, int EBTS_size);
void write_unfolds(int num_unfolds);
void display_prompts(string& FSM_file, string& ISP_file, string& property);


///////////////////////////////////////////////////////////////////////////////


int main(int argc, char* argv[]) {
	if (argc == 1) MODE_FLAG = INTERACTIVE;
	system(INITIAL_CLEAN_UP);
	static struct option long_options[] = {
		{"mode", required_argument, NULL, 'm'},
		{"MPO_condition", required_argument, NULL, 'c'},
		{"FSM_file", required_argument, NULL, 'f'},
		{"property", required_argument, NULL, 'p'},
		{"ISP_file", required_argument, NULL, 'i'},
		{"verbose", no_argument, NULL, 'v'},
		{"write_to_file", no_argument, NULL, 'w'},
		{"help", no_argument, NULL, 'h'},
		{0, 0, 0, 0}
	};

	char pause;
	int c, index = 0;
	string FSM_file, ISP_file, property;
	while ((c = getopt_long(argc, argv, "m:c:f:p:i:vwh", long_options, &index)) != -1) {
		switch (c) {
			case 'm':
				if (optarg) make_lower(optarg);
				if (strcmp(optarg, "mpo") == 0) MODE_FLAG = MPO;
				else if (strcmp(optarg, "bscopnbmax") == 0) MODE_FLAG = BSCOPNBMAX;
				else if (strcmp(optarg, "convert") == 0) MODE_FLAG = CONVERT;
				else if (strcmp(optarg, "interactive") == 0) MODE_FLAG = INTERACTIVE;
				else if (VERBOSE_FLAG)
					cerr << "Error: " << optarg << " is not a valid mode."
						 << "Using default mode bscopnbmax.\n";
				break;
			case 'c':
				make_lower(optarg);
				if (strcmp(optarg, "max") == 0) MPO_CONDITION_FLAG = true;
				else if (strcmp(optarg, "min") == 0) MPO_CONDITION_FLAG = false;
				else if (VERBOSE_FLAG)
					cerr << "Error: " << optarg
						 << " is not a valid MPO condition."
						 << " Using default MPO condition min.\n";
				break;
			case 'f':
				FSM_file = optarg;
				break;
			case 'p':
				make_lower(optarg);
				property = optarg;
				break;
			case 'i':
				ISP_file = optarg;
				break;
			case 'v':
				VERBOSE_FLAG = true;
				break;
			case 'w':
				FILE_OUT_FLAG = true;
				break;
			case '?':
				cerr << "Error: command " << c
					 << " is not defined. Printing help menu and exiting\n";
			case 'h':
				print_help();
				cin.get();
				return 0;
		}
	}
	if (MODE_FLAG == INTERACTIVE) display_prompts(FSM_file, ISP_file, property);
	if (MODE_FLAG == BSCOPNBMAX) do_BSCOPNBMAX(FSM_file, property, ISP_file);
	else if (MODE_FLAG == MPO) do_MPO(FSM_file, property, ISP_file);
	else if (MODE_FLAG == CONVERT) convert_fsm(FSM_file);

#ifndef DEBUG
	cout << oss.str();
#endif

	cout << "Press any key to continue...";
	cin >> pause;
	cout << '\n';
	return 0;
}


///////////////////////////////////////////////////////////////////////////////


/* Basic Supervisory Control and Observation Problem: Non-blocking
and Maximally Permissive Case */
void do_BSCOPNBMAX(const string& FSM_file, const string& property,
				   const string& ISP_file) {
	FSM* fsm = new FSM(FSM_file);
	IS_Property* isp = get_ISP(property, ISP_file,
							   fsm->states.regular, VERBOSE_FLAG);
	NBAIC* nbaic = new NBAIC(fsm, isp, out);
	if (!nbaic->is_empty()) generate_supervisor(nbaic, fsm);
	else if (VERBOSE_FLAG)
		out << "No maximally permissive supervisor exists for this FSM\n";
	delete fsm;
	delete isp;
	delete nbaic;
}

/* Most Permissive Observer */
void do_MPO(const string& FSM_file, const string& property,
		    const string& ISP_file) {
	FSM* fsm = new FSM(FSM_file, MODE_FLAG);
	IS_Property* isp = get_ISP(property, ISP_file,
							   fsm->states.regular, VERBOSE_FLAG);
	NBAIC* nbaic = new NBAIC(fsm, isp, out, MODE_FLAG);
	if (!nbaic->is_empty()) generate_activation_policy(nbaic, fsm);
	else if (VERBOSE_FLAG)
		out << "No " << (MPO_CONDITION_FLAG ? "maximal" : "minimal")
			<< " activation policy exists for this FSM\n";
	else write_unfolds(0);
	delete fsm;
	delete isp;
	delete nbaic;
}

/* Finite State Machine file conversion utility */
void convert_fsm(const string& FSM_file) {
	FSM* fsm = new FSM(FSM_file);
	/* File extension is .fsm--convert to .txt format */
	if (FSM_file.find(".fsm") != string::npos) {
		if (VERBOSE_FLAG) fsm->print_txt(out);
		if (FILE_OUT_FLAG) {
			ofstream file_out(FSM_TXT_FILE);
			fsm->print_txt(file_out);
			file_out.close();
		}
	}
	/* File extension is not .fsm--convert to .fsm format */
	else {
		if (VERBOSE_FLAG) fsm->print_fsm(out);
		if (FILE_OUT_FLAG) {
			ofstream file_out(FSM_FSM_FILE);
			fsm->print_fsm(file_out);
			file_out.close();
		}
	}
}

void generate_supervisor(NBAIC* nbaic, FSM* fsm) {
	/* Build the inital unfolded bipartite transition system */
	UBTS ubts(nbaic, out);
	ubts.expand();
	/* Build the ics representation of our ubts */
	ICS ics(ubts, fsm, out);
	if (VERBOSE_FLAG) {
		nbaic->print();
		ubts.print();
		ics.print();
	}
	int num_unfolds = 0;
	/* Enters loop if there exists a state that is not coaccessible */
	while (ICS_STATE* entrance_state = ics.get_entrance_state(ubts)) {
		if (VERBOSE_FLAG) entrance_state->print(out);
		/* Build the live decision string from the
		entrance state to a marked state*/
		LDS lds(out, nbaic, entrance_state);
		lds.compute_maximal();
		/* Add transitions in the live decision string to our ubts */
		ubts.augment(lds);
		ubts.expand();
		/* Rebuild ICS for new ubts */
		ics = ICS(ubts, fsm, out);
		if (VERBOSE_FLAG) {
			lds.print();
			ubts.print();
			ics.print();
		}
		++num_unfolds;
	}
	
	if (VERBOSE_FLAG) ubts.print();
	ofstream file_out(A_UxG_FILE);
	ics.print_A_UxG(ubts, file_out, FILE_OUT_FLAG, VERBOSE_FLAG);
	file_out.close();
	if (FILE_OUT_FLAG) {
		fsm->print_fsm(FSM_FSM_FILE);
		nbaic->print_fsm(NBAIC_FILE);
		ubts.print(UBTS_FILE, false);
		ubts.print(EBTS_FILE, true);
		ics.print_fsm(ICS_FILE);
		ics.reduce_A_UxG(A_UxG_FILE, A_UxG_REDUCED_FILE);
	}
}

void generate_activation_policy(NBAIC* nbaic, FSM* fsm) {
	if (VERBOSE_FLAG) nbaic->print();
	if (FILE_OUT_FLAG) nbaic->print_fsm(MPO_FILE);
	nbaic->reduce_MPO(MPO_CONDITION_FLAG);
	if (VERBOSE_FLAG) nbaic->print(true);
	if (FILE_OUT_FLAG) nbaic->print_fsm(BDO_FILE);
	if (FILE_OUT_FLAG) fsm->print_fsm(FSM_FSM_FILE);
}

void print_help() {
	cout << "DES Supervisor Application for BSCOPNBMAX and MPO \n"
		 << "Controls:\n"
		 << "\tMode [-m] - switch between the [INTERACTIVE], [BSCOPNBMAX], [MPO], and [CONVERT] modes\n"
		 << "\tMPO_condition [-c] - request the MPO to find a [min]imal or [max]imal solution\n"
		 << "\tFSM_file [-f] - provide an FSM file for processing\n"
		 << "\tProperty [-p] - provide an implemented information state property\n"
		 << "\tISP_file [-i] - provide a corresponding file for the specified ISP property\n"
		 << "\tVerbose [-v] - request more detailed output\n"
		 << "\tWrite_to_File [-w] - write the UBTS, EBTS, NBAIC, and A_UxG to separate .fsm files in the ./results folder\n"
		 << "\tHelp [-h] - display help menu\n"
		 << "For more information, please see the README document\n" << flush;
}

void display_prompts(string& FSM_file, string& ISP_file, string& property) {
	string arg_str, arg_str_lower;
	cout << "*********************DES Supervisor Application***********************\n"
		 << endl;
	bool no_valid_argument = true;
	while (no_valid_argument) {
		cout << "Please select a mode for program execution [BSCOPNBMAX | MPO | CONVERT]: "
			 << flush;
		cin >> arg_str;
		cout << endl;
		arg_str_lower.resize(arg_str.size());
		transform(arg_str.begin(), arg_str.end(), arg_str_lower.begin(), ::tolower);
		switch (arg_str_lower[0]) {
			case 'b':
				if (arg_str_lower == "bscopnbmax")
					no_valid_argument = false;
				break;
			case 'm':
				if (arg_str_lower == "mpo") {
					MODE_FLAG = MPO;
					no_valid_argument = false;
				}
				break;
			case 'c':
				if (arg_str_lower == "convert") {
					MODE_FLAG = CONVERT;
					no_valid_argument = false;
				}
				break;
		}
		if (no_valid_argument) cout << "Error reading mode type " << arg_str << endl;
	}
	if (MODE_FLAG == MPO) {
		no_valid_argument = true;
		while (no_valid_argument) {
			cout << "Would you like to synthesize a minimal or maximal "
				 << "sensor activation policy? [MIN | MAX]: " << flush;
			cin >> arg_str;
			cout << endl;
			arg_str_lower.resize(arg_str.size());
			transform(arg_str.begin(), arg_str.end(), arg_str_lower.begin(), ::tolower);
			switch (arg_str_lower[1]) {
				case 'i':
					if (arg_str_lower == "min")
						no_valid_argument = false;
					break;
				case 'a':
					if (arg_str_lower == "max") {
						MPO_CONDITION_FLAG = true;
						no_valid_argument = false;
					}
					break;
			}
			if (no_valid_argument) cout << "Error reading MPO condition " << arg_str << endl;
		}
	}
	no_valid_argument = true;
	while (no_valid_argument) {
		cout << "Please enter the FSM file you would like to process: " << flush;
		cin >> FSM_file;
		cout << endl;
		ifstream file_open_test(FSM_file.c_str());
		if (file_open_test.is_open()) no_valid_argument = false;
		else cout << "Error: file " << FSM_file << " could not be opened" << endl;
		file_open_test.close();
	}
	if (MODE_FLAG != CONVERT) {
		no_valid_argument = true;
		bool using_ISP = false;
		while (no_valid_argument) {
			cout << "Would you like to use an inforamtion state property? [y | n]: "
				 << flush;
			cin >> arg_str;
			cout << endl;
			if (tolower(arg_str[0]) == 'n') no_valid_argument = false;
			else if (tolower(arg_str[0] != 'y')) continue;
			else {
				no_valid_argument = false;
				using_ISP = true;
			}
		}
		if (using_ISP) {
			no_valid_argument = true;
			while (no_valid_argument) {
				cout << "Please enter which property you would like "
					 << "to use [SAFETY | OPACITY | DISAMBIGUATION]: " << flush;
				cin >> arg_str;
				cout << endl;
				property.resize(arg_str.size());
				transform(arg_str.begin(), arg_str.end(), property.begin(), ::tolower);
				if (property == "safety" || property == "opacity" ||
					property == "disambiguation")
					no_valid_argument = false;
			}
			no_valid_argument = true;
			while (no_valid_argument) {
				cout << "Please enter the information state "
					 << "property file you would like to use: " << flush;
				cin >> ISP_file;
				cout << endl;
				ifstream file_open_test(ISP_file);
				if (file_open_test.is_open()) no_valid_argument = false;
				else cout << "Error: file " << ISP_file << " could not be opened" << endl;
				file_open_test.close();
			}
		}
	}
	no_valid_argument = true;
	while (no_valid_argument) {
		cout << "Would you like to turn on console output? "
			 << "This is not recommended for large inputs [y | n]: " << flush;
		cin >> arg_str;
		cout << endl;
		if (tolower(arg_str[0]) == 'n') no_valid_argument = false;
		else if (tolower(arg_str[0] != 'y')) continue;
		else {
			no_valid_argument = false;
			VERBOSE_FLAG = true;
		}
	}
	no_valid_argument = true;
	while (no_valid_argument) {
		cout << "Would you like to turn on file output? [y | n]: " << flush;
		cin >> arg_str;
		cout << endl;
		if (tolower(arg_str[0]) == 'n') no_valid_argument = false;
		else if (tolower(arg_str[0] != 'y')) continue;
		else {
			no_valid_argument = false;
			FILE_OUT_FLAG = true;
		}
	}
	cout << "Executing program..." << endl;
}

void write_unfolds(int num_unfolds) {
	ofstream unfold_out("./test/scalability_test/results/unfolds.txt");
	unfold_out << num_unfolds;
	unfold_out.close();
}

///////////////////////////////////////////////////////////////////////////////
