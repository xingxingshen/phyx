#ifndef _UPGMA_H_
#define _UPGMA_H_

#include <string>
#include <vector>
#include <map>
#include <iostream>

class UPGMA {
private:
    int ntax_;
    int nchar_;
    std::string seqfile_;
    std::string newickstring_; // temporary
    
    std::map<int, std::string> nameKey_;
    std::map<std::string, std::string> sequences_;
    std::vector<std::string> names_;
    std::vector< std::vector<double> > distmatrix_;
    
    void set_name_key();
    std::vector< std::vector<double> > build_matrix (std::map<std::string,
        std::string>& sequences);
    void make_tree (std::vector<std::string>&, std::map <int, std::string>&,
        std::vector< std::vector<double> >&);
    void choose_small (int& node_list, const std::vector< std::vector<double> >& Matrix,
        int& mini1, int& mini2);

public:
    UPGMA (std::istream* pios);
    std::string get_newick () const;
};

#endif /* _UPGMA_H_ */
