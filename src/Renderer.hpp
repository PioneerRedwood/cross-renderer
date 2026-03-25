#pragma once

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Mesh.hpp"
#include "MeshLoader.hpp"
#include "Model.hpp"
#include "Shader.hpp"
#include "TextureLoader.hpp"
#include "WorldCamera.hpp"
#include "render/RenderPass.hpp"

/**
 * Description:
 * Renderer: Software (Metal or Vulkan will be added in the future)
 * Framebuffer Format: Apple: BGRA8888, Windows: RGBA8888; Fallback to ARGB8888
 * if unsupported Window Size: 800x600 Depth Target Size: 800x600 (Same as
 * window size for simplicity) Resource Directory: ./resources (Absolute path
 * will be defined in CMakeLists.txt) In Use Meshes: bunny.obj (Just for
 * testing, will be replaced with more complex models in the future)
 */
class Renderer {
 public:
  Renderer(SDL_Window* window, int width, int height)
      : m_Width(width),
        m_Height(height),
        m_FramebufferFormatEnum(0),
        m_FramebufferFormat(nullptr),
        m_Renderer(nullptr),
        m_MainTexture(nullptr),
        m_BunnyModel(nullptr),
        m_PlaneModel(nullptr),
        m_CubeModel(nullptr),
        m_RotateRadian(0.0f),
        m_ZBuffer(m_Width, m_Height),
        m_ShadowDepth(256, 256) {
    m_Framebuffer = new uint32_t[m_Width * m_Height];

    m_Renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (m_Renderer == nullptr) {
      LogF("SDL_CreateRenderer failed: %s", SDL_GetError());
      SDL_assert(false);
      return;
    }

    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(m_Renderer, &info) == 0) {
      LogF("Renderer backend: %s", info.name);
      if (info.num_texture_formats > 0) {
        m_FramebufferFormatEnum = info.texture_formats[0];
      }
      for (uint32_t i = 0; i < info.num_texture_formats; ++i) {
        LogF("Supported[%u]: %s", i,
             SDL_GetPixelFormatName(info.texture_formats[i]));
      }
    }

    if (SDL_ISPIXELFORMAT_FOURCC(m_FramebufferFormatEnum) ||
        SDL_BYTESPERPIXEL(m_FramebufferFormatEnum) !=
            static_cast<int>(sizeof(uint32_t))) {
      LogF("Unsupported primary format (%s). Fallback to ARGB8888.",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum));
      m_FramebufferFormatEnum = SDL_PIXELFORMAT_ARGB8888;
    }

    m_FramebufferFormat = SDL_AllocFormat(m_FramebufferFormatEnum);
    if (m_FramebufferFormat == nullptr) {
      LogF("SDL_AllocFormat failed for %s: %s",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum), SDL_GetError());
      m_FramebufferFormatEnum = SDL_PIXELFORMAT_ARGB8888;
      m_FramebufferFormat = SDL_AllocFormat(m_FramebufferFormatEnum);
    }
    if (m_FramebufferFormat != nullptr) {
      LogF("Selected framebuffer format: %s (bpp=%u)",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum),
           m_FramebufferFormat->BitsPerPixel);
    }

    m_MainTexture =
        SDL_CreateTexture(m_Renderer, m_FramebufferFormatEnum,
                          SDL_TEXTUREACCESS_STREAMING, m_Width, m_Height);
    if (m_MainTexture == nullptr) {
      LogF("SDL_CreateTexture failed for %s: %s",
           SDL_GetPixelFormatName(m_FramebufferFormatEnum), SDL_GetError());
      SDL_assert(false);
      return;
    }

    m_TextureLoader = new TextureLoader(*m_Renderer);
    m_MeshLoader = new MeshLoader(*m_Renderer);

    m_Camera.aspect = (float)m_Width / m_Height;
    m_Camera.fov = 45.0f;

    SetupMatrices();

    LoadModels();
  }

  ~Renderer() {
    delete[] m_Framebuffer;

    delete m_BunnyModel->mesh;
    delete m_BunnyModel;

    delete m_PlaneModel->mesh;
    delete m_PlaneModel;

    delete m_CubeModel->mesh;
    delete m_CubeModel;

    delete m_TextureLoader;
    delete m_MeshLoader;

    if (m_FramebufferFormat != nullptr) {
      SDL_FreeFormat(m_FramebufferFormat);
      m_FramebufferFormat = nullptr;
    }
    SDL_DestroyTexture(m_MainTexture);
    SDL_DestroyRenderer(m_Renderer);
  }

  void Render(double delta) {
    UpdateScene(delta);

    ClearBuffers();
    m_HasLoggedShadowTriThisFrame = false;

    RenderShadowMapScene();

    if (m_ShowShadowDepthOnly) {
      VisualizeDepthTarget(m_ShadowDepth);
    } else {
      RenderMainScene(m_BlinnPhongShader, m_BlinnPhongShaderUniforms);
    }

    SDL_UpdateTexture(m_MainTexture, nullptr, m_Framebuffer,
                      m_Width * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderCopy(m_Renderer, m_MainTexture, nullptr, nullptr);
    SDL_RenderPresent(m_Renderer);

    SDL_Delay(32);
  }

  void HandleKeyInput(const SDL_Event& event) {
    switch (event.key.keysym.sym) {
      case SDLK_UP: {
        m_Camera.fov++;
        m_ProjectionMatrix = Matrix4x4::identity;
        math::SetupPerspectiveProjectionMatrix(
            m_ProjectionMatrix, m_Camera.fov, m_Camera.aspect, m_ZNear, m_ZFar);
        break;
      }
      case SDLK_DOWN: {
        m_Camera.fov--;
        m_ProjectionMatrix = Matrix4x4::identity;
        math::SetupPerspectiveProjectionMatrix(
            m_ProjectionMatrix, m_Camera.fov, m_Camera.aspect, m_ZNear, m_ZFar);
        break;
      }
      case SDLK_RIGHT: {
        m_Camera.eye.x += 0.1f;
        m_CameraMatrix = Matrix4x4::identity;
        math::SetupCameraMatrix(m_CameraMatrix, m_Camera.eye, m_Camera.at,
                                m_Camera.up);
        break;
      }
      case SDLK_LEFT: {
        m_Camera.eye.x -= 0.1f;
        m_CameraMatrix = Matrix4x4::identity;
        math::SetupCameraMatrix(m_CameraMatrix, m_Camera.eye, m_Camera.at,
                                m_Camera.up);
        break;
      }
      case SDLK_r: {
        // Reset camera
        m_Camera.eye = {2.2f, 1.2f, -6.5f};
        m_Camera.at = {0.0f, 0.0f, 0.0f};
        m_Camera.up = {0.0f, 1.0f, 0.0f};
        m_Camera.fov = 45.0f;
        SetupMatrices();
        break;
      }
      case SDLK_d: {
        const bool cmdPressed = (event.key.keysym.mod & KMOD_GUI) != 0;
        const bool shiftPressed = (event.key.keysym.mod & KMOD_SHIFT) != 0;
        if (cmdPressed && shiftPressed) {
          LogF("[DEBUG] Turn on debug mode rendering shadow depth only ");
          m_ShowShadowDepthOnly = !m_ShowShadowDepthOnly;
        }
      }
      default: {
        break;
      }
    }
  }

 private:
  void UpdateScene(double delta) {
    m_BlinnPhongShaderUniforms.model = Matrix4x4::identity;

    m_RotateRadian += 0.45f;
    if (m_RotateRadian > 360.0f) {
      m_RotateRadian -= 360.0f;
    }
    m_BunnyModel->modelMatrix.Rotate(0.0f, m_RotateRadian, 0.0f);
  }

  void ClearBuffers() {
    std::fill(m_Framebuffer, m_Framebuffer + m_Width * m_Height, 0);
    m_ZBuffer.Clear(1.0f);
    m_ShadowDepth.Clear(1.0f);
  }

  uint32_t PackColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) const {
    if (m_FramebufferFormat == nullptr) {
      return (static_cast<uint32_t>(a) << 24) |
             (static_cast<uint32_t>(r) << 16) |
             (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    }
    return SDL_MapRGBA(m_FramebufferFormat, r, g, b, a);
  }

  bool ProjectClipPointToScreen(const Vector4& clipPosition,
                                const Matrix4x4& viewportMat,
                                Vector3& out) const {
    Vector4 clip = clipPosition;
    if (clip.w <= 1e-6f) {
      return false;
    }

    clip.PerspectiveDivide();
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
        !std::isfinite(clip.z)) {
      return false;
    }

    //    LogF("[ShadowDebug] clip=(%.4f, %.4f, %.4f, %.4f)", clip.x, clip.y,
    //    clip.z, clip.w);

    if (clip.z < 0.0f || clip.z > 1.0f) {
      return false;
    }

    clip = viewportMat * clip;
    out = {clip.x, clip.y, clip.z};
    return true;
  }

  void SetupMatrices() {
    math::SetupCameraMatrix(m_CameraMatrix, m_Camera.eye, m_Camera.at,
                            m_Camera.up);
    math::SetupPerspectiveProjectionMatrix(m_ProjectionMatrix, m_Camera.fov,
                                           m_Camera.aspect, m_ZNear, m_ZFar);
    math::SetupViewportMatrix(m_ViewportMatrix, 0, 0, m_Width, m_Height,
                              m_ZNear, m_ZFar);

    SetupShadowMatrices();
    math::SetupViewportMatrix(m_ShadowViewportMatrix, 0, 0, m_ShadowDepth.width,
                              m_ShadowDepth.height, m_ZNear, m_ZFar);
  }

  void SetupShadowMatrices() {
    Vector3 lightUp{0.0f, 1.0f, 0.0f};
    const Vector3 lightForward = (m_LightTarget - m_LightPosition).Normalize();
    if (std::abs(math::DotProduct(lightForward, lightUp)) > 0.99f) {
      lightUp = {0.0f, 0.0f, 1.0f};
    }

    math::SetupCameraMatrix(m_LightViewMatrix, m_LightPosition, m_LightTarget,
                            lightUp);
    math::SetupOrthographicProjectionMatrix(
        m_LightProjectionMatrix, m_ShadowOrthoLeft, m_ShadowOrthoRight,
        m_ShadowOrthoBottom, m_ShadowOrthoTop, m_ShadowNear, m_ShadowFar);

    // Lighting uses a fixed direction from the scene toward the light.
    m_LightDir = (m_LightPosition - m_LightTarget).Normalize();
  }

  void DrawTri(const Vector3& v0, const Vector3& v1, const Vector3& v2,
               uint32_t color, bool cullBackface = true) {
    const float minX = std::min({v0.x, v1.x, v2.x});
    const float maxX = std::max({v0.x, v1.x, v2.x});
    const float minY = std::min({v0.y, v1.y, v2.y});
    const float maxY = std::max({v0.y, v1.y, v2.y});

    const int x0 = std::max(0, (int)std::floor(minX));
    const int y0 = std::max(0, (int)std::floor(minY));
    const int x1 = std::min(m_Width - 1, (int)std::ceil(maxX));
    const int y1 = std::min(m_Height - 1, (int)std::ceil(maxY));

    const Vector2 p0{v0.x, v0.y};
    const Vector2 p1{v1.x, v1.y};
    const Vector2 p2{v2.x, v2.y};
    const float area = math::EdgeFunction(p0, p1, v2.x, v2.y);
    if (std::abs(area) < 1e-6f) {
      return;
    }

    // Back-face culling (treat CCW as front-facing)
    if (cullBackface && area < 0.0f) {
      return;
    }

    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const float px = (float)x + 0.5f;
        const float py = (float)y + 0.5f;

        const float w0 = math::EdgeFunction(p1, p2, px, py);
        const float w1 = math::EdgeFunction(p2, p0, px, py);
        const float w2 = math::EdgeFunction(p0, p1, px, py);

        const bool insideCCW = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
        const bool insideCW = (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);

        const bool checkInside = cullBackface ? insideCCW
                                              : ((area > 0.0f && insideCCW) ||
                                                 (area < 0.0f && insideCW));

        if (checkInside) {
          // Zbuffer test
          const float invArea = 1.0f / area;
          const float b0 = w0 * invArea;
          const float b1 = w1 * invArea;
          const float b2 = w2 * invArea;

          const float z = b0 * v0.z + b1 * v1.z + b2 * v2.z;
          if (z < 0.0f || z > 1.0f) continue;

          const int idx = x + y * m_Width;
          if (z < m_ZBuffer.data[idx]) {
            m_ZBuffer.data[idx] = z;
            m_Framebuffer[idx] = color;
          }
        }
      }
    }
  }

  void DrawShaderTri(const VertexOutput& out0, const VertexOutput& out1,
                     const VertexOutput& out2, const ShaderUniforms& uniforms,
                     const Shader& shader, bool cullBackface = true) {
    Vector3 screen0{};
    Vector3 screen1{};
    Vector3 screen2{};
    if (!ProjectClipPointToScreen(out0.clipPosition, m_ViewportMatrix,
                                  screen0) ||
        !ProjectClipPointToScreen(out1.clipPosition, m_ViewportMatrix,
                                  screen1) ||
        !ProjectClipPointToScreen(out2.clipPosition, m_ViewportMatrix,
                                  screen2)) {
      return;
    }

    const float minX = std::min({screen0.x, screen1.x, screen2.x});
    const float maxX = std::max({screen0.x, screen1.x, screen2.x});
    const float minY = std::min({screen0.y, screen1.y, screen2.y});
    const float maxY = std::max({screen0.y, screen1.y, screen2.y});

    const int x0 = std::max(0, static_cast<int>(std::floor(minX)));
    const int y0 = std::max(0, static_cast<int>(std::floor(minY)));
    const int x1 = std::min(m_Width - 1, static_cast<int>(std::ceil(maxX)));
    const int y1 = std::min(m_Height - 1, static_cast<int>(std::ceil(maxY)));

    const Vector2 p0{screen0.x, screen0.y};
    const Vector2 p1{screen1.x, screen1.y};
    const Vector2 p2{screen2.x, screen2.y};
    const float area = math::EdgeFunction(p0, p1, p2.x, p2.y);
    if (std::abs(area) < 1e-6f) {
      return;
    }

    if (cullBackface && area < 0.0f) {
      return;
    }

    const float invArea = 1.0f / area;

    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const float px = static_cast<float>(x) + 0.5f;
        const float py = static_cast<float>(y) + 0.5f;

        const float w0 = math::EdgeFunction(p1, p2, px, py);
        const float w1 = math::EdgeFunction(p2, p0, px, py);
        const float w2 = math::EdgeFunction(p0, p1, px, py);

        const bool insideCCW = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
        const bool insideCW = (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
        const bool inside = cullBackface ? insideCCW
                                         : ((area > 0.0f && insideCCW) ||
                                            (area < 0.0f && insideCW));
        if (!inside) {
          continue;
        }

        const float b0 = w0 * invArea;
        const float b1 = w1 * invArea;
        const float b2 = w2 * invArea;
        const float z = b0 * screen0.z + b1 * screen1.z + b2 * screen2.z;
        if (z < 0.0f || z > 1.0f) {
          continue;
        }

        const int idx = x + y * m_Width;
        if (z >= m_ZBuffer.data[idx]) {
          continue;
        }

        const float pw0 = b0 * out0.invW;
        const float pw1 = b1 * out1.invW;
        const float pw2 = b2 * out2.invW;
        const float invWeightSum = pw0 + pw1 + pw2;
        if (std::abs(invWeightSum) <= 1e-8f) {
          continue;
        }

        const float invSum = 1.0f / invWeightSum;
        PixelInput pixelInput{};
        pixelInput.worldPosition =
            (out0.worldPosition * pw0 + out1.worldPosition * pw1 +
             out2.worldPosition * pw2) *
            invSum;
        pixelInput.normal =
            (out0.normal * pw0 + out1.normal * pw1 + out2.normal * pw2) *
            invSum;
        pixelInput.uv =
            (out0.uv * pw0 + out1.uv * pw1 + out2.uv * pw2) * invSum;

        const Color color = shader.PixelShader(pixelInput, uniforms);
        m_ZBuffer.data[idx] = z;
        m_Framebuffer[idx] = PackColor(color.r, color.g, color.b, color.a);
      }
    }
  }

  void RasterizeDepthTri(const Vector3& v0, const Vector3& v1,
                         const Vector3& v2, const Matrix4x4& modelMat,
                         render::DepthTarget& depthTarget) {
    // 월드 좌표계로 변환
    Vector4 v4_0 = modelMat * Vector4(v0.x, v0.y, v0.z, 1.0f);
    Vector4 v4_1 = modelMat * Vector4(v1.x, v1.y, v1.z, 1.0f);
    Vector4 v4_2 = modelMat * Vector4(v2.x, v2.y, v2.z, 1.0f);

    // 클립 좌표계 변환 (카메라 + 투영)
    Vector4 cv4_0 = m_LightProjectionMatrix * (m_LightViewMatrix * v4_0);
    Vector4 cv4_1 = m_LightProjectionMatrix * (m_LightViewMatrix * v4_1);
    Vector4 cv4_2 = m_LightProjectionMatrix * (m_LightViewMatrix * v4_2);

    // 스크린 좌표계로 변환
    Vector3 sv3_0;
    Vector3 sv3_1;
    Vector3 sv3_2;
    if (!ProjectClipPointToScreen(cv4_0, m_ShadowViewportMatrix, sv3_0) ||
        !ProjectClipPointToScreen(cv4_1, m_ShadowViewportMatrix, sv3_1) ||
        !ProjectClipPointToScreen(cv4_2, m_ShadowViewportMatrix, sv3_2)) {
      return;
    }

    if (m_ShowShadowDepthOnly && !m_HasLoggedShadowTriThisFrame) {
      //      LogF("[ShadowDebug] tri screen v0=(%.2f, %.2f, %.4f) v1=(%.2f,
      //      %.2f, %.4f) v2=(%.2f, %.2f, %.4f)",
      //           sv3_0.x, sv3_0.y, sv3_0.z,
      //           sv3_1.x, sv3_1.y, sv3_1.z,
      //           sv3_2.x, sv3_2.y, sv3_2.z);
      m_HasLoggedShadowTriThisFrame = true;
    }

    // 깊이 값 기록
    const float minX = std::min({sv3_0.x, sv3_1.x, sv3_2.x});
    const float maxX = std::max({sv3_0.x, sv3_1.x, sv3_2.x});
    const float minY = std::min({sv3_0.y, sv3_1.y, sv3_2.y});
    const float maxY = std::max({sv3_0.y, sv3_1.y, sv3_2.y});

    const int x0 = std::max(0, static_cast<int>(std::floor(minX)));
    const int y0 = std::max(0, static_cast<int>(std::floor(minY)));
    const int x1 =
        std::min(depthTarget.width - 1, static_cast<int>(std::ceil(maxX)));
    const int y1 =
        std::min(depthTarget.height - 1, static_cast<int>(std::ceil(maxY)));

    const Vector2 p0{sv3_0.x, sv3_0.y};
    const Vector2 p1{sv3_1.x, sv3_1.y};
    const Vector2 p2{sv3_2.x, sv3_2.y};
    const float area = math::EdgeFunction(p0, p1, p2.x, p2.y);
    if (std::abs(area) < 1e-6f) {
      return;
    }

    if (area < 0.0f) {
      return;
    }

    const float invArea = 1.0f / area;

    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const float px = static_cast<float>(x) + 0.5f;
        const float py = static_cast<float>(y) + 0.5f;

        const float w0 = math::EdgeFunction(p1, p2, px, py);
        const float w1 = math::EdgeFunction(p2, p0, px, py);
        const float w2 = math::EdgeFunction(p0, p1, px, py);

        const bool insideCCW = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
        if (!insideCCW) {
          continue;
        }

        const float b0 = w0 * invArea;
        const float b1 = w1 * invArea;
        const float b2 = w2 * invArea;
        const float z = b0 * sv3_0.z + b1 * sv3_1.z + b2 * sv3_2.z;
        if (z < 0.0f || z > 1.0f) {
          continue;
        }

        const int idx = x + y * depthTarget.width;
        if (z < depthTarget.data[idx]) {
          depthTarget.data[idx] = z;
        }
      }
    }
  }

  void VisualizeDepthTarget(const render::DepthTarget& depthTarget) {
    float minDepth = 1.0f;
    float maxDepth = 0.0f;
    bool hasWrittenDepth = false;

    for (float depthValue : depthTarget.data) {
      if (depthValue >= 1.0f) {
        continue;
      }
      minDepth = std::min(minDepth, depthValue);
      maxDepth = std::max(maxDepth, depthValue);
      hasWrittenDepth = true;
    }

    for (int y = 0; y < m_Height; ++y) {
      const int sy =
          std::min(depthTarget.height - 1, y * depthTarget.height / m_Height);

      for (int x = 0; x < m_Width; ++x) {
        const int sx =
            std::min(depthTarget.width - 1, x * depthTarget.width / m_Width);
        const int depthIdx = sx + sy * depthTarget.width;
        const int framebufferIdx = x + y * m_Width;

        const float depthValue = depthTarget.data[depthIdx];
        uint8_t intensity = 0;
        if (hasWrittenDepth && depthValue < 1.0f) {
          float normalizedDepth = 0.0f;
          if (maxDepth > minDepth + 1e-6f) {
            normalizedDepth = (depthValue - minDepth) / (maxDepth - minDepth);
          }
          intensity = static_cast<uint8_t>((1.0f - normalizedDepth) * 255.0f);
        }

        m_Framebuffer[framebufferIdx] =
            PackColor(intensity, intensity, intensity, 255);
      }
    }
  }

  void LogDepthTargetStats(const render::DepthTarget& depthTarget) const {
    if (depthTarget.data.empty()) {
      LogF("[ShadowDebug] depth target is empty");
      return;
    }

    float minDepth = 1.0f;
    float maxDepth = 0.0f;
    int writtenPixels = 0;

    for (float depthValue : depthTarget.data) {
      if (depthValue < 1.0f) {
        minDepth = std::min(minDepth, depthValue);
        maxDepth = std::max(maxDepth, depthValue);
        ++writtenPixels;
      }
    }

    if (writtenPixels == 0) {
      LogF("[ShadowDebug] written=0 size=%dx%d", depthTarget.width,
           depthTarget.height);
      return;
    }

    LogF("[ShadowDebug] written=%d size=%dx%d min=%.6f max=%.6f", writtenPixels,
         depthTarget.width, depthTarget.height, minDepth, maxDepth);
  }

  void RenderDepthMesh(const Model& model, render::DepthTarget& depthTarget) {
    for (size_t idx = 0; idx + 2 < model.mesh->indices.size(); idx += 3) {
      uint32_t i0 = model.mesh->indices[idx];
      uint32_t i1 = model.mesh->indices[idx + 1];
      uint32_t i2 = model.mesh->indices[idx + 2];
      RasterizeDepthTri(model.mesh->verts[i0], model.mesh->verts[i1],
                        model.mesh->verts[i2], model.modelMatrix, depthTarget);
    }
  }

  void LoadModels() {
    if (m_TextureLoader == nullptr || m_MeshLoader == nullptr) {
      return;
    }

    // Load bunny, plane models
    m_BunnyModel = new Model();
    m_BunnyModel->mesh = m_MeshLoader->LoadMesh("bunny.obj");
    Mesh* mesh = m_BunnyModel->mesh;

    m_PlaneModel = new Model();
    m_PlaneModel->mesh = CreateGridPlaneMesh(10.0f, 10.0f, 10, 10);
    const float bunnyBottomY =
        -1.968211f;               // bunny 모델의 바닥 Y 좌표 (최소 Y 값)
    const float epsilon = 0.01f;  // 살짝 아래로 내려 z-fighting 방지

    m_PlaneModel->modelMatrix = Matrix4x4::identity;
    m_PlaneModel->modelMatrix.Translate(0.0f, bunnyBottomY - epsilon, 0.0f);
    m_PlaneModel->mesh->tga =
        m_TextureLoader->LoadTGATextureWithName("numbered_checker.tga");

    m_CubeModel = new Model();
    m_CubeModel->mesh = m_MeshLoader->LoadMesh("cube.obj");
    m_CubeModel->modelMatrix = Matrix4x4::identity;
    m_CubeModel->modelMatrix.Translate(0.0f, 1.0f, 0.0f);
  }

  void RenderMeshWithShader(const Shader& shader,
                            const ShaderUniforms& uniforms, const Mesh& mesh) {
    for (size_t idx = 0; idx + 2 < mesh.indices.size(); idx += 3) {
      uint32_t i0 = mesh.indices[idx];
      uint32_t i1 = mesh.indices[idx + 1];
      uint32_t i2 = mesh.indices[idx + 2];
      const VertexInput v0{mesh.verts[i0], mesh.normals[i0], mesh.uvs[i0]};
      const VertexInput v1{mesh.verts[i1], mesh.normals[i1], mesh.uvs[i1]};
      const VertexInput v2{mesh.verts[i2], mesh.normals[i2], mesh.uvs[i2]};
      const VertexOutput out0 = shader.VertexShader(v0, uniforms);
      const VertexOutput out1 = shader.VertexShader(v1, uniforms);
      const VertexOutput out2 = shader.VertexShader(v2, uniforms);
      DrawShaderTri(out2, out1, out0, uniforms, shader, true);
    }
  }

  void RenderMainScene(const Shader& shader, ShaderUniforms& uniforms) {
    if (m_PlaneModel == nullptr) {
      return;
    }
    Mesh* planeMesh = m_PlaneModel->mesh;
    if (planeMesh == nullptr) {
      return;
    }

    uniforms.model = m_PlaneModel->modelMatrix;
    // uniforms.texture = planeMesh->tga;
    uniforms.view = m_CameraMatrix;
    uniforms.projection = m_ProjectionMatrix;
    uniforms.lightView = m_LightViewMatrix;
    uniforms.lightProjection = m_LightProjectionMatrix;
    uniforms.shadowViewport = m_ShadowViewportMatrix;
    uniforms.shadowMap = &m_ShadowDepth;
    uniforms.lightDir = m_LightDir.Normalize();
    uniforms.cameraPosition = m_Camera.eye;
    uniforms.shadowBias = m_ShadowBias;

    // Fixed material components
    uniforms.ambientStrength = 0.25f;
    uniforms.diffuseStrength = 0.8f;
    uniforms.specularStrength = 1.4f;
    uniforms.shininess = 8.0f;
    uniforms.specularColor = Color(0xAAAAAAAA);

    RenderMeshWithShader(shader, uniforms, *m_PlaneModel->mesh);
#if 0
    // Bunny 모델 렌더링
    if (m_BunnyModel == nullptr)
    {
      return;
    }
    Mesh *bunnyMesh = m_BunnyModel->mesh;
    if (bunnyMesh == nullptr)
    {
      return;
    }
    uniforms.model = m_BunnyModel->modelMatrix;
    uniforms.texture = bunnyMesh->tga;

    //      uniforms.specularColor = Color(0xFF0000FF); // red color
    uniforms.specularColor = Color(0xFFFFFFFF); // white color

    for (size_t idx = 0; idx + 2 < bunnyMesh->indices.size(); idx += 3)
    {
      uint32_t i0 = bunnyMesh->indices[idx];
      uint32_t i1 = bunnyMesh->indices[idx + 1];
      uint32_t i2 = bunnyMesh->indices[idx + 2];

      const VertexInput v0{bunnyMesh->verts[i0], bunnyMesh->normals[i0],
                           bunnyMesh->uvs[i0]};
      const VertexInput v1{bunnyMesh->verts[i1], bunnyMesh->normals[i1],
                           bunnyMesh->uvs[i1]};
      const VertexInput v2{bunnyMesh->verts[i2], bunnyMesh->normals[i2],
                           bunnyMesh->uvs[i2]};

      const VertexOutput out0 = shader.VertexShader(v0, uniforms);
      const VertexOutput out1 = shader.VertexShader(v1, uniforms);
      const VertexOutput out2 = shader.VertexShader(v2, uniforms);

      DrawShaderTri(out2, out1, out0, uniforms, shader, true);
    }
#else
    if (m_CubeModel == nullptr || m_CubeModel->mesh == nullptr) {
      return;
    }

    uniforms.model = m_CubeModel->modelMatrix;
    uniforms.texture = planeMesh->tga;
    uniforms.specularColor = Color(0xFFFFFFFF);
    RenderMeshWithShader(shader, uniforms, *m_CubeModel->mesh);
#endif
  }

  void RenderShadowMapScene() {
    // 일반적으로 쉐도우 캐스트를 써서 일부 모델만 그림자 맵을 렌더링
    RenderDepthMesh(*m_PlaneModel, m_ShadowDepth);
    // RenderDepthMesh(*m_BunnyModel, m_ShadowDepth);
    RenderDepthMesh(*m_CubeModel, m_ShadowDepth);

    if (m_ShowShadowDepthOnly) {
      LogDepthTargetStats(m_ShadowDepth);
    }
  }

 private:
  int m_Width{0};
  int m_Height{0};
  uint32_t* m_Framebuffer{nullptr};
  WorldCamera m_Camera;

  Matrix4x4 m_ViewportMatrix, m_ProjectionMatrix, m_CameraMatrix;

  SDL_Renderer* m_Renderer{nullptr};
  SDL_Texture* m_MainTexture{nullptr};
  uint32_t m_FramebufferFormatEnum{SDL_PIXELFORMAT_ARGB8888};
  SDL_PixelFormat* m_FramebufferFormat{nullptr};

  TextureLoader* m_TextureLoader{nullptr};
  MeshLoader* m_MeshLoader{nullptr};
  render::DepthTarget m_ZBuffer;

  const float m_ZNear{1.0f}, m_ZFar{200.0f};

  float m_RotateRadian{0.0f};

  Model* m_BunnyModel{nullptr};
  Model* m_PlaneModel{nullptr};
  Model* m_CubeModel{nullptr};

  // Using BlinnPhongShader
  BlinnPhongShader m_BlinnPhongShader;
  ShaderUniforms m_BlinnPhongShaderUniforms;

  // Light parameters
  // Fixed light setup for a simple plane + cube shadow scene.
  Vector3 m_LightPosition{-4.0f, 6.0f, -3.0f};
  Vector3 m_LightTarget{0.0f, 0.0f, 0.0f};
  Vector3 m_LightDir{-0.51f, 0.77f, -0.38f};
  float m_ShadowOrthoLeft{-8.0f};
  float m_ShadowOrthoRight{8.0f};
  float m_ShadowOrthoBottom{-6.0f};
  float m_ShadowOrthoTop{6.0f};
  float m_ShadowNear{0.1f};
  float m_ShadowFar{20.0f};
  Matrix4x4 m_LightViewMatrix;
  Matrix4x4 m_LightProjectionMatrix;
  Matrix4x4 m_ShadowViewportMatrix;
  render::DepthTarget m_ShadowDepth;
  float m_ShadowBias{0.003f};  // bias to prevent shadow acne
  bool m_ShowShadowDepthOnly{false};
  bool m_HasLoggedShadowTriThisFrame{false};
};
