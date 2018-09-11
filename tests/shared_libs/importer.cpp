#include <iostream>
#include "importer.h"
#include "XML.h"

using namespace ospcommon;

// need to read binary file
template<typename T> T readBinary(std::ifstream& file) {
  T x; file.read((char*)&x, sizeof(T));
  return x;
}

static const char *delim = "\n\t\r ";

namespace importData {
  
  void osxParseInts(std::vector<int> &vec, const std::string &content)
  {
    char *s = strdup(content.c_str());
    char *tok = strtok(s,delim);
    while (tok) {
      vec.push_back(atof(tok));
      tok = strtok(NULL,delim);
    }
    free(s);
  }
  void osxParsefloats(std::vector<float> &vec, const std::string &content)
  {
    char *s = strdup(content.c_str());
    char *tok = strtok(s,delim);    
    while (tok) {
      vec.push_back(std::stof(tok));
      tok = strtok(NULL,delim);
    }
    free(s);
  }

  template<typename T> T ato(const char *);
  template<> inline int ato<int>(const char *s) { return atol(s); }
  template<> inline float ato<float>(const char *s) { return atof(s); }

  template<typename T>
  vec_t<T,3> osxParseVec3(char * &tok) {
    vec_t<T,3> v;
    assert(tok);
    v.x = ato<T>(tok);
    tok = strtok(NULL,delim);
    assert(tok);
    v.y = ato<T>(tok);
    tok = strtok(NULL,delim);
    assert(tok);
    v.z = ato<T>(tok);
    tok = strtok(NULL,delim);
    return v;
  }

  template<typename T>
  vec_t<T,4> osxParseVec4(char * &tok) {
    vec_t<T,4> v;
    assert(tok);
    v.x = ato<T>(tok);
    tok = strtok(NULL,delim);
    assert(tok);
    v.y = ato<T>(tok);
    tok = strtok(NULL,delim);
    assert(tok);
    v.z = ato<T>(tok);
    tok = strtok(NULL,delim);
    assert(tok);
    v.w = ato<T>(tok);
    tok = strtok(NULL, delim);
    return v;
  }

  void osxParseVec3is(std::vector<vec3i> &vec, const std::string &content)
  {
    char *s = strdup(content.c_str());
    char *tok = strtok(s,delim);
    while (tok)
      vec.push_back(osxParseVec3<int>(tok));
    free(s);
  }

  void osxParseVec3fas(std::vector<vec3f> &vec, const std::string &content)
  {
    char *s = strdup(content.c_str());
    char *tok = strtok(s,delim);
    while (tok)
      vec.push_back(osxParseVec3<float>(tok));
    free(s);
  }

  void osxParseVec4is(std::vector<vec4i> &vec, const std::string &content)
  {
    char *s = strdup(content.c_str());
    char *tok = strtok(s,delim);
    while (tok){
      vec.push_back(osxParseVec4<int>(tok));
    }

    free(s);
  }
    
  void parseRAW(const std::string file, Tube &tubes) 
  {
    std::ifstream myfile;
    myfile.open(file, std::ios::binary);
    myfile.seekg(0);
            
    int numStreamLines = readBinary<int>(myfile);
    std::cout << numStreamLines << std::endl;

    for (int i = 0; i < numStreamLines; ++i) {
      int numPoints = readBinary<int>(myfile);
      for (int j = 0; j < numPoints; j++) {
	Node tempNode;
	Link tempLink;
	vec3f p = readBinary<vec3f>(myfile);
	p.y  *= -1.f; 
	tempNode = {p, 0.02};
	size_t curNodeID = tubes.nodeData.size();
	if (j == 0) {
	  tempLink = {static_cast<int>(curNodeID), 
		      static_cast<int>(curNodeID)};
	} else {
	  tempLink = {static_cast<int>(curNodeID), 
		      static_cast<int>(curNodeID-1)};
	}
	tubes.nodeData.push_back(tempNode);
	tubes.linkData.push_back(tempLink);
      }
    }
    std::cout << "done parsing raw data" << std::endl;
  }

  bool parseOSP(const std::string file, Volume &volume){
    std::shared_ptr<xml::XMLDoc> doc;
    std::cout << "#osp:osp: starting to read OSPRay XML file '" << file << "'" 
	      << std::endl;
    doc = xml::readXML(file);
    std::cout << "#osp:osp: XML file read, starting to parse content..." << std::endl;
    assert(doc); 
    if (doc->child.empty())
      throw std::runtime_error("ospray xml input file does not contain any nodes!?");

    if (doc->child[0]->name == "volume") {
      vec3i dimensions(-1);
      vec3f origin(0);
      std::string fileName = "";
      std::string voxelType = "";
      xml::for_each_child_of(*doc->child[0],[&](const xml::Node &child){
	  if (child.name == "dimensions")
	    dimensions = toVec3i(child.content.c_str());
	  else if (child.name == "voxelType")
	    voxelType = child.content;
	  else if (child.name == "filename")
	    fileName = child.content;
	  else if (child.name == "origin") {
	    origin = toVec3f(child.content.c_str());
	  } else {
	    throw std::runtime_error("unknown old-style osp file " 
				     "component volume::" + child.name);
	  }
	});
      volume.fileName = fileName;
      volume.fileNameOfCorrespondingXmlDoc = doc->fileName;
      volume.dimensions = dimensions;
      volume.voxelType = voxelType;
      volume.origin = origin;
    }
    return true;
  }

  bool  parseOSX(const std::string file, Tetrahedral_Volume &volume){
    std::shared_ptr<xml::XMLDoc> doc;
    std::cout << "#osp:osx: starting to read OSPRay XML file '" << file << "'" 
	      << std::endl;
    doc = xml::readXML(file);
    std::cout << "#osp:osx: XML file read, starting to parse content..." << std::endl;
    assert(doc); 

    if (doc->child.size() != 1 || doc->child[0]->name != "OSPRay")
      throw std::runtime_error("could not parse osx file: Not in OSPRay format!?");
            
    const xml::Node &root_element = *doc->child[0];

    xml::for_each_child_of(root_element,[&](const xml::Node &node){
	if (node.name == "Model") {

	  const xml::Node &model_node = node;
	  xml::for_each_child_of(model_node, [&](const xml::Node &node){
	      if (node.name == "UnstructuredVolume") {

		const xml::Node &sl_node = node;
		xml::for_each_child_of(sl_node, [&](const xml::Node &node){
		    if (node.name == "vertex"){
		      osxParseVec3fas(volume.vertices, node.content);
		    }
		    else if (node.name == "field"){
		      osxParsefloats(volume.field, node.content);
		    }
		    else if(node.name == "tetrahedra"){
		      osxParseVec4is(volume.tetrahedra, node.content);
		    }

		  });
              } 
            });
	}
      });
  }

  bool parseSWC(const std::string file, Tube &Tube, std::vector<vec4f> &colors) 
  {
    int lineNum = 0;

    std::ifstream infile(file);
    vec3f p;
    float radius;
    int first, second;
    int partIndex;
    vec4f color;
    Link tempNodeLink;
    Node tempNode;

    while(infile 
	  >> first 
	  >> partIndex
	  >> p.x >> p.y >> p.z 
	  >> radius 
	  >> second 
	  >> color.x >> color.y >> color.z >> color.w) {

      lineNum++; // the number of nodes
      if (second == -1 ){
	second = first;
      }

      tempNodeLink = {first - 1, second - 1};
      Tube.linkData.push_back(tempNodeLink);
      tempNode = {p, radius};
      Tube.nodeData.push_back(tempNode);
      colors.push_back(color);
    }

    // add partIndex to link
    // for(size_t j = 0; j < Tube.linkData.size(); j++){
    //   int pre = Tube.linkData[j].second;
    //   int partIndex = Tube.nodeData[pre].partIndex;
    //   Tube.linkData[j].partIndex = partIndex;
    //   Tube.linkData[j].preNumStreamLines = 1;
    // }

    std::cout << "#osp:tube:importer: done parsing " << file 
	      << " (" << Tube.nodeData.size() << " nodes)" << std::endl;
    return true;
  } 

  void readTubeFromFileList(const std::vector<std::string> &fileList, 
			    box3f &worldBounds,
			    Tube &tubes,
			    std::vector<vec4f> &colorList)
  {
    for(int i = 0; i < fileList.size(); i++){
      std::string fileName = fileList[i];
      readTubeFromFile(fileName, tubes, colorList);
    }
    worldBounds = empty;
    for (int i = 0; i < tubes.nodeData.size(); i++) {
      worldBounds.extend(tubes.nodeData[i].pos-vec3f(tubes.nodeData[i].rad));
      worldBounds.extend(tubes.nodeData[i].pos+vec3f(tubes.nodeData[i].rad));
    }
  }

  void readTubeFromFile(const std::string &fileName,
			Tube &tubes,
			std::vector<vec4f>& colorList)
  {
    FILE *file = fopen(fileName.c_str(), "r");
    FileName filename = fileName;
    if(!file) {
      throw std::runtime_error("cannot load " + fileName);
    }
    if(filename.ext() == "slraw") {
      parseRAW(fileName, tubes);
    } else if(filename.ext() == "swc") {
      parseSWC(fileName, tubes, colorList);
      std::cout << "#osp:tube:importer: done parse swc file... " << std::endl;
    }
    fclose(file);
  }

  void readVolumeFromFile(const std::string &fileName,
			  Volume& volume) 
  {
    FILE *file = fopen(fileName.c_str(), "r");
    FileName filename = fileName;
    if(!file) {
      throw std::runtime_error("load fail " + fileName);
    }
    if(filename.ext() == "osp") {
      parseOSP(fileName, volume);
    }
  }

  void readTransferFunctionFromFile(const std::string &fileName, TFN &tfn)
 {
    std::ifstream myfile(fileName);
    vec3f color; float alpha;
    if(myfile.is_open()) {
      while(myfile >> color.x >> color.y >> color.z >> alpha) {
	tfn.colors.push_back(color);
	tfn.opacities.push_back(alpha);
      }
    }
  }

  void readTetrahedralFromFile(const std::string &fileName,
			       Tetrahedral_Volume &volume) {
    parseOSX(fileName, volume);
  }
}
