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
    SideScroll2D UMETA(DisplayName="橫向 2D"), // 對應 Godot CameraController.cs:18，原文從未出現「2.5D」
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

    // 是否使用正交投影（對應 Godot CameraController.cs:164-171 ApplyProjection()：
    // Isometric/SideScroll2D 用 Orthogonal，Third/FirstPerson 用 Perspective）。
    // 2026-06-22 補：UE5 原本 4 個視角全部都是透視投影，從沒切換過，這正是「2D 視角看起來
    // 像 2.5D」的根本原因——固定角度的透視相機仍有透視深度線索，不會真的看起來平。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    bool bOrthographic = false;

    // 正交投影寬度（cm）。對應 Godot _cam.Size = OrthoSize（30 tiles），
    // 30 tiles × WorldScale::TileSizeCm(30cm) = 900cm。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    float OrthoWidth = 900.f;
};
