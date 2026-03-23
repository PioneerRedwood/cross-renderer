#pragma once

#include <cstdint>
#include "RenderPass.hpp"

namespace render {

/**
 * @brief Raw texture control, begin/end render pass, present
 * 
 */
class IRenderBackend {
public:
  virtual ResourceHandle CreateTexture(void* nativePointer, ResourceDescriptor&& desc) = 0;

  virtual void ImportTexture(ResourceHandle& resourceHandle, void* nativePointer, ResourceDescriptor&& desc) = 0;

  virtual void* ResolveTexture(ResourceHandle& resourceHandle) = 0;

  virtual void BeginRenderPass(RenderPassDescriptor rpd) = 0;

  virtual void EndRenderPass() = 0;

  virtual void Present(ResourceHandle resourceHandle) = 0;
};

/**
 * 기존 CPU 렌더러
 */
class CPURenderer : public IRenderBackend {
public:
  virtual ResourceHandle CreateTexture(void* nativePointer, ResourceDescriptor&& desc) override {
    // Create texture
  }

  virtual void ImportTexture(ResourceHandle& resourceHandle, void* nativePointer, ResourceDescriptor&& desc) override {
    // Import texture
  }

  virtual void* ResolveTexture(ResourceHandle& resourceHandle) override {
    // Resolve texture
  }

  virtual void BeginRenderPass(RenderPassDescriptor rpd) override {
    // Begin render pass
  }

  virtual void EndRenderPass() override {
    // End render pass
  }

  virtual void Present(ResourceHandle resourceHandle) override {
    // Present
  }
};

}