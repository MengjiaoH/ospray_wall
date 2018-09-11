// ospray
#include "ospray/fb/PixelOp.h"
#include "ospray/fb/FrameBuffer.h"
#include "mpiCommon/MPICommon.h"

// display wald client
#include "../../client/Client.h"

namespace ospray{
    namespace dw{
        struct DisplayWaldPixelOp : public ospray::PixelOp{
            struct Instance : public ospray::PixelOp::Instance{
                Instance(FrameBuffer *fb,
                         PixelOp::Instance *prev,
                         dw::Client *client):client(client){
                fb -> pixelOp = this;
                }

                dw::Client *client;


            
            
            
            
            };// struct instance

        
        dw::Client *client;
        
        
        
        
        }; // struct DisplayWaldPixelOp

    }
}
