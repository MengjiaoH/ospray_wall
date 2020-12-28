// Copyright 2009-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "DisplayWall.h"
#include <vector>
#include "dw2_client.h"
#include "rkcommon/math/rkmath.h"

using namespace rkcommon;

namespace ospray {
namespace dw2 {

void DisplayWallOp::commit()
{
  host = getParam<std::string>("host");
  port = getParam<int>("port", -1);
  numPeers = getParam<int>("numPeers", 1);

  if (port == -1 || host.empty()) {
    throw std::runtime_error(
        "A 'host' and 'port' must be set for the display wall to connect to");
  }
}

std::unique_ptr<LiveImageOp> DisplayWallOp::attach(FrameBufferView &fbView)
{
  return rkcommon::make_unique<LiveDisplayWallOp>(fbView, host, port, numPeers);
}

std::string DisplayWallOp::toString() const
{
  return "dw2::DisplayWallOp";
}

LiveDisplayWallOp::LiveDisplayWallOp(FrameBufferView &fbView,
    const std::string &host,
    const int port,
    const int numPeers)
    : LiveTileOp(fbView)
{
  auto rc = dw2_connect(host.c_str(), port, numPeers);
  if (rc != DW2_OK) {
    throw std::runtime_error("Failed to connect to display wall host " + host);
  }
}

LiveDisplayWallOp::~LiveDisplayWallOp()
{
  dw2_disconnect();
}

void LiveDisplayWallOp::beginFrame()
{
  dw2_begin_frame();
}

void LiveDisplayWallOp::endFrame()
{
  dw2_end_frame();
}

float linearToSRGB(float x)
{
  if (x <= 0.0031308f) {
    return 12.92f * x;
  }
  return 1.055f * pow(x, 1.f / 2.4f) - 0.055f;
}

void LiveDisplayWallOp::process(Tile &t)
{
  // Convert the tile to sRGBA8 and send it to the display wall
  std::vector<uint32_t> rgba8Tile(TILE_SIZE * TILE_SIZE, 0);
  for (size_t i = 0; i < rgba8Tile.size(); ++i) {
    const uint8_t r = math::clamp(linearToSRGB(t.r[i]), 0.f, 255.f);
    const uint8_t g = math::clamp(linearToSRGB(t.g[i]), 0.f, 255.f);
    const uint8_t b = math::clamp(linearToSRGB(t.b[i]), 0.f, 255.f);
    const uint8_t a = math::clamp(t.a[i], 0.f, 255.f);

    rgba8Tile[i] = r | (g << 8) | (b << 16) | (a << 24);
  }
  dw2_send_rgba(t.region.lower.x,
      t.region.lower.y,
      TILE_SIZE,
      TILE_SIZE,
      TILE_SIZE,
      rgba8Tile.data());
}
} // namespace dw2
} // namespace ospray

extern "C" OSPError OSPRAY_DLLEXPORT ospray_module_init_denoiser(
    int16_t versionMajor, int16_t versionMinor, int16_t /*versionPatch*/)
{
  auto status = ospray::moduleVersionCheck(versionMajor, versionMinor);

  if (status == OSP_NO_ERROR)
    ospray::ImageOp::registerType<ospray::dw2::DisplayWallOp>("display_wall");

  return status;
}
