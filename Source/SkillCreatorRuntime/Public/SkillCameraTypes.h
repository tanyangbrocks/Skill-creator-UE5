#pragma once
#include "CoreMinimal.h"
#include "SkillCameraTypes.generated.h"

// 攝影機模式（對應 Godot CameraController.cs CameraMode enum）
UENUM(BlueprintType)
enum class ECameraMode : uint8
{
    ThirdPerson  UMETA(DisplayName="第三人稱"),
    FirstPerson  UMETA(DisplayName="第一人稱"),
    Isometric    UMETA(DisplayName="等角視角"),
    SideScroll2D UMETA(DisplayName="橫向 2.5D"),
};

// 各模式的攝影機參數（可在 Editor 調整）
USTRUCT(BlueprintType)
struct FCameraModeParams
{
    GENERATED_BODY()

    // 彈簧臂長度（cm）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    float ArmLength = 300.f;

    // 彈簧臂 Pitch 角（負值 = 由上往下看）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    float PitchDeg = -15.f;

    // 是否由控制器旋轉決定朝向（false = 鎖定固定角度）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    bool bUsePawnControlRotation = true;

    // 固定 Yaw（bUsePawnControlRotation=false 時有效）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    float FixedYawDeg = 0.f;
};
