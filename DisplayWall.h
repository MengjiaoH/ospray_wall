// Copyright 2009-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "fb/ImageOp.h"
#include "ospray_module_wall_export.h"

using namespace rkcommon;

namespace ospray {
namespace dw2 {

struct OSPRAY_MODULE_WALL_EXPORT DisplayWallOp : public TileOp
{
  void commit() override;

  std::unique_ptr<LiveImageOp> attach(FrameBufferView &fbView) override;

  std::string toString() const override;

  std::string host;
  int port;
  int numPeers;
};

struct OSPRAY_MODULE_WALL_EXPORT LiveDisplayWallOp : public LiveTileOp
{
  LiveDisplayWallOp(FrameBufferView &fbView,
      const std::string &host,
      const int port,
      const int numPeers);

  ~LiveDisplayWallOp() override;

  void beginFrame() override;

  void endFrame() override;

  void process(Tile &t) override;
};
} // namespace dw

} // namespace ospray

