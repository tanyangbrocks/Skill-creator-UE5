#pragma once
#include "CoreMinimal.h"
#include "AttackTypes.generated.h"

// S-2 攻擊框架：攻擊種類 + 攻擊階段 + 打擊區定義。

UENUM(BlueprintType)
enum class EAttackType : uint8
{
    Light   UMETA(DisplayName="輕攻"),
    Heavy   UMETA(DisplayName="重攻"),
    Charged UMETA(DisplayName="蓄力攻"),
    Aerial  UMETA(DisplayName="空中輕攻"),
    Plunge  UMETA(DisplayName="下墜攻"),
    Rising  UMETA(DisplayName="上升攻"),
    Rush    UMETA(DisplayName="衝刺攻"),
};

UENUM(BlueprintType)
enum class EAttackPhase : uint8
{
    None       UMETA(DisplayName="無"),
    WindingUp  UMETA(DisplayName="前搖"),
    Active     UMETA(DisplayName="攻擊幀"),
    Recovering UMETA(DisplayName="後搖"),
};

USTRUCT()
struct FAttackHitbox
{
    GENERATED_BODY()

    // 相對角色前方偏移（cm）；150cm ≈ 1.5 遊戲單位（Grain=16）
    UPROPERTY() FVector     Offset = FVector(150.f, 0.f, 0.f);
    // 球形半徑（cm）；100cm = 1 遊戲單位
    UPROPERTY() float       Radius = 100.f;
    // 填入 PerformAttack 時的實際傷害值
    UPROPERTY() float       Damage = 0.f;
    UPROPERTY() EAttackType Type   = EAttackType::Light;
};
