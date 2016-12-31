#include "SeqLib/BamReader.h"
#include "SeqLib/BamWriter.h"
#include <sstream>

int main(int argc, char** argv) {

  std::cerr << "Original BAM with readgroups in RG tag: " << argv[1] << std::endl;
  std::cerr << "Second BAM without readgroups:          " << argv[2] << std::endl;

  SeqLib::BamReader r;
  SeqLib::BamReader r2;

  if (!r.Open(std::string(argv[1]))) {
    std::cerr << "....could not open original BAM: " << argv[1] << std::endl;
    exit(EXIT_FAILURE);
  } 

  if (!r2.Open(std::string(argv[2]))) {
    std::cerr << "....could not open second BAM: " << argv[2] << std::endl;
    exit(EXIT_FAILURE);
  } 

  std::unordered_map<std::string, std::string> map;
  std::unordered_set<std::string> rmap;
  SeqLib::BamRecord rr;
  size_t count=0;

  // loop first BAM and get all the read groups
  while (r.GetNextRecord(rr)) {
    const std::string rg = rr.GetZTag("RG");
    if (!rg.empty()) {
      map[rr.Qname()] = rr.GetZTag("RG");
      rmap.insert(rr.GetZTag("RG"));
    } else {
      std::cerr << "no read group in original BAM for read: " << rr.Qname() << std::endl;
    }
    if (count++ % 1000000 == 0)
      std::cerr << "...getting RG from read at " << rr.Brief() << std::endl;
    
  }
  r.Close();
  
  std::string header = r2.Header().AsString();
  std::istringstream iss(header);
  std::string val;
  std::stringstream newheader;
  while (std::getline(iss, val, '\n'))
    if (val.find("RG") == std::string::npos)
      newheader << val << std::endl;
  for (auto& i : rmap)
    newheader << "@RG\tID:\t" << i << "\tPL:Illumina\tLB:Solexa-271417\tPI:0\tDT:2014-08-19T00:00:00-0400\tSM:NA12878\tCN:\tBI" << std::endl;
  
  std::string newheaderstring = newheader.str();
  newheaderstring.pop_back(); // delete the last newline

  SeqLib::BamHeader nh(newheaderstring);
  SeqLib::BamWriter w;
  w.SetHeader(nh);
  if (!w.Open("-")) {
    std::cerr << "...could not open output stream" << std::endl;
    exit(EXIT_FAILURE);
  }
  w.WriteHeader();

  while (r2.GetNextRecord(rr)) {
    const std::string rg = map[rr.Qname()];
    if (rg.empty()) 
      std::cerr << "RG empty for read " << rr.Qname() << std::endl;
    else {
      rr.AddZTag("RG", rg);
    }
    w.WriteRecord(rr);
    if (count++ % 1000000 == 0)
      std::cerr << "...adding RG to read " << rr.Brief() << std::endl;
  }

  r2.Close();
  w.Close();  
}
