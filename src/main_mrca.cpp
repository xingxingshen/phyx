#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <getopt.h>

#include "node.h"
#include "tree_reader.h"
#include "string_node_object.h"
#include "vector_node_object.h"
#include "tree.h"
#include "utils.h"
#include "log.h"
#include "constants.h"

extern std::string PHYX_CITATION;


void print_help() {
    std::cout << "Get the number of descendant tips of internal nodes specified by mrca statements." << std::endl;
    std::cout << "This will take a newick- or nexus-formatted tree from a file or STDIN," << std::endl;
    std::cout << "and an MRCA file with format:" << std::endl;
    std::cout << "MRCANAME = tip1 tip2 ..." << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: pxmrca [OPTIONS]..." << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << " -t, --treef=FILE    input newick tree file, STDIN otherwise" << std::endl;
    std::cout << " -m, --mrca=FILE     file containing MRCA declarations" << std::endl;
    std::cout << " -o, --outf=FILE     output newick file, STOUT otherwise" << std::endl;
    std::cout << " -h, --help          display this help and exit" << std::endl;
    std::cout << " -V, --version       display version and exit" << std::endl;
    std::cout << " -C, --citation      display phyx citation and exit" << std::endl;
    std::cout << std::endl;
    std::cout << "Report bugs to: <https://github.com/FePhyFoFum/phyx/issues>" << std::endl;
    std::cout << "phyx home page: <https://github.com/FePhyFoFum/phyx>" << std::endl;
}
std::string versionline("pxmrca 1.1\nCopyright (C) 2013-2020 FePhyFoFum\nLicense GPLv3\nWritten by Stephen A. Smith (blackrim)");

static struct option const long_options[] =
{
    {"treef", required_argument, NULL, 't'},
    {"outf", required_argument, NULL, 'o'},
    {"mrca", required_argument, NULL, 'm'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {"citation", no_argument, NULL, 'C'},
    {NULL, 0, NULL, 0}
};

int main(int argc, char * argv[]) {
    
    log_call(argc, argv);
    
    char * outf = NULL;
    char * treef = NULL;
    char * mrcaf = NULL;
    bool outfileset = false;
    bool fileset = false;
    bool mrcaset = false;
    
    while (1) {
        int oi = -1;
        int c = getopt_long(argc, argv, "t:o:m:hVC", long_options, &oi);
        if (c == -1) {
            break;
        }
        switch(c) {
            case 't':
                fileset = true;
                treef = strdup(optarg);
                check_file_exists(treef);
                break;
            case 'o':
                outfileset = true;
                outf = strdup(optarg);
                break;
            case 'm':
                mrcaset = true;
                mrcaf = strdup(optarg);
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
        check_inout_streams_identical(treef, outf);
    }
    
    if (!mrcaset) {
        std::cout << "Must supply mrca file" << std::endl;
        exit(0);
    }
    
    std::istream * pios = NULL;
    std::ostream * poos = NULL;
    std::ifstream * fstr = NULL;
    std::ofstream * ofstr = NULL;
    
    if (outfileset == true) {
        ofstr = new std::ofstream(outf, std::ios::app);
        poos = ofstr;
    } else {
        poos = &std::cout;
    }
    if (fileset == true) {
        fstr = new std::ifstream(treef);
        pios = fstr;
    } else {
        pios = &std::cin;
        if (check_for_input_to_stream() == false) {
            print_help();
            exit(1);
        }
    }
    
    std::ifstream inmrca(mrcaf);
    std::string mrcaline;
    std::map<std::string, std::vector<std::string> > mrcas;
    while (getline(inmrca, mrcaline)) {
        if (mrcaline.empty()) {
            continue;
        }
        std::vector<std::string> searchtokens;
        tokenize(mrcaline, searchtokens, "=");
        std::string mrcaname = searchtokens[0];
        trim_spaces(mrcaname);
        searchtokens.erase(searchtokens.begin());
        searchtokens = tokenize(searchtokens[0]);
        mrcas[mrcaname] = searchtokens;
    }
    inmrca.close();
    
    std::string retstring;
    int ft = test_tree_filetype_stream(*pios, retstring);
    if (ft != 0 && ft != 1) {
        std::cerr << "Error: this really only works with nexus or newick. Exiting." << std::endl;
        exit(0);
    }
    
    bool going = true;
    std::map<std::string, std::vector<std::string> >::iterator it;
    
    if (ft == 1) {
        Tree * tree;
        while (going) {
            tree = read_next_tree_from_stream_newick(*pios, retstring, &going);
            if (tree != NULL) {
                for (it=mrcas.begin(); it != mrcas.end(); it++) {
                    Node * nd = tree->getMRCA((*it).second);
                    (*poos) << (*it).first << " " << nd->get_num_leaves() << " "
                        << nd->getName() << std::endl;
                }
                delete tree;
            }
        }
    } else if (ft == 0) { // Nexus. need to worry about possible translation tables
        std::map<std::string, std::string> translation_table;
        bool ttexists;
        ttexists = get_nexus_translation_table(*pios, &translation_table, &retstring);
        Tree * tree;
        while (going) {
            tree = read_next_tree_from_stream_nexus(*pios, retstring, ttexists,
                &translation_table, &going);
            if (tree != NULL) {
                for (it=mrcas.begin(); it != mrcas.end(); it++) {
                    Node * nd = tree->getMRCA((*it).second);
                    (*poos) << (*it).first << " " << nd->get_num_leaves() << " "
                        << nd->getName() << std::endl;
                }
                delete tree;
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
