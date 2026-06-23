#pragma once
#include "CoreMinimal.h"
#include "AttackTypes.generated.h"

// S-2 攻擊框架骨架：攻擊種類 + 打擊區定義。
// ASkillCreatorCharacter::PerformLightAttack() 使用 FAttackHitbox 做球形掃描。

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
