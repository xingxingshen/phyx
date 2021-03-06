/*
 Bare-bones sequence recoding. RY-coding at first, but eventually codon-recoding.
 Codon-recoding will require genetic codes, and so knowledge of the taxon-specific codes.
 TODO: implement 'degen' coding.
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <getopt.h>

#include "utils.h"
#include "sequence.h"
#include "seq_reader.h"
#include "recode.h"
#include "log.h"
#include "constants.h"

extern std::string PHYX_CITATION;

void print_help() {
    std::cout << "Nucleotide sequence recoding." << std::endl;
    std::cout << "This will take fasta, fastq, phylip, and nexus formats from a file or STDIN." << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: pxrecode [OPTIONS]..." << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << " -s, --seqf=FILE      input sequence file, STDIN otherwise" << std::endl;
    std::cout << " -r, --recode=STRING  string identifying recoding scheme (default: RY)" << std::endl;
    std::cout << "  Supported recodings (use any valid combination):" << std::endl;
    std::cout << "      R = A|G" << std::endl;
    std::cout << "      Y = C|T" << std::endl;
    std::cout << "      S = C|G" << std::endl;
    std::cout << "      W = A|T" << std::endl;
    std::cout << "      M = A|C" << std::endl;
    std::cout << "      K = G|T" << std::endl;
    std::cout << "      B = C|G|T" << std::endl;
    std::cout << "      D = A|G|T" << std::endl;
    std::cout << "      H = A|C|T" << std::endl;
    std::cout << "      V = A|C|G" << std::endl;
    std::cout << " -o, --outf=FILE      output sequence file, STOUT otherwise" << std::endl;
    std::cout << " -h, --help           display this help and exit" << std::endl;
    std::cout << " -V, --version        display version and exit" << std::endl;
    std::cout << " -C, --citation       display phyx citation and exit" << std::endl;
    std::cout << std::endl;
    std::cout << "Report bugs to: <https://github.com/FePhyFoFum/phyx/issues>" << std::endl;
    std::cout << "phyx home page: <https://github.com/FePhyFoFum/phyx>" << std::endl;
}

std::string versionline("pxrecode 1.1\nCopyright (C) 2013-2020 FePhyFoFum\nLicense GPLv3\nWritten by Joseph W. Brown");

static struct option const long_options[] =
{
    {"seqf", required_argument, NULL, 's'},
    {"recode", required_argument, NULL, 'r'},
    {"outf", required_argument, NULL, 'o'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {"citation", no_argument, NULL, 'C'},
    {NULL, 0, NULL, 0}
};

int main(int argc, char * argv[]) {
    
    log_call(argc, argv);
    
    bool outfileset = false;
    bool fileset = false;
    std::string recodescheme = "";
    char * outf = NULL;
    char * seqf = NULL;
    while (1) {
        int oi = -1;
        int c = getopt_long(argc, argv, "s:r:o:hVC", long_options, &oi);
        if (c == -1) {
            break;
        }
        switch(c) {
            case 's':
                fileset = true;
                seqf = strdup(optarg);
                check_file_exists(seqf);
                break;
            case 'r':
                recodescheme = strdup(optarg);
                break;
            case 'o':
                outfileset = true;
                outf = strdup(optarg);
                break;
            case 'h':
                print_help();
                exit(0);
            case 'V':
                std::cout << versionline << std::endl;
                exit(0);
            case 'C':
                std::cout << PHYX_CITATION << std::endl;
                exit(0);
            default:
                print_error(argv[0], (char)c);
                exit(0);
        }
    }
    
    if (fileset && outfileset) {
        check_inout_streams_identical(seqf, outf);
    }
    
    // set default if arg not provided
    if (recodescheme == "") {
        recodescheme = "RY";
    }
    
    std::istream * pios = NULL;
    std::ostream * poos = NULL;
    std::ifstream * fstr = NULL;
    std::ofstream * ofstr = NULL;
    
    if (outfileset == true) {
        ofstr = new std::ofstream(outf);
        poos = ofstr;
    } else {
        poos = &std::cout;
    }
    
    if (fileset == true) {
        fstr = new std::ifstream(seqf);
        pios = fstr;
    } else {
        pios = &std::cin;
        if (check_for_input_to_stream() == false) {
            print_help();
            exit(1);
        }
    }
    
    SequenceRecoder sr (recodescheme);
    
    Sequence seq;
    std::string retstring;
    
    int ft = test_seq_filetype_stream(*pios, retstring);
    int num_taxa, num_char; // not used, but required by some reader functions
    bool first = true; // check first seq alphabet to make sure DNA. exit otherwise
    std::string alpha = "";
    
    // extra stuff to deal with possible interleaved nexus
    if (ft == 0) {
        bool interleave = false;
        get_nexus_dimensions(*pios, num_taxa, num_char, interleave);
        retstring = ""; // need to do this to let seqreader know we are mid-file
        if (!interleave) {
            while (read_next_seq_from_stream(*pios, ft, retstring, seq)) {
                if (first) {
                    alpha = seq.get_alpha_name();
                    if (alpha.compare("DNA") != 0) {
                        std::cerr << "Error: this only works for DNA. Exiting." << std::endl;
                        exit(0);
                    }
                    first = false;
                }
                (*poos) << ">" << seq.get_id() << std::endl;
                (*poos) << sr.get_recoded_seq(seq.get_sequence()) << std::endl;
            }
        } else {
            std::vector<Sequence> seqs = read_interleaved_nexus(*pios, num_taxa, num_char);
            for (unsigned int i = 0; i < seqs.size(); i++) {
                seq = seqs[i];
                if (first) {
                    alpha = seq.get_alpha_name();
                    if (alpha.compare("DNA") != 0) {
                        std::cerr << "Error: this only works for DNA. Exiting." << std::endl;
                        exit(0);
                    }
                    first = false;
                }
                (*poos) << ">" << seq.get_id() << std::endl;
                (*poos) << sr.get_recoded_seq(seq.get_sequence()) << std::endl;
            }
        }
    } else {
        bool complicated_phylip = false;
        // check if we are dealing with a complicated phylip format
        if (ft == 1) {
            get_phylip_dimensions(retstring, num_taxa, num_char);
            complicated_phylip = is_complicated_phylip(*pios, num_char);
        }
        if (complicated_phylip) {
            std::vector<Sequence> seqs = read_phylip(*pios, num_taxa, num_char);
            for (unsigned int i = 0; i < seqs.size(); i++) {
                seq = seqs[i];
                if (first) {
                    alpha = seq.get_alpha_name();
                    if (alpha.compare("DNA") != 0) {
                        std::cerr << "Error: this only works for DNA. Exiting." << std::endl;
                        exit(0);
                    }
                    first = false;
                }
                (*poos) << ">" << seq.get_id() << std::endl;
                (*poos) << sr.get_recoded_seq(seq.get_sequence()) << std::endl;
            }
        } else {
            // fasta, fastq, or simple phylip
            while (read_next_seq_from_stream(*pios, ft, retstring, seq)) {
                if (first) {
                    alpha = seq.get_alpha_name();
                    if (alpha.compare("DNA") != 0) {
                        std::cerr << "Error: this only works for DNA. Exiting." << std::endl;
                        exit(0);
                    }
                    first = false;
                }
                (*poos) << ">" << seq.get_id() << std::endl;
                (*poos) << sr.get_recoded_seq(seq.get_sequence()) << std::endl;
            }
            // fasta has a trailing one
            if (ft == 2) {
                (*poos) << ">" << seq.get_id() << std::endl;
                (*poos) << sr.get_recoded_seq(seq.get_sequence()) << std::endl;
            }
        }
    }
    
    
    if (fileset) {
        fstr->close();
        delete pios;
    }
    if (outfileset) {
        ofstr->close();
        delete poos;
    }
    return EXIT_SUCCESS;
}
