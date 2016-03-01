/*
 * seqgen.cpp
 *
 *  Created on: Jun 23, 2015
 *      Author: joe
 */

#include <iostream>
#include <string>
#include <vector>
#include <nlopt.hpp>
#include <armadillo>
#include <random>
#include <ctime>
#include <numeric>

using namespace std;
using namespace arma;

#include "seqgen.h"
#include "utils.h"
#include "node.h"
#include "tree.h"
#include "tree_reader.h"

// TODO: do we want this order?
/*Default Rate Matrix looks like this, I don't know why but I always go A,T,C,G
 *
 *    A   T   C   G
 * A -1  .33 .33 .33
 * T .33 -1  .33 .33
 * G .33 .33  -1 .33
 * G .33 .33 .33  -1
 */


// this should enable easy changing of order, if desired
map <char, int> SequenceGenerator::nucMap = {
   {'A', 0},
   {'T', 1},
   {'C', 2},
   {'G', 3}
};


/* Use the P matrix probabilities and randomly draw numbers to see
 * if each individual state will undergo some type of change
 */
string SequenceGenerator::simulate_sequence (string const& anc, vector < vector <double> >& QMatrix, float const& brlength) {

    std::vector<double>::iterator low;
    vector < vector <double> > PMatrix(4, vector <double>(4, 0.0));
    
    string chars = "ATCG";
    string newstring = anc; // instead of building, set size and replace
    for (unsigned int i = 0; i < anc.size(); i++) {
        float RandNumb = get_uniform_random_deviate();
        int ancChar = nucMap[anc[i]];
        float brnew = brlength * site_rates[i];
        PMatrix = calculate_p_matrix(QMatrix, brnew);
        for (int i = 0; i < 4; i++) {
            // this calculates a cumulative sum
            std::partial_sum(PMatrix[i].begin(), PMatrix[i].end(), PMatrix[i].begin(), plus<double>());
        }
        low = std::lower_bound (PMatrix[ancChar].begin(), PMatrix[ancChar].end(), RandNumb);
        newstring[i] = chars[low - PMatrix[ancChar].begin()];
    }
    return newstring;
}


/*
 * Calculate the Q Matrix (Substitution rate matrix)
 */
vector < vector <double> > SequenceGenerator::calculate_q_matrix () {

    vector < vector <double> > bigpi(4, vector <double>(4, 1.0));
    vector < vector <double> > t(4, vector <double>(4, 0.0));
    
    double tscale = 0.0;
    
    // doing the same looping multiple times here. simplify?
    for (unsigned int i = 0; i < rmatrix.size(); i++) {
        for (unsigned int j = 0; j < rmatrix.size(); j++) {
            if (i != j) {
                bigpi[i][j] *= basefreqs[i] * basefreqs[j] * rmatrix[i][j];
                tscale += bigpi[i][j];
            } else {
                bigpi[i][j] = 0.0;
            }
        }
    }
    for (unsigned int i = 0; i < rmatrix.size(); i++) {
        for (unsigned int j = 0; j < rmatrix.size(); j++) {
            if (i != j) {
                bigpi[i][j] /= tscale;
            } else {
                // set the diagnols to zero *** are they not set to zero above?
                bigpi[i][j] = 0.0;
            }
        }
    }
    for (unsigned int i = 0; i < rmatrix.size(); i++) {
        double diag = 0.0;
        for (unsigned int j = 0; j < rmatrix.size(); j++) {
            if (i != j) {
                diag -= bigpi[i][j];
            }
        }
        bigpi[i][i] = diag;
    }
    //Divide and Transpose
    for (unsigned int i = 0; i < rmatrix.size(); i++) {
        for (unsigned int j = 0; j < rmatrix.size(); j++) {
            bigpi[i][j] /= basefreqs[i];
        }
    }
    return bigpi;
}


/* Calculate the P Matrix (Probability Matrix)
 * Changes to armadillos format then back I don't like the way could be more
 * efficient but yeah...
 */
vector < vector <double> > SequenceGenerator::calculate_p_matrix (vector < vector <double> > QMatrix, float br) {

    vector < vector <double> > Pmatrix(4, vector <double>(4, 0.0));
    mat A = randn<mat>(4,4);
    mat B = randn<mat>(4,4);
    int count = 0;
    //Q * t moved into Matrix form for armadillo
    for (unsigned int i = 0; i < QMatrix.size(); i++) {
        for (unsigned int j = 0; j < QMatrix.size(); j++) {
            A[count] = (QMatrix[i][j] * br);
            count++;
        }
    }
   //exponentiate the matrix
   B = expmat(A);
   //cout << B << endl;
   count = 0;
   //convert the matrix back to C++ vector
   for (unsigned int i = 0; i < Pmatrix.size(); i++) {
        for (unsigned int j = 0; j < Pmatrix.size(); j++) {
            Pmatrix[i][j] = B[count];
            count++;
        }
   }
   return Pmatrix;
}


/*
 * Pre-Order traversal works
 * Calculates the JC Matrix
 */
// TODO: how to name ancestor nodes (sequences)
//       - if we have this we can add to results (if desired))
void SequenceGenerator::preorder_tree_traversal (Tree * tree, bool showancs, vector<double> multirates, bool mm) {

    double brlength = 0.0;
    double gamma = 0.0;
    int count = 0;
    int rate_count = 0;
    string str = "";
    int check = 0;
    vector < vector <double> > QMatrix(4, vector <double>(4, 0.0));
    vector< vector <double> > rate_matrix(4, vector<double>(4, 0.33));

    //vector < vector <double> > PMatrix(4, vector <double>(4, 0.0));
    if (mm == true) {        
        rmatrix[0][2] = multirates[0]; //A->C
        rmatrix[2][0] = multirates[0]; //C->A
        rmatrix[0][3] = multirates[1]; //A->G
        rmatrix[3][0] = multirates[1]; //G->A
        rmatrix[0][1] = multirates[2]; //A->T
        rmatrix[1][0] = multirates[2]; //T->A
        rmatrix[2][3] = multirates[3]; //C->G
        rmatrix[3][2] = multirates[3]; //G->C
        rmatrix[1][2] = multirates[4]; //C->T
        rmatrix[2][1] = multirates[4]; //T->C
        rmatrix[1][3] = multirates[5]; //G->T
        rmatrix[3][1] = multirates[5]; //T->G
        rmatrix[0][0] = (multirates[0]+multirates[1]+multirates[2]) * -1;
        rmatrix[1][1] = (multirates[2]+multirates[4]+multirates[5]) * -1;
        rmatrix[2][2] = (multirates[0]+multirates[3]+multirates[4]) * -1;
        rmatrix[3][3] = (multirates[1]+multirates[3]+multirates[5]) * -1;
        for (unsigned int i = 0; i < 6; i++) {
            multirates.erase (multirates.begin() + 0); 
        }
        //QMatrix = calcQmatrix(rate_matrix);
        //QMatrix = calculate_q_matrix();
    }
    QMatrix = calculate_q_matrix();

    Node * root = tree->getRoot();
    seqs[root] = Ancestor;
    ancq[root] = QMatrix;
    
    // Pre-Order Traverse the tree
    for (int k = (tree->getNodeCount() - 2); k >= 0; k--) {
        brlength = tree->getNode(k)->getBL();
        /*for (unsigned int i = 0; i < QMatrix.size(); i++) {
            for (unsigned int j = 0; j < QMatrix.size(); j++) {
                cout << QMatrix[i][j] << " ";
            }
            cout << "\n";
        }
        cout << "\n";*/
        if (mm == true) {
            check = (int)round(multirates[0]);
            //cout << check << " " << rate_count << endl;
            if (tree->getNode(k)->isInternal() == true && multirates.size() != 0) {
                if (check == rate_count) {
                    rmatrix[0][2] = multirates[1];
                    rmatrix[2][0] = multirates[1];
                    rmatrix[0][3] = multirates[2];
                    rmatrix[3][0] = multirates[2];
                    rmatrix[0][1] = multirates[3];
                    rmatrix[1][0] = multirates[3];
                    rmatrix[2][3] = multirates[4];
                    rmatrix[3][2] = multirates[4];
                    rmatrix[1][2] = multirates[5];
                    rmatrix[2][1] = multirates[5];
                    rmatrix[1][3] = multirates[6];
                    rmatrix[3][1] = multirates[6];
                    rmatrix[0][0] = (multirates[1]+multirates[2]+multirates[3]) * -1;
                    rmatrix[1][1] = (multirates[3]+multirates[5]+multirates[6]) * -1;
                    rmatrix[2][2] = (multirates[1]+multirates[4]+multirates[5]) * -1;
                    rmatrix[3][3] = (multirates[2]+multirates[4]+multirates[6]) * -1;

                    for (unsigned int i = 0; i < 7; i++){
                        multirates.erase(multirates.begin() + 0);
                    }
                    //cout << "Size " << multirates.size() << endl;
                    //cout << multirates[0] << endl;
                    /*
                    for (unsigned int i = 0; i < multirates.size(); i++){
                        cout << multirates[i] << endl;
                    }*/        
                }
                rate_count++;
            }
        }
        QMatrix = calculate_q_matrix();

        //PMatrix = calculate_p_matrix(QMatrix, brlength);
        Node * dec = tree->getNode(k);
        Node * parent = tree->getNode(k)->getParent();
        //ancq[dec] = QMatrix;
        vector < vector <double> > Qparent = ancq[parent];
        string ancSeq = seqs[parent];
        string decSeq = simulate_sequence(ancSeq, Qparent, brlength);
        /*
        for (unsigned int i = 0; i < Qparent.size(); i++) {
            for (unsigned int j = 0; j < Qparent.size(); j++) {
                cout << Qparent[i][j] << " ";
            }
            cout << "\n";
        }
        cout << "\n";*/
        seqs[dec] = decSeq;
        ancq[dec] = QMatrix;
        
        //cout << "anc: " << ancSeq << "; dec: " << decSeq << endl;
        if (showancs == true && tree->getNode(k)->isInternal() == true) {
            str = to_string(count);
            cout << ">" << "Node_" << str << "\n" << decSeq << endl;
            count++; 
        }
        // If its a tip print the name and the sequence
        if (tree->getNode(k)->isInternal() != true) {
            string tname = tree->getNode(k)->getName();
            //cout << ">" << tname << "\n" << decSeq << endl;
            Sequence seq(tname, decSeq);
            res.push_back(seq);
        }
    }
}


void SequenceGenerator::PrintNodeNames(Tree * tree) {
    
    int count = 0;
    string str = "Node";
    string newick = "";
    Node * root = tree->getRoot();
    seqs[root] = Ancestor;
    for (int k = (tree->getNodeCount() - 2); k >= 0; k--) {
        if (tree->getNode(k)->isInternal() == true) {
            //cout << k << endl;
            str = to_string(count);
            newick = "Node_" + str;
            tree->getNode(k)->setName(newick);
            count++;
        }
    }
    cout << tree->getRoot()->getNewick(true) <<";" << endl;
    
}


// initialized as string of length seqlenth, all 'G'
void SequenceGenerator::generate_random_sequence () {

    //string hold = "";
    //mt19937 generator(rand());
    vector<float> foo(seqlen,1.0);
    site_rates = foo;
    float gamma = 0.0;
    float normalized = 0.0;
    // keep this outside of the function, as it is constant
    float A = basefreqs[0];
    float T = basefreqs[1] + A;
    float C = basefreqs[2]  + T;
    for (int i = 0; i < seqlen; i++) {
//        int RandNumb = 0;
//        int A = (basefreqs[0] * 100);
//        int T = ((basefreqs[1] * 100) + (basefreqs[0] * 100));
//        int C = ((basefreqs[2] * 100) + T);
        //random_device rand_dev;
        //mt19937 generator(rand_dev());
        //mt19937 generator(rand());
        
//      std::uniform_int_distribution<int>  distr(0, 100);
//      hold = to_string(distr(generator));
//      RandNumb = stod(hold);
        float RandNumb = get_uniform_random_deviate();
        if (alpha != -1.0) {
            float GammaNumb = get_gamma_random_deviate(alpha);
            site_rates[i] = GammaNumb;
        }
        //cout << GammaNumb << endl;
        if (RandNumb < A) {
            Ancestor[i] = 'A';
        } else if (RandNumb > A && RandNumb < T) {
            Ancestor[i] = 'T';
        } else if (RandNumb > T && RandNumb < C) {
            Ancestor[i] = 'C';
        }
        // initialized as all Gs, so the following is not needed
        //else {
        //    Ancestor[i] = 'G';
        //}
    }
    //cout << (gamma / 1000.0) << endl;
    //cout << ">Ancestral_Seq" << "\n" << Ancestor << endl;
}



SequenceGenerator::SequenceGenerator () {
    // TODO Auto-generated constructor stub
}


SequenceGenerator::SequenceGenerator (int const &seqlenth, vector <double> const& basefreq,
    vector < vector<double> >& rmatrix, Tree * tree, bool const& showancs, 
    int const& nreps, int const& seed, float const& alpha, bool const& printpost, vector<double> const& multirates, bool const& mm):seqlen(seqlenth), basefreqs(basefreq), 
    rmatrix(rmatrix), tree(tree), showancs(showancs), nreps(nreps), seed(seed), 
    Ancestor(seqlenth, 'G'),alpha(alpha),printpost(printpost),multirates(multirates), mm(mm) {

    // set the number generator being used
    if (seed != -1) { // user provided seed
        srand(seed);
        generator = mt19937(rand());
    } else {
        random_device rand_dev;
        generator = mt19937(rand_dev());
    }
    //Print out the nodes names
    if(printpost == true){
        PrintNodeNames(tree);
    }
    // TODO: what if suer wants to pass in ancestral sequence?
    //       - need to allow this to be passed in
    generate_random_sequence();
    
    preorder_tree_traversal(tree, showancs, multirates, mm);
    //TakeInTree();
}


// not sure of a more elegant way to do this...
vector<Sequence> SequenceGenerator::get_sequences () {
    return res;
}


// call this whenever a random float is needed
float SequenceGenerator::get_uniform_random_deviate () {
    std::uniform_real_distribution<float> distribution(0.0, 1.0);
    return distribution(generator);
}


float SequenceGenerator::get_gamma_random_deviate (float alpha) {

    //default_random_engine generator;
    std::gamma_distribution<float> distribution(alpha,(1/alpha));
    return distribution(generator);
}

//SEQGEN::~SEQGEN() {
//    // TODO Auto-generated destructor stub
//}
