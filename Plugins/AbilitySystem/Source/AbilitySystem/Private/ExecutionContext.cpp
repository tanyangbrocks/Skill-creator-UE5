#include "ExecutionContext.h"

TMap<FName, float>         FExecutionContext::GlobalVars;
TMap<FName, TArray<float>> FExecutionContext::GlobalLists;
TMap<FName, float>         FExecutionContext::TaskCounters;
TSet<FName>                FExecutionContext::TaskCounterReached;
bool                       FExecutionContext::bTraceMode = false;

FExecutionContext::FExecutionContext(TArray<FInstruction> InCode)
    : Code(MoveTemp(InCode))
{}

TArray<float>& FExecutionContext::GetOrCreateList(FName Name, bool bGlobal)
{
    auto& Dict = bGlobal ? GlobalLists : InstanceLists;
    if (TArray<float>* L = Dict.Find(Name)) return *L;
    return Dict.Add(Name);
}

TArray<float>* FExecutionContext::GetList(FName Name, bool bGlobal)
{
    return (bGlobal ? GlobalLists : InstanceLists).Find(Name);
}
