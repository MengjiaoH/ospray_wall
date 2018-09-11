#include "shared_libs/importer.h"
#include "ospcommon/vec.h"
#include "ospcommon/box.h"
#include "ospcommon/LinearSpace.h"

//#define cut

OSPGeometry tube(const std::string filename){
        std::vector<std::string> files;
        files.push_back(filename);
        ospcommon::box3f worldBounds = ospcommon::empty;
	    
        importData::Tube tubes;
	    std::vector<ospcommon::vec4f> colorList;
        importData::readTubeFromFileList(files, worldBounds, tubes, colorList);
        ospcommon::vec3f lower = worldBounds.lower;
        ospcommon::vec3f upper = worldBounds.upper;
        std::cout << "world bounding box lower is : " << lower.x << " " << lower.y << " " << lower.z << std::endl;
        std::cout << "world bounding box upper is : " << upper.x << " " << upper.y << " " << upper.z << std::endl;
        std::cout << "color size: " << colorList.size() << "\n";
        
	    //std::cout << "===== DEBUG =====" << std::endl;
	    //for(int i = 0; i < tubes.linkData.size(); i++){
		    //std::cout << "node position " << tubes.nodeData[i].pos.x << ", "
			    			  //<< tubes.nodeData[i].pos.y << ", "
				    		  //<< tubes.nodeData[i].pos.z << std::endl;

		    //std::cout << "link first " << tubes.linkData[i].first << ", " << 
			    			  //tubes.linkData[i].second << std::endl;
		    //std::cout << "color      " << colorList[i].x << ", " 
				    	   //<< colorList[i].y << ", "
					       //<< colorList[i].z << ", "
					     //<< colorList[i].w << std::endl;
	    //}
	    //std::cout << "=================" << std::endl;
	    //std::cout << std::endl;
        
        // Hack here to cut geomety in half
    #ifdef cut
        std::vector<importData::Node> cuted_node;
        std::vector<importData::Link> cuted_link;
        std::vector<ospcommon::vec4f> cuted_colorList;
        ospcommon::box3f cuted_bounds = worldBounds;
        cuted_bounds.upper.z = (upper.z + lower.z) / 2;
        //cuted_bounds.upper.x = (upper.x + lower.x) / 2;    
        //cuted_bounds.upper.y = (upper.y + lower.y) / 2;
        std::cout << "cuted bounding box lower is: " << cuted_bounds.lower.x << " " << 
                                                        cuted_bounds.lower.y << " " <<
                                                        cuted_bounds.lower.z << std::endl; 
        std::cout << "cuted bounding box upper is: " << cuted_bounds.upper.x << " " << 
                                                        cuted_bounds.upper.y << " " <<
                                                        cuted_bounds.upper.z << std::endl; 
        
        std::vector<int> linkIndex;
        for(size_t j = 0; j < tubes.linkData.size(); j++){
            if(tubes.linkData[j].first == tubes.linkData[j].second){
                linkIndex.push_back(j);
            }
        }

        for(size_t j = 0; j < linkIndex.size(); j++){
            int start, end;
            if(j != linkIndex.size() - 1){
                start = linkIndex[j];
                end = linkIndex[j + 1];
            }else{
                start = linkIndex[j];
                end = tubes.linkData.size();
            }
            for(size_t k = start + 1; k < end; k++){
                importData::Link link0 = tubes.linkData[k];
                importData::Link link1 = tubes.linkData[k - 1];
                if(link0.second != link1.first){
                    std::cout << "Is not continued and k = " << k << std::endl; 
                } 
            }
        }
        // DEBUG
        //for(size_t j = 0; j < linkIndex.size(); j++){
            //std::cout << linkIndex[j] << " " ;
        //} 
        
        std::cout << std::endl;
        for(size_t j = 0; j < linkIndex.size(); j++){
            int start, end;
            if(j != linkIndex.size() - 1){
                start = linkIndex[j];
                end = linkIndex[j + 1];
            }else{
                start = linkIndex[j];
                end = tubes.linkData.size();
            }
            
            importData::Link link = tubes.linkData[start];
            importData::Node node = tubes.nodeData[start];
            ospcommon::vec3f pos = node.pos;
            bool x = node.pos.x > cuted_bounds.lower.x && node.pos.x < cuted_bounds.upper.x;
            bool y = node.pos.y > cuted_bounds.lower.y && node.pos.y < cuted_bounds.upper.y;
            bool z = node.pos.z > cuted_bounds.lower.z && node.pos.z < cuted_bounds.upper.z;
            if(x && y && z){
                for(size_t i = start; i < end; i++){
                    link = tubes.linkData[i];
                    node = tubes.nodeData[i];
                    pos = node.pos;
                    x = node.pos.x > cuted_bounds.lower.x && node.pos.x < cuted_bounds.upper.x;
                    y = node.pos.y > cuted_bounds.lower.y && node.pos.y < cuted_bounds.upper.y;
                    z = node.pos.z > cuted_bounds.lower.z && node.pos.z < cuted_bounds.upper.z;
                    //if(x && y && z){
                        cuted_node.push_back(node);
                        importData::Link temp_link;
                        if(link.first == link.second){
                            temp_link.first = cuted_node.size() - 1;
                            temp_link.second = temp_link.first;
                        }else{
                            temp_link.first = cuted_node.size() - 1;
                            temp_link.second = temp_link.first - 1;
                        }
                        cuted_link.push_back(temp_link);    
                        //std::cout << "link first = " << link.first << " link second = " << link.second << std::endl; 
                        //std::cout << "temp_link first = " << temp_link.first << " temp_link second = " << temp_link.second << std::endl;
                        cuted_colorList.push_back(colorList[i]);
                    //}
                }
                
            }
        }
        // cut the start striaght lines part
        cuted_bounds.lower.x = 40.f;
        std::vector<importData::Node> revised_node;
        std::vector<importData::Link> revised_link;
        std::vector<ospcommon::vec4f> revised_color;

        std::vector<int> revised_linkIndex;
        
        for(size_t j = 0; j < cuted_link.size(); j++){
            if(cuted_link[j].first == cuted_link[j].second){
                //std::cout << "line start = " << j << std::endl;
                revised_linkIndex.push_back(j);
            }
        }

        for(size_t j = 0; j < revised_linkIndex.size(); j++){
            int start, end;
            if(j != revised_linkIndex.size() - 1){
                start = revised_linkIndex[j];
                end = revised_linkIndex[j + 1];
            }else{
                start = revised_linkIndex[j];
                end = cuted_link.size();
            }
            //std::cout << "start = " << start << " end = " << end << " ";
            for(int k = start; k < end; k++){
                importData::Node node = cuted_node[k];
                importData::Link link = cuted_link[k];
                bool x = node.pos.x > cuted_bounds.lower.x;
                if(x){
                    // this would be the first node
                    revised_node.push_back(node);
                    importData::Link tempLink;
                    tempLink.first = revised_node.size() - 1;
                    tempLink.second = tempLink.first;
                    revised_link.push_back(tempLink);
                    revised_color.push_back(cuted_colorList[k]);
                    start = k;
                    break;
                }
            }

            for(int m = start + 1; m < end; m++){
                importData::Node node = cuted_node[m];
                importData::Link link = cuted_link[m];
                revised_node.push_back(node);
                importData::Link tempLink;
                tempLink.first = revised_node.size() - 1;
                tempLink.second = tempLink.first - 1;
                revised_link.push_back(tempLink);
                revised_color.push_back(cuted_colorList[m]);
            }
        }
        
        OSPData nodeData = ospNewData(revised_node.size() * sizeof(importData::Node), OSP_RAW, revised_node.data());
        OSPData linkData   = ospNewData(revised_link.size() * sizeof(importData::Link), OSP_RAW, revised_link.data());
        OSPData colorData   = ospNewData(revised_color.size() * sizeof(ospcommon::vec4f), OSP_RAW, revised_color.data());
#else
        OSPData nodeData = ospNewData(tubes.nodeData.size() * sizeof(importData::Node), OSP_RAW, tubes.nodeData.data());
        OSPData linkData   = ospNewData(tubes.linkData.size() * sizeof(importData::Link), OSP_RAW, tubes.linkData.data());
        OSPData colorData   = ospNewData(colorList.size() * sizeof(ospcommon::vec4f), OSP_RAW, colorList.data());
#endif
        
        ospCommit(nodeData);
        ospCommit(linkData);
        ospCommit(colorData);
	
        // ospray geometry
        //OSPGeometry tubeGeo = ospNewGeometry("linkprimitive_tubes");
        OSPGeometry tubeGeo = ospNewGeometry("tubes");
        ospSetData(tubeGeo, "nodeData", nodeData);
        ospSetData(tubeGeo, "linkData", linkData);
        ospSetData(tubeGeo, "colorData", colorData);

        ospCommit(tubeGeo);
	return tubeGeo;
}

OSPGeometry streamlines(){
    float vertex[] = { -1.0f, -1.0f, 3.0f, 0.f,
                       -1.0f,  1.0f, 3.0f, 0.f,
                       1.0f, -1.0f, 3.0f, 0.f,
                       0.1f,  0.1f, 0.3f, 0.f };
    float color[] =  { 0.9f, 0.5f, 0.5f, 1.0f,
                     0.8f, 0.8f, 0.8f, 1.0f,
                     0.8f, 0.8f, 0.8f, 1.0f,
                     0.5f, 0.9f, 0.5f, 1.0f };
    int32_t index[] = {0, 1, 2};
    float radius[] = {0.5, 0.5 ,0.5 ,0.5};

    OSPGeometry streamlines = ospNewGeometry("streamlines");
    OSPData data = ospNewData(4, OSP_FLOAT3A, vertex, 0);
    ospCommit(data);
    ospSetData(streamlines, "vertex", data);
    
    data = ospNewData(4, OSP_FLOAT4, color, 0);
    ospCommit(data);
    ospSetData(streamlines, "vertex.color", data);

    data = ospNewData(1, OSP_INT, index, 0);
    ospCommit(data);
    ospSetData(streamlines, "index", data);

    data = ospNewData(4, OSP_FLOAT, radius, 0);
    ospCommit(data);
    ospSetData(streamlines, "vertex.radius", data);

    ospCommit(streamlines);
    return streamlines;




}
