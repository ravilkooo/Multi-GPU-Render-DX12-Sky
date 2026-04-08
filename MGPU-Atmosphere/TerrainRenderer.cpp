#include "pch.h"
#include "TerrainRenderer.h"
#include "GDescriptor.h"
#include "GMesh.h"
#include "GModel.h"

TerrainRenderer::TerrainRenderer(const std::shared_ptr<GDevice>& device, const std::shared_ptr<GModel>& model,
	GTexture& heigtMapTexture,
	GDescriptor* srvMemory,
	UINT offset, uint32_t terrainResolution) : ModelRenderer(device, model)
{
	mTerrainResolution = terrainResolution;
	
	gpuTextureHandle = srvMemory->GetGPUHandle(offset);
	cpuTextureHandle = srvMemory->GetCPUHandle(offset);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	auto desc = heigtMapTexture.GetD3D12Resource()->GetDesc();
	srvDesc.Format = GetSRGBFormat(desc.Format);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = desc.MipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;
	heigtMapTexture.CreateShaderResourceView(&srvDesc, srvMemory, offset);
}

void TerrainRenderer::Draw(const std::shared_ptr<GCommandList>& cmdList)
{
	/*
	cmdList->SetRootConstantBufferView(StandardShaderSlot::ObjectData,
		*modelDataBuffer, 0);
	*/
	// 7th root parameter
	cmdList->GetGraphicsCommandList()->SetGraphicsRootDescriptorTable(7,
		gpuTextureHandle);

	cmdList->GetGraphicsCommandList()->IASetVertexBuffers(0, 0, nullptr);
	cmdList->GetGraphicsCommandList()->IASetIndexBuffer(nullptr);
	cmdList->GetGraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->Draw(6, mTerrainResolution * mTerrainResolution);
}

void TerrainRenderer::Update()
{
}
