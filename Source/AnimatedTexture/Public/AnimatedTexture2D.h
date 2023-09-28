#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Engine/Texture.h"

#if UE_BUILD_SHIPPING
	#include "Compression/lz4.h"
#else
	#include "TraceLog/Private/Trace/LZ4/lz4.c.inl"
#endif

#include "AnimatedTexture2D.generated.h"

class FAnimatedTextureResource;
ANIMATEDTEXTURE_API bool isGifData(const void* data);

USTRUCT()
struct FGIFFrame
{
	GENERATED_BODY()
public:
	float Time;	// next frame delay in sec
	uint32 Index;	// 0-based index of the current frame
	uint32 Width;	// current frame width
	uint32 Height;	// current frame height
	uint32 OffsetX;	// current frame horizontal offset
	uint32 OffsetY;	// current frame vertical offset
	bool Interlacing;	// see: https://en.wikipedia.org/wiki/GIF#Interlacing
	uint8 Mode;	// next frame (sic next, not current) blending mode
	int16 TransparentIndex;	// 0-based transparent color index (or −1 when transparency is disabled)

	uint8* PixelIndices = nullptr;
	uint64 PixelIndicesSize = 0;
	uint64 CompPixelIndicesSize = 0;

	TArray<FColor> Palette;

	FGIFFrame() :Time(0), Index(0), Width(0), Height(0), OffsetX(0), OffsetY(0),
		Interlacing(false), Mode(0), TransparentIndex(-1)
	{}
};


UCLASS(BlueprintType, Category = AnimatedTexture, hideCategories = (Adjustments, Compression, LevelOfDetail))
class ANIMATEDTEXTURE_API UAnimatedTexture2D : public UTexture
{
	GENERATED_BODY()

public:
	friend FAnimatedTextureResource;

	UAnimatedTexture2D(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture, meta = (DisplayName = "X-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
		TEnumAsByte<enum TextureAddress> AddressX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture, meta = (DisplayName = "Y-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
		TEnumAsByte<enum TextureAddress> AddressY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture)
		bool SupportsTransparency = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture)
		float DefaultFrameDelay = 1.0f / 24.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture)
		float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture)
		bool bLooping = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimatedTexture)
		//是否一直绘制
		bool bAlwaysTickEvenNoSee = true;

	UPROPERTY(VisibleAnywhere, Transient,Category = AnimatedTexture)
		int FrameNum;


	virtual void PostLoad() override;


	virtual void BeginDestroy() override;

	bool ImportGIF(const uint8* Buffer, uint32 BufferSize);

	void ResetToInVaildGif()
	{
		GlobalWidth = 0;
		GlobalHeight = 0;
		Background = 0;
		Duration = 0.0f;
		Frames.Empty();
		FrameNum = 0;
	}

	void Import_Init(uint32 InGlobalWidth, uint32 InGlobalHeight, uint8 InBackground, uint32 InFrameCount);

	int GetFrameCount() const
	{ 
		return FramNum;
	}

	FGIFFrame GetCacheFrame();
	float GetCacheFrameDelay();

	float GetFrameDelay()
	{
		return GetCacheFrameDelay();
	}

	float GetTotalDuration() const { return Duration; }

	void PostInitProperties() override;

private:
	bool ParseRawData();

public:
	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void Play();

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void PlayFromStart();

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void Stop();

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		bool IsPlaying() const { return bPlaying; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void SetLooping(bool bNewLooping) { bLooping = bNewLooping; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		bool IsLooping() const { return bLooping; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		void SetPlayRate(float NewRate) { PlayRate = NewRate; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		float GetPlayRate() const { return PlayRate; }

	UFUNCTION(BlueprintCallable, Category = AnimatedTexture)
		float GetAnimationLength() const;

	virtual float GetSurfaceWidth() const override;
	virtual float GetSurfaceHeight() const override;
	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_Texture2D; }
	virtual uint32 CalcTextureMemorySizeEnum(ETextureMipCount Enum) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	UPROPERTY()
		bool bPlaying = true;

public:
	uint32 GlobalWidth = 0;
	uint32 GlobalHeight = 0;
	uint8 Background = 0;
	float Duration = 0.0f;

	uint32 NowFramIndex = 0;		//实际播放到哪一帧
	uint32 FramNum = 0;				//帧总数
	bool IsStart = true;
	bool IsLoading = false;
	TMap<uint64, FGIFFrame*> Frames;

	UPROPERTY()
	TArray<uint8> RawData;

	uint64 NotCompSize = 0;
	uint64 CompSize = 0;
};