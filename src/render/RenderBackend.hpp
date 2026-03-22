#pragma once

#include <cstdint>
#include "RenderPass.hpp"

namespace render {

/**
 * @brief Raw texture control, begin/end render pass, present
 * 
 */
class RenderBackend {
public:
  ResourceHandle CreateTexture(void* nativePointer, ResourceDescriptor&& desc) {

  }

  void ImportTexture(ResourceHandle& resourceHandle, void* nativePointer, ResourceDescriptor&& desc) {

  }

  void* ResolveTexture(ResourceHandle& resourceHandle) {

  }

  void BeginRenderPass(RenderPassDescriptor rpd) {

  }

  void EndRenderPass() {

  }

  void Present(ResourceHandle resourceHandle) {

  }

  

private:
  std::vector<ResourceHandle*> m_ResourceHandle;
};

}