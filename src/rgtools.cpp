#include <iostream>
#include <sstream>

#include "rgtools.h"

#include "SeqLib/BamReader.h"
#include "SeqLib/BamWriter.h"

static const std::string delimiter = ":";

void parseRG(const std::string& qname, std::string& parsed_rg) {

  // try parsing the RG (http://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c)
  size_t pos = 0;
  std::string token;
  size_t dnum = 0;
  std::string s = qname;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    ++dnum;
    parsed_rg += s.substr(0, pos) + "_";
    if (dnum == 2) { // only get up to second delim (flowcell:lanenumber)
      if (parsed_rg.size())
	parsed_rg.pop_back(); // remove last "_"
      break;
    }
    s.erase(0, pos + delimiter.length());
  }
}

void runRGTools(int argc, char** argv) {

  if (argc < 4) {
    std::cerr << "rgtools old_bam norg_bam sample_name > new_bam_with_rg" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cerr << "Original BAM with readgroups in RG tag: " << argv[1] << std::endl;
  std::cerr << "Second BAM without readgroups:          " << argv[2] << std::endl;

  bool diff_bams = std::string(argv[1]) != std::string(argv[2]);

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

  //std::unordered_map<std::string, std::string> map;
  std::unordered_map<uint32_t, uint8_t> map_hash;
  std::unordered_map<std::string, uint8_t> rmap;
  std::unordered_map<uint8_t, std::string> id_to_rg;
  size_t rg_count = 0;
  SeqLib::BamRecord rr;
  size_t count=0;

  size_t no_rg = 0;

  std::string rg;
  // loop first BAM and get all the read groups
  while (r.GetNextRecord(rr)) {

    if (!rr.GetZTag("RG", rg)) {
      ++no_rg;

      // parse the RG
      parseRG(rr.Qname(), rg);
      
      if (no_rg < 10)
	std::cerr << "no read group in original BAM for read: " << rr.Qname() << " -- parsing read group from name and got: " << rg << std::endl;
      if (no_rg == 10)
	std::cerr << "...won't pring this warning anymore. Will proceed by parsing read names" << std::endl;
    }

    // add the RG
    if (!rg.empty()) {
      if (!rmap.count(rg)) { // not seen this rg before
	++rg_count;
	rmap[rg] = rg_count;
	id_to_rg[rg_count] = rg;
      }
      if (diff_bams) { // if diff bams, store rg info
	uint32_t k = __ac_Wang_hash(__ac_X31_hash_string(rr.Qname().c_str()));
	map_hash[k] = rmap[rg]; //rmap[rg] is uint8_t
	//map[rr.Qname()] = rmap[rg];
      }
    }
    
    if (count++ % 1000000 == 0)
      std::cerr << "...getting RG from read at " << rr.Brief() << " map.size() " << map_hash.size() << std::endl;
    
  }
  r.Close();
  
  std::string header = r2.Header().AsString();
  std::istringstream iss(header);
  std::string val;
  std::stringstream newheader;
  while (std::getline(iss, val, '\n'))
    if (val.find("RG") == std::string::npos)
      newheader << val << std::endl;

  const std::string pl = "Illumina"; // platform
  const std::string lb = "Solexa"; // library
  const std::string sm = std::string(argv[3]); // sample
  const std::string cn = "BI"; // sequencing center (Broad Institute)

  assert(!pl.empty());
  assert(!sm.empty());
  assert(!lb.empty());
  assert(!cn.empty());
  
  for (auto& i : rmap) {
    assert(!i.first.empty());
    std::stringstream ss; 
    ss << "@RG\tID:" << i.first << "\tPL:" << pl << "\tLB:" << lb << 
      "\tSM:" << sm << "\tCN:" << cn << std::endl;
    std::cerr << ss.str();
    newheader << ss.str();
  }
  
  newheader << "@PG\tID:rgtools\tVN:0.1.0\tCL:" << argv[0] << " " << argv[1] << " " << argv[2] << " " << argv[3] << std::endl;
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
    uint32_t k = __ac_Wang_hash(__ac_X31_hash_string(rr.Qname().c_str()));    
    //std::string rg = map[rr.Qname()];
    std::string rg = id_to_rg[map_hash[k]];
    if (rg.empty()) {
      if (diff_bams) // if diff bams, then this should not happen
	std::cerr << "RG empty for read " << rr.Qname() << std::endl;
      else { // if same, we are parsing RG from qname, its expected
	parseRG(rr.Qname(), rg);
      }
    }
    
    if (!rg.empty())
      rr.AddZTag("RG", rg);

    w.WriteRecord(rr);
    if (count++ % 1000000 == 0)
      std::cerr << "...adding RG to read " << rr.Brief() << std::endl;
  }

  r2.Close();
  w.Close();  
}
