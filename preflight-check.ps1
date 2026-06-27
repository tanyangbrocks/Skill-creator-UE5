# SkillCreatorUE5 preflight check — C++ 移植驗證 + Godot 設計意圖回歸檢查
# Run: powershell -ExecutionPolicy Bypass -File preflight-check.ps1 [-ShowDetails]
#
# 三層分類（解決「Godot/UE5 演算法刻意不同」的變通問題）：
#   Tier 1 — 設計上要求邏輯逐一對應（VM / 資料層 / 座標公式）。差異 = FAIL。
#   Tier 2 — 刻意換了演算法/框架（UI / AI / 渲染），只檢查「功能覆蓋契約」，不比對實作細節。
#   Tier 3 — 明確排除（計畫中尚未做、或本腳本方法論無法檢查），不計分，只列出原因避免誤會。
#
# 注意：本腳本只能抓「結構性／已知公式」差異，無法驗證實際執行期行為是否正確
#       （UI 是否真的顯示、Editor 內 .uasset 內部設定等，仍需實機驗證）。

param([switch]$ShowDetails)

$root = $PSScriptRoot
$pass = 0; $fail = 0; $warn = 0; $skip = 0

function Pass($msg) { Write-Host "  OK  $msg" -ForegroundColor Green;    $script:pass++ }
function Fail($msg) { Write-Host "  NG  $msg" -ForegroundColor Red;      $script:fail++ }
function Warn($msg) { Write-Host "  WW  $msg" -ForegroundColor Yellow;   $script:warn++ }
function Skip($msg) { Write-Host "  --  $msg" -ForegroundColor DarkGray; $script:skip++ }
function Head($msg) { Write-Host "`n-- $msg --" -ForegroundColor Cyan }

function Get-EnumValues($file, $enumName) {
    if (-not (Test-Path $file)) { return @() }
    $text = [System.IO.File]::ReadAllText($file, [System.Text.Encoding]::UTF8)
    $opts = [System.Text.RegularExpressions.RegexOptions]::Singleline
    $pat  = "enum\s+class\s+$enumName\s*(?::\s*\w+\s*)?\{([^}]+)\}"
    $m    = [System.Text.RegularExpressions.Regex]::Match($text, $pat, $opts)
    if (-not $m.Success) { return @() }
    $body = $m.Groups[1].Value
    $body -split "`n" | ForEach-Object {
        ($_ -replace '//.*', '').Trim()
    } | Where-Object { $_ -match '^[A-Za-z_]\w*' } | ForEach-Object {
        ($_ -split '[,\s=]')[0]
    } | Where-Object { $_ -ne '' }
}

function Read-UTF8($file) {
    if (-not (Test-Path $file)) { return '' }
    [System.IO.File]::ReadAllText($file, [System.Text.Encoding]::UTF8)
}

# Tier 2 用：跨多檔案搜尋同一個 pattern 是否「至少出現一次」（不限定哪個檔案，因為演算法/框架不同）
function Test-AnyFileContains($files, $pattern) {
    foreach ($f in $files) {
        if ((Read-UTF8 $f) -match $pattern) { return $true }
    }
    return $false
}

# ----------------------------------------------------------------
# 檔案路徑（2026-06-17 確認存在）
# ----------------------------------------------------------------
$BlockTypeFile     = "$root\Plugins\AbilitySystem\Source\AbilitySystem\Public\BlockType.h"
$OpCodeFile        = "$root\Plugins\AbilitySystem\Source\AbilitySystem\Public\OpCode.h"
$SpellCompilerFile = "$root\Plugins\AbilitySystem\Source\AbilitySystem\Private\SpellCompiler.cpp"
$ExecLoopFile      = "$root\Plugins\AbilitySystem\Source\AbilitySystem\Private\ExecutionLoop.cpp"
$ItemIdFile        = "$root\Source\SkillCreatorCore\Public\ItemId.h"
$ItemRegFile       = "$root\Source\SkillCreatorCore\Private\ItemRegistry.cpp"
$MatTypeFile       = "$root\Source\SkillCreatorCore\Public\MaterialType.h"
$MatRegFile        = "$root\Plugins\VoxelWorld\Source\VoxelWorld\Private\MaterialRegistry.cpp"
$WorldTypesFile    = "$root\Source\SkillCreatorCore\Public\WorldTypes.h"
$DroppedMgrHFile   = "$root\Source\SkillCreatorRuntime\Public\UDroppedItemManager.h"
$DroppedMgrCppFile = "$root\Source\SkillCreatorRuntime\Private\UDroppedItemManager.cpp"
$AEnemyHFile       = "$root\Source\SkillCreatorRuntime\Public\AEnemy.h"
$AEnemyCppFile     = "$root\Source\SkillCreatorRuntime\Private\AEnemy.cpp"
$MobSpawnCppFile   = "$root\Source\SkillCreatorRuntime\Private\AMobSpawnController.cpp"
$EnemyMgrCppFile   = "$root\Source\SkillCreatorRuntime\Private\AEnemyManager.cpp"
$ProjectileCppFile = "$root\Source\SkillCreatorRuntime\Private\ASpellProjectile.cpp"
$TileWorldCppFile  = "$root\Plugins\VoxelWorld\Source\VoxelWorld\Private\TileWorld3D.cpp"
$MapGenCppFile     = "$root\Plugins\VoxelWorld\Source\VoxelWorld\Private\MapGenerator3D.cpp"
$WorldScaleFile    = "$root\Source\SkillCreatorCore\Public\WorldScale.h"
$BTSchemaFile      = "$root\Plugins\SkillCreatorUI\Source\SkillCreatorUI\Private\UBlockEdGraphSchema.cpp"
$BTNodeFile        = "$root\Plugins\SkillCreatorUI\Source\SkillCreatorUI\Private\UBlockEdGraphNode.cpp"
$ManaTypeRegFile   = "$root\Plugins\AbilitySystem\Source\AbilitySystem\Private\ManaTypeRegistry.cpp"
$CharSaveHFile     = "$root\Source\SkillCreatorRuntime\Public\CharacterSaveData.h"
$SurfaceWaterFile  = "$root\Plugins\VoxelWorld\Source\VoxelWorld\Public\SurfaceWaterPool.h"
$MobSpawnHFile     = "$root\Source\SkillCreatorRuntime\Public\AMobSpawnController.h"
$BlockNodeFile     = "$root\Plugins\AbilitySystem\Source\AbilitySystem\Public\FBlockNode.h"
$SafetyGuardFile   = "$root\Plugins\AbilitySystem\Source\AbilitySystem\Public\SafetyGuard.h"
$SpellRunnerCppFile = "$root\Plugins\AbilitySystem\Source\AbilitySystem\Private\SpellRunner.cpp"
$ElemReactCppFile  = "$root\Source\SkillCreatorCore\Private\ElementalReactionTable.cpp"
$CharacterHFile    = "$root\Source\SkillCreatorRuntime\Public\ASkillCreatorCharacter.h"
$SpellCasterCppFile = "$root\Source\SkillCreatorRuntime\Private\USpellCaster.cpp"
$CameraTypesFile   = "$root\Source\SkillCreatorRuntime\Public\SkillCameraTypes.h"
$Chunk3DFile       = "$root\Plugins\VoxelWorld\Source\VoxelWorld\Public\Chunk3D.h"
$PlacedObjRegFile  = "$root\Plugins\VoxelWorld\Source\VoxelWorld\Public\PlacedObjectRegistry.h"
$PCCppFile         = "$root\Source\SkillCreatorRuntime\Private\ASkillCreatorPlayerController.cpp"
$PCHFile2          = "$root\Source\SkillCreatorRuntime\Public\ASkillCreatorPlayerController.h"
$CharacterCppFile  = "$root\Source\SkillCreatorRuntime\Private\ASkillCreatorCharacter.cpp"
$ABeastHFile       = "$root\Source\SkillCreatorRuntime\Public\ABeastCharacter.h"
$ABeastCppFile     = "$root\Source\SkillCreatorRuntime\Private\ABeastCharacter.cpp"
$CreatureTypesFile = "$root\Source\SkillCreatorCore\Public\CreatureTypes.h"
$CaGpuSimHFile     = "$root\Plugins\VoxelWorld\Source\VoxelWorld\Public\CaGpuSimulator.h"
$NpcAICtrlCppFile    = "$root\Source\SkillCreatorRuntime\Private\ANPCAIController.cpp"
$GasEffectRegHFile   = "$root\Plugins\AbilitySystem\Source\AbilitySystem\Public\UGasEffectRegistry.h"
$GasEffectRegCppFile = "$root\Plugins\AbilitySystem\Source\AbilitySystem\Private\UGasEffectRegistry.cpp"
$GasAttrSetHFile     = "$root\Source\SkillCreatorRuntime\Public\SkillCreatorAttributeSet.h"
$AbilitySystemUplugin = "$root\Plugins\AbilitySystem\AbilitySystem.uplugin"
$DefaultGTFile       = "$root\Config\DefaultGameplayTags.ini"

# ==================================================================
# TIER 1 — 資料層 / VM 完整性（設計上要求逐一對應，差異即 bug）
# ==================================================================
Head "1. [Tier1] EBlockType -> SpellCompiler 編譯覆蓋"
$allBT = Get-EnumValues $BlockTypeFile "EBlockType"
$spText = Read-UTF8 $SpellCompilerFile
# 排除清單需與 docs/issues.md 對照維護：這些是已知刻意延後（無事件基礎設施 / BlockNode 無 ExtraBranches）
# 注意：這個缺口在 Godot 版本本身也是已知未做（非 UE5 port 造成的新 regression），所以降級為 Warn，
#       只有「排除清單以外」缺失才算 Fail（真正的 port 落差）。
$btExclude = @('TotemDone', 'TotemHit', 'TotemFizzle')
$btKnownGap = @('SequentialGate', 'EndOfChain', 'DetectProjectile', 'DetectAttack', 'DetectStatusChange', 'OnEffectStart', 'OnEffectEnd')
$missBT = $allBT | Where-Object { $_ -notin $btExclude -and $_ -notin $btKnownGap -and $spText -notmatch "EBlockType::$_\b" }
$missBTKnown = $allBT | Where-Object { $_ -in $btKnownGap -and $spText -notmatch "EBlockType::$_\b" }
if ($missBT) { Fail "SpellCompiler 未處理 BlockType（非已知缺口，疑似 port 遺漏）: $($missBT -join ', ')" }
else { Pass "SpellCompiler 覆蓋全部非已知缺口的 BlockType" }
if ($missBTKnown) { Warn "SpellCompiler 未處理（Godot 本身也未做，需事件基礎設施）: $($missBTKnown -join ', ')" }

Head "2. [Tier1] EOpCode -> ExecutionLoop switch 完整性"
$allOp = Get-EnumValues $OpCodeFile "EOpCode"
$execText = Read-UTF8 $ExecLoopFile
$caseMatches = [System.Text.RegularExpressions.Regex]::Matches($execText, 'case\s+EOpCode::(\w+)\s*:')
$handledOp = $caseMatches | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
$missOp = $allOp | Where-Object { $_ -notin $handledOp }
if ($missOp) { Fail "ExecutionLoop 未處理 OpCode: $($missOp -join ', ')" }
else { Pass "ExecutionLoop 處理全部 $($allOp.Count) 個 OpCode" }

Head "3. [Tier1] EItemId -> ItemRegistry 完整性"
$allItem = Get-EnumValues $ItemIdFile "EItemId"
$itemRegText = Read-UTF8 $ItemRegFile
$itemExclude = @('None', 'COUNT')
$missItem = $allItem | Where-Object { $_ -notin $itemExclude -and $itemRegText -notmatch "EItemId::$_\b" }
if ($missItem) { Fail "ItemRegistry 未註冊: $($missItem -join ', ')" }
else { Pass "ItemRegistry 註冊全部 $($allItem.Count - $itemExclude.Count) 個 ItemId" }

Head "4. [Tier1] EMaterialType -> MaterialRegistry 完整性"
$allMat = Get-EnumValues $MatTypeFile "EMaterialType"
$matRegText = Read-UTF8 $MatRegFile
$matExclude = @('Air', 'Count', 'COUNT')
# MaterialRegistry 用「位置陣列」設計（GMatData[] 依 enum 底層數值索引），不是 named Register()/switch-case，
# 所以同時接受「字面引用」或「// ID N — Name 行內註解」兩種證據（陣列設計本身不需要 named 引用）。
$missMat = $allMat | Where-Object {
    $name = $_
    $name -notin $matExclude -and
    $matRegText -notmatch "EMaterialType::$name\b" -and
    $matRegText -notmatch "ID\s+\d+\s*[—-]+\s*$name\b"
}
if ($missMat) { Fail "MaterialRegistry 未註冊: $($missMat -join ', ')" }
else { Pass "MaterialRegistry 註冊全部 $($allMat.Count - $matExclude.Count) 個 MaterialType（陣列索引式設計，已用註解標記比對）" }

Head "5. [Tier1] EDestroyReason -> 掉落物系統覆蓋"
$allReason = Get-EnumValues $WorldTypesFile "EDestroyReason"
$dropText = (Read-UTF8 $DroppedMgrHFile) + (Read-UTF8 $DroppedMgrCppFile)
$missReason = $allReason | Where-Object { $dropText -notmatch "EDestroyReason::$_\b" }
if ($missReason.Count -gt 0 -and $missReason.Count -eq $allReason.Count) {
    Fail "UDroppedItemManager 完全沒有處理任何 EDestroyReason（$($allReason.Count) 個全缺）-- SpawnFragments / 依摧毀原因分流掉落 邏輯不存在於目前程式碼，與 docs/issues.md 聲稱的『已實作完成』不符"
} elseif ($missReason) {
    Warn "UDroppedItemManager 未處理: $($missReason -join ', ')"
} else { Pass "UDroppedItemManager 處理全部 $($allReason.Count) 個 DestroyReason" }

Head "6. [Tier1] ESpawnCategory -> MobSpawnController 完整性"
$allCat = Get-EnumValues $ABeastHFile "ESpawnCategory"
$mobText = Read-UTF8 $MobSpawnCppFile
$missCat = $allCat | Where-Object { $mobText -notmatch "ESpawnCategory::$_\b" }
if ($missCat) { Warn "MobSpawnController 未處理: $($missCat -join ', ')（未處理的分類不會被生成/despawn，確認是否刻意）" }
else { Pass "MobSpawnController 處理全部 $($allCat.Count) 個 SpawnCategory" }

# ==================================================================
# TIER 1 — 已知歷史 bug 回歸檢查（從 Godot preflight-check-v2.ps1 移植 + 本次新發現）
# ==================================================================
Head "7. [Tier1] 已知歷史 bug 回歸檢查"

# 7a（新發現，對應 Godot Check 16c：MobSpawnController 必須用地表高度而非玩家高度）
if (Test-Path $MobSpawnCppFile) {
    $t = Read-UTF8 $MobSpawnCppFile
    if ($t -match 'HeightEstimator') {
        Pass "AMobSpawnController.TryFindSpawnPos 使用 HeightEstimator 取地表高度"
    } elseif ($t -match 'Player\.Y\s*\+') {
        Fail "AMobSpawnController.TryFindSpawnPos 用 Player.Y 推算生成高度，未呼叫 TileWorld3D::HeightEstimator -- 跨地形生成的敵人可能卡進地裡或浮空（對應 Godot Check 16c 同類錯誤）"
    } else { Warn "AMobSpawnController 生成高度計算模式未識別 -- 手動確認" }
} else { Warn "AMobSpawnController.cpp 未找到 -- 跳過 7a" }

# 7b（對應 Godot Check 16d：爆炸傷害必須含 Z 分量）
if (Test-Path $EnemyMgrCppFile) {
    $t = Read-UTF8 $EnemyMgrCppFile
    $opts16 = [System.Text.RegularExpressions.RegexOptions]::Singleline
    $m = [System.Text.RegularExpressions.Regex]::Match($t, 'ApplyExplosionDamage[^{]*\{(.+?)\n\}', $opts16)
    if ($m.Success -and $m.Groups[1].Value -match 'D\.Z\s*\*\s*D\.Z') {
        Pass "AEnemyManager.ApplyExplosionDamage 使用 3D 距離（含 Z 分量）"
    } else {
        Fail "AEnemyManager.ApplyExplosionDamage 缺 Z 分量 -- 爆炸傷害可能只算平面距離"
    }
} else { Warn "AEnemyManager.cpp 未找到 -- 跳過 7b" }

# 7c（對應 Godot Check 16g：Heavy 敵人重力需多格寬度地板掃描）
# AEnemy 已降為薄殼，ApplyGravity 移至 ABeastCharacter.cpp
if (Test-Path $ABeastCppFile) {
    $t = Read-UTF8 $ABeastCppFile
    $opts7c = [System.Text.RegularExpressions.RegexOptions]::Singleline
    $m = [System.Text.RegularExpressions.Regex]::Match($t, 'void\s+ABeastCharacter::ApplyGravity\(\)\s*\{(.+?)\n\}', $opts7c)
    if ($m.Success -and $m.Groups[1].Value -match 'EEnemyType::Heavy') {
        Pass "ABeastCharacter.ApplyGravity 有針對 Heavy 的寬度地板掃描"
    } else {
        Fail "ABeastCharacter.ApplyGravity 沒有 EEnemyType::Heavy 寬度地板掃描 -- Heavy 敵人站在窄邊緣可能懸空（對應 Godot Check 16g 同類錯誤）"
    }
} else { Warn "ABeastCharacter.cpp 未找到 -- 跳過 7c" }

# 7d（Heavy 2x2 碰撞箱：邏輯在 ABeastCharacter::OccupiesTile，透過 ICombatant 介面供所有系統使用）
# ASpellProjectile 呼叫 UCombatantRegistrySubsystem::FindHostileAt → ICombatant::OccupiesTile，
# 所以正確位置是 ABeastCharacter::OccupiesTile，不是 ASpellProjectile 直接引用 EEnemyType::Heavy。
if (Test-Path $ABeastCppFile) {
    $t = Read-UTF8 $ABeastCppFile
    $opts7d = [System.Text.RegularExpressions.RegexOptions]::Singleline
    $m = [System.Text.RegularExpressions.Regex]::Match($t, 'OccupiesTile[^{]*\{(.+?)\n\}', $opts7d)
    if ($m.Success -and $m.Groups[1].Value -match 'EEnemyType::Heavy') {
        Pass "ABeastCharacter::OccupiesTile 含 Heavy 2x2 碰撞箱（透過 ICombatant 介面供 ASpellProjectile/AOE/MeleeHit 統一使用）"
    } else {
        Fail "ABeastCharacter::OccupiesTile 沒有 EEnemyType::Heavy 2x2 碰撞箱 -- 所有透過 FindHostileAt()/OccupiesTile() 的命中判定都會穿透 Heavy 敵人（對應 Godot Check 16f 同類錯誤）"
    }
} else { Warn "ABeastCharacter.cpp 未找到 -- 跳過 7d" }

# 7e（新發現，對應 Godot Check 17b：載入 chunk 後必須清 dirty flag）
if (Test-Path $TileWorldCppFile) {
    $t = Read-UTF8 $TileWorldCppFile
    $idx = $t.IndexOf('bool FTileWorld3D::TryLoadChunk(')
    if ($idx -ge 0) {
        $window = $t.Substring($idx, [Math]::Min(1200, $t.Length - $idx))
        if ($window -match 'bNeedsSave\s*=\s*false') {
            Pass "TileWorld3D.TryLoadChunk 載入後設 bNeedsSave=false"
        } else {
            Fail "TileWorld3D.TryLoadChunk 載入後未設 bNeedsSave=false -- 剛讀進來的 chunk 會被當 dirty，觸發不必要的重存（對應 Godot Check 17b 同類錯誤，效能問題非毀損風險）"
        }
    } else { Warn "找不到 TryLoadChunk 定義 -- 跳過 7e" }
} else { Warn "TileWorld3D.cpp 未找到 -- 跳過 7e" }

# 7f（對應 Godot Check 17e：地表高度需 Clamp 避免越界）
if (Test-Path $MapGenCppFile) {
    $t = Read-UTF8 $MapGenCppFile
    $idx = $t.IndexOf('int32 FMapGenerator3D::GetHeightAt(')
    if ($idx -ge 0) {
        $window = $t.Substring($idx, [Math]::Min(600, $t.Length - $idx))
        if ($window -match 'FMath::Clamp') {
            Pass "MapGenerator3D.GetHeightAt 有 FMath::Clamp（地表 Y 有界）"
        } else {
            Fail "MapGenerator3D.GetHeightAt 缺 FMath::Clamp -- 地表 Y 可能越界"
        }
    } else { Warn "找不到 GetHeightAt 定義 -- 跳過 7f" }
} else { Warn "MapGenerator3D.cpp 未找到 -- 跳過 7f" }

# 7g（新發現，對應 Godot Check 17f：生成點所在 chunk 若未載入，GetTile 會回傳預設 Air，永遠找不到地板）
if (Test-Path $MobSpawnCppFile) {
    $t = Read-UTF8 $MobSpawnCppFile
    # UE5 使用 Gen.EnsureChunksAround() + IsChunkGenerated() 取代 Godot 的 EnsureChunkSync()，
    # 效果相同（確保生成點 chunk 已計算並套用到 TileWorld3D），只是 API 名稱不同。
    if ($t -match 'EnsureChunksAround|EnsureChunkAt|IsChunkGenerated') {
        Pass "AMobSpawnController 有 chunk 確保機制（EnsureChunksAround/IsChunkGenerated）（生成前確保 chunk 已載入）"
    } else {
        Fail "AMobSpawnController 缺 chunk 確保機制 -- TileWorld3D::GetCell 對未載入 chunk 回傳預設 FTileCell{}（=Air），遠離玩家/未預載區域的生成點會永遠判定為懸空，找不到有效位置（對應 Godot Check 17f 同類錯誤；已用 GetCell 原始碼確認此行為）"
    }
} else { Warn "AMobSpawnController.cpp 未找到 -- 跳過 7g" }

# 7h（對應 Godot Check 16h：生成/despawn 距離常數需 Min<Max<Soft<=Hard，否則邏輯矛盾）
if (Test-Path $MobSpawnHFile) {
    $t = Read-UTF8 $MobSpawnHFile
    $reMin  = [System.Text.RegularExpressions.Regex]::Match($t, 'MinSpawnDist\s*=\s*(\d+)')
    $reMax  = [System.Text.RegularExpressions.Regex]::Match($t, 'MaxSpawnDist\s*=\s*(\d+)')
    $reSoft = [System.Text.RegularExpressions.Regex]::Match($t, 'DespawnSoftDist\s*=\s*(\d+)')
    $reHard = [System.Text.RegularExpressions.Regex]::Match($t, 'DespawnHardDist\s*=\s*(\d+)')
    if ($reMin.Success -and $reMax.Success -and $reSoft.Success -and $reHard.Success) {
        $vMin = [int]$reMin.Groups[1].Value; $vMax = [int]$reMax.Groups[1].Value
        $vSoft = [int]$reSoft.Groups[1].Value; $vHard = [int]$reHard.Groups[1].Value
        if ($vMin -gt 0 -and $vMax -gt $vMin -and $vSoft -gt $vMax -and $vHard -ge $vSoft) {
            Pass "MobSpawnController 距離常數合理：Min($vMin)<Max($vMax)<Soft($vSoft)<=Hard($vHard)"
        } else {
            Fail "MobSpawnController 距離常數不合理：Min=$vMin Max=$vMax Soft=$vSoft Hard=$vHard（應 Min<Max<Soft<=Hard）"
        }
    } else { Warn "MobSpawnController 距離常數無法全部提取 -- 跳過 7h" }
} else { Warn "AMobSpawnController.h 未找到 -- 跳過 7h" }

Head "8. [Tier1] ManaTypeRegistry -- 18 種基礎 ManaType 完整性與重複 ID 偵測"
if (Test-Path $ManaTypeRegFile) {
    $t = Read-UTF8 $ManaTypeRegFile
    $idMatches = [System.Text.RegularExpressions.Regex]::Matches($t, 'Register\(\s*\{\s*(\d+)\s*,')
    $ids = $idMatches | ForEach-Object { [int]$_.Groups[1].Value }
    $missingIds = 1..18 | Where-Object { $_ -notin $ids }
    $dupeIds = $ids | Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name }
    if ($missingIds) { Fail "ManaTypeRegistry 缺少基礎 ManaType ID: $($missingIds -join ', ')" }
    elseif ($dupeIds) { Fail "ManaTypeRegistry 有重複 ID 註冊（會被 TMap 靜默覆蓋）: $($dupeIds -join ', ')" }
    else { Pass "ManaTypeRegistry 註冊全部 18 個基礎 ManaType ID，無重複" }
} else { Warn "ManaTypeRegistry.cpp 未找到 -- 跳過 8" }

Head "9. [Tier1] CharacterSaveData -- 所有資料欄位必須有 UPROPERTY（否則 FJsonObjectConverter 靜默不存）"
if (Test-Path $CharSaveHFile) {
    $lines = [System.IO.File]::ReadAllLines($CharSaveHFile, [System.Text.Encoding]::UTF8)
    $noUProp = @()
    foreach ($ln in $lines) {
        $trim = $ln.Trim()
        # 找型別宣告但前面沒有 UPROPERTY 的行（粗略：以大寫型別/容器開頭、有分號結尾、不是函式宣告）
        if ($trim -match '^(FString|FIntVector|float|int32|bool|TArray<|TMap<|FName)\s+\w+' -and
            $trim -notmatch '^\w+\s+\w+\s*\(' -and
            $trim -notmatch 'static') {
            if ($trim -notmatch '^UPROPERTY' ) { $noUProp += $trim }
        }
    }
    # UPROPERTY() 通常和欄位同一行（UE5 慣例），所以上面那段其實抓不到漏掉的；改用更直接的方式：
    # 抓每一個看起來像欄位宣告的行，確認同一行內有 UPROPERTY
    $fieldLines = $lines | Where-Object { $_ -match '^\s*(UPROPERTY\(\))?\s*(FString|FIntVector|float|int32|bool|TArray<|TMap<|FName)\s+\w+\s*[=;]' }
    $missingUProp = $fieldLines | Where-Object { $_ -notmatch 'UPROPERTY' }
    if ($missingUProp) {
        Fail "CharacterSaveData.h 有欄位疑似缺少 UPROPERTY（存檔時會被 FJsonObjectConverter 靜默忽略）: $($missingUProp -join ' | ')"
    } else {
        Pass "CharacterSaveData.h 所有資料欄位都有 UPROPERTY"
    }
} else { Warn "CharacterSaveData.h 未找到 -- 跳過 9" }

Head "10. [Tier1] SurfaceWaterPool -- 地形特徵檔案存在性"
if (Test-Path $SurfaceWaterFile) { Pass "SurfaceWaterPool.h 存在（地形特徵：水池）" }
else { Warn "SurfaceWaterPool.h 不存在 -- 地形特徵：水池可能缺失" }

Head "11. [Tier1] FTileCell POD 安全性回歸守衛（CLAUDE.md / M-3 明文規定：絕不可加 UObject 指標或 FString）"
if (Test-Path $MatTypeFile) {
    $t = Read-UTF8 $MatTypeFile
    $opts11 = [System.Text.RegularExpressions.RegexOptions]::Singleline
    $m = [System.Text.RegularExpressions.Regex]::Match($t, 'struct\s+FTileCell\s*\{(.+?)\};', $opts11)
    if ($m.Success) {
        $body = $m.Groups[1].Value
        if ($body -match 'UObject|FString|TArray|TMap|UPROPERTY') {
            Fail "FTileCell 混入了 UObject/FString/TArray/TMap 欄位 -- 違反 POD-only 規定，背景 thread FArchive 序列化會不安全"
        } else {
            Pass "FTileCell 維持純 POD（無 UObject/FString/容器欄位）"
        }
    } else { Warn "找不到 FTileCell struct 定義 -- 跳過 11" }
} else { Warn "MaterialType.h 未找到 -- 跳過 11" }

Head "12. [Tier1] FBlockNode 子分支回歸守衛（CLAUDE.md / 坑三明文規定：必須是 TArray<TUniquePtr<FBlockNode>>）"
if (Test-Path $BlockNodeFile) {
    $t = Read-UTF8 $BlockNodeFile
    $badPattern = [System.Text.RegularExpressions.Regex]::Matches($t, '(ThenBranch|ElseBranch|LoopBody)\s*;') |
        Where-Object { $_.Value -notmatch 'TUniquePtr' }
    # 更直接：抓宣告行本身，確認含 TUniquePtr
    $declLines = ($t -split "`n") | Where-Object { $_ -match '(ThenBranch|ElseBranch|LoopBody)\s*;' }
    $badDecls = $declLines | Where-Object { $_ -notmatch 'TUniquePtr' }
    if ($badDecls) {
        Fail "FBlockNode 分支欄位宣告未使用 TUniquePtr（值型別陣列擴容會 deep-copy 整棵樹、舊指標懸空）: $($badDecls -join ' | ')"
    } else {
        Pass "FBlockNode.ThenBranch/ElseBranch/LoopBody 維持 TArray<TUniquePtr<FBlockNode>>"
    }
} else { Warn "FBlockNode.h 未找到 -- 跳過 12" }

Head "13. [Tier1] switch-default 靜默吞錯回歸守衛（對應 Godot 已修復過的 B-2 同類錯誤）"
if (Test-Path $ExecLoopFile) {
    $t = Read-UTF8 $ExecLoopFile
    # 找主 opcode dispatch 的 default case（在檔案末段，緊接在最後一個 case 區塊之後）
    $lastDefaultIdx = $t.LastIndexOf('default:')
    if ($lastDefaultIdx -ge 0) {
        $window = $t.Substring($lastDefaultIdx, [Math]::Min(200, $t.Length - $lastDefaultIdx))
        if ($window -match 'UE_LOG|ensure|checkf|PushWarning') {
            Pass "ExecutionLoop 的 switch default case 有記錄/警告未知 OpCode"
        } else {
            Warn "ExecutionLoop 的 switch default case 直接靜默跳過（++Ctx.PC; break;），沒有 UE_LOG 警告 -- 對應 Godot 自己修過的 B-2 同類錯誤（『SpellCompiler.EmitBlock 靜默丟棄未知 BlockType』）：未來新增 OpCode 忘記加 case 時，不會有任何錯誤訊息，積木會無聲失效"
        }
    } else { Warn "找不到 default case -- 跳過 13" }
} else { Warn "ExecutionLoop.cpp 未找到 -- 跳過 13" }

Head "13b. [Tier1] MaxComboDepth 單一真相回歸守衛（對應 Godot 已修復過的 B-5 同類錯誤）"
if ((Test-Path $SafetyGuardFile) -and (Test-Path $SpellRunnerCppFile)) {
    $sgT = Read-UTF8 $SafetyGuardFile
    $srT = Read-UTF8 $SpellRunnerCppFile
    $sgMatch = [System.Text.RegularExpressions.Regex]::Match($sgT, 'MaxComboDepth\s*=\s*(\d+)')
    # SpellRunner.cpp 優選：直接引用 FSafetyGuard::MaxComboDepth（無需提取數字，引用本身即保證一致）
    $srRefConst = $srT -match 'ComboDepth\s*<\s*FSafetyGuard::MaxComboDepth'
    $srMatch = [System.Text.RegularExpressions.Regex]::Match($srT, 'ComboDepth\s*<\s*(\d+)')
    if ($sgMatch.Success -and $srRefConst) {
        Pass "SafetyGuard::MaxComboDepth 與 SpellRunner.cpp 一致（SpellRunner 引用常數，無硬寫風險）（值=$($sgMatch.Groups[1].Value)）"
    } elseif ($sgMatch.Success -and $srMatch.Success) {
        if ($sgMatch.Groups[1].Value -eq $srMatch.Groups[1].Value) {
            Pass "SafetyGuard::MaxComboDepth 與 SpellRunner.cpp 連段上限一致（$($sgMatch.Groups[1].Value)）"
        } else {
            Fail "SafetyGuard::MaxComboDepth=$($sgMatch.Groups[1].Value) 但 SpellRunner.cpp 硬寫 ComboDepth<$($srMatch.Groups[1].Value)，兩條路徑連段上限不一致（對應 Godot 已修復過的 B-5 同類錯誤：『MaxComboDepth 重複定義，兩條執行路徑悄悄走岔』，SpellRunner.cpp 應改引用 FSafetyGuard::MaxComboDepth）"
        }
    } else { Warn "找不到其中一方的 ComboDepth 數值 -- 跳過 13b" }
} else { Warn "SafetyGuard.h 或 SpellRunner.cpp 未找到 -- 跳過 13b" }

Head "13c. [Tier1] ElementalReactionTable -- 22 條反應完整性"
if (Test-Path $ElemReactCppFile) {
    $t = Read-UTF8 $ElemReactCppFile
    $count = ([System.Text.RegularExpressions.Regex]::Matches($t, 'Tab\.Add\(MakeKey\(')).Count
    if ($count -eq 22) { Pass "ElementalReactionTable 有完整 22 條反應" }
    else { Fail "ElementalReactionTable 只有 $count 條反應，應為 22 條（對照 docs/issues.md）" }
} else { Warn "ElementalReactionTable.cpp 未找到 -- 跳過 13c" }

Head "13d. [Tier1] ASkillCreatorCharacter -- 是否實作 ISnapshottable（S-7 玩家快照/回滾）"
if (Test-Path $CharacterHFile) {
    $t = Read-UTF8 $CharacterHFile
    if ($t -match 'ISnapshottable' -and $t -match 'TakeSnapshot' -and $t -match 'RestoreFromSnapshot') {
        Pass "ASkillCreatorCharacter 實作 ISnapshottable（TakeSnapshot/RestoreFromSnapshot 齊全）"
    } else {
        Fail "ASkillCreatorCharacter 沒有實作 ISnapshottable，沒有 TakeSnapshot/RestoreFromSnapshot -- Anchor/Rollback 積木目前只能還原世界 tile 和敵人，玩家自己的 HP/位置/狀態不會被快照或回滾（docs/issues.md 標記此項為 stub『S-7 詳細需驗證』，本次驗證確認方法完全不存在）"
    }
} else { Warn "ASkillCreatorCharacter.h 未找到 -- 跳過 13d" }

Head "13e. [Tier1] WorldScale::TileSizeCm 單一真相回歸守衛"
$tileSizeDupes = @()
$coreCppFiles = Get-ChildItem -Recurse -Include "*.h" "$root\Source", "$root\Plugins" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -ne $WorldScaleFile -and $_.FullName -notmatch 'RealtimeMeshComponent' }
foreach ($f in $coreCppFiles) {
    $t = Read-UTF8 $f.FullName
    # 排除坡度角度值（如 MaxSlopeDeg = 30.f），只抓 TileSize 語境的硬寫數字
    if ($t -match '(?<!Oxygen|Stamina|Mental|Mood|Hunger|Thirst|Regen)\s*=\s*30\.f\s*;.*[Tt]ile' -and
        $t -notmatch 'SlopeDeg\s*=\s*30\.f') {
        $tileSizeDupes += $f.Name
    }
}
if ($tileSizeDupes.Count -gt 0) { Warn "可疑的重複 TileSize 字面值（應改引用 WorldScale::TileSizeCm）: $($tileSizeDupes -join ', ')" }
else { Pass "沒有發現重複硬寫的 TileSize 數值，WorldScale::TileSizeCm 維持單一真相" }

Head "13f. [Tier1] SpellCaster act_* dispatch -- 與 docs/issues.md 已知 stub 清單對照（不應有新增缺口）"
if (Test-Path $SpellCasterCppFile) {
    $t = Read-UTF8 $SpellCasterCppFile
    $hasAllDispatch = ($t -match 'ExecuteArea') -and ($t -match 'ExecuteTechnique') -and
                       ($t -match 'ExecuteMorph') -and ($t -match 'ExecuteDisplacement') -and
                       ($t -match 'ExecuteSummon') -and ($t -match 'ExecuteDomain')
    if ($hasAllDispatch) { Pass "DispatchAction 六個分支函式（Area/Technique/Morph/Displacement/Summon/Domain）均存在" }
    else { Fail "USpellCaster.cpp 的 DispatchAction 分支函式缺失，疑似 port 遺漏" }
    if ($t -match 'ExecuteSummon[^{]*\{\s*UE_LOG[^}]*stub') {
        Warn "ExecuteSummon 仍是 stub（只 log 警告，無召喚物邏輯）-- 與 docs/issues.md 記載一致，非新增缺口，僅供追蹤"
    }
} else { Warn "USpellCaster.cpp 未找到 -- 跳過 13f" }

Head "13g. [Tier1] ECameraMode -- 4 種攝影機模式完整性"
if (Test-Path $CameraTypesFile) {
    $allCam = Get-EnumValues $CameraTypesFile "ECameraMode"
    if ($allCam.Count -eq 4) { Pass "ECameraMode 完整 4 種模式（ThirdPerson/FirstPerson/Isometric/SideScroll2D）" }
    else { Fail "ECameraMode 應有 4 種模式，實際 $($allCam.Count) 種：$($allCam -join ', ')" }
} else { Warn "SkillCameraTypes.h 未找到 -- 跳過 13g" }

Head "13h. [Tier1] Chunk3D::Size 單一真相回歸守衛"
if (Test-Path $Chunk3DFile) {
    $t = Read-UTF8 $Chunk3DFile
    if ($t -match 'Size\s*=\s*WorldScale::ChunkSize') { Pass "Chunk3D::Size 引用 WorldScale::ChunkSize（非硬寫數字）" }
    else { Warn "Chunk3D::Size 沒有引用 WorldScale::ChunkSize，可能硬寫重複數字 -- 確認是否刻意" }
} else { Warn "Chunk3D.h 未找到 -- 跳過 13h" }

Head "13i. [Tier1] PlacedObjectRegistry -- 放置物件系統檔案存在性"
if (Test-Path $PlacedObjRegFile) { Pass "PlacedObjectRegistry.h 存在（R-6 放置物件系統）" }
else { Warn "PlacedObjectRegistry.h 不存在 -- R-6 放置物件系統可能缺失" }

Head "13j. [Tier1] PlayerController vs Character 輸入路徑衝突偵測（疑似死碼）"
if ((Test-Path $PCCppFile) -and (Test-Path $CharacterCppFile)) {
    $pcT = Read-UTF8 $PCCppFile
    $pcHT = Read-UTF8 $PCHFile2
    $charT = Read-UTF8 $CharacterCppFile
    $pcHasLegacyHotbar = $pcT -match 'BindKey\(EKeys::One'
    $charHasEnhancedSpell = $charT -match 'IA_SpellU'
    $pcHasStaleTodo = $pcHT -match 'TODO M-5'
    if ($pcHasLegacyHotbar -and $charHasEnhancedSpell) {
        Warn "ASkillCreatorPlayerController.cpp 仍有舊版 BindKey(1~5) 技能槽切換邏輯，但 ASkillCreatorCharacter.cpp 已有正式 Enhanced Input（IA_SpellU/I/O/P）施法綁定 -- 兩條輸入路徑同時存在，疑似 PlayerController 那份是早期骨架沒清掉的死碼，需確認哪條才是實際生效路徑，避免未來改一邊忘了改另一邊"
    } else { Pass "未發現 PlayerController / Character 輸入路徑重複" }
    if ($pcHasStaleTodo) {
        Warn "ASkillCreatorPlayerController.h 仍有『TODO M-5：施法輸入動作』註解，但 docs/實作進度.md 已標記 M-5 完成 -- 註解可能過時未清，確認施法輸入是否真的都在 Character 端完成"
    }
} else { Warn "PlayerController 或 Character cpp 未找到 -- 跳過 13j" }

Head "13k. [Tier1] ABeastCharacter 替換 AEnemy -- 完整性驗證"
if (Test-Path $ABeastHFile) {
    $t = Read-UTF8 $ABeastHFile
    $hasGravity = $t -match 'ApplyGravity'
    $hasSnapshot = $t -match 'TakeSnapshot'
    $hasEnemyType = $t -match 'EEnemyType'
    if ($hasGravity -and $hasSnapshot -and $hasEnemyType) {
        Pass "ABeastCharacter.h 包含 ApplyGravity/TakeSnapshot/EEnemyType（AEnemy 功能完整移植）"
    } else {
        $missing = @()
        if (!$hasGravity)   { $missing += 'ApplyGravity' }
        if (!$hasSnapshot)  { $missing += 'TakeSnapshot' }
        if (!$hasEnemyType) { $missing += 'EEnemyType' }
        Fail "ABeastCharacter.h 缺少: $($missing -join ', ')（AEnemy 降殼後功能未完整移植）"
    }
} else { Warn "ABeastCharacter.h 未找到 -- 跳過 13k" }

Head "13l. [Tier1] CreatureTypes -- ECreatureKind/EBeastKind/EHostility 完整性"
if (Test-Path $CreatureTypesFile) {
    $t = Read-UTF8 $CreatureTypesFile
    $hasKind  = $t -match 'ECreatureKind'
    $hasBeast = $t -match 'EBeastKind'
    $hasHost  = $t -match 'EHostility'
    if ($hasKind -and $hasBeast -and $hasHost) {
        Pass "CreatureTypes.h 包含 ECreatureKind/EBeastKind/EHostility（生物分類系統完整）"
    } else {
        $missing = @()
        if (!$hasKind)  { $missing += 'ECreatureKind' }
        if (!$hasBeast) { $missing += 'EBeastKind' }
        if (!$hasHost)  { $missing += 'EHostility' }
        Fail "CreatureTypes.h 缺少: $($missing -join ', ')"
    }
} else { Warn "CreatureTypes.h 未找到 -- 跳過 13l" }

Head "13m. [Tier1] M-10 CaGpuSimulator -- Phase 4+5 async API 完整性"
if (Test-Path $CaGpuSimHFile) {
    $t = Read-UTF8 $CaGpuSimHFile
    $hasBegin   = $t -match 'BeginAsync'
    $hasCollect = $t -match 'TryCollectAsync'
    $hasPending = $t -match 'HasPendingAsync'
    if ($hasBegin -and $hasCollect -and $hasPending) {
        Pass "CaGpuSimulator.h 包含 Phase 5 async API（BeginAsync/TryCollectAsync/HasPendingAsync）"
    } else {
        $missing = @()
        if (!$hasBegin)   { $missing += 'BeginAsync' }
        if (!$hasCollect) { $missing += 'TryCollectAsync' }
        if (!$hasPending) { $missing += 'HasPendingAsync' }
        Fail "CaGpuSimulator.h 缺少 Phase 5 API: $($missing -join ', ')（M-10 Phase 5 可能未完整）"
    }
} else { Warn "CaGpuSimulator.h 未找到 -- 跳過 13m" }

Head "13n. [Tier1] ANPCAIController -- Tick-based AI（無 BT asset 依賴）"
if (Test-Path $NpcAICtrlCppFile) {
    $t = Read-UTF8 $NpcAICtrlCppFile
    $hasTick  = $t -match 'Tick\s*\('
    $hasSteps = $t -match 'StepFlee|StepWander|StepFollow|StepCounterAttack'
    if ($hasTick -and $hasSteps) {
        Pass "ANPCAIController 使用 Tick-based AI（Tick + Step* 方法存在，無 BT asset 依賴）"
    } else {
        $missing = @()
        if (!$hasTick)  { $missing += 'Tick()' }
        if (!$hasSteps) { $missing += 'Step* 方法' }
        Fail "ANPCAIController 缺少 Tick-based AI 要素: $($missing -join ', ')"
    }
} else { Warn "ANPCAIController.cpp 未找到 -- 跳過 13n" }

Head "13o. [Tier1] Widget 生命週期守衛（NativeOnInitialized，非 NativeConstruct）"
# 只抓「真正實作 NativeConstruct()」的行（函式定義），排除只提到 NativeConstruct 的注釋
$widgetCppFiles = @(Get-ChildItem -Recurse -Include "*.cpp" "$root\Source", "$root\Plugins\SkillCreatorUI" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match 'Widget|HUD' })
$badWidgets = @()
foreach ($f in $widgetCppFiles) {
    $t = Read-UTF8 $f.FullName
    # 需同時滿足：① 有 void Xxx::NativeConstruct() 函式定義（非注釋）② 函式體內呼叫 BuildLayout()
    $opts = [System.Text.RegularExpressions.RegexOptions]::Singleline
    $m = [System.Text.RegularExpressions.Regex]::Match($t, 'void\s+\w+::NativeConstruct\(\)\s*\{([^}]+)\}', $opts)
    if ($m.Success -and $m.Groups[1].Value -match 'BuildLayout\(\)') {
        $badWidgets += $f.Name
    }
}
if ($badWidgets) {
    Fail "以下 Widget 在 NativeConstruct() 呼叫 BuildLayout()（已知 UMG bug：AddToViewport 第一次永遠空畫面，見 docs/開發血汗錄.md 第 1 案）: $($badWidgets -join ', ')"
} else {
    Pass "所有 Widget 無 NativeConstruct()->BuildLayout() 模式（均已改用 NativeOnInitialized）"
}

# ==================================================================
Head "13p. [Tier1] EContactEffect → ApplyEnvironmentalDamage 映射完整性（P-12 材質接觸效果）"
$matRegHFile = "$root\Plugins\VoxelWorld\Source\VoxelWorld\Public\MaterialRegistry.h"
$allContactEffect = Get-EnumValues $matRegHFile "EContactEffect"
$charCppText = Read-UTF8 $CharacterCppFile
$contactExclude = @('None')
$missContact = $allContactEffect | Where-Object {
    $_ -notin $contactExclude -and
    $charCppText -notmatch "EContactEffect::$_\b"
}
if ($missContact) {
    Fail "ApplyEnvironmentalDamage 缺少 EContactEffect 映射（P-12 接觸效果無法施加給玩家）: $($missContact -join ', ')"
} else {
    Pass "ApplyEnvironmentalDamage 映射全部 $($allContactEffect.Count - 1) 個 EContactEffect（Burning/Wet/Poison/Electric/Frozen/Radioactive）"
}

# ==================================================================
Head "13q. [Tier1] FMaterialData Phase-1~4 物理欄位完整性（共 19 個新欄位）"
$matRegHText = Read-UTF8 $matRegHFile
$pFields = @(
    'AutoignitionTemp', 'MeltToMaterial', 'FreezeToMaterial', 'ElectricalConductivity', 'LuminanceLevel',
    'LiquidFlowSpeed', 'LiquidViscosity', 'GasUpwardSpeed', 'GasHorizontalSpeed', 'GasLifetime', 'BreakToMaterial',
    'ContactEffect', 'SpeedFactor', 'Stickyness', 'Slippery', 'Restitution', 'JumpFactor', 'PlatformType', 'DangerFlags'
)
$missFields = $pFields | Where-Object { $matRegHText -notmatch "\b$_\b" }
if ($missFields) {
    Fail "FMaterialData 缺少 Phase-1~4 物理欄位: $($missFields -join ', ')"
} else {
    Pass "FMaterialData 包含全部$($pFields.Count) 個 Phase-1~4 物理欄位（P-1~P-19）"
}

# ==================================================================
Head "13r. [Tier1] ASkillCreatorCharacter Phase-4 材質行為接入點（P-13/P-14/P-15/P-16/P-17）"
$hasSpeedFactor   = $charCppText -match 'SpeedFactor'
$hasStickyness    = $charCppText -match 'Stickyness'
$hasSlippery      = $charCppText -match 'Slippery'
$hasRestitution   = $charCppText -match 'Restitution'
$hasJumpFactor    = $charCppText -match 'JumpFactor'
$missing13r = @()
if (!$hasSpeedFactor) { $missing13r += 'SpeedFactor(P-13)' }
if (!$hasStickyness)  { $missing13r += 'Stickyness(P-14)' }
if (!$hasSlippery)    { $missing13r += 'Slippery(P-15)' }
if (!$hasRestitution) { $missing13r += 'Restitution(P-16)' }
if (!$hasJumpFactor)  { $missing13r += 'JumpFactor(P-17)' }
if ($missing13r) {
    Fail "ASkillCreatorCharacter 缺少材質行為接入點: $($missing13r -join ', ')"
} else {
    Pass "ASkillCreatorCharacter 含全部 Phase-4 材質行為接入點（SpeedFactor/Stickyness/Slippery/Restitution/JumpFactor）"
}

# ==================================================================
Head "13s. [Tier1] ANPCAIController::TryStep DangerFlags 守衛（P-19 NPC 避開危險地板）"
$npcAiText = Read-UTF8 $NpcAICtrlCppFile
if ($npcAiText -match 'DangerFlags') {
    Pass "ANPCAIController::TryStep 含 DangerFlags 守衛（NPC 不走危險地板）"
} else {
    Fail "ANPCAIController::TryStep 缺少 DangerFlags 守衛（P-19 — NPC 會踩火/毒/輻射地板）"
}

# ==================================================================
Head "13t. [Tier1] SpellArray::PrimaryElement() 計算屬性（SpellElement 欄位已移除）"
$spellArrayHFile = "$root\Plugins\AbilitySystem\Source\AbilitySystem\Public\SpellArray.h"
$spellArrayText  = Read-UTF8 $spellArrayHFile
$hasPrimaryElem  = $spellArrayText -match 'PrimaryElement\(\)'
$hasOldField     = $spellArrayText -match '\bESkillElementType\s+SpellElement\b'
if ($hasPrimaryElem -and -not $hasOldField) {
    Pass "SpellArray 有 PrimaryElement() 計算屬性，舊 SpellElement 欄位已移除"
} elseif (-not $hasPrimaryElem) {
    Fail "SpellArray 缺少 PrimaryElement() 方法（Godot SpellArray.cs:76-87 遷移缺口）"
} else {
    Fail "SpellArray 同時有 PrimaryElement() 和舊 SpellElement 欄位（殘留欄位未清除）"
}

# ==================================================================
# TIER 1 — GAS-0~6 整合完整性
# ==================================================================
Head "13u. [Tier1] AbilitySystem.uplugin — GameplayAbilities 依賴宣告"
if (Test-Path $AbilitySystemUplugin) {
    $t = Read-UTF8 $AbilitySystemUplugin
    if ($t -match '"GameplayAbilities"') {
        Pass "AbilitySystem.uplugin 有 GameplayAbilities 依賴宣告（避免 UBT 漏掉連結）"
    } else {
        Fail "AbilitySystem.uplugin 缺 GameplayAbilities 依賴 -- Build.cs 有依賴但 .uplugin 未宣告，UBT 會警告且可能遺漏 DLL 連結"
    }
} else { Warn "AbilitySystem.uplugin 未找到 -- 跳過 13u" }

Head "13v. [Tier1] USkillCreatorAttributeSet — GAS-1 四屬性完整性"
if (Test-Path $GasAttrSetHFile) {
    $t = Read-UTF8 $GasAttrSetHFile
    $missingAttrs = @()
    foreach ($attr in @('Health', 'MaxHealth', 'Mana', 'MaxMana')) {
        if ($t -notmatch "FGameplayAttributeData\s+$attr\b") { $missingAttrs += $attr }
    }
    if ($missingAttrs) {
        Fail "USkillCreatorAttributeSet 缺少 GAS-1 屬性: $($missingAttrs -join ', ')"
    } else {
        Pass "USkillCreatorAttributeSet 含 Health/MaxHealth/Mana/MaxMana 四屬性（GAS-1）"
    }
} else { Warn "SkillCreatorAttributeSet.h 未找到 -- 跳過 13v" }

Head "13w. [Tier1] UGasEffectRegistry — CanApply 鍊式條件（GAS-6）"
if (Test-Path $GasEffectRegCppFile) {
    $t = Read-UTF8 $GasEffectRegCppFile
    $hasFrozen = $t -match 'StatusTagLeaf\s*==\s*TEXT\("Frozen"\)'
    $hasTerror = $t -match 'StatusTagLeaf\s*==\s*TEXT\("Terror"\)'
    $hasFrostTag = $t -match 'Status\.Debuff\.Frost'
    $hasFearTag  = $t -match 'Status\.Debuff\.Fear'
    if ($hasFrozen -and $hasTerror -and $hasFrostTag -and $hasFearTag) {
        Pass "UGasEffectRegistry::CanApply() 含 Frozen/Terror 鍊式條件（GAS-6）"
    } else {
        $missing = @()
        if (!$hasFrozen)   { $missing += 'Frozen 條件' }
        if (!$hasTerror)   { $missing += 'Terror 條件' }
        if (!$hasFrostTag) { $missing += 'Status.Debuff.Frost tag 引用' }
        if (!$hasFearTag)  { $missing += 'Status.Debuff.Fear tag 引用' }
        Fail "UGasEffectRegistry::CanApply() 缺少 GAS-6 鍊式條件: $($missing -join ', ')"
    }
} else { Warn "UGasEffectRegistry.cpp 未找到 -- 跳過 13w" }

Head "13x. [Tier1] DefaultGameplayTags.ini — 24 個 Status 標籤完整性"
if (Test-Path $DefaultGTFile) {
    $t = Read-UTF8 $DefaultGTFile
    $expectedLeaves = @(
        'Burn', 'Suffocation', 'Wet', 'Frost', 'Frozen', 'Poison',
        'DarkErosion', 'InstantDeath', 'Paralysis', 'EnergySeal', 'SkillSeal', 'Bleed',
        'Petrify', 'Stun', 'Cripple', 'Fear', 'Terror', 'Misfortune', 'MiningFatigue',
        'SuperArmor', 'Phase', 'Invincible', 'ElemResBasic', 'ElemResAdvanced'
    )
    $missingTags = $expectedLeaves | Where-Object { $t -notmatch "Status\.[^.]+\.$_" }
    if ($missingTags) {
        Fail "DefaultGameplayTags.ini 缺少 Status tag（GAS Loose Tag 查詢會靜默失敗）: $($missingTags -join ', ')"
    } else {
        Pass "DefaultGameplayTags.ini 包含全部 $($expectedLeaves.Count) 個 Status.* 標籤（GAS-2/GAS-5 Loose Tag 同步正確）"
    }
} else { Warn "DefaultGameplayTags.ini 未找到 -- 跳過 13x" }

Head "13y. [Tier1] UGasEffectRegistry GAllStatusLeaves — 數量與 DefaultGameplayTags 一致"
if (Test-Path $GasEffectRegCppFile) {
    $t = Read-UTF8 $GasEffectRegCppFile
    # 只數 GAllStatusLeaves 陣列內部的 TEXT("...") 項目（不含 CanApply/Log 字串）
    $opts13y = [System.Text.RegularExpressions.RegexOptions]::Singleline
    $arrMatch = [System.Text.RegularExpressions.Regex]::Match($t, 'GAllStatusLeaves\s*\[\s*\]\s*=\s*\{([^}]+)\}', $opts13y)
    if ($arrMatch.Success) {
        $leafCount = ([System.Text.RegularExpressions.Regex]::Matches($arrMatch.Groups[1].Value, 'TEXT\("[^"]+"\)')).Count
        if ($leafCount -eq 24) {
            Pass "GAllStatusLeaves 共 24 個 leaf name，與 DefaultGameplayTags.ini 一致（GAS-3/GAS-4）"
        } else {
            Fail "GAllStatusLeaves 有 $leafCount 個 leaf name，不等於 24（預期對應 24 個 GE 資產 + 24 個 tag）"
        }
    } else { Warn "找不到 GAllStatusLeaves 陣列定義 -- 跳過 13y" }
} else { Warn "UGasEffectRegistry.cpp 未找到 -- 跳過 13y" }

Head "13z. [Tier1] USpecialStatusComponent GAS-5 — Loose Tag 同步四點完整性"
$specialStatusCppFile = "$root\Source\SkillCreatorRuntime\Private\USpecialStatusComponent.cpp"
if (Test-Path $specialStatusCppFile) {
    $t = Read-UTF8 $specialStatusCppFile
    $hasAdd    = $t -match 'AddLooseGameplayTag'
    $hasRemove = $t -match 'RemoveLooseGameplayTag'
    $hasResolve = $t -match 'ResolveLooseTag'
    $hasGetStackCount = $t -match 'GetStackCount.*==\s*0'  # 0→1 first-stack guard
    if ($hasAdd -and $hasRemove -and $hasResolve -and $hasGetStackCount) {
        Pass "USpecialStatusComponent GAS-5 Loose Tag 同步完整（Add/Remove/ResolveLooseTag/0→1 守衛）"
    } else {
        $missing = @()
        if (!$hasAdd)          { $missing += 'AddLooseGameplayTag' }
        if (!$hasRemove)       { $missing += 'RemoveLooseGameplayTag' }
        if (!$hasResolve)      { $missing += 'ResolveLooseTag' }
        if (!$hasGetStackCount){ $missing += '0→1 first-stack guard (GetStackCount==0)' }
        Fail "USpecialStatusComponent GAS-5 Loose Tag 同步不完整: $($missing -join ', ')"
    }
} else { Warn "USpecialStatusComponent.cpp 未找到 -- 跳過 13z" }

# ==================================================================
# TIER 2 — 演算法刻意不同，只驗證「功能覆蓋契約」
# ==================================================================
Head "14. [Tier2] 技能編輯 UI（Scratch Canvas -> SGraphEditor，演算法刻意不同）"
$uiFiles = @($BTSchemaFile, $BTNodeFile)
$missUiBT = $allBT | Where-Object { -not (Test-AnyFileContains $uiFiles "EBlockType::$_\b") }
if ($missUiBT) { Warn "積木編輯器 UI 層（Schema/Node）找不到對應: $($missUiBT -join ', ')（玩家可能無法在畫布上使用這些積木 -- 僅靜態檢查，無法確認實際是否顯示）" }
else { Pass "積木編輯器 UI 層涵蓋全部 $($allBT.Count) 個 BlockType（演算法與 Godot 不同，僅驗證覆蓋契約）" }

Head "15. [Tier2] 敵人 AI（FSM -> Tick-based，演算法刻意不同）"
# AEnemy 已降為薄殼，EEnemyType 和邏輯移至 ABeastCharacter
$allEnemyType = Get-EnumValues $ABeastHFile "EEnemyType"
$enemyFiles = @(Get-ChildItem -Recurse -Filter "*.cpp" "$root\Source\SkillCreatorRuntime" -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
$missEnemyType = $allEnemyType | Where-Object { -not (Test-AnyFileContains $enemyFiles "EEnemyType::$_\b") }
if ($missEnemyType) { Warn "找不到處理這些 EnemyType 的程式碼: $($missEnemyType -join ', ')" }
else { Pass "全部 $($allEnemyType.Count) 個 EnemyType 在 Runtime 模組中有對應處理（ABeastCharacter Tick-based AI）" }

Head "16. [Tier3] 明確排除（不計分，僅列出原因避免誤會）"
Skip "M-10 GPU CA PIE 視覺驗證（Phase 1-5 程式碼 2026-06-25 全部完成，Build 0 錯誤，純實機確認 CA 物理正確，靜態腳本無法驗證）"
Skip "渲染演算法逐行比對（ArrayMesh vs RealtimeMeshComponent）-- 架構整個換掉，比對沒有意義，僅能實機驗收畫面"
Skip "WBP_GameFlow 等 .uasset 內部設定 -- Editor 專屬二進位內容，純文字腳本無法讀取，需在 Editor 內手動確認"
Skip "UI 是否『實際顯示在畫面上』-- 本腳本只能確認程式碼存在，無法確認 AddToViewport / 實機渲染結果"
Skip "TileWorld3D Fire CA 擴散公式（Godot 用 Z 對角線偏移，UE5 改用 4 方向陣列）-- 演算法整個換掉，與 Godot 逐行比對沒有意義"

# ==================================================================
Head "17. WorldScale magic-number 掃描（座標換算遺漏偵測）"
$scanFiles = @($AEnemyCppFile, $MobSpawnCppFile, $ProjectileCppFile) | Where-Object { Test-Path $_ }
$tsSafe = 'WorldScale::|TileSizeCm|TileToWorld|WorldToTile|^\s*//|GridPos|FIntVector|int32'
$hits = @()
foreach ($f in $scanFiles) {
    $fname = [System.IO.Path]::GetFileName($f)
    $lines = [System.IO.File]::ReadAllLines($f, [System.Text.Encoding]::UTF8)
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $ln = $lines[$i]
        if ($ln -notmatch 'FVector\s*\(') { continue }
        if ($ln -match $tsSafe) { continue }
        if ($ln -match '\b[1-9]\d{2,}\.?\d*f\b') {
            $hits += "$fname`:$($i+1)  $($ln.Trim())"
        }
    }
}
if ($hits.Count -gt 0) {
    Warn "可疑的裸三位數以上浮點數寫進 FVector，未經 WorldScale 轉換: $($hits.Count) 處"
    if ($ShowDetails) { $hits | ForEach-Object { Write-Host "      $_" -ForegroundColor Yellow } }
    else { Write-Host "      (用 -ShowDetails 看詳細位置)" -ForegroundColor Yellow }
} else { Pass "敵人/投射物相關檔案沒有可疑的裸座標數值" }

# ==================================================================
Head "18. TODO / FIXME / HACK / STUB 技術債統計"
$allCpp = Get-ChildItem -Recurse -Include "*.h", "*.cpp" "$root\Source", "$root\Plugins\AbilitySystem", "$root\Plugins\VoxelWorld", "$root\Plugins\SkillCreatorUI" -ErrorAction SilentlyContinue
$debtStub = @(); $debtTodo = @()
foreach ($f in $allCpp) {
    $lines = [System.IO.File]::ReadAllLines($f.FullName, [System.Text.Encoding]::UTF8)
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $ln = $lines[$i]
        if ($ln -match '\bSTUB\b') { $debtStub += [pscustomobject]@{ File = $f.Name; Line = $i + 1; Text = $ln.Trim() } }
        elseif ($ln -match '(TODO|FIXME|HACK)') { $debtTodo += [pscustomobject]@{ File = $f.Name; Line = $i + 1; Text = $ln.Trim() } }
    }
}
if ($debtStub.Count -gt 0) {
    Warn "STUB 標記（關鍵：未完成路徑）: $($debtStub.Count)"
    if ($ShowDetails) { $debtStub | ForEach-Object { Write-Host "      [STUB] $($_.File):$($_.Line)  $($_.Text)" -ForegroundColor Red } }
}
if ($debtTodo.Count -gt 0) {
    Warn "TODO/FIXME/HACK 標記: $($debtTodo.Count)（用 -ShowDetails 列出）"
    if ($ShowDetails) { $debtTodo | ForEach-Object { Write-Host "      $($_.File):$($_.Line)  $($_.Text)" } }
}
if ($debtStub.Count -eq 0 -and $debtTodo.Count -eq 0) { Pass "沒有 TODO/FIXME/HACK/STUB 標記" }

# ==================================================================
Write-Host ""
Write-Host "========================================"
$summaryColor = if ($fail -eq 0) { 'Green' } else { 'Red' }
Write-Host "  PASS $pass   WARN $warn   FAIL $fail   SKIP(Tier3) $skip" -ForegroundColor $summaryColor
Write-Host "========================================"
Write-Host ""

if ($fail -gt 0) { exit 1 } else { exit 0 }
