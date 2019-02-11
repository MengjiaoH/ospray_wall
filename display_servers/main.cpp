#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "ospcommon/vec.h"
#include "ospcommon/networking/Socket.h"
#include "ospcommon/networking/SocketFabric.h"
#include "ospcommon/networking/BufferedDataStreaming.h"
#include "mpiCommon/MPICommon.h"

#include "Server.h"
#include "../common/WallConfig.h"
#include "../common/ServiceInfo.h"
#include "glfwWindow.h"

using namespace std;
using namespace ospcommon;

void usage(const std::string &err)
    {
      if (!err.empty()) {
        cout << "Error: " << err << endl << endl;
      }
      cout << "usage: ./ospDisplayWald [args]*" << endl << endl;
      cout << "w/ args: " << endl;
      cout << "--width|-w <numDisplays.x>        - num displays in x direction" << endl;
      cout << "--height|-h <numDisplays.y>       - num displays in y direction" << endl;
      cout << "--window-size|-ws <res_x> <res_y> - window size (in pixels)" << endl;
      cout << "--[no-]head-node | -[n]hn         - use / do not use dedicated head node" << endl;
      exit(!err.empty());
    }



namespace ospray{
    namespace dw{
        
        // size_t numWrittenThisFrame = 0;
        // std::mutex addMutex;
        /*! the display callback */
        void displayNewFrame(const uint32_t *left, 
                             const uint32_t *right, 
                             void *object)
        {
            GLFWindow *window = (GLFWindow*)object;
            window->setFrameBuffer(left,right);
        }

        extern "C" int main(int argc, const char **argv)
        {
    
            //! Initialize mpi
            mpicommon::init(&argc, argv, true);
            if(!mpicommon::mpiIsThreaded){
                throw std::runtime_error("currently requiring MPI with multi-threading support");
            }
            mpicommon::Group world(MPI_COMM_WORLD);

            auto error_callback = [](int error, const char* description){
                fprintf(stderr, "glfw error %d: %s\n", error, description);
            };    
            glfwSetErrorCallback(error_callback);

            if (!glfwInit()) {
                fprintf(stderr, "Failed to initialize GLFW\n");
                exit(EXIT_FAILURE);
            }

            //! Default settings
            bool hasHeadNode  = false;
            bool doStereo     = false;
            bool doFullScreen = false;
            WallConfig::DisplayArrangement arrangement = WallConfig::Arrangement_xy;
            vec2f relativeBezelWidth(0.f);
            vec2i windowSize(320,240);
            vec2i windowPosition(0,0);//default is (0, 0)
            vec2i numDisplays(0,0);
            int desiredInfoPortNum = 2903; // Used for receiving tiles
            int process_pernode = 4;
            int remote_mode = 0;
            int clientNum = 0;
            //! Command line arguments 
            for (int i = 1;i < argc; i++) {
                const std::string arg = argv[i];
                if (arg == "--head-node" || arg == "-hn") {
                    hasHeadNode = true;
                } else if (arg == "--stereo" || arg == "-s") {
                    doStereo = true;
                } else if (arg == "--no-head-node" || arg == "-nhn") {
                    hasHeadNode = false;
                } else if (arg == "--width" || arg == "-w") {
                    assert(i+1 < argc);
                    numDisplays.x = atoi(argv[++i]);
                } else if (arg == "--arrangement" || arg == "-a") {
                    assert(i+1 < argc);
                    std::string arrangementName = argv[++i];
                    if (arrangementName == "xy")
                        arrangement = WallConfig::Arrangement_xy;
                    else if(arrangementName == "yx")
                        arrangement = WallConfig::Arrangement_yx;
                    else if (arrangementName == "xY")
                        /* y is inverted */
                        arrangement = WallConfig::Arrangement_xY;
                    else if (arrangementName == "Yx")
                        /* y minor, y is inverted */
                        arrangement = WallConfig::Arrangement_Yx;
                    else 
                        throw std::runtime_error("arrangement type not implemented");
                } else if (arg == "--height" || arg == "-h") {
                    assert(i+1 < argc);
                    numDisplays.y = atoi(argv[++i]);
                } else if (arg == "--window-size" || arg == "-ws") {
                    assert(i+1 < argc);
                    windowSize.x = atoi(argv[++i]);
                    assert(i+1 < argc);
                    windowSize.y = atoi(argv[++i]);
                } else if (arg == "--full-screen" || arg == "-fs") {
                    windowSize = vec2i(-1);
                    doFullScreen = true;
                } else if (arg == "--bezel" || arg == "-b") {
                    if (i+2 >= argc) {
                        printf("format for --bezel|-b argument is '-b <x> <y>'\n");
                        exit(1);
                    }
                    assert(i+1 < argc);
                    relativeBezelWidth.x = atof(argv[++i]);
                    assert(i+1 < argc);
                    relativeBezelWidth.y = atof(argv[++i]);
                } else if (arg == "--port" || arg == "-p") {
                    desiredInfoPortNum = atoi(argv[++i]);
                } else if(arg == "--perhost"){
                    process_pernode = atoi(argv[++i]);
                }
                //else if(arg == "--remote"){
                    //remote_mode = atoi(argv[++i]);
                //}
                else {
                    usage("unkonwn arg "+arg);
                } 
            }
     
            if (numDisplays.x < 1) 
                usage("no display wall width specified (--width <w>)");
            if (numDisplays.y < 1) 
                usage("no display wall height specified (--heigh <h>)");

            if (world.size != (numDisplays.x * numDisplays.y + hasHeadNode * process_pernode))
                throw std::runtime_error("invalid number of ranks for given display/head node config");

            const int displayNo = hasHeadNode ? world.rank - process_pernode : world.rank;
            std::cout << "display No is " << displayNo << " on rank #" << world.rank << std::endl;
    
            const vec2i displayID(displayNo % numDisplays.x, displayNo / numDisplays.x);

            char title[1000];
            if(hasHeadNode && world.rank >= process_pernode){
                sprintf(title, "rank %i/%i, display (%i,%i)", world.rank - 1, world.size - 1, displayID.x, displayID.y);
            }else{
                sprintf(title,"rank %i/%i, display (%i,%i)",world.rank,world.size,displayID.x,displayID.y);
            }
            //if (doFullScreen)
            //windowSize = GLFWindow::getScreenSize();
    
            //! Initialize WallConfig
            WallConfig wallConfig(numDisplays, windowSize,
                                  relativeBezelWidth,
                                  arrangement,doStereo);
 
            if (world.rank == 0) {
                cout << "#osp:dw: display wall config is" << endl;
                wallConfig.print();
                std::cout << "wall arrangement: " << std::endl;
                if(hasHeadNode){
                    for(int i = process_pernode; i < world.size; i++){
                        box2i region = wallConfig.regionOfRank(i - process_pernode);
                        std::cout << "region of rank #" << i << " : " << region << std::endl; 
                    }
                }else{
                    for (int i=0;i<world.size;i++) {
                        box2i region = wallConfig.regionOfRank(i);
                        std::cout << "region of rank #" << i << " : " << region << std::endl;
                    }
                }
            }
            world.barrier();

            MPI_Comm intraComm, interComm;
            mpicommon::Group displayGroup, dispatchGroup;
            if(hasHeadNode){
                if(process_pernode > 1){
                    //split into 3 groups
                    int color;
                    if(world.rank == 0){
                        color = 0;
                    }else if(world.rank > 0 && world.rank < process_pernode){
                        color = 1;
                    }else{
                        color = 2;
                    }
                    MPI_CALL(Comm_split(world.comm, color, world.rank, &intraComm));
                }else{
                    MPI_CALL(Comm_split(world.comm, 1 + (world.rank > 0), world.rank, &intraComm));
                }
        
                if (world.rank == 0){
                    dispatchGroup = mpicommon::Group(intraComm);
                    MPI_Intercomm_create(intraComm,0, world.comm, process_pernode, 1, &interComm); 
                    displayGroup = mpicommon::Group(interComm);
                }else if(world.rank >= process_pernode){
                    displayGroup = mpicommon::Group(intraComm);
                    MPI_Intercomm_create(intraComm,0, world.comm, 0, 1, &interComm); 
                    dispatchGroup = mpicommon::Group(interComm);
                }else{
                    std::cout << "do nothing" << std::endl;
                }
            }else{
                displayGroup = world.dup();
            }

            std::cout << std::endl;
            //std::cout << "Rank : " << world.rank << " Display Group size is : " << displayGroup.size <<
                                          //" Dispatch Group size is: " << dispatchGroup.size << std::endl;
            world.barrier();
            char hostName[1024] = {0};
            int portNum = desiredInfoPortNum;	
            gethostname(hostName, 1023);

            WallInfo wallInfo;
            wallInfo.numDisplays = wallConfig.numDisplays;
            wallInfo.pixelsPerDisplay = wallConfig.pixelsPerDisplay;
            wallInfo.stereo = wallConfig.stereo;
            wallInfo.relativeBezelWidth = wallConfig.relativeBezelWidth;
            wallInfo.arrangement = wallConfig.displayArrangement;
            wallInfo.totalPixelsInWall = wallConfig.totalPixels();

            //! Use TCP/IP Port
            if(hasHeadNode){
                // powerwall has to have 4 process per node
                if(world.rank == 0){
                    // ! Open info port and send wall info/ receive how many client we have
                    std::string host = hostName;
                    ServiceInfo serviceInfo;
                    serviceInfo.sendTo(wallInfo, host, clientNum);
                    GLFWindow *masterWindow = nullptr;
                    vec2i Position(1000, 0);//default is (0, 0)
                    sprintf(title, "Control Window on the Master Node");
                    masterWindow = new GLFWindow(windowSize, Position, title, doFullScreen, doStereo);
                    startWallServer(world, 
                                    displayGroup, 
                                    dispatchGroup, 
                                    wallConfig, 
                                    hasHeadNode, 
                                    displayNewFrame, 
                                    masterWindow, 
                                    portNum, 
                                    process_pernode,
                                    clientNum);
                    assert(masterWindow);
                    masterWindow ->run();
                // this rank is used to receive tiles and send tiles to other ranks 
                // no window here
                }else if(world.rank >= process_pernode){
                    GLFWindow *glfwWindow = nullptr;
                    if(arrangement == WallConfig::Arrangement_yx){
                        // set window position by rank number and arrangement
                        // for arrangement yx
                        // node
                        int n = (world.rank - process_pernode) % numDisplays.y;
                        int m = (world.rank - process_pernode) / numDisplays.y;
                        // windowPosition
                        windowPosition.y = (numDisplays.y - 1 - n) * windowSize.y;
                        //windowPosition.x = windowSize.x * m;
                    }else{
                        box2i region = wallConfig.regionOfRank(world.rank - process_pernode);
                        windowPosition = region.lower;
                    }
                    //std::cout << "Window positon = #" << world.rank << " x = " << windowPosition.x << " " << windowPosition.y << std::endl;

                    glfwWindow = new GLFWindow(windowSize, windowPosition, title, doFullScreen, doStereo);
                    startWallServer(world, 
                                    displayGroup, 
                                    dispatchGroup, 
                                    wallConfig, 
                                    hasHeadNode, 
                                    displayNewFrame, 
                                    glfwWindow, 
                                    portNum, 
                                    process_pernode,
                                    clientNum);
                
                    assert(glfwWindow);
                    glfwWindow ->run();
                }
            }else{
                if(arrangement == WallConfig::Arrangement_yx){
                    // set window position by rank number and arrangement
                    // for arrangement yx
                    // node
                    int n = world.rank % numDisplays.y;
                    // windowPosition
                    windowPosition.y = (numDisplays.y - 1 - n) * windowSize.y;
                }else{
                    box2i region = wallConfig.regionOfRank(world.rank);
                    windowPosition = region.lower;
                }
            //glfwWindow = new GLFWindow(windowSize, windowPosition, title, doFullScreen, doStereo);
            }
            world.barrier();
   	        return 0;
        }
    }// end of ospary::dw
}// end of ospray
