#pragma once
#include "ModelRenderer.h"

using namespace PEPEngine;
using namespace Graphics;
using namespace Allocator;
using namespace Utils;

class TerrainRenderer :
    public ModelRenderer
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuTextureHandle{};
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuTextureHandle{};

public:
    TerrainRenderer(const std::shared_ptr<GDevice>& device, const std::shared_ptr<GModel>& model,
        GTexture& heigtMapTexture,
        GDescriptor* srvMemory, UINT offset = 0,
        uint32_t terrainResolution = 512u);
    
    static const uint32_t sHeightMapShaderSlot = 0u;
    static const uint32_t sHeightMapShaderSpace = 2u;

protected:
    void Draw(const std::shared_ptr<GCommandList>& cmdList) override;

    uint32_t mTerrainResolution = 512u;
};

