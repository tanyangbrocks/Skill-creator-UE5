#include "UBlockDragDropOp.h"
#include "FBlockNode.h"
#include "Instruction.h"
#include "TotemLibrary.h"

TUniquePtr<FBlockNode> UBlockDragDropOp::CreateBlockNode() const
{
    TUniquePtr<FBlockNode> Node = MakeUnique<FBlockNode>();

    if (bIsTotemBlock)
    {
        Node->Type = EBlockType::Totem;
        FTotemBlockArgs Args;
        Args.TotemId = PaletteTotemId;
        if (const FTotemData* Data = FTotemLibrary::FindTotem(PaletteTotemId))
        {
            Args.TotemType            = Data->Type;
            Args.BaseAbilityPointCost = Data->BaseAbilityPointCost;
        }
        Node->Params.Add(TEXT("args"), FInstancedStruct::Make<FTotemBlockArgs>(Args));
    }
    else if (bIsEngravingBlock)
    {
        Node->Type = EBlockType::Engraving;
        FEngravingBlockArgs Args;
        Args.EngraveId = PaletteEngraveId;
        Args.Points    = 0.f;
        Node->Params.Add(TEXT("args"), FInstancedStruct::Make<FEngravingBlockArgs>(Args));
    }
    else
    {
        // 一般積木：對應 Godot ScratchCanvas.MakeDefaultBlock(t)（無註冊 Make() 時
        // fallback 為 new BlockNode { Type = t }，目前 UE5 尚無逐型別預設 Params 表，
        // 留空 Params 即等同 Godot 的 fallback 路徑）
        Node->Type = PaletteBlockType;
    }

    return Node;
}
