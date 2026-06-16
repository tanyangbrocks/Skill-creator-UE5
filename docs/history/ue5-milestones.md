# UE5 已完成里程碑（歸檔）

> 歸檔時間：2026-06-16（超過 8 筆觸發歸檔，保留最新 5 筆於 `實作進度.md`）

| 功能 | 關鍵檔案 | 摘要 |
|------|---------|------|
| M-5 SpellProjectile 元素命中 | `USpellCaster.cpp` | OnHitEnemy lambda 補 ApplyImmediate（元素 Aura）+ ApplyElementalImpact（命中 tile CA 反應）；Elem 由 FSpellArray.SpellElement 提供；VM SpellRunner ↔ TileWorld3D/EnemyManager 全 delegate 已在 TryCast/BeginPlay 完成接線 |
| M-5 Contact + ActionBus | `ActionBus.h/.cpp` `SpellArray.h` `ASkillCreatorCharacter.h/.cpp` `USpellCaster.h/.cpp` | FActionBus（DeltaTime 計時，pause-aware）：DamageShield/DeathGuard 攔截 TakeDirectDamage；EContainerType::Contact + FSpellArray.SpellElement；ExecuteContactHit（3D 前向掃描 MeleeRange=3、ApplyImmediate、BaseDamage、SpawnEffect→ApplyElementalImpact）；RegisterFilterFn 接通 ActionBus |
| M-5 CA 元素反應 | `MaterialRegistry.h/.cpp` `TileWorld3D.h/.cpp` | FMaterialData 加 NativeElement 欄位（17 種材質映射）；CheckElementalCaReactions（Water→Sand 0.5%、Lava+Water→Stone+Steam 3%）；ApplyElementalImpact（法術命中 API：沸騰/流沙/燃燒）|
| M-6 RMC 渲染 | `AVoxelWorldActor.h/.cpp` `GreedyMesher.h/.cpp` `VoxelWorld.Build.cs` | RealtimeMeshComponent Mega-Chunk 64³ / Greedy Meshing 6 方向 / PolyGroups |
| M-7 存讀檔 | `CharacterSaveData.h/.cpp` `FlowSaveSystem.h/.cpp` `ChunkStreamingManager.h/.cpp` | JSON 角色存檔 / Chunk binary 序列化 / 按需載入與卸載 |
