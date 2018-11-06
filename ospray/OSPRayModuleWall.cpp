/* 
Copyright (c) 2016 Ingo Wald

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// ospray pixel
#include "ospray/fb/PixelOp.h"
#include "ospray/fb/FrameBuffer.h"
#include "ospray/common/Data.h"
#include "mpiCommon/MPICommon.h"
// displaywald client
#include "../render_clients/Client.h"
#include "../common/WallConfig.h"
#include "../common/helper.h"

#include <atomic>
#include <thread>
#include <mutex>

namespace ospray {
  namespace dw {
    
    std::mutex sendMutex;
    std::mutex state_mutex;

    struct WallPixelOp : public ospray::PixelOp 
    {
      struct Instance : public ospray::PixelOp::Instance 
      {
        Instance(FrameBuffer *fb, 
                 PixelOp::Instance *prev,
                 dw::Client *client)
          : client(client), framebuffer(fb)
        {
          fb ->pixelOp = this;
          fb ->frameID = 0; 
          //sendThread = std::thread([&](){
                  ////std::lock_guard<std::mutex> lock(sendMutex);
                  //client -> sendTile();
          //});
          
        }

       //~Instance(){
           ////std::lock_guard<std::mutex> lock(state_mutex);
           //client -> quit_state = true;
           //sendThread.join();
           ////mpicommon::world.barrier();
       //} 

        // /*! gets called every time the frame buffer got 'commit'ted */
        // virtual void  commitNotify() {}
        // /*! gets called once at the end of the frame */
        virtual void beginFrame()
        {
            //std::cout << "frame id = " << framebuffer ->frameID << std::endl;
        }
        virtual void endFrame() 
        { client->endFrame(); }
        
        unsigned int clampColorComponent(float c)
        {
          if (c<0.0f)
            c = 0.0f;
          if (c > 1.0f)
            c = 1.0f;
          return (unsigned int)(255.0f*c);
        }
        
        float simpleGammaCorrection(float c, float gamma)
        {
          float r = powf(c, 1.0f/gamma);
          return r;
        }
        
        unsigned int packColor(unsigned int r, unsigned int g, unsigned int b, unsigned int a=255)
        {
            return (r<<0) | (g<<8) | (b<<16) | (a<<24);
        }
      
        /*! called right after the tile got accumulated; i.e., the
          tile's RGBA values already contain the accu-buffer blended
          values (assuming an accubuffer exists), and this function
          defines how these pixels are being processed before written
          into the color buffer */
        virtual void postAccum(Tile &tile) 
        {
            
            //std::cout << " postAccum " << std::endl;
            //std::cout << client -> getWallConfig() -> numDisplays.x << " x " << client -> getWallConfig() -> numDisplays.y << std::endl;
            
          PlainTile plainTile(vec2i(TILE_SIZE));
          plainTile.pitch = TILE_SIZE;
          for (int i=0;i<TILE_SIZE*TILE_SIZE;i++) {
            float gamma = 2.2;
            unsigned int r = clampColorComponent(simpleGammaCorrection(tile.r[i], gamma));
            unsigned int g = clampColorComponent(simpleGammaCorrection(tile.g[i], gamma));
            unsigned int b = clampColorComponent(simpleGammaCorrection(tile.b[i], gamma));

            unsigned int rgba = packColor(r,g,b);// (b<<24)|(g<<16)|(r<<8);
            plainTile.pixel[i] = rgba;
          }
          plainTile.region = tile.region;
          bool stereo = client->getWallConfig()->doStereo();
          {
              //## Debug : write tiles to images
              //std::cout << "write tiles to images before client write tiles" << std::endl;
              //std::string filename = "before_client_write" + std::to_string(mpicommon::world.rank) + "_" +
                                                            //std::to_string(tile.region.lower.x) + "_" +
                                                            //std::to_string(tile.region.upper.x) + "_" +   
                                                            //std::to_string(tile.region.lower.y) + "_" +  
                                                            //std::to_string(tile.region.upper.y) + ".ppm";   
              //vec2i img_size; img_size.x = TILE_SIZE; img_size.y = TILE_SIZE;
              //writePPM(filename.c_str(), &img_size, plainTile.pixel);
              //Correct!
          }
          
          if (!stereo) {
            plainTile.eye = 0;
            plainTile.frameID = framebuffer ->frameID;
            client->writeTile(plainTile);
          } else {
            int trueScreenWidth = client->getWallConfig()->totalPixels().x;
            if (plainTile.region.upper.x <= trueScreenWidth) {
              // all on left eye
              plainTile.eye = 0;
              client->writeTile(plainTile);
            } else if (plainTile.region.lower.x >= trueScreenWidth) {
              // all on right eye - just shift coordinates
              plainTile.region.lower.x -= trueScreenWidth;
              plainTile.region.upper.x -= trueScreenWidth;
              plainTile.eye = 1;
              client->writeTile(plainTile);
            } else {
              // overlaps both sides - split it up
              const int original_lower_x = plainTile.region.lower.x;
              const int original_upper_x = plainTile.region.upper.x;
              // first, 'clip' the tile and write 'clipped' one to the left side
              plainTile.region.lower.x = original_lower_x - trueScreenWidth;
              plainTile.region.upper.x = trueScreenWidth;
              plainTile.eye = 0;
              client->writeTile(plainTile);

              // now, move right true to the left, clip on lower side, and shift pixels
              plainTile.region.lower.x = 0;
              plainTile.region.upper.x = original_upper_x - trueScreenWidth;
              // since pixels didn't start at 'trueWidth' but at 'lower.x', we have to shift
              int numPixelsInRegion = plainTile.region.size().y*plainTile.pitch;
              int shift = trueScreenWidth - original_lower_x;
              plainTile.eye = 1;
              for (int i=0;i<numPixelsInRegion;i++)
                plainTile.pixel[i] = plainTile.pixel[i+shift];
              // push tile to outbox 
              client->writeTile(plainTile);
            }    
          }
        }
 
        //! \brief common function to help printf-debugging 
        /*! Every derived class should overrride this! */
        virtual std::string toString() const;

        dw::Client *client;
        FrameBuffer *framebuffer;
        //std::thread sendThread;
      };
      
      //! \brief common function to help printf-debugging 
      /*! Every derived class should overrride this! */
      virtual std::string toString() const;
      
      /*! \brief commit the object's outstanding changes (such as changed
       *         parameters etc) */
      virtual void commit()
      {
        int portNum = getParam1i("portNum", 0);
        //int remote_mode = getParam1i("remote", 0);
        std::string hostName = getParamString("hostName","");
        Ref<Data> wallInfoData = getParamData("wallInfo", nullptr);
        const WallInfo *wallInfo = (const WallInfo *)wallInfoData ->data;
        std::cout << "#osp:dw: trying to establish connection to display wall service at host " << hostName << " port " << portNum << std::endl;
        //PING;
        //PRINT(mpicommon::worker.size);
        client = new dw::Client(hostName, portNum, mpicommon::worker, wallInfo);
      }

      //! \brief create an instance of this pixel op
      virtual ospray::PixelOp::Instance *createInstance(FrameBuffer *fb, 
                                                        PixelOp::Instance *prev) override
      {
        return new Instance(fb, prev, client);
      }
      dw::Client *client; 
    };
    
    //! \brief common function to help printf-debugging 
    std::string WallPixelOp::toString() const 
    { return "ospray::dw::DisplayWaldPixelOp (displayWald module)"; }

    //! \brief common function to help printf-debugging 
    std::string WallPixelOp::Instance::toString() const 
    { return "ospray::dw::DisplayWaldPixelOp::Instance (displayWald module)"; }

    extern "C" void ospray_init_module_wall()
    {
         printf("loading the 'Wall' module...\n");
    }

    OSP_REGISTER_PIXEL_OP(WallPixelOp, wall);

  } // ::ospray::dw
} // ::ospray
