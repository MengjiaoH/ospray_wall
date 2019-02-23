#pragma once

// ospray side 
//#include "ospray/ospray.h"
#include <ospcommon/vec.h>
#include <ospcommon/box.h>
#include <ospcommon/FileName.h>

// c++ side
#include <vector>

/**
 *  Read File lists 
 **/
namespace importData {

  using namespace ospcommon;

  struct Node {
    vec3f pos;
    float rad;
  };

  struct Link {
    int first, second;
  };
        
  struct Tube {
    std::vector<Node> nodeData;
    std::vector<Link> linkData;
  };

  struct Volume {
    FileName fileNameOfCorrespondingXmlDoc;
    std::string fileName;
    std::string voxelType;
    vec3i dimensions;
    vec3f origin;
  };
  struct TFN {
    std::vector<vec3f> colors;
    std::vector<float> opacities;
  };
  struct Tetrahedral_Volume {
    std::vector<vec3f> vertices;
    std::vector<float> field;
    std::vector<vec4i> tetrahedra;
  };

  struct Triangle_Mesh {
    std::vector<vec3f> vertex;
    std::vector<vec3f> vertex_normal;
    std::vector<vec4f> vertex_color;
    std::vector<vec3i> index;
  };

  void readTubeFromFile(const std::string &fileName, 
			Tube &tube,
			std::vector<vec4f> &colorList);
  void readTubeFromFileList(const std::vector<std::string> &fileList, 
			    box3f &worldBounds,
			    Tube &tubes,
			    std::vector<vec4f> &colorList);
  void readVolumeFromFile(const std::string &fileName,
			  Volume &volume);
  void readTransferFunctionFromFile(const std::string &fileName, 
				    TFN &transferfunction);
  void readTetrahedralFromFile(const std::string &fileName,
			       Tetrahedral_Volume& volume);
}
