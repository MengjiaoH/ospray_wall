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

#include "CompressedTile.h"
#include "helper.h"
#include <atomic>
#include <vector>

#if TURBO_JPEG
# include "turbojpeg.h"
//# define JPEG_QUALITY 100
 #define JPEG_QUALITY 75
#endif

#if SNAPPY
#include <snappy.h>
#endif


static double lastTime;
static double thisTime;

namespace ospray {
  namespace dw {

#if TURBO_JPEG
    void *CompressedTile::createCompressor() { return (void *)tjInitCompress(); }
    void *CompressedTile::createDecompressor() { return (void *)tjInitDecompress(); }
    void CompressedTile::freeCompressor(void *compressor) { tjDestroy((tjhandle)compressor); }
    void CompressedTile::freeDecompressor(void *decompressor) { tjDestroy((tjhandle)decompressor); }
#else
    void *CompressedTile::createCompressor() { return NULL; }
    void *CompressedTile::createDecompressor() { return NULL; }
    void CompressedTile::freeCompressor(void *) {}
    void CompressedTile::freeDecompressor(void *) {}
#endif

    CompressedTile::CompressedTile() 
      : fromRank(-1), 
        numBytes(-1), 
        data(NULL) 
    {}

    CompressedTile::~CompressedTile() 
    { 
      if (data) delete[] data; 
    }

    void CompressedTile::encode(void *compressor, const PlainTile &tile)
    {
      assert(tile.pixel);
      const vec2i begin = tile.region.lower;
      const vec2i end   = tile.region.upper;

      int numPixels = (end-begin).product();
      this->numBytes = sizeof(CompressedTileHeader) + numPixels*sizeof(int);
      this->data = new unsigned char[this->numBytes];
      assert(this->data != NULL);
      CompressedTileHeader *header = (CompressedTileHeader *)this->data;
      header->region.lower = begin;
      header->region.upper = end;
      header->eye          = tile.eye;

#if TURBO_JPEG 
      // save data into jepgBuffer after compressed by Turbojpeg      
      unsigned char *jpegBuffer = header -> payload;
      //unsigned char *jpegBuffer = new unsigned char[numPixels * sizeof(int)];
      size_t jpegSize = numPixels * sizeof(int);
      int rc = tjCompress2((tjhandle)compressor, (unsigned char *)tile.pixel,
                           tile.size().x,tile.pitch*sizeof(int),tile.size().y,
                           TJPF_RGBX, 
                           &jpegBuffer, 
                           &jpegSize,TJSAMP_420,JPEG_QUALITY,0); 
      this->numBytes = jpegSize + sizeof(*header);
      // PING;

       //compress again
      //size_t out_length; 
      //unsigned char *compressed = header -> payload;
      //compressed = new unsigned char(snappy::MaxCompressedLength(jpegSize));
      //snappy::RawCompress((char *)jpegBuffer, jpegSize, (char*)compressed, &out_length);
      //this->numBytes = out_length + sizeof(*header);     

      //printf("compress %i: %li->%li bytes\n",rc,jpegSize, out_length);

      //delete[] jpegBuffer;

#else
       uint32_t *out = (uint32_t *)header->payload;
       const uint32_t *in = (const uint32_t *)tile.pixel;
      // // {
      // // //## Debug : write tiles to images
      // //   std::cout << "compressedTile after encode" << std::endl;
      // //   std::string filename = "encode_compressedTile" + std::to_string(mpicommon::world.rank) + "_" +
      // //                                                   std::to_string(tile.region.lower.x) + "_" +
      // //                                                   std::to_string(tile.region.upper.x) + "_" +   
      // //                                                   std::to_string(tile.region.lower.y) + "_" +  
      // //                                                   std::to_string(tile.region.upper.y) + ".ppm";   
      // //   vec2i img_size; img_size.x = tile.size().x; img_size.y = tile.size().y;
      // //   writePPM(filename.c_str(), &img_size, in);
      // // Correct !
      // // }
       for (uint32_t iy = begin.y;iy < end.y;iy++) {
         for (uint32_t ix = begin.x;ix < end.x;ix++) {
           //out[iy * tile.size().x + ix] = *(in + iy * tile.size().x + ix);
           *out++ = *in++;
         }
         in += (tile.pitch -(end.x-begin.x));
       }
#endif
    }
    
    void CompressedTile::decode(void *decompressor, PlainTile &tile)
    {
      //std::cout << "debug2" << std::endl;
      const CompressedTileHeader *header = (const CompressedTileHeader *)data;
      tile.region = header->region;
      tile.eye = header->eye;
      vec2i size = tile.region.size();
      assert(tile.pixel != NULL);

#if TURBO_JPEG      
      //std::cout << "receive bytes" <<  this -> numBytes - sizeof(*header) << std::endl;    
      
      //size_t in_length = this ->numBytes - sizeof(*header);
      //char * uncompressed = new char[this -> interBytes];
      //if (snappy::RawUncompress((char*)header->payload, in_length, uncompressed)) {;
	//std::cout << "snappy uncompressed" << std::endl;    
      //} else {
	//std::cout << "snappy uncompressed FAILED !!!!!!!!!!!!!!! " << std::endl;    
      //}

       size_t jpegSize = this->numBytes-sizeof(*header);
       int rc = tjDecompress2((tjhandle)decompressor, (unsigned char *)header->payload,
                              this->numBytes-sizeof(*header),
                              (unsigned char*)tile.pixel,
                              size.x,tile.pitch*sizeof(int),size.y,
                              TJPF_RGBX, 0);

      //size_t jpegSize = this->interBytes;
      //int rc = tjDecompress2((tjhandle)decompressor, (unsigned char *)uncompressed,
				 //jpegSize,
				 //(unsigned char*)tile.pixel,
				 //size.x,tile.pitch*sizeof(int),size.y,
				 //TJPF_RGBX, 0);

      //std::string filename = "encode_compressedTile" + std::to_string(mpicommon::world.rank) + "_" +
	//std::to_string(tile.region.lower.x) + "_" +
	//std::to_string(tile.region.upper.x) + "_" +   
	//std::to_string(tile.region.lower.y) + "_" +  
	//std::to_string(tile.region.upper.y) + ".ppm";   
      //vec2i img_size; img_size.x = tile.size().x; img_size.y = tile.size().y;
      //writePPM(filename.c_str(), &img_size, tile.pixel);

#else
      uint32_t *out = tile.pixel;
      uint32_t *in = (uint32_t *)(data+sizeof(CompressedTileHeader));
      for (int iy=0;iy<size.y;iy++)
        for (int ix=0;ix<size.x;ix++) {
          *out++ = *in++;
        }
#endif
    }

    /*! get region that this tile corresponds to */
    box2i CompressedTile::getRegion() const
    {
      const CompressedTileHeader *header = (const CompressedTileHeader *)data;
      assert(header);
      return header->region;
    }
    
    /*! send the tile to the given rank in the given group */
    void CompressedTile::sendTo(const mpicommon::Group &group, const int rank) const
    {
      //std::cout << "debug0" << std::endl;
      static std::atomic<int> tileID;
      int myTileID = tileID++;
      // double lastTime = getSysTime();
      MPI_CALL(Send(data,numBytes,MPI_BYTE,rank,myTileID,group.comm));
      // double thisTime = getSysTime();
      //std::cout << "send time : " << thisTime - lastTime << std::endl;
      // std::cout << "debug1" << std::endl;
    }

    /*! receive one tile from the outside communicator */
    void CompressedTile::receiveOne(const mpicommon::Group &outside)
    {
      //printf("receiveone fom outside %i/%i\n",outside.rank,outside.size);

      MPI_Status status;
      MPI_CALL(Probe(MPI_ANY_SOURCE,MPI_ANY_TAG,outside.comm,&status));
      fromRank = status.MPI_SOURCE;
      MPI_CALL(Get_count(&status,MPI_BYTE,&numBytes));        
       //printf("incoming from %i, %i bytes\n",status.MPI_SOURCE,numBytes);
      data = new unsigned char[numBytes];
      MPI_CALL(Recv(data,numBytes,MPI_BYTE,status.MPI_SOURCE,status.MPI_TAG,
                    outside.comm,&status));

    //  std::cout << "time of receiving a tile = " << lastTime << " " << thisTime << " " << thisTime - lastTime << std::endl;
    }
    void CompressedTile::receiveOneRemote(unsigned char* message, int Size){
        // get rank num from msg
        //unsigned char *recv_data = (unsigned char*)message.data(); 
        memcpy(&toRank, message, sizeof(int));
        static std::atomic<int> tileID;
        std::cout << "tile ID = " << tileID << std::flush;
        std::cout << " toRank = " << toRank << std::flush;
        std::cout << "rece data size = " << Size << std::endl;
        numBytes = Size;
        //encoded.numBytes = tilemsg.size() - sizeof(int);
        //std::cout << " numBytes = " << encoded.numBytes << std::endl;
        //encoded.data = new unsigned char[encoded.numBytes]; 
        //data = new unsigned char[Size];
        data = &message[4];
    }

  } // ::ospray::dw
} // ::ospray
