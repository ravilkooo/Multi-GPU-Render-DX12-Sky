#pragma once
#include "C:\Users\user\my_catalogue\Studyspace\MGPU-sky-fork\Multi-GPU-Render-DX12-Sky\Common\ModelRenderer.h"
class SkyAtmosphereRenderer :
    public ModelRenderer
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE rayMarchGpuTextureHandle{};
	CD3DX12_CPU_DESCRIPTOR_HANDLE rayMarchCpuTextureHandle{};

	CD3DX12_GPU_DESCRIPTOR_HANDLE terrainGpuTextureHandle{};
	CD3DX12_CPU_DESCRIPTOR_HANDLE terrainCpuTextureHandle{};

public:
	SkyAtmosphereRenderer(const std::shared_ptr<GDevice>& device, const std::shared_ptr<GModel>& model,
		GDescriptor* rayMarchSrvMemory, 
		GDescriptor* terrainSrvMemory,
		UINT offset = 0,
		uint32_t terrainResolution = 512u);

	static const uint32_t sRayMarchShaderSlot = 0u;
	static const uint32_t sRayMarchShaderSpace = 2u;

protected:
	void Draw(const std::shared_ptr<GCommandList>& cmdList) override;
};

