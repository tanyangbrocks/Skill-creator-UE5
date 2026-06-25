#include "FNPCResponseParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FNPCBrainResponse FNPCResponseParser::Parse(const FString& RawResponse)
{
    FNPCBrainResponse Out;

    const FString Cleaned = StripThinkBlock(RawResponse);
    const FString Payload = ExtractPayload(Cleaned);

    if (Payload.IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("FNPCResponseParser: no JSON payload found in LLM response: %.200s"), *RawResponse);
        return Out;
    }

    TSharedPtr<FJsonObject> Json;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("FNPCResponseParser: JSON parse failed for payload: %.200s"), *Payload);
        return Out;
    }

    Json->TryGetStringField(TEXT("dialogue"),    Out.Dialogue);
    Json->TryGetStringField(TEXT("emotion"),     Out.Emotion);
    Json->TryGetStringField(TEXT("memory_note"), Out.MemoryNote);

    FString ActionStr;
    if (Json->TryGetStringField(TEXT("action"), ActionStr))
        Out.Action = ActionFromString(ActionStr);

    Out.bValid = true;
    return Out;
}

FString FNPCResponseParser::StripThinkBlock(const FString& Raw)
{
    FString Result = Raw;
    while (true)
    {
        const int32 S = Result.Find(TEXT("<think>"),  ESearchCase::IgnoreCase);
        const int32 E = Result.Find(TEXT("</think>"), ESearchCase::IgnoreCase);
        if (S == INDEX_NONE || E == INDEX_NONE || E <= S) break;
        Result = Result.Left(S) + Result.Mid(E + 8); // 8 = len("</think>")
    }
    return Result.TrimStartAndEnd();
}

FString FNPCResponseParser::ExtractPayload(const FString& Raw)
{
    const int32 TagS = Raw.Find(TEXT("<response>"),  ESearchCase::IgnoreCase);
    const int32 TagE = Raw.Find(TEXT("</response>"), ESearchCase::IgnoreCase);
    if (TagS != INDEX_NONE && TagE != INDEX_NONE && TagE > TagS)
        return Raw.Mid(TagS + 10, TagE - TagS - 10).TrimStartAndEnd();

    // Fall back: first '{' … last '}'
    const int32 BS = Raw.Find(TEXT("{"));
    const int32 BE = Raw.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (BS != INDEX_NONE && BE > BS)
        return Raw.Mid(BS, BE - BS + 1);

    return TEXT("");
}

ENPCAction FNPCResponseParser::ActionFromString(const FString& S)
{
    if (S == TEXT("Attack"))    return ENPCAction::Attack;
    if (S == TEXT("Flee"))      return ENPCAction::Flee;
    if (S == TEXT("Follow"))    return ENPCAction::Follow;
    if (S == TEXT("Trade"))     return ENPCAction::Trade;
    if (S == TEXT("CastSpell")) return ENPCAction::CastSpell;
    if (S == TEXT("PlaceTile")) return ENPCAction::PlaceTile;
    if (S == TEXT("BreakTile")) return ENPCAction::BreakTile;
    if (S == TEXT("GiveItem"))  return ENPCAction::GiveItem;
    return ENPCAction::Idle;
}
