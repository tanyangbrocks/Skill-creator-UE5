# UE5 已完成里程碑（歸檔）

> 歸檔時間：2026-06-16（超過 8 筆觸發歸檔，保留最新 5 筆於 `實作進度.md`）

| 功能 | 關鍵檔案 | 摘要 |
|------|---------|------|
| M-5 SpellProjectile 元素命中 | `USpellCaster.cpp` | OnHitEnemy lambda 補 ApplyImmediate（元素 Aura）+ ApplyElementalImpact（命中 tile CA 反應）；Elem 由 FSpellArray.SpellElement 提供；VM SpellRunner ↔ TileWorld3D/EnemyManager 全 delegate 已在 TryCast/BeginPlay 完成接線 |
| M-5 Contact + ActionBus | `ActionBus.h/.cpp` `SpellArray.h` `ASkillCreatorCharacter.h/.cpp` `USpellCaster.h/.cpp` | FActionBus（DeltaTime 計時，pause-aware）：DamageShield/DeathGuard 攔截 TakeDirectDamage；EContainerType::Contact + FSpellArray.SpellElement；ExecuteContactHit（3D 前向掃描 MeleeRange=3、ApplyImmediate、BaseDamage、SpawnEffect→ApplyElementalImpact）；RegisterFilterFn 接通 ActionBus |
| M-5 CA 元素反應 | `MaterialRegistry.h/.cpp` `TileWorld3D.h/.cpp` | FMaterialData 加 NativeElement 欄位（17 種材質映射）；CheckElementalCaReactions（Water→Sand 0.5%、Lava+Water→Stone+Steam 3%）；ApplyElementalImpact（法術命中 API：沸騰/流沙/燃燒）|
| M-6 RMC 渲染 | `AVoxelWorldActor.h/.cpp` `GreedyMesher.h/.cpp` `VoxelWorld.Build.cs` | RealtimeMeshComponent Mega-Chunk 64³ / Greedy Meshing 6 方向 / PolyGroups |
| M-7 存讀檔 | `CharacterSaveData.h/.cpp` `FlowSaveSystem.h/.cpp` `ChunkStreamingManager.h/.cpp` | JSON 角色存檔 / Chunk binary 序列化 / 按需載入與卸載 |
| M-8 HUD | `UPlayerHUDWidget.h/.cpp` `ASkillCreatorHUD.h/.cpp` | HP/MP/Stamina/Mood 條 / 熱鍵欄 / Scroll 切換施法槽 |
| M-9 積木編輯器 | `UBlockEdGraphNode` `UBlockEdGraph` `UBlockEdGraphSchema` `SBlockEditorWidget` | Slate SGraphEditor / FBlockNode↔UEdGraphNode 雙向序列化 / Window 選單入口 |
| P1 MobSpawnController | `AMobSpawnController.h/.cpp` | Checkerboard ring spawn（Min=32 / Max=128 tile）；加權 MobTable；SoftDespawn 5%/s（Dist=192）+ HardDespawn（Dist=256）；MaxCommonActive=8；BaseInterval=8s |
| P1 Items 系統 | `ItemId.h` `ItemStack.h` `ItemDrop.h` `ItemData.h` `ItemRegistry.cpp` `EquipmentSlotType.h` `UInventoryComponent.h/.cpp` `UEquipmentComponent.h/.cpp` `ASkillCreatorCharacter.h/.cpp` | 28 個 EItemId；FItemRegistry（函數局部靜態 TArray）；UInventoryComponent（30 格，TryAdd 雙次搜尋，GetActiveToolTier）；UEquipmentComponent（Weapon/Armor/Accessory，TryEquip/Unequip）；Character constructor 注入兩組件 |
