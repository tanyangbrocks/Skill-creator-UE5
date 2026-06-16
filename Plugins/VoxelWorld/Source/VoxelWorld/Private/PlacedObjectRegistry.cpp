#include "PlacedObjectRegistry.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

int32 FPlacedObjectRegistry::Register(FPlacedUnit& InUnit)
{
    InUnit.PlacedUnitId = NextId++;
    for (const FIntVector& Tile : InUnit.Tiles)
        TileToUnitId.Add(Tile, InUnit.PlacedUnitId);
    Units.Add(InUnit);
    return InUnit.PlacedUnitId;
}

void FPlacedObjectRegistry::Unregister(int32 PlacedUnitId)
{
    for (int32 i = Units.Num() - 1; i >= 0; --i)
    {
        if (Units[i].PlacedUnitId != PlacedUnitId) continue;
        for (const FIntVector& Tile : Units[i].Tiles)
            TileToUnitId.Remove(Tile);
        Units.RemoveAt(i);
        return;
    }
}

FPlacedUnit* FPlacedObjectRegistry::Find(int32 PlacedUnitId)
{
    for (FPlacedUnit& U : Units)
        if (U.PlacedUnitId == PlacedUnitId) return &U;
    return nullptr;
}

const FPlacedUnit* FPlacedObjectRegistry::Find(int32 PlacedUnitId) const
{
    for (const FPlacedUnit& U : Units)
        if (U.PlacedUnitId == PlacedUnitId) return &U;
    return nullptr;
}

FPlacedUnit* FPlacedObjectRegistry::FindAtTile(FIntVector WorldTile)
{
    const int32* Id = TileToUnitId.Find(WorldTile);
    return Id ? Find(*Id) : nullptr;
}

const FPlacedUnit* FPlacedObjectRegistry::FindAtTile(FIntVector WorldTile) const
{
    const int32* Id = TileToUnitId.Find(WorldTile);
    return Id ? Find(*Id) : nullptr;
}

bool FPlacedObjectRegistry::OccupiedByPlaced(FIntVector WorldTile) const
{
    return TileToUnitId.Contains(WorldTile);
}

void FPlacedObjectRegistry::Clear()
{
    Units.Empty();
    TileToUnitId.Empty();
    NextId = 1;
}

void FPlacedObjectRegistry::RebuildTileIndex()
{
    TileToUnitId.Empty();
    for (const FPlacedUnit& U : Units)
        for (const FIntVector& Tile : U.Tiles)
            TileToUnitId.Add(Tile, U.PlacedUnitId);
}

// ── JSON 序列化 ─────────────────────────────────────────────────────────────

FString FPlacedObjectRegistry::SerializeToJson() const
{
    TArray<TSharedPtr<FJsonValue>> JArr;
    for (const FPlacedUnit& U : Units)
    {
        TSharedRef<FJsonObject> JObj = MakeShared<FJsonObject>();
        JObj->SetNumberField(TEXT("id"),       U.PlacedUnitId);
        JObj->SetNumberField(TEXT("mat"),      (int32)U.Material);
        JObj->SetNumberField(TEXT("maxDur"),   U.MaxDurability);
        JObj->SetNumberField(TEXT("curDur"),   U.CurrentDurability);

        TArray<TSharedPtr<FJsonValue>> JTiles;
        for (const FIntVector& T : U.Tiles)
        {
            TSharedRef<FJsonObject> JT = MakeShared<FJsonObject>();
            JT->SetNumberField(TEXT("x"), T.X);
            JT->SetNumberField(TEXT("y"), T.Y);
            JT->SetNumberField(TEXT("z"), T.Z);
            JTiles.Add(MakeShared<FJsonValueObject>(JT));
        }
        JObj->SetArrayField(TEXT("tiles"), JTiles);
        JArr.Add(MakeShared<FJsonValueObject>(JObj));
    }

    FString Out;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(JArr, Writer);
    return Out;
}

bool FPlacedObjectRegistry::DeserializeFromJson(const FString& Json)
{
    TArray<TSharedPtr<FJsonValue>> JArr;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, JArr)) return false;

    Clear();
    for (const TSharedPtr<FJsonValue>& JVal : JArr)
    {
        const TSharedPtr<FJsonObject>* JObjPtr;
        if (!JVal->TryGetObject(JObjPtr)) continue;
        const TSharedPtr<FJsonObject>& JObj = *JObjPtr;

        FPlacedUnit U;
        U.PlacedUnitId      = (int32)JObj->GetNumberField(TEXT("id"));
        U.Material          = (EMaterialType)(int32)JObj->GetNumberField(TEXT("mat"));
        U.MaxDurability     = (float)JObj->GetNumberField(TEXT("maxDur"));
        U.CurrentDurability = (float)JObj->GetNumberField(TEXT("curDur"));

        const TArray<TSharedPtr<FJsonValue>>* JTiles;
        if (JObj->TryGetArrayField(TEXT("tiles"), JTiles))
        {
            for (const TSharedPtr<FJsonValue>& JTVal : *JTiles)
            {
                const TSharedPtr<FJsonObject>* JTPtr;
                if (!JTVal->TryGetObject(JTPtr)) continue;
                FIntVector T;
                T.X = (int32)(*JTPtr)->GetNumberField(TEXT("x"));
                T.Y = (int32)(*JTPtr)->GetNumberField(TEXT("y"));
                T.Z = (int32)(*JTPtr)->GetNumberField(TEXT("z"));
                U.Tiles.Add(T);
            }
        }

        if (U.PlacedUnitId >= NextId) NextId = U.PlacedUnitId + 1;
        Units.Add(U);
    }
    RebuildTileIndex();
    return true;
}
