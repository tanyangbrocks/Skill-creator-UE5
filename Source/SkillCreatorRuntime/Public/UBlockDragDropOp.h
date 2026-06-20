#pragma once
#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "BlockType.h"
#include "UBlockDragDropOp.generated.h"

struct FBlockNode;

// 積木拖拉中的 payload（取代 Godot BlockDrag 靜態類，Main.cs/ScratchCanvas.cs 的
// _palDragBlock/_palDragType/BeginNew/BeginMove）。
// 不直接攜帶 TUniquePtr<FBlockNode>（UDragDropOperation 是 UObject，UPROPERTY 不支援
// TUniquePtr）；改成攜帶「如何建立」的輕量資訊，實際 FBlockNode 在 Drop 端才建立/搬移。
UCLASS()
class SKILLCREATORRUNTIME_API UBlockDragDropOp : public UDragDropOperation
{
    GENERATED_BODY()
public:
    // 來源：Palette 新建（true）或既有卡片搬移（false，Phase 3 接通）
    UPROPERTY() bool bFromPalette = true;

    // Palette 新建時三選一：Totem/Engraving 用 Id 查表；一般積木用 Type 直接建立預設值
    UPROPERTY() bool       bIsTotemBlock     = false;
    UPROPERTY() bool       bIsEngravingBlock = false;
    UPROPERTY() FName      PaletteTotemId;
    UPROPERTY() FName      PaletteEngraveId;
    UPROPERTY() EBlockType PaletteBlockType = EBlockType::If;

    // 既有卡片搬移模式（bFromPalette=false，Phase 3）：來源清單+索引，Drop 端從這裡
    // 移除節點（對應 Godot BlockDrag.BeginMove(idx, src, block)，ScratchCanvas.cs:917）。
    // 純 C++ member（非 UPROPERTY，TArray<TUniquePtr<>>* 不可反射）；生命週期只在拖拉
    // 手勢期間有效，指向 UBlockEditorWidget 持有的 FSpellArray.Blocks 樹，不會跨越該樹的生命週期。
    TArray<TUniquePtr<FBlockNode>>* SourceList  = nullptr;
    int32                           SourceIndex = -1;

    // 依上面欄位建立一個新的 FBlockNode（對應 Godot ScratchCanvas.MakeDefaultBlock /
    // AbilityEditorUI.cs:637-645,789-796,825-833 三種來源各自的 BlockNode 建構）
    TUniquePtr<FBlockNode> CreateBlockNode() const;
};
