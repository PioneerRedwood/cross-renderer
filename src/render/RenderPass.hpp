#pragma once

#include "Color.hpp"

#include <cstdint>
#include <functional>
#include <vector>
#include <algorithm>

namespace render {

enum PixelFormat : uint8_t {
  PixelFormatUnknown = 0,
  PixelFormatBGRA8 = 1,
  PixelFormatRGBA8 = 2,
  PixelFormatDepth32Float = 3,
};

struct ResourceDescriptor {
  uint32_t width;
  uint32_t height;
  PixelFormat pixelFormat;
};

struct ResourceHandle {
  uint16_t index;
  bool IsValid() const {
    // TODO: Validate 
    return true;
  }
};

enum LoadAction : uint8_t {
  LoadActionNone = 0,
  LoadActionDontCare = 1,
  LoadActionLoad = 2,
  LoadActionClear = 3,
};

enum StoreAction : uint8_t {
  StoreActionNone = 0,
  StoreActionStore = 1,
  StoreActionDontCare = 2,
};

struct ColorAttachment {
  ResourceHandle resourceHandle;
  PixelFormat pixelFormat;
  LoadAction loadAction;
  StoreAction storeAction;
};

struct DepthAttachment {
  ResourceHandle resourceHandle;
  PixelFormat pixelFormat;
  LoadAction loadAction;
  StoreAction storeAction;
};

struct RenderPassDescriptor {
  std::vector<ColorAttachment> colorAttachments;
  Color clearColor;

  DepthAttachment depthAttachment;
  double clearDepth;
};

enum RenderPassFlag : uint8_t {
  RenderPassFlagNone,
  RenderPassFlagRaster,
};

enum RenderPassSetupResult : uint8_t {
  RenderPassSetupResultSuccess = 0,
  RenderPassSetupResultFail = 1,
};

class RenderPassBuilder;

class RenderPass {
public:
private:
  const char* m_Name;
  RenderPassFlag m_Flag;
  RenderPassDescriptor m_Desc;
  RenderPassSetupResult m_SetupResult;
  std::function<void(RenderPassBuilder&)> m_ExecuteLambda;
  std::vector<ResourceHandle> m_ReadResourceHandles;
  std::vector<ResourceHandle> m_WriteResourceHandles;
};

// Temporary struct for depth target descriptor

struct DepthTarget {
  int width;
  int height;
  std::vector<float> data;

  explicit DepthTarget(int width, int height) : width(width), height(height) {
    data.resize(width * height);
  }
  
  ~DepthTarget() {
    data.clear();
  }

  void Clear(float clearValue) {
    std::fill(data.begin(), data.end(), clearValue);
  }
};

}