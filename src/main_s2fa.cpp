#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <getopt.h>

#include "utils.h"
#include "seq_reader.h"
#include "sequence.h"
#include "log.h"

void print_help() {
    std::cout << "Convert seqfiles from nexus, phylip, fastq to fasta." << std::endl;
    std::cout << "Can read from stdin or file." << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: pxs2fa [OPTION]... [FILE]..." << std::endl;
    std::cout << std::endl;
    std::cout << " -s, --seqf=FILE     input sequence file, stdin otherwise" << std::endl;
    std::cout << " -o, --outf=FILE     output sequence file, stout otherwise" << std::endl;
    std::cout << " -u, --uppercase     export characters in uppercase" << std::endl;
    std::cout << " -h, --help          display this help and exit" << std::endl;
    std::cout << " -V, --version       display version and exit" << std::endl;
    std::cout << std::endl;
    std::cout << "Report bugs to: <https://github.com/FePhyFoFum/phyx/issues>" << std::endl;
    std::cout << "phyx home page: <https://github.com/FePhyFoFum/phyx>" << std::endl;
}

std::string versionline("pxs2fa 0.1\nCopyright (C) 2013 FePhyFoFum\nLicense GPLv3\nwritten by Stephen A. Smith (blackrim), Joseph W. Brown");

static struct option const long_options[] =
{
    {"seqf", required_argument, NULL, 's'},
    {"outf", required_argument, NULL, 'o'},
    {"uppercase", no_argument, NULL, 'u'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {NULL, 0, NULL, 0}
};

int main(int argc, char * argv[]) {
    
    log_call(argc, argv);
    
    bool fileset = false;
    bool outfileset = false;
    bool toupcase = false;
    char * seqf = NULL;
    char * outf = NULL;
    while (1) {
        int oi = -1;
        int c = getopt_long(argc, argv, "s:o:uhV", long_options, &oi);
        if (c == -1) {
            break;
        }
        switch(c) {
            case 's':
                fileset = true;
                seqf = strdup(optarg);
                check_file_exists(seqf);
                break;
            case 'o':
                outfileset = true;
                outf = strdup(optarg);
                break;
            case 'u':
                toupcase = true;
                break;
            case 'h':
                print_help();
                exit(0);
            case 'V':
                std::cout << versionline << std::endl;
                exit(0);
            default:
                print_error(argv[0], (char)c);
                exit(0);
        }
    }
    
    if (fileset && outfileset) {
        check_inout_streams_identical(seqf, outf);
    }
    
    Sequence seq;
    std::string retstring;
    
    std::istream * pios = NULL;
    std::ostream * poos = NULL;
    std::ifstream * fstr = NULL;
    std::ofstream * ofstr = NULL;
    
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
    if (outfileset == true) {
        ofstr = new std::ofstream(outf);
        poos = ofstr;
    } else {
        poos = &std::cout;
    }

    int ft = test_seq_filetype_stream(*pios, retstring);
    // extra stuff to deal with possible interleaved nexus
    if (ft == 0) {
        int ntax, nchar = 0;
        bool interleave = false;
        get_nexus_dimensions(*pios, ntax, nchar, interleave);
        retstring = ""; // ugh hacky
        if (!interleave) {
            while (read_next_seq_from_stream(*pios, ft, retstring, seq)) {
                (*poos) << seq.get_fasta(toupcase);
            }
        } else {
            std::vector<Sequence> seqs = read_interleaved_nexus (*pios, ntax, nchar);
            for (unsigned int i = 0; i < seqs.size(); i++) {
                (*poos) << seqs[i].get_fasta(toupcase);
            }
        }
    } else {
        while (read_next_seq_from_stream(*pios, ft, retstring, seq)) {
            (*poos) << seq.get_fasta(toupcase);
        }
        //fasta has a trailing one
        if (ft == 2) {
            (*poos) << seq.get_fasta(toupcase);
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

