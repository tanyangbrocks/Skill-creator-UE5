# Godot 4 ??Unreal Engine 5 ?瑞宏撖虫?閮

> ?啣神?交?嚗?026-06-14
> 靘?嚗?獢???勗? + UE5 ?瑞宏?銵?蝛?> ?格?撘?嚗E5.4+ / C++
> ?瑞宏蝑嚗?*?閮剛?嚗?蝧餉陌**嚗?雿輻?芸? C# ??C++ 撌亙嚗?
---

## 銝?蝘餅捱蝑???
| ?? | 隤芣? |
|------|------|
| Godot ?曉撠勗歇蝬 | 蝛箔??停???賢????寞????Godot ??chunk mesh rebuild 蝞∠? |
| Grain 閮隤踹???64+ | 瘥摰嗅雿? 64?128?64 tile嚗???蝛?曉??64 ??Godot ?⊥??? |
| 閬死?格?嚗AA 閬嚗?| Nanite 擃?豢芋?umen ??????∠蝣??頂蝯梧?Godot ?撩銋???pipeline |
| CA + 瘥?tile ?拍? | ?閬?GPU Compute Shader ?貉?璅⊥嚗odot 瘝????寞? |
| **蝯?** | UE5 ?臬銝????璅像??|

---

## 鈭??菜?銵捱蝑?
### 2-1 C# ??C++ 蝑嚗??啗身閮?銝蕃霅?
?芸?蝧餉陌撌亙嚗odePorting?lterNative 蝑?**銝??*?潮??脤?頛荔?
- C# GC vs C++ RAII 閮擃芋?榆頝云憭?- `virtual generic method`?INQ?sync/await ?∪??蕃霅?- 蝧餃??C++ ?虜瘥???C# ?湔

**??嚗身閮宏璊?蝔?蝣潮?撖?*
- ??頂蝯梁?閮剛?嚗??芋?????摩?M opcode 隤儔嚗?交窒??- 蝔?蝣潛 C++ ?撖衣嚗誑 UE5 ??璅∪??蹂誨 Godot API

### 2-2 Tile 銝?皜脫?嚗ealtimeMeshComponent

| ?寞? | 閰摯 |
|------|------|
| ProceduralMeshComponent | ??瘥??遣 mesh ?擃?閮擃 RMC ?拙?|
| **RealtimeMeshComponent (RMC)** | ?????湔瘥?PMC 敹?2 ??閮擃? 50%+嚗E5 撠 |
| Voxel Plugin 2 | ?拙? Minecraft ?啣耦嚗A ?拍???血????漲撠? |

**?豢?嚗ealtimeMeshComponent**
- GitHub嚗ttps://github.com/TriAxis-Games/RealtimeMeshComponent
- Greedy Meshing ?摩敺?C# 蝘餅?嚗??mesh ?漱 API

### 2-3 CA 銝?璅⊥嚗閮?Compute Shader嚗DG嚗?
| ?寞? | Grain 64+ ?航???|
|------|----------------|
| CPU Task Graph 憭銵? | 銝哨?Grain 32 隞乩?憭嚗?4 憯?憭改? |
| Niagara Grid2D Simulation Stage | 銝哨?閬死撅斗???gameplay ??撱園嚗?|
| **?芾? Compute Shader + RDG** | ???憭批??改?Grain 64+ ?臭??賊? |

**?豢?嚗ompute Shader + RDG嚗ender Dependency Graph嚗?*
- Phase 1 ?? CPU ?嚗??暹? C# ?摩?湔撠?嚗?- Phase 3 ??GPU ?嚗??斗 checkerboard ?湔璅∪?閫?捱銝西?蝡嗥嚗?- ??[GPU Falling Sand嚗eatbatgames嚗(https://meatbatgames.com/blog/falling-sand-gpu/)

### 2-4 UI嚗late嚗??函楊頛臬嚗? UMG嚗??脣 HUD嚗?
| 雿蔭 | 獢 | ? |
|------|------|------|
| 蝛??賜楊頛臬 | **Slate SGraphEditor** | UE5 ??Blueprint 蝺刻摩?具aterial 蝺刻摩?冽頨怠停??Slate嚗??喟?暺????|
| ???HUD / ?詨 | **UMG** | 敹恍???BP ?航???撅 |

蝛 UI 頝臬?嚗?```
?芾? UEdGraph + UEdGraphNode嚗”蝷?BlockNode嚗???SGraphPanel / SGraphEditor嚗??喟撣?
???芾? SGraphNode Widget嚗??典?閫嚗?```
??[UMG-Slate Compendium](https://github.com/YawLighthouse/UMG-Slate-Compendium)

### 2-5 璅∠?蝯?嚗yra 撘?GameFeature Plugin

```
YourGame/
??? Source/
??  ??? SkillCreatorCore/       ???箇?????ｇ???gameplay 靘陷嚗???  ??? SkillCreatorRuntime/    ??銝?Gameplay Module嚗layer?orld?I嚗???  ??? SkillCreatorEditor/     ??蝛蝺刻摩?剁???Editor ?蝺刻陌嚗???? Plugins/
    ??? AbilitySystem/          ??VM + ??賣?橘??舐蝡葫閰佗?
    ??? VoxelWorld/             ??CA 銝? + Chunk + RMC 皜脫?
    ??? SkillCreatorUI/         ??蝛蝺刻摩??Slate UI
```

### 2-6 撌脤?霅? UE5 ??琿

#### ??嚗ive Coding + UHT ??鞈?銝?甇????⊿?霅?crash

**閫貊璇辣**嚗???UE Editor嚗 Live Coding嚗?嚗耨??`.h` 鋆∠? `UCLASS/UENUM/UPROPERTY/USTRUCT` 銝衣蝺刻陌??
**??**嚗ive Coding ? `.dll` patch ?脫迤?典銵? process嚗? UHT ??撠???`*.generated.h`?TClassCompiledInDefer` ?餉?銵剁?銝??湔? `.h` ?寡?鈭?struct ???園? layout嚗???UObject 蝟餌絞隞??offset 霈撖???crash ??暺???憯?
**撘瑕撌乩?瘚?*嚗?- **摰**嚗ive Coding ?舐嚗??芣 `.cpp` ???摩嚗witch/case ?批捆??瘜擃?
- **?梢嚗?????Editor + VS Rebuild**嚗 `.h` 鋆∩遙雿?`U*` 撌券?靽桅ˇ???⊥? enum

#### ??嚗LatentActionManager 銝??SpellRunner

`FLatentActionManager` ??Blueprint Delay 蝭暺?摨惜撖衣嚗身閮雿雿輻?pellRunner ?閬?撟?券?VM ??嚗??寧**?? Tick 蝝臬???*嚗?
```cpp
struct FSpellRunner {
    TArray<FInstruction> Instructions;
    int32  ProgramCounter  = 0;
    float  TimeAccumulator = 0.f;

    // ??AActor::Tick ??GameMode Tick 鋆⊥?撟?澆
    void Advance(float DeltaTime);
};
```

?見??舀?捆??debug?FLatentActionInfo` ??UE5.5+ ???臬?閬?撣豢楛?楷? async 隤儔???嚗? SpellRunner ?楊撟 Wait ?敞?頞喳???
> 瘜冽?嚗A ?衣?銝? SpellRunner VM嚗?頂蝯勗??函蝡LatentActionManager ????皞 SpellRunner 撖虫??賂?撖阡?銝衣????詨撟曉???銝???寞?嚗?*?舀?扳??舫?? Tick ??甇???晞?*

#### ??嚗Array\<FBlockNode\> ?艘璅寧?閮擃摰?
`TArray<T>` ?舫???潸??園??FBlockNode` ?怠?????嚗摰寞??璉萄?璅?deep copy嚗???璅?冽蝛箝?
**?航炊??**嚗?```cpp
TArray<FBlockNode> ThenBranch;  // ???游捆?璉萄?璅?deep copy
```

**甇?Ⅱ??**嚗銝??甈邦嚗 `TUniquePtr`嚗?
```cpp
USTRUCT()
struct FBlockNode {
    GENERATED_BODY()
    EBlockType Type;
    // FName key嚗???FString嚗?FName ??int32 蝝Ｗ?嚗?頛?O(1)嚗??芸蝺刻陌??銝甈?    TMap<FName, FInstancedStruct> Params;
    // 摮??臭???UPROPERTY嚗HT ?⊥?餈質馱 TUniquePtr嚗? BlockNode 銝?閬?BP 摨???
    TArray<TUniquePtr<FBlockNode>> ThenBranch;
    TArray<TUniquePtr<FBlockNode>> ElseBranch;
    TArray<TUniquePtr<FBlockNode>> LoopBody;
};
// 瘜冽?嚗Instruction嚗untime嚗?桐? FInstancedStruct Payload嚗???TMap嚗?//       FBlockNode嚗esign-time嚗 TMap<FName, ...>嚗pellCompiler 蝺刻陌??銝甈?```

`TUniquePtr` vs `TSharedPtr`嚗lockNode ?????臬?潛??嚗邦嚗?銝?閬??刻??詨??瑁?蝺?嚗 `TUniquePtr` 隤儔?渡移蝣箝verhead ?港???
> BlockNode ?胯身閮???蝺刻陌銝甈～霈?瑁??? AST嚗?憿敶梢 spell 頛??嚗?敶梢瘥??瑁??靘踹?甇歹???M-1 ?挾撠望迤蝣箏恐???踹?銋?憭折?蝛??spell 頛??stall??
---

## 銝??獢頂蝯梢蝘餉?隡?
> ??蝯?嚗?06 ??C# 瑼?17,529 銵?撘Ⅳ

| 蝟餌絞 | 銵 | Godot ?血? | ?瑞宏??漲 | UE5 撠? |
|------|------|-----------|---------|---------|
| VM ?瑁?撅歹?ExecutionLoop / SpellCompiler嚗?| ~1,671 | 璆萎?嚗??GD.Print嚗?| **雿?* | 蝝?C++ struct/class嚗?仿?撖?|
| ??質??惜嚗pellArray / BlockNode / Data嚗?| ~321 | ??| **雿?* | UE5 USTRUCT |
| ??/Aura 蝟餌絞 | ~325 | ??| **雿?* | 蝝?C++ |
| 敹怎/?遝蝟餌絞 | ~342 | ??| **雿?* | 蝝?C++ |
| ?拙?/鋆?蝟餌絞 | ~302 | ??| **雿?* | 蝝?C++嚗??GAS ItemFragment |
| ??賣?橘?SpellCaster / SpellRunner嚗?| ~1,650 | 璆萎? | **雿?* | 蝝?C++嚗pellRunner ?寧?? Tick 蝝臬??剁?閬?禮2-6 ??嚗?|
| ?萎犖 AI嚗nemy / EnemyManager嚗?| ~1,000 | 雿?Vector3I ??FIntVector嚗?| **雿?銝?* | C++ ???嚗?????Behavior Tree |
| 銝?璅⊥嚗ileWorld3D / Chunk3D嚗?| ~2,500 | 雿?Vector ?嚗?| **銝?* | C++ + GPU Compute嚗hase 3嚗?|
| 摮?瑼?FlowSaveSystem嚗?| ~518 | 雿?user:// 頝臬?嚗?| **銝?* | UE FArchive + ?芾? JSON |
| Main ?餈游? | ~2,969 | 擃?_Process / _Input嚗?| **銝?* | AGameMode + APlayerController |
| 皜脫?嚗ileWorldRenderer3D嚗?| ~800 | 擃?ArrayMesh嚗?| **銝?擃?* | RealtimeMeshComponent |
| UI嚗bilityEditorUI / ScriptCanvas嚗?| ~3,818 | **璆菟?**嚗??Control 蝭暺? | **擃?* | Slate SGraphEditor + UMG嚗??券?撖恬? |

**?瑞宏瘥?嚗?*
- ~60% 蝝?頛???撟曆??湔?神嚗?瘜?嚗身閮???
- ~24% 瘛瑕? ???摩靽?嚗PI ?踵?
- ~16% UI/皜脫? ??摰?閮剛?

---

## ?瑽??扯”

| Godot | UE5 C++ |
|-------|---------|
| `Node3D` | `AActor` + `USceneComponent` |
| `Control` | `UUserWidget` (UMG) / `SWidget` (Slate) |
| `_Ready()` | `BeginPlay()` |
| `_Process(delta)` | `Tick(float DeltaTime)` |
| `Signal` / C# event | `DECLARE_DYNAMIC_MULTICAST_DELEGATE` |
| `Vector3I` | `FIntVector` |
| `Vector3` | `FVector` |
| `Dictionary<K,V>` | `TMap<K,V>` |
| `List<T>` | `TArray<T>` |
| `GD.Load<T>()` | `LoadObject<T>()` / `ConstructorHelpers::FObjectFinder` |
| `ArrayMesh` | `URuntimeMeshComponent` / `UProceduralMeshComponent` |
| `StandardMaterial3D` | `UMaterialInstanceDynamic` |
| `async/await` | ?? Tick 蝝臬??剁?SpellRunner嚗? UE Coroutine嚗?.5+嚗???async嚗?|
| `interface`嚗? C++ 蝟餌絞?? | **蝝??砍憿?* `class IFoo { virtual void Bar() = 0; }`嚗 UINTERFACE嚗?|
| `interface`嚗?脩策 Blueprint / AActor嚗?| UE5 `UINTERFACE` + `IFoo`嚗???嚗?Cast\<IFoo\> ????|

---

## 鈭??挾撖虫?閮

### M-0 ??皞???????

**??撌脣???2026-06-14嚗?**
- [x] ??GitHub 撱箇???repo嚗ttps://github.com/tanyangbrocks/Skill-creator-UE5.git
- [x] ?砍頝臬?嚗C:\Users\霅??許SkillCreatorUE5\`
- [x] 銴ˊ????急?隞嗚??冗蝯?????鞈?
- [x] 撱箇? `Source/SkillCreatorCore/`?Source/SkillCreatorRuntime/`?Source/SkillCreatorEditor/`
- [x] 撱箇? `Plugins/AbilitySystem/`?Plugins/VoxelWorld/`?Plugins/SkillCreatorUI/`
- [x] Godot `main` branch 撠?嚗eature/3d-cutover 撌脣?雿蛛?
- [x] 摰? UE5.4+嚗脰?銝???摰?敺銵??寞郊撽?

**??UE5 鋆末敺??餃?嚗???嚗?**

#### 甇仿? 1嚗遣蝡?UE5 C++ 蝛箏?獢??Ｙ? .uproject + Config/嚗?
1. ?? UE5 Editor ??New Project ??Games ??Blank ??**C++**
2. Location 閮剔 `C:\Users\霅??許`嚗roject Name 閮剔 `SkillCreatorUE5`
3. Create Project ??蝑? VS 蝺刻陌
4. 蝣箄??Ｙ?鈭?`SkillCreatorUE5.uproject`?Config/`?Source/SkillCreatorUE5/`

> ?? UE5 ? Source/ 銝????撠?????閮?Module??銝嚗??Ｘ郊撽??渡???
#### 甇仿? 2嚗遣蝡? Module ??Build.cs

瘥?Module ?閬???`.Build.cs`嚗E5 ??隤?璅∠?靘陷??
**`Source/SkillCreatorCore/SkillCreatorCore.Build.cs`**
```csharp
using UnrealBuildTool;

public class SkillCreatorCore : ModuleRules
{
    public SkillCreatorCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "StructUtils"
        });
    }
}
```

**`Source/SkillCreatorRuntime/SkillCreatorRuntime.Build.cs`**
```csharp
using UnrealBuildTool;

public class SkillCreatorRuntime : ModuleRules
{
    public SkillCreatorRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "GameplayTags",
            "SkillCreatorCore"
        });
    }
}
```

**`Source/SkillCreatorEditor/SkillCreatorEditor.Build.cs`**
```csharp
using UnrealBuildTool;

public class SkillCreatorEditor : ModuleRules
{
    public SkillCreatorEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "UnrealEd",
            "Slate", "SlateCore", "GraphEditor",
            "SkillCreatorCore", "SkillCreatorRuntime"
        });
    }
}
```

#### 甇仿? 3嚗遣蝡?Plugin ??.uplugin ?辣

**`Plugins/AbilitySystem/AbilitySystem.uplugin`**
```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "1.0",
    "FriendlyName": "AbilitySystem",
    "Description": "Spell VM, SpellRunner, SpellCaster",
    "Category": "SkillCreator",
    "Modules": [
        {
            "Name": "AbilitySystem",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ]
}
```

**`Plugins/VoxelWorld/VoxelWorld.uplugin`**
```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "1.0",
    "FriendlyName": "VoxelWorld",
    "Description": "CA Tile World, Chunk3D, RMC Renderer",
    "Category": "SkillCreator",
    "Modules": [
        {
            "Name": "VoxelWorld",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ]
}
```

**`Plugins/SkillCreatorUI/SkillCreatorUI.uplugin`**
```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "1.0",
    "FriendlyName": "SkillCreatorUI",
    "Description": "Slate Block Editor (SGraphEditor)",
    "Category": "SkillCreator",
    "Modules": [
        {
            "Name": "SkillCreatorUI",
            "Type": "Editor",
            "LoadingPhase": "Default"
        }
    ]
}
```

#### 甇仿? 4嚗? Plugin ??Build.cs

**`Plugins/AbilitySystem/Source/AbilitySystem/AbilitySystem.Build.cs`**
```csharp
using UnrealBuildTool;

public class AbilitySystem : ModuleRules
{
    public AbilitySystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "StructUtils",
            "SkillCreatorCore"
        });
    }
}
```

**`Plugins/VoxelWorld/Source/VoxelWorld/VoxelWorld.Build.cs`**
```csharp
using UnrealBuildTool;

public class VoxelWorld : ModuleRules
{
    public VoxelWorld(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "RenderCore", "RHI",
            "SkillCreatorCore"
            // RealtimeMeshComponent 敺??
        });
    }
}
```

**`Plugins/SkillCreatorUI/Source/SkillCreatorUI/SkillCreatorUI.Build.cs`**
```csharp
using UnrealBuildTool;

public class SkillCreatorUI : ModuleRules
{
    public SkillCreatorUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "UnrealEd",
            "Slate", "SlateCore", "GraphEditor",
            "SkillCreatorCore", "SkillCreatorRuntime", "AbilitySystem"
        });
    }
}
```

#### 甇仿? 5嚗Ⅱ隤?Build

?? UE5 Editor ???喲 `.uproject` ??Generate Visual Studio project files ??VS ?? ??Rebuild All

蝣箄? 0 ?航炊敺脣 **M-1**??
---

### M-1 ???詨?鞈?撅歹?雿◢?迎????嚗?
> ?格?嚗遣蝡?C++ 鞈?蝯?嚗?敺?蝟餌絞???亙蝷?
- [ ] `BlockNode.h/cpp`嚗?*runtime ?嚗STRUCT嚗UniquePtr 璅對?**嚗????`TArray<TUniquePtr<FBlockNode>>`嚗???UPROPERTY嚗? 禮2-6 ??嚗?  - ?? ??瑁????伐?**銝** M-9 Slate editor ?函?蝭暺???  - M-9 ??`UMyEdGraphNode : UEdGraphNode` ??editor 撠惇?嚗??交?敺?FBlockNode 頧??
- [ ] `BlockType.h`嚗pCode enum ??UE5 UENUM
- [ ] `SpellArray.h/cpp`嚗pellSlot?pellArray?ngraveData
- [ ] `ManaType.h/cpp`嚗anaTypeRegistry嚗?8 蝔桀蝷?MP嚗?- [ ] `WorldScale.h`嚗虜?賂?Grain?layerW?layerH 蝑?
- [ ] `GridPos.h`嚗IntVector wrapper嚗? Manhattan distance ?寞?嚗? **敹?鋆?`GetTypeHash` 隞乩噶雿?TMap key**嚗?  ```cpp
  // FIntVector ?祈澈??UE5 撌脫? GetTypeHash嚗??芾? wrapper 敹?????  FORCEINLINE uint32 GetTypeHash(const FGridPos& Pos)
  {
      uint32 Hash = GetTypeHash(Pos.X);
      Hash = HashCombine(Hash, GetTypeHash(Pos.Y));
      return HashCombine(Hash, GetTypeHash(Pos.Z));
  }
  // 憒? ChunkMap ?湔??FIntVector 雿?key嚗?銝?閬??ａ挾嚗E5 撌脣撱綽?
  ```
- [ ] `MaterialType.h`嚗ile ?釭 enum嚗?*??摰儔 gameplay ????**嚗?  ```cpp
  enum class ETileCategory : uint8 {
      GameplayBlock,   // 撖血?蝣唳? tile嚗????/??質風?橘???CPU authoritative嚗? GPU
      VisualCA,        // 蝝?閬?CA ?湔嚗???鋆ˇ瘞湔?嚗? GPU 璅⊥嚗sync readback ?迂 ?? 撟撱園
  };
  ```
  ?? ??????M-10 ???嗥??箇?嚗 M-1 撠勗???蝢拇?璆?閬?禮2-6 M-10 鋆?隤芣?嚗?
Build 蝣箄? 0 ?航炊嚗脣 M-2??
---

### M-2 ??VM ?瑁?撅?
> ?格?嚗 UE5 C++ 鋆∟?韏瑟???VM嚗?靘陷隞颱?皜脫?

- [ ] `Instruction.h/cpp`嚗?*?桐? Payload 閮剛?嚗???TMap嚗?*
  ```cpp
  struct FInstruction {
      EOpCode OpCode;
      FInstancedStruct Payload;  // ???OpCode ?勗?瘙箏?嚗??閬?key ?亥岷
  };
  // ?瑁???Instr.Payload.Get<FDamageArgs>()
  // 瘥車 OpCode 撠?銝?摰? Args struct嚗MoveArgs / FDamageArgs / FWaitArgs ?佗?
  ```
  ?? 銝???`TMap<FString, FInstancedStruct>`嚗String key 瘥活?瑁??賢?摮葡??嚗?銝???`TMap<FName, FInstancedStruct>`嚗Name 瘥?FString 敹恬?雿???Map ?亥岷 overhead嚗?opcode ?祈澈撌脣??亙??伐??湔 Get\<T\>() ?舀??芾楝敺?- [ ] `ExecutionContext.h/cpp`嚗??elegate 瘜典嚗ntityQuery?aycastQuery嚗?- [ ] `ExecutionLoop.h/cpp`嚗pCode dispatch table嚗witch/case嚗????~50 ??opcode嚗?- [ ] `SpellCompiler.h/cpp`嚗lockNode AST ??Instruction list
- [ ] `SpellRunner.h/cpp`嚗楊撟?瑁?摰孵嚗???Tick 蝝臬??剁?閬?禮2-6 ??嚗?- [ ] ?桀?皜祈岫嚗 UE5 Automation Test Framework 頝嗾???質??
---

### M-3 ??銝?璅⊥嚗PU ??

> ?格?嚗ileWorld3D + CA ??C++ 鋆∟?嚗銝? GPU

- [ ] `TileCell.h`嚗?*蝝?POD 蝯?嚗?怠?砍???*
  ```cpp
  struct FTileCell {         // 蝯?蝜潭 UObject嚗???UObject ??
      uint8  MaterialID;     // ?亥”??ID嚗?摮?璅?      uint8  CA_State;       // CA 璅⊥???      uint8  Category;       // ETileCategory嚗ameplayBlock / VisualCA嚗?      uint8  Flags;          // dirty / occupied ??  };
  // ??? Thread FArchive ????摰嚗??湔嚗?閫貊１ GC / UObject 蝟餌絞嚗?  // ??蝳迫??UMaterialInterface*?String??隞颱? UObject 甈?
  // Game Thread 頧?嚗 MaterialID ??MaterialRegistry嚗???UMaterialInstanceDynamic*
  ```
- [ ] `Chunk3D.h/cpp`嚗?6糧 FTileCell ??? + dirty tracking
- [ ] `TileWorld3D.h/cpp`嚗hunk 摮嚗TMap<FIntVector, TUniquePtr<FChunk3D>>`嚗IntVector 撌脫? UE5 ?批遣 hash嚗etTile/SetTile?A ?湔餈游?
- [ ] `WorldSaveData.h/cpp`嚗hunk 鈭脖?摨???`FArchive << uint8`嚗? POD嚗ackground thread 摰嚗?- [ ] `MapGenerator3D.h/cpp`嚗?摨敶Ｙ???Perlin noise ??tile嚗?- [ ] ?亙 UE5 Task Graph ???瑁?蝺?Chunk ?湔

---

### M-4 核心實體 Gameplay 層（設計決策已定稿）

> 目標：玩家、敵人、戰鬥狀態的 C++ 骨架，純 code-only（不開 Editor）

#### 已確認決策（2026-06-14）

| 決策點 | 結論 |
|--------|------|
| Q1 CharacterState 範圍 | **骨架模式**：M-4 只做戰鬥相關（Stamina/MentalEnergy/Mood 存根），W-5b 完整生存系統 → M-7 |
| Q2 Enemy AI 架構 | **BehaviorTree（B方案）+ 自製 Tile A***：`UBTTask_MoveOnGrid` 做格子尋路，**禁止**用標準 `MoveTo`（NavMesh 不適合 voxel 世界）。M-4 寫 C++ 框架；BT/BB `.uasset` → M-8 Editor 補建 |
| Q3 PlayerController 範圍 | **框架層只**：移動 + HP/MP 追蹤；施法→M-5、庫存→M-7、攝影機→M-8、溫度/生存→M-7（這些**必須**在對應 Module 實作，非可選） |
| Q4 玩家移動方式 | **物理移動**：`ACharacter` + `UCharacterMovementComponent`；敵人 = `APawn` 計時步進 tile 移動（Godot 原版邏輯） |

#### Build.cs 追加（SkillCreatorRuntime.Build.cs）

```
"EnhancedInput", "AIModule", "NavigationSystem", "GameplayTasks"
```

#### 待建立檔案清單

**SkillCreatorCore（純 C++，無 gameplay 依賴）**
- [ ] `Source/SkillCreatorCore/Public/CharacterStats.h` — USTRUCT(BlueprintType)，移植自 CharacterStats.cs 的 30+ float 欄位（MaxHpBase=100, MaxMpBase=100, MpRegenRate=8, CritRate=0.05, CritDmgMult=1.5, MoveSpeedMult=1.0 等）
- [ ] `Source/SkillCreatorCore/Public/IElementalTarget.h` — 純虛擬 C++ interface（非 UINTERFACE）：`ApplyAura / RemoveAura`
- [ ] `Source/SkillCreatorCore/Public/ICreature.h` — 純虛擬 C++ interface：`GetHp / GetMaxHp / IsAlive / GetTilePos`

**SkillCreatorRuntime（Gameplay Module）**
- [ ] `Source/SkillCreatorRuntime/Public/ASkillCreatorCharacter.h + cpp` — ACharacter 子類，持有 HP/MP、UCharacterStateComponent、UElementalAuraComponent，死亡/重生存根
- [ ] `Source/SkillCreatorRuntime/Public/ASkillCreatorPlayerController.h + cpp` — APlayerController，EnhancedInput 存根，施法觸發存根（M-5 填實）
- [ ] `Source/SkillCreatorRuntime/Public/UCharacterStateComponent.h + cpp` — UActorComponent：float Stamina=100, MentalEnergy=100, Mood=100；Tick 更新存根（M-7 填實）
- [ ] `Source/SkillCreatorRuntime/Public/UElementalAuraComponent.h + cpp` — FAuraEntry{EElementType, float Remaining}；TArray<FAuraEntry> Auras；聚合輸出：SpeedPenalty, bIsImmobilized, DamageTakenBonus, DefensePenalty；Advance(DeltaTime)
- [ ] `Source/SkillCreatorRuntime/Public/AEnemy.h + cpp` — APawn，EEnemyType(Melee/Ranged/Patrol/Heavy)，float MoveInterval=0.4f 計時器，離散 tile 移動，實作 ICreature + IElementalTarget
- [ ] `Source/SkillCreatorRuntime/Public/AEnemyAIController.h + cpp` — AAIController；Possess() 時設定 Blackboard + 啟動 BT（BT asset 在 M-8 補）
- [ ] `Source/SkillCreatorRuntime/Public/UBTTask_MoveOnGrid.h + cpp` — 向 Blackboard 取目標 tile，執行一步 tile A*（A* 完整實作 → M-5，M-4 先做 Manhattan 直線移動存根）
- [ ] `Source/SkillCreatorRuntime/Public/UBTTask_AttackPlayer.h + cpp` — 攻擊判定存根（M-5 接 SpellCaster 後填實）
- [ ] `Source/SkillCreatorRuntime/Public/UBTService_UpdateTarget.h + cpp` — 每幀更新 Blackboard 中的玩家位置 key
- [ ] `Source/SkillCreatorRuntime/Public/AEnemyManager.h + cpp` — 敵人生成/管理 Actor
- [ ] `Source/SkillCreatorRuntime/Public/UCombatStateSubsystem.h + cpp` — UGameInstanceSubsystem：bInCombat, BattleId(FGuid), CastCount/DamageDealt/KillCount, OutOfCombatTimeout=5.0f, EnterCombat/ExitCombat

#### Godot 對照原始碼

| Godot 檔案 | 對應 UE5 類別 |
|-----------|-------------|
| `C:\skill-creator\Scripts\World\CharacterStats.cs` | `CharacterStats.h`（30+ float 欄位） |
| `C:\skill-creator\Scripts\World\Enemy.cs` | `AEnemy` + `UBTTask_*`（3狀態 FSM 升級為 BT） |
| `C:\skill-creator\Scripts\World\CombatState.cs` | `UCombatStateSubsystem` |
| `C:\skill-creator\Scripts\AbilitySystem\Elemental\ElementalAuraComponent.cs` | `UElementalAuraComponent` |
| `C:\skill-creator\Scripts\World\CharacterState.cs` | `UCharacterStateComponent`（M-4 骨架，M-7 完整） |

#### M-4 不實作（強制延後）

- 施法邏輯（SpellCaster 連接）→ M-5
- 物品欄系統 → M-7
- 攝影機系統 → M-8
- 溫度/生存系統完整實作 → M-7
- BT/BB `.uasset` 資產建立 → M-8（Editor 開啟後）
- BT 路徑中的完整 tile A* 實作 → M-5

---
### M-5 ????賣?暹??
> ?格?嚗摰嗉?賣??踝?VM ?瑁?蝯?敶梢銝?

- [ ] `SpellCaster.h/cpp`嚗ryCast????MP??曉??- [ ] `SpellProjectile.h/cpp`嚗?撠蝘餃??１?銝剖?隤?- [ ] ?仿?SpellRunner ??TileWorld3D嚗??賣??耨??tile???貊?嚗?- [ ] ?仿?SpellRunner ??EnemyManager嚗摰喋?嗆???

---

### M-6 ??皜脫?嚗MC + ?箇?閬死嚗?
> ?格?嚗??隞交迤蝣粹＊蝷綽??衣??????釭

> ?? **M-6 ?????捱摰?Mesh Section 蝎漲**嚗? 
> ?? Chunk = 1 RMC Section? Grain 64+ 銝???舫? ~9,000 chunks ??9,000 Draw Calls ??Render Thread ???? 
> **閫??嚗ega-Chunk嚗??4?4 = 64糧嚗??箔???Section ??鈭文雿?*嚗?> - 4?4?4 ??16糧 chunk ?梁銝??RMC Section嚗?? Section ?詨??啣嗾??> - ?嗡葉隞颱?撠?chunk dirty ????Task Graph 鋆⊿?蝞??64糧 ??? Greedy Mesh ??銝甈⊥??Section
> - 甇文?瘜? Draw Call ?????詨?詨??????Grain 64+ 銝雁????> - M-6 ???臬? 1:1 撖虫?撽?甇?Ⅱ?改?**M-6 銝?????? Mega-Chunk 璅∪?**

- [x] 撱箇? `VoxelWorldRenderer` Actor嚗???RealtimeMeshComponent嚗?- [x] 蝘餅? Greedy Meshing ?摩嚗# ??C++嚗?蝞?銝?嚗?- [x] **Mega-Chunk嚗?4糧嚗???RMC Section ?桐?**嚗???1:1 chunk ??section
- [x] 撠?chunk dirty ????Task Graph 鋆⊿?蝞?撅?Mega-Chunk ??Greedy Mesh
- [x] ?箇??釭嚗???vertex color ???tile 憿?
- [ ] ?豢??批嚗?D 閬?嚗?曌?頧??脰憚蝮格嚗?
---

### M-7 ??摮?瑼?
> ?格?嚗??脰???+ 銝???隞交?銋?

> **瘛瑕??澆?瘙箇?嚗??券?? JSON嚗?*嚗?> - **Chunk 鞈?** ??蝝??脖? FArchive嚗???憭扼??嚗inary 敹?嚗?> - **??賜? / 閫鋆?** ??JSON嚗???撠?頛銝甈∴???瑞宏摰寞?嚗?> - ?券?嫣??脖?撠?spell 鞈??舫?摨血極蝔?~2,000 ??FBlockNode ??JSON 閫?? ??20ms嚗??脣???銝甈∴?摰銝?園
>
> ?? **Grain 64+ ??撖衣?豢 Chunk 銝脫?嚗???JSON**嚗?> - ?拙振蝘餃???脣閬???chunk 敹???background thread ???? + 撱?Greedy Mesh
> - 銝?其蜓?瑁?蝺? ??撱箇? `ChunkStreamingManager`嚗E5 `AsyncTask` 蝞∠?鞎痊 chunk 頛

- [x] `FlowSaveSystem.h/cpp`嚗Archive 鈭脖?嚗hunk嚗? JSON嚗haracter/spell嚗毽??- [x] `CharacterSaveData.h/cpp`嚗pellGroup?anaSlots?????JSON 摨???
- [x] `ChunkStreamingManager.h/cpp`嚗摰嗥宏?? async ???? chunk嚗UE::Tasks::Launch` ??`AsyncTask`嚗?- [x] Chunk 鈭脖?摮?嚗FArchive <<` ??摮??? TileCell ???嚗?- [x] ?????摮?嚗???Godot ?撽?鞈?甇?Ⅱ??
---

### M-8 ???箇? HUD嚗MG嚗?
> ?格?嚗? HUD ?臭誑?嚗??賜楊頛臬??蝪∠?

- [x] HP / MP 璇?1?? 璇???賜????Ｙ?嚗?- [x] ??賜?菜?嚗?0 ?潘?
- [x] ??賢?銵剁??舫????踝?
- [x] 蝪⊥?蝛蝺刻摩?剁??怎 ListView ?嚗??喳??賢?蝥?嚗?
---

### M-9 ??蝛蝺刻摩??UI嚗late嚗?
> ?格?嚗???ScriptCanvas ?嚗?璅??Godot ?

> **runtime / editor ??**嚗-1 ??`FBlockNode`嚗STRUCT + TUniquePtr 璅對??臬銵??嚗??湔?冽 Slate?-9 ??`UMyEdGraphNode : UEdGraphNode`嚗Object嚗C 蝞∠?嚗 editor 撠惇????交??賣???甈∟???`FBlockNode` ??`UMyEdGraphNode`嚗摮???頧??見 Slate ?臭誑??UObject ??DetailsView ?芸??? UPROPERTY ??UI ?批???銵?摰銝? GC 敶梢??
- [x] ?芾? `UMyEdGraphNode : UEdGraphNode`嚗???FBlockNode嚗???EBlockType ??UPROPERTY Params嚗?- [x] ?芾? `SGraphPanel`嚗??函撣?????詻??嚗?- [x] 隤輯?歹?蝛摨怠甈?????憿?
- [x] 蝛?∠?嚗＊蝷粹??憛???SpinBox / OptionButton嚗?- [x] ?脣?撽??摩嚗? M-1 鞈?撅斗窒?剁?
- [x] ??賜???嚗 ?蛛?5 蝯?

---

### M-10 ??GPU CA嚗ompute Shader嚗?璅?

> ?格?嚗rain 64+ 銝? CA 璅⊥??舀????
#### ?? ???塚?Hybrid嚗瑽?CPU authoritative + GPU simulation

**?詨?蝝?**嚗pellRunner VM?鈭?AI 撠楝?RaycastQuery`嚗??寞?瘝? tile???券??CPU ?瑁?嚗-2/M-4嚗???閬??胯?摮?隞暻?tile???單?蝑???
憒? GPU CA ?誨 CPU tile ???嚗PU 瘥活?亥岷?賡?閬?GPU Readback嚗? GPU fence嚗? pipeline stall ???菜?????賢?Ｕ?
**閫??嚗??胯PU ?誨 CPU????PU ??CPU??*嚗?
| 鞈? | 雿蔭 | 隤芣? |
|------|------|------|
| `Chunk3D` tile ??? | **CPU**嚗偶???? | AI / SpellRunner / Raycast ??authoritative source |
| GPU Tile Buffer | **GPU VRAM** | Compute Shader 璅⊥?函??∪?嚗?撟敺?CPU sync |
| Simulation 蝯? | GPU ??CPU嚗?*async嚗??郊**嚗?| CA 璅⊥摰?撌桃?潘?dirty set嚗甇亙神??CPU tile ??? |

**雿???嚗hase B嚗rain 32~64嚗?*嚗?- GPU ?芣芋?祉?閬死? gameplay 蝣唳???摮?CA嚗??扼?擃????恬?
- ?瑟? gameplay 蝣唳??′?豢憛?瘜亙???准???靘??CPU `Chunk3D`
- GPU simulation ???撖怠???閬箏惜 mesh 鞈???銝神??tile ???

**擃???嚗hase C嚗???**嚗?- ???CA 璅⊥蝘餃 GPU嚗??冽? chunk update ?望??恬??芣? dirty tile ?榆?圈??甇?readback ??CPU嚗??舀??buffer嚗?- SpellRunner VM?I 蝑?CPU 蝟餌絞霈?啁??胯?憭?1 撟?? tile ???acceptable latency嚗?- ?芯???AI ?陛?桃?撖阡??菜葫銋宏??Compute Shader嚗??賢祕?曄?甇???? GPU Gameplay 餈游???
**M-10 撖虫???**嚗?- [ ] 瘙箏? Phase B 蝑嚗?閬?CA ? or Hybrid readback嚗?????CPU tile ???
- [ ] 閮剛? GPU Tile Buffer ?澆?嚗? tile 2-4 byte ???嚗??CA 閬死撅?tile
- [ ] 撖虫?璉?潘?checkerboard嚗ompute Shader嚗圾瘙箔蒂銵神?亦奎?哨?
- [ ] RDG `FRDGBuilder` ?游?嚗?撟 dispatch嚗eadback ??dirty set嚗sync ?神
- [ ] Chunk ??鈭斗??摩嚗PU ??CPU ?郊嚗sync ping-pong buffer嚗?- [ ] ?皜祈岫嚗rain 64 ? 8 chunk radius 蝬剜? 60fps嚗PU AI/Raycast 撱園 ??1 frame

---

## ?准odot ???UE5 ?銝西?蝑

```
GitHub
??? main                    ??蝛拙????桀? Godot嚗???? feature/3d-cutover      ??Godot ??圈??潛?嚗?摮?雿?頛臬??改?
??? feature/ue5-migration   ??UE5 ?瑞宏嚗閮嚗?```

- Godot ?靽?嚗遙雿???賢?皛?- ?瑞宏??憒??啁??摩閮剛?嚗???Godot ?Ⅱ隤??宏璊 UE5
- M-1 ??M-5 摰?敺??脰?擐活?撠皜祈岫嚗??賣?曄????湛?

---

## 銝甈⊥蝺?
| ? | 隤芣? |
|------|------|
| ??蝟餌絞 | 靘陷 runtime skeletal mesh 霈耦嚗-6 銋?閰摯 |
| ??渡? | Chaos Physics ?游?嚗-5 銋?閮剛? |
| 閫? | ?憭撉券盲璅∪?嚗R 閮嚗???嚗????亙 |
| ?舀?/Replication | ?桀??⊥迨閮嚗E5 Replication Graph ?嗆?撌脤???|
| GPU CA | M-10嚗?閬?M-1~M-6 ?券蝛拙?敺???|
| 銴? MP 擃頂嚗-13嚗?| 鞈?撅方身閮?湔蝘餅?嚗祕雿?璈? W-6 ?詨? |

---

## ?怒??萄???皞?
| 鞈? | ?券?|
|------|------|
| [RealtimeMeshComponent](https://github.com/TriAxis-Games/RealtimeMeshComponent) | Tile 銝??? Mesh |
| [GPU Falling Sand嚗eatbatgames嚗(https://meatbatgames.com/blog/falling-sand-gpu/) | CA GPU ????|
| [UE5NiagaraComputeShaderIntegration](https://github.com/Shadertech/UE5NiagaraComputeShaderIntegration) | Compute Shader ?嗆???|
| [UMG-Slate Compendium](https://github.com/YawLighthouse/UMG-Slate-Compendium) | Slate UI 摰?? |
| [Lyra Starter Game](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine) | C++ Module 蝯??雿喟???|
| [Tom Looman UE5 C++ Guide](https://tomlooman.com/unreal-engine-cpp-guide/) | C++ ?仿??圈脤? |
| [Epic C++ Coding Standard](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) | ?賢?閬? |
| [UE5 Procedural Voxel Mesh ?飛](https://dev.epicgames.com/community/learning/tutorials/k8am/unreal-engine-procedural-voxel-mesh-generation) | Voxel Mesh 韏琿? |

---

## 銋祕雿?摨蜇閬?
```
M-0嚗憓???
  ??M-1嚗??惜嚗? M-2嚗M嚗? M-3嚗??芋??CPU嚗?                              ??                          M-4嚗????萎犖嚗? M-5嚗??賣?橘?
                                               ??                                           M-6嚗葡????M-7嚗?霈瑼???M-8嚗蝷?HUD嚗?                                                                            ??                                                                        M-9嚗??函楊頛臬 Slate嚗?                                                                            ??                                                                        M-10嚗PU CA嚗??
```

M-1 ??M-5 ?臭誑?冽???UE5 Editor 憿舐內??瘜?頝?葫閰佗?
M-6 銋???閬?????UE5 Editor ??葫閰艾?
