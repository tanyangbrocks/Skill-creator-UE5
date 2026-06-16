#pragma once
#include "SnapshotTypes.h"

// 純 C++ 介面 — 可被快照/還原的遊戲物件（對應 Godot ISnapshottable.cs）
class ISnapshottable
{
public:
    virtual ~ISnapshottable() = default;
    virtual FEntitySnapshot TakeSnapshot()                             const = 0;
    virtual void            RestoreFromSnapshot(const FEntitySnapshot&)      = 0;
};
