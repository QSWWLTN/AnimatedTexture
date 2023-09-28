#pragma once

#include "CoreMinimal.h"
#include "TextureResource.h"	// Engine

class UAnimatedTexture2D;

struct FAnmatedTextureState {
	int CurrentFrame;
	float FrameTime;

	FAnmatedTextureState() :CurrentFrame(0), FrameTime(0) {}
};

class FAnimatedTextureResource : public FTextureResource, public FTickableObjectRenderThread
{
public:
	FAnimatedTextureResource(UAnimatedTexture2D* InOwner);

	virtual uint32 GetSizeX() const override { return Owner ? Owner->GlobalWidth : 2; };
	virtual uint32 GetSizeY() const override { return Owner ? Owner->GlobalHeight : 2; };
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override
	{
		return true;
	}
	virtual TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UAnimatedTexture2D, STATGROUP_Tickables);
	}

	bool TickAnim(float DeltaTime);
	void DecodeFrameToRHI();


private:
	int32 GetDefaultMipMapBias() const;

	void CreateSamplerStates(float MipMapBias);

private:
	UAnimatedTexture2D* Owner;
	FAnmatedTextureState AnimState;
	TArray<FColor>	FrameBuffer[2];
	uint32 LastFrame;
};
