#pragma once

#include <vector>
#include <functional>

#include "RenderPass.hpp"
#include "RenderBackend.hpp"

namespace render {

class RenderPassBuilder {
public:
  using RenderPassFunc = std::function<void(RenderPassBuilder&)>;
  using RenderContextFunc = std::function<void(IRenderBackend&)>;

  void BeginFrame() {
    Reset();
  }

  void Reset() {
    m_RenderPasses.clear();
    m_RenderPassDescriptors.clear();
    m_ResourceHandles.clear();
  }

  bool Validate() const {
    // Check below: 
    // 1. 모든 attachment 핸들이 유효한지
    // 2. 래스터 패스 단계에서 적어도 color 나 depth 하나 이상 사용할 것
    // 3. 렌더 패스에 LoadActionClear로 설정되어있는데 clearColor 값이 설정되어있는지
    // 4. read-only 패스가 한번도 쓰이지 않은 리소스를 Load하려 하지 않는지
    return true;
  }

  void ImportTexture(ResourceHandle& resourceHandle) {

  }

  /**
   * @brief Create a Texture object
   * 
   * @return ResourceHandle
   */
  ResourceHandle CreateTexture(void* nativePointer, ResourceDescriptor&& desc) {
    
  }
  
  void AddRenderPass(const char* passName, RenderPassFlag passFlag,
                     RenderPassFunc&& passFunc,
                     RenderContextFunc&& contextFunc) {
    // Add render pass
  }

  void Submit() {
    if(!Validate()) {
      return;
    }

    // 등록된 패스를 순서대로 순회

    // pass descriptor로 context 준비

    // 저장해둔 execute lambda 호출
  }

private:
  std::vector<RenderPass> m_RenderPasses;
  std::vector<ResourceHandle> m_ResourceHandles;
  std::vector<RenderPassDescriptor> m_RenderPassDescriptors;
  IRenderBackend* m_RenderBackend; // Temporaily holding 
};

}  // namespace render