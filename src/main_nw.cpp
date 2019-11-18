#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <getopt.h>

#ifdef OMP
#include <omp.h>
#endif

#include "utils.h"
#include "seq_reader.h"
#include "sequence.h"
#include "seq_utils.h"
#include "seq_models.h"
#include "pairwise_alignment.h"
#include "log.h"

void print_help() {
    std::cout << "Conduct Needleman-Wunsch analysis for all the seqs in a file." << std::endl;
    std::cout << "This will take fasta, fastq, phylip, and nexus inputs." << std::endl;
    std::cout << "Output is a list of the scores and distances (and the alignments"<< std::endl;
    std::cout << "if asked)." << std::endl;
    std::cout << "Can read from stdin or file." << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: pxnw [OPTION]... [FILE]..."<<std::endl;
    std::cout << std::endl;
    std::cout << " -s, --seqf=FILE     input sequence file, stdin otherwise"<<std::endl;
    std::cout << " -o, --outf=FILE     output score/distance file, stout otherwise"<<std::endl;
    std::cout << " -a, --outalnf=FILE  output sequence file, won't output otherwise"<<std::endl;
    std::cout << " -t, --seqtype=INT   sequence type, default=DNA (DNA=0,AA=1)"<<std::endl;
    std::cout << " -m, --matrix=FILE   scoring matrix, default DNA=EDNAFULL, AA=BLOSUM62"<<std::endl;
    std::cout << " -n, --nthreads=INT  number of threads (open mp), default=2" << std::endl;
    std::cout << " -v, --verbose       make the output more verbose, turns off parallel" << std::endl;
    std::cout << " -h, --help          display this help and exit"<<std::endl;
    std::cout << " -V, --version       display version and exit"<<std::endl;
    std::cout << std::endl;
    std::cout << "Report bugs to: <https://github.com/FePhyFoFum/phyx/issues>" <<std::endl;
    std::cout << "phyx home page: <https://github.com/FePhyFoFum/phyx>"<<std::endl;
}

std::string versionline("pxnw 0.1\nCopyright (C) 2013 FePhyFoFum\nLicense GPLv3\nwritten by Stephen A. Smith (blackrim)");

static struct option const long_options[] =
{
    {"seqf", required_argument, NULL, 's'},
    {"outf", required_argument, NULL, 'o'},
    {"outalnf", required_argument, NULL, 'a'},
    {"seqtype", required_argument, NULL, 't'},
    {"matrix", required_argument, NULL, 'm'},
    {"nthreads", required_argument, NULL, 'n'},
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {NULL, 0, NULL, 0}
};

int main(int argc, char * argv[]) {
    
    log_call(argc, argv);
    
    bool fileset = false;
    bool outfileset = false;
    //bool outalnfileset = false; // not used
    bool matrixfileset = false;
    char * seqf = NULL;
    char * outf = NULL;
    //char * outaf = NULL; not used
    char * matf = NULL;
    int seqtype = 0;//DNA default, 1 = aa
    int num_threads = 2;//DNA default, 1 = aa
    bool verbose = false;
    while (1) {
        int oi = -1;
        int c = getopt_long(argc, argv, "s:o:a:t:m:n:vhV", long_options, &oi);
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
            case 'a':
                //outalnfileset = true;
                //outaf = strdup(optarg);
                break;
            case 't':
                seqtype = string_to_int(optarg, "-t");
                if (seqtype > 1) {
                    std::cout << "Don't recognize seqtype " << seqtype
                        << ". Must be 0 (DNA) or 1 (AA)." << std::endl;
                    exit(0);
                }
                break;
            case 'm':
                matrixfileset = true;
                matf = strdup(optarg);
                break;
            case 'n':
                num_threads = string_to_int(optarg, "-n");
                break;
            case 'v':
                verbose = true;
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
    std::map<char, std::map<char, int> > sc_mat;
    if (matrixfileset == true) {
        read_scoring_matrix(matf, sc_mat);
    } else {
        if (seqtype == 0) {
            get_ednafull(sc_mat);
        } else {//aa
            
            // TODO
            
        }
    }
    std::vector<Sequence> seqs;
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
    while (read_next_seq_from_stream(*pios, ft, retstring, seq)) {
        seqs.push_back(seq);
    }
    //fasta has a trailing one
    if (ft == 2) {
        seqs.push_back(seq);
    }

    //go all by all
    for (unsigned int i=0; i < seqs.size(); i++) {
#ifdef OMP
        omp_set_num_threads(num_threads);
#endif        
        #pragma omp parallel for
        for (unsigned int j=0; j < seqs.size(); j++) {
            if (j > i) {
                std::string aln1;
                std::string aln2;
                double sc = nw(seqs[i], seqs[j], sc_mat, 0, aln1, aln2);
                #pragma omp critical
                {
                    std::cout << seqs[i].get_id() << "\t" << seqs[j].get_id()
                        << "\t" << sc << std::endl;
                    if (verbose) {
                        std::cout << seqs[i].get_id() <<  "\t" << aln1 << "\n"
                            << seqs[j].get_id()  << "\t" << aln2 << std::endl;
                    }
                }
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
