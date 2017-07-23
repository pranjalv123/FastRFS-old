#include "CladeExtractor.hpp"
#include "TaxonSet.hpp"
#include <util/Logger.hpp>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <util/Logger.hpp>
#include "AstralInterface.hpp"

unordered_set<Clade> CladeExtractor::cl_clades;
unordered_set<clade_bitset> CladeExtractor::cl_cladetaxa;
TaxonSet* CladeExtractor::ts;

TaxonSet& CladeExtractor::get_taxonset() {
  get_from_cl();
  return *ts;
}
unordered_set<Clade>& CladeExtractor::get_clades() {
  get_from_cl();
  return cl_clades;
}
unordered_set<clade_bitset>& CladeExtractor::get_cladetaxa() {
  get_from_cl();
  return cl_cladetaxa;
}


void CladeExtractor::get_from_cl() {
  if (cl_clades.size()) {
    return;
  }

  PROGRESS << "Reading Clades" << endl;
  
  string genetreesfile;
  Options::get("g genetrees", &genetreesfile); 
  
  stringstream clade_stream;

  string cladefile;
  
  if (Options::get("X cladefile", &cladefile)) {
     ifstream cladefilestream(cladefile);
     clade_stream << cladefilestream.rdbuf();
  }

  string astralfile;
  string scoretree;
  
  if (Options::get("a astral", &astralfile)) {
    AstralInterface ai(astralfile);
    string genetreesfile;
    string extratreesfile;

    Options::get("g genetrees", &genetreesfile);
   
    Options::get("e extragenetrees", &extratreesfile);
    if (Options::get("x exact")) {
      INFO << "Running ASTRAL in exact mode " << endl;
      clade_stream << ai.getClades(genetreesfile, extratreesfile, true, false);
    } else if (Options::get("limited")){
      INFO << "Running ASTRAL in limited mode " << endl;
      clade_stream << ai.getClades(genetreesfile, extratreesfile, false, true);
    } else if (Options::get("s score", &scoretree)) {
      INFO << "Scoring tree " << scoretree << endl;
      clade_stream << ai.getClades(scoretree, extratreesfile, false, true);
    } else if (Options::get("rootedscore", &scoretree)) {
      INFO << "Scoring rooted tree " << scoretree << endl;
      clade_stream << ai.getClades(scoretree, extratreesfile, false, true);
    } else {
      INFO << "Running ASTRAL in default mode  " << extratreesfile << endl;
      clade_stream << ai.getClades(genetreesfile, extratreesfile, false, false);
      if (Options::get("extraextra")) {
	clade_stream << ai.getClades(extratreesfile, "", false, false);
	string fname = tmpnam(0);
	DEBUG << "Getting lots more clades " << fname << endl;
	ofstream fs(fname);
	ifstream gtstream(genetreesfile);
	ifstream extrastream(extratreesfile);
	fs << gtstream.rdbuf() << '\n' << extrastream.rdbuf() << endl;
	fs.flush();
	clade_stream  << ai.getClades(fname, "", false, false);
      }
    }
  }

  INFO << "Got clades as string!" << endl;

  DEBUG << clade_stream.str() << endl;
  
  ts = new TaxonSet(clade_stream.str());
  INFO << ts->size() << " taxa" << endl;
  ts->freeze();
  
  if (Options::get("rootedscore", &scoretree)) {
    ifstream f(scoretree);
    string tree;
    getline(f, tree);
    unordered_set<Taxon> tt;
    cl_clades = CladeExtractor::extract(*ts, tree, tt);
    for (auto& clade: cl_clades) {
      cl_cladetaxa.insert(clade.taxa);      
    }
  }

  else {
    stringstream ss(clade_stream.str());
    string s;
    int n = 0;
    while (!ss.eof()) {
      getline(ss, s);
      Clade c(*ts, s);
      cl_clades.insert(c);
      cl_cladetaxa.insert(c.taxa);
      if (n % 10000 == 0) {
	INFO << "Read " << n << " clades" << endl;
      }
      n++;

    }
    INFO << "Read " << n << " clades" << endl;
  }


  unordered_set<Clade> to_add;

  string enhancelevel_str;
  int enhancelevel;
  
  if (Options::get("enhance", &enhancelevel_str)) {
    enhancelevel = atoi(enhancelevel_str.c_str());
    INFO << "Before enhancing: " << cl_clades.size() << endl;
    INFO << "Adding clades of size at most " << enhancelevel << endl;
    for (const Clade& x: cl_clades) {
      for (const Clade& y: cl_clades) {
	if (x.contains(y) && (x.size() > y.size()) && (x.size() - y.size() < enhancelevel)) {
	  to_add.insert(x.minus(y));
	  if (to_add.size() % 10000 == 0) {
	    INFO << "have " << to_add.size() << " clades" << endl;
	  }
	}
      }
    }
    for (const Clade& x : to_add) {
      cl_clades.insert(x);
      cl_cladetaxa.insert(x.taxa);
    }
    INFO << "After enhancing: " << cl_clades.size() << endl;
  }
  
  Clade alltaxa(*ts);
  for (size_t i = 0; i < ts->size(); i++) {
    alltaxa.add(i);
    Clade single(*ts);
    single.add(i);
    cl_clades.insert(single);
    cl_cladetaxa.insert(single.taxa);
  }
  
  cl_clades.insert(alltaxa);
  INFO << "Total clades: " << cl_clades.size() << endl;
  if (alltaxa.size() == 0) {
    ERR << "No clades found\n";
    exit(-1);
  }

}



unordered_set<Clade> CladeExtractor::extract(TaxonSet& ts, const string& tree_c, unordered_set<Taxon>& tree_taxa) {
  unordered_set<Clade> clades;
  unordered_set<string> taxa;
  
  newick_to_clades(tree_c, ts, clades);
  newick_to_ts(tree_c, taxa);
  for (const string& t : taxa)  {
    tree_taxa.insert(ts[t]);
  }

  return unordered_set<Clade>(clades.begin(), clades.end());
}


void CladeExtractor::test() {
  cout << "Testing CladeExtractor..." << endl;
  TaxonSet ts("");
  string tree("(((a,b),(c,d)),e,(f,g,(h,i)));");
  cout << tree << endl;
  unordered_set<Taxon> tree_taxa;
  unordered_set<Clade> clades = CladeExtractor::extract(ts, tree, tree_taxa);
  for (const Clade& c : clades) {
    cout << c.str() << endl;
  }
  cout << "Done!" << endl;
}
