# Far Far West — Cheat Engine 指针链手册

> **引擎版本**: UE 5.7 (Build 5.7.3-50162420)  
> **游戏**: Far Far West  
> **基址模块**: `FarFarWest-Win64-Shipping.exe`

---

## 1. 全局基址

| 符号 | 偏移量 | CE 写法 |
|------|--------|---------|
| GWorld | `0x92CCD20` | `"FarFarWest-Win64-Shipping.exe"+0x92CCD20` |
| GObjects | `0x9141F60` | `"FarFarWest-Win64-Shipping.exe"+0x9141F60` |
| ProcessEvent | `0x13FCEE0` | `"FarFarWest-Win64-Shipping.exe"+0x13FCEE0` |

---

## 2. 引擎中间层偏移

以下是从 SDK 提取的引擎类关键指针偏移，构成所有链路的中间节点：

| 类 | 成员 | 偏移 | 类型 | 说明 |
|----|------|------|------|------|
| **UWorld** | PersistentLevel | `+0x0030` | ULevel* | 关卡 (含 Actors 数组) |
| **UWorld** | GameState | `+0x01B0` | AGameStateBase* | 游戏状态 (含 PlayerArray) |
| **UWorld** | OwningGameInstance | `+0x0228` | UGameInstance* | 游戏实例 |
| **UGameInstance** | LocalPlayers | `+0x0038` | TArray\<ULocalPlayer*\> | 本地玩家数组 (Data 指针) |
| **UPlayer** | PlayerController | `+0x0030` | APlayerController* | 玩家控制器 |
| **APlayerController** | AcknowledgedPawn | `+0x0358` | APawn* | 当前操控的 Pawn |
| **AGameStateBase** | PlayerArray | `+0x02C8` | TArray\<APlayerState*\> | 所有玩家状态 |
| **ULevel** | Actors | `+0x00A0` | TArray\<AActor*\> | 关卡内所有 Actor |

> **TArray 内存布局** (UE5): `[DataPtr(8)] [Count(4)] [Max(4)]`  
> 读取 Data 指针后，`+0x0000` 取第 0 个元素，`+0x0008` 取第 1 个，以此类推。

---

## 3. 本地玩家核心路径

### 3.1 公共指针链（GWorld → PlayerState）

所有本地玩家属性共享以下前缀：

```
"FarFarWest-Win64-Shipping.exe"+0x92CCD20
  +0x0228     // UWorld → OwningGameInstance
  +0x0038     // UGameInstance → LocalPlayers.Data
  +0x0000     // [0] → 第一个 ULocalPlayer*
  +0x0030     // ULocalPlayer → PlayerController
  +0x0358     // APlayerController → AcknowledgedPawn
```

到达 **Pawn (ABP_Player_C\*)** 后，可直接访问 Pawn 自身字段，或继续解引用到 PlayerState。

---

### 3.2 Pawn 直属属性

在 Pawn 指针链末尾追加以下偏移（无需额外解引用）：

| 属性 | 追加偏移 | CE 类型 | 说明 |
|------|----------|---------|------|
| clientTransform | `+0x06E0` | — (结构体) | 位置/旋转，见下方拆分 |
| 位置 X | `+0x0700` | Float | FTransform.Translation.X (Rotation 4 floats + 0x10 padding 后) |
| 位置 Y | `+0x0704` | Float | FTransform.Translation.Y |
| 位置 Z | `+0x0708` | Float | FTransform.Translation.Z |
| iaMove | `+0x0748` | — (FVector2D) | 移动输入 |
| cameraRotation | `+0x0778` | — (FRotator) | 摄像机旋转 |
| isStunned | `+0x07A8` | 1 Byte | 被击晕 |
| killCombo | `+0x07C8` | 4 Bytes | 连杀数 |
| isRolling | `+0x07E8` | 1 Byte | 翻滚中 |
| maxDash | `+0x0840` | 4 Bytes | 最大冲刺次数 |
| **currentDashLeft** | `+0x0844` | **4 Bytes** | **剩余冲刺次数** |
| invincibleFrame | `+0x0891` | 1 Byte | 无敌帧 |
| stability | `+0x08D8` | Double (8) | 瞄准稳定性 |
| **weaponRecoil** | `+0x08E0` | **Double (8)** | **武器后坐力** |
| dashCooldown | `+0x0940` | Double (8) | 冲刺冷却 |
| canUseWeapons | `+0x0958` | 1 Byte | 能否使用武器 |
| buffMoveSpeed | `+0x09A8` | Double (8) | 增益移动速度 |
| spawnProtectionInvincible | `+0x0A2A` | 1 Byte | 出生保护无敌 |
| jumpsLeft | `+0x0A4C` | 4 Bytes | 剩余跳跃次数 |
| tempoInvicible | `+0x0B31` | 1 Byte | 临时无敌 |
| IsLobby | `+0x07CC` | 1 Byte | 是否在 Lobby |
| isExtracting | `+0x0740` | 1 Byte | 正在撤离 |
| isRolling | `+0x07E8` | 1 Byte | 翻滚中 (已列) |
| aimFocal | `+0x08A0` | Double (8) | 瞄准焦距 |
| cameraMovementTilt | `+0x08A8` | Double (8) | 摄像机倾斜 |
| weaponAimSpeed | `+0x08B0` | Double (8) | 瞄准速度 |
| speedBuffs | `+0x08E8` | — (TMap) | 速度增益映射 |
| isInCinematic | `+0x0938` | 1 Byte | 过场动画中 |
| isGameModeDebug | `+0x0939` | 1 Byte | 调试游戏模式 |
| dashCooldown | `+0x0940` | Double (8) | 冲刺冷却 (已列) |
| hasBeenResurected | `+0x0948` | 1 Byte | 已被复活 |
| isTpsMode | `+0x0949` | 1 Byte | 第三人称模式 |
| dashCount | `+0x098C` | 4 Bytes | 冲刺计数 |
| canSpawnMount | `+0x0990` | 1 Byte | 能否召唤坐骑 |
| isFocusingTarget | `+0x09A0` | 1 Byte | 正在聚焦目标 |
| chromaLightLevel | `+0x0980` | Double (8) | 色彩光照等级 (Net同步) |
| isDashOnCooldown | `+0x0A29` | 1 Byte | 冲刺是否冷却中 |
| nextLandsCanDealDamages | `+0x0AA0` | 1 Byte | 下次落地造成伤害 |
| mountOnCooldown | `+0x0A48` | 1 Byte | 坐骑冷却中 |
| savedPlayerScale | `+0x0AE8` | Double (8) | 保存的玩家缩放 |
| jokerSecondWindTrigger | `+0x0B30` | 1 Byte | 回旋Joker触发 |
| isTutorial | `+0x0B32` | 1 Byte | 是否教程 |
| isInteracting | `+0x0B33` | 1 Byte | 正在交互 |

> **FTransform 内存布局** (UE5): `[Rotation(FQuat=4×float)] [Translation(FVector=3×float)] [Scale3D(FVector=3×float)]`  
> Rotation: +0x06E0 (16 bytes) → Translation: +0x06F0 (12 bytes) → Scale3D: +0x06FC (12 bytes)  
> 但 UE5 有 vectorization，实际偏移可能有 padding，以上位置 X/Y/Z 为 FTransform+0x10/0x14/0x18 即 Pawn+0x06F0/0x06F4/0x06F8，需运行时验证。

---

### 3.3 PlayerState 属性

在 Pawn 指针链末尾追加 `+0x0960` 解引用到 PlayerState，再追加字段偏移：

**完整前缀**:
```
"FarFarWest-Win64-Shipping.exe"+0x92CCD20 +0x0228 +0x0038 +0x0000 +0x0030 +0x0358 +0x0960
```
此处读出的是 **ABP_PlayerState_C\*** 指针，继续追加：

#### 血量 / 生命

| 属性 | 追加偏移 | CE 类型 | 说明 |
|------|----------|---------|------|
| **playerHeal** | `+0x0658` | **Double (8)** | **当前血量** |
| **playerMaxHeal** | `+0x0660` | **Double (8)** | **最大血量** |
| playerSize | `+0x0668` | Double (8) | 玩家尺寸 |
| **invincible** | `+0x08E0` | **1 Byte** | **无敌模式** |
| tweakCooldownReduction | `+0x08A8` | Double (8) | 冷却缩减 |

#### 玩家状态 / 身份

| 属性 | 追加偏移 | CE 类型 | 说明 |
|------|----------|---------|------|
| playerSize | `+0x0668` | Double (8) | 玩家尺寸 |
| CurrentItem | `+0x0670` | 8 Bytes | 当前物品 UClass* |
| assignedMount | `+0x0678` | 8 Bytes | 已分配坐骑 ABP_Mount_C* |
| GameInstance | `+0x08A0` | 8 Bytes | 游戏实例引用 |
| equipedTool | `+0x08B0` | 8 Bytes | 装备工具 UClass* |
| lastUsedMainWeapon | `+0x08B8` | 8 Bytes | 上次使用主武器 UClass* |
| enemyKillCombo | `+0x08C0` | 4 Bytes | 击杀连击数 |
| playerOwnsMissionMap | `+0x08E1` | 1 Byte | 是否拥有任务地图 |
| spellARuntime | `+0x08E8` | 8 Bytes | 运行时技能A UClass* |
| spellBRuntime | `+0x08F0` | 8 Bytes | 运行时技能B UClass* |
| spellCRuntime | `+0x08F8` | 8 Bytes | 运行时技能C UClass* |
| isHost | `+0x0910` | 1 Byte | 是否主机 |
| mountDisabled | `+0x0928` | 1 Byte | 坐骑被禁用 |
| connectedId | `+0x0980` | 4 Bytes | 连接ID |
| canGoUnderLandscape | `+0x0984` | 1 Byte | 能否穿越地面 |
| isSuspicious | `+0x0985` | 1 Byte | 可疑标记 |
| playerColor | `+0x0998` | 4 Bytes | 玩家颜色 (Net同步) |

#### 武器弹药 (FS_PlayerRuntimeStats 内联结构体)

PlayerState+0x03B8 处为内联结构体 `FS_PlayerRuntimeStats`，**无需解引用**，直接加偏移即可：

| 属性 | 计算过程 | 追加偏移 | CE 类型 | 说明 |
|------|----------|----------|---------|------|
| **主武器当前弹匣** | 0x03B8+0x00+0x04 | **`+0x03BC`** | **4 Bytes** | **主武器弹匣弹药** |
| **主武器最大弹匣** | 0x03B8+0x00+0x08 | **`+0x03C0`** | **4 Bytes** | 主武器弹匣容量 |
| **主武器总弹药** | 0x03B8+0x00+0x0C | **`+0x03C4`** | **4 Bytes** | **主武器总弹药** |
| 主武器最大总弹药 | 0x03B8+0x00+0x10 | `+0x03C8` | 4 Bytes | 主武器总弹药上限 |
| **副武器当前弹匣** | 0x03B8+0x14+0x04 | **`+0x03D0`** | **4 Bytes** | **副武器弹匣弹药** |
| **副武器最大弹匣** | 0x03B8+0x14+0x08 | **`+0x03D4`** | **4 Bytes** | 副武器弹匣容量 |
| **副武器总弹药** | 0x03B8+0x14+0x0C | **`+0x03D8`** | **4 Bytes** | **副武器总弹药** |
| 副武器最大总弹药 | 0x03B8+0x14+0x10 | `+0x03DC` | 4 Bytes | 副武器总弹药上限 |
| **手雷数量** | 0x03B8+0x28 | **`+0x03E0`** | **4 Bytes** | **手雷** |
| 技能A冷却 | 0x03B8+0x30 | `+0x03E8` | Double (8) | 技能A剩余冷却 |
| 技能B冷却 | 0x03B8+0x38 | `+0x03F0` | Double (8) | 技能B剩余冷却 |
| 技能C冷却 | 0x03B8+0x40 | `+0x03F8` | Double (8) | 技能C剩余冷却 |

---

## 4. 完整 CE 指针链速查表

> **使用方法**: 在 CE 中点击 "Add Address Manually" → 勾选 "Pointer" → 输入基址和偏移序列  
> **基址栏填写**: `"FarFarWest-Win64-Shipping.exe"+0x92CCD20`  
> **偏移从下往上填** (CE 界面中最后一步的偏移在最下面)

### 4.1 玩家血量 / 无敌

| # | 用途 | 偏移序列 (从上到下) | 类型 |
|---|------|---------------------|------|
| 1 | 当前血量 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x0658 | Double |
| 2 | 最大血量 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x0660 | Double |
| 3 | 无敌开关 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x08E0 | Byte |

### 4.2 武器弹药

| # | 用途 | 偏移序列 (从上到下) | 类型 |
|---|------|---------------------|------|
| 4 | 主武器当前弹匣 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03BC | 4 Bytes |
| 5 | 主武器最大弹匣 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03C0 | 4 Bytes |
| 6 | 主武器总弹药 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03C4 | 4 Bytes |
| 7 | 主武器最大总弹药 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03C8 | 4 Bytes |
| 8 | 副武器当前弹匣 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03D0 | 4 Bytes |
| 9 | 副武器最大弹匣 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03D4 | 4 Bytes |
| 10 | 副武器总弹药 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03D8 | 4 Bytes |
| 11 | 副武器最大总弹药 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03DC | 4 Bytes |
| 12 | 手雷数量 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03E0 | 4 Bytes |

### 4.3 技能冷却

| # | 用途 | 偏移序列 (从上到下) | 类型 |
|---|------|---------------------|------|
| 13 | 技能A冷却 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03E8 | Double |
| 14 | 技能B冷却 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03F0 | Double |
| 15 | 技能C冷却 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x03F8 | Double |
| 16 | 冷却缩减 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x08A8 | Double |

### 4.4 Pawn 直属属性

| # | 用途 | 偏移序列 (从上到下) | 类型 |
|---|------|---------------------|------|
| 17 | 剩余冲刺次数 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0844 | 4 Bytes |
| 18 | 最大冲刺次数 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0840 | 4 Bytes |
| 19 | 武器后坐力 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x08E0 | Double |
| 20 | 瞄准稳定性 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x08D8 | Double |
| 21 | 冲刺冷却 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0940 | Double |
| 22 | 增益移速 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x09A8 | Double |
| 23 | 连杀数 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x07C8 | 4 Bytes |
| 24 | 剩余跳跃次数 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0A4C | 4 Bytes |
| 25 | 被击晕 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x07A8 | Byte |
| 26 | 无敌帧 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0891 | Byte |
| 27 | 出生保护无敌 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0A2A | Byte |
| 28 | 临时无敌 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0B31 | Byte |
| 29 | 能否使用武器 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0958 | Byte |

---

## 5. 玩家进度 / 经济系统 (FS_PlayerProgress)

`ABP_PlayerState_C+0x0400` 处为 `FS_PlayerProgress playerProgressReplicated`（0x258 字节，Net 同步），这是玩家持久化进度的核心结构体。

### 5.1 FS_PlayerProgress 结构体偏移 (0x258 字节)

| 偏移 | 类型 | 属性名 | 说明 |
|------|------|--------|------|
| `+0x0000` | int32 | versionId | 版本号 |
| `+0x0004` | FName | selectedPlayerSkin | 选中的皮肤 |
| `+0x000C` | FName | selectedMountSkin | 选中的坐骑皮肤 |
| `+0x0014` | int32 | selectedDifficulty | 选中的难度 |
| `+0x0018` | int32 | selectedAmountModifiers | 修正器数量 |
| `+0x001C` | byte | specialWeaponElement | 特殊武器元素 (E_DamageCategory) |
| `+0x0020` | UClass* | selectedSpellA | 选中的技能A |
| `+0x0028` | UClass* | selectedSpellB | 选中的技能B |
| `+0x0030` | UClass* | selectedSpellC | 选中的技能C |
| `+0x0038` | TArray\<UClass*\> | rewards | 奖励列表 |
| **`+0x0048`** | **TMap\<E_DamageCategory, int32\>** | **spellXp** | **人物经验值 (按元素分类，累加=总XP)** |
| `+0x0098` | TMap | brokenGraveCrosses | 破碎墓碑 |
| **`+0x00E8`** | **int32** | **unlockedDifficulties** | **已解锁难度** |
| `+0x00EC` | FName | selectedEmoteA | 表情A |
| `+0x00F4` | FName | selectedEmoteB | 表情B |
| `+0x00FC` | FName | selectedEmoteC | 表情C |
| `+0x0104` | FName | selectedEmoteD | 表情D |
| **`+0x010C`** | **FName** | **selectedItemA** | **选中的主武器** |
| **`+0x0114`** | **FName** | **selectedItemB** | **选中的副武器** |
| **`+0x011C`** | **FName** | **selectedItemUtility** | **选中的工具** |
| **`+0x0128`** | **TMap\<FName, FS_ItemTweaks\>** | **itemsUpgrades** | **武器升级/技能等级** |
| **`+0x0178`** | **TMap\<FName, FS_PlayerItemJokers\>** | **itemJokers** | **武器 Jokers (含4个槽位)** |
| **`+0x01C8`** | **TArray\<FS_RuntimeInventory\>** | **runtimeInventory** | **背包 (金币/灵魂等)** |
| **`+0x01D8`** | **TMap\<FName, int32\>** | **challenges** | **挑战进度** |
| `+0x0228` | TArray | pendingRewardNotifications | 待领取奖励通知 |
| `+0x0238` | TArray\<FName\> | rewardedChallenges | 已完成挑战 |
| `+0x0248` | FName | fragmentTrackedWeaponName | 追踪武器碎片 |
| `+0x0250` | FName | title | 称号 |

### 5.2 子结构体定义

#### FS_RuntimeInventory (0x0C 字节) — 背包条目

| 偏移 | 类型 | 说明 |
|------|------|------|
| `+0x0000` | FName (8B) | 物品名称 |
| `+0x0008` | int32 | 物品数量 |

> `runtimeInventory` 是一个数组，每条目 `{FName name, int32 amount}`。  
> **金币/灵魂数量** = 遍历数组，找到 FName 匹配 `Gold`/`Soul` 的条目后读取 amount。  
> 因为 TMap/TArray 无法用固定指针链直接访问，需要 Lua 脚本遍历。

#### FS_ItemTweaks (0x50 字节) — 武器升级

| 偏移 | 类型 | 说明 |
|------|------|------|
| `+0x0000` | TMap\<FName, int32\> | 各项升级等级 (key=升级名, value=等级) |

> `itemsUpgrades` 是 TMap\<FName, FS_ItemTweaks\>，key 为武器 FName，value 为该武器的升级数据。  
> 每把武器有一个 FS_ItemTweaks，其内部又是一个 TMap 记录各项属性等级。

#### FS_PlayerItemJokers (0x30 字节) — 武器 Joker 槽

| 偏移 | 类型 | 说明 |
|------|------|------|
| `+0x0000` | FName | jokerA |
| `+0x0008` | FName | jokerB |
| `+0x0010` | FName | jokerC |
| `+0x0018` | FName | jokerD |
| `+0x0020` | TArray\<FName\> | 附加 jokers |

### 5.3 玩家等级计算

游戏中**没有直接存储玩家等级**的字段。等级由函数 `BPFL_FunctionLibrary_C::F_CalculateXpLevel` 根据总 XP 动态计算：

```
F_CalculateXpLevel(int32 Xp) → CurrentLevel, RequiredTotalXpForThisLevel, XpRequiredToCompleteLevel, CompletionPercentage
```

**经验值来源**: `spellXp` (FS_PlayerProgress+0x0048) 中的各元素 XP 累加值。  
该字段为 `TMap<E_DamageCategory, int32>`，无法用固定偏移直接读取，需脚本遍历。

### 5.4 CE 指针链 — 可直接读取的进度字段

在 PlayerState 指针链前缀 `...+0x0960` 末尾追加：

| # | 用途 | 追加偏移 | CE 类型 | 说明 |
|---|------|----------|---------|------|
| 30 | 选中的难度 | `+0x0400+0x0014` = `+0x0414` | 4 Bytes | selectedDifficulty |
| 31 | 已解锁难度 | `+0x0400+0x00E8` = `+0x04E8` | 4 Bytes | unlockedDifficulties |
| 32 | 特殊武器元素 | `+0x0400+0x001C` = `+0x041C` | 1 Byte | E_DamageCategory 枚举 |
| 33 | 选中的主武器 | `+0x0400+0x010C` = `+0x050C` | — (FName) | selectedItemA (8B) |
| 34 | 选中的副武器 | `+0x0400+0x0114` = `+0x0514` | — (FName) | selectedItemB (8B) |

---

## 6. 经济系统 — 金币 / 灵魂

### 6.1 本局累计 (ABP_GameState_C)

金币和灵魂数量存储在 `ABP_GameState_C` 中，通过 GWorld→GameState 路径访问：

**前缀**: `"FarFarWest-Win64-Shipping.exe"+0x92CCD20 +0x01B0` → 到达 GameState

| # | 用途 | 偏移序列 | CE 类型 | 说明 |
|---|------|----------|---------|------|
| 35 | 本局捡到金币 | +0x01B0, +0x03B0 | 4 Bytes | pickedUpGolds (Net同步) |
| 36 | 本局捡到灵魂数 | +0x01B0, +0x03B4 | 4 Bytes | pickedUpSouls (Net同步) |
| 37 | 本地已用金币 | +0x01B0, +0x03B8 | 4 Bytes | localUsedGolds |
| 38 | 本地已用灵魂数 | +0x01B0, +0x03BC | 4 Bytes | localUsedSouls |
| 39 | 额外XP | +0x01B0, +0x044C | 4 Bytes | additionalXp (Net同步) |
| 40 | 已杀小Boss数 | +0x01B0, +0x03C0 | 4 Bytes | killedMinibosses (Net同步) |
| 41 | 任务难度 | +0x01B0, +0x0348 | 4 Bytes | missionDifficulty (Net同步) |
| 42 | 已救玩家数 | +0x01B0, +0x0338 | 4 Bytes | amountPlayersSaved (Net同步) |
| 43 | Boss是否被杀 | +0x01B0, +0x0448 | 1 Byte | isBossKilled (Net同步) |
| 44 | 已清营地数 | +0x01B0, +0x0454 | 4 Bytes | clearedCamps (Net同步) |
| 45 | 已捡TNT数 | +0x01B0, +0x0478 | 4 Bytes | amountPickedTnt (Net同步) |
| 46 | 已完成支线 | +0x01B0, +0x047C | 4 Bytes | completedSideObjectives (Net同步) |
| 47 | 主线完成 | +0x01B0, +0x0484 | 1 Byte | hasFinishedMainObjective (Net同步) |
| 48 | 任务完成 | +0x01B0, +0x04E8 | 1 Byte | hasFinishedMission (Net同步) |

**可用金币** = pickedUpGolds - localUsedGolds  
**可用灵魂** = pickedUpSouls - localUsedSouls

### 6.2 持久化金币/灵魂 — CE 指针链 (runtimeInventory)

`runtimeInventory` 是 `TArray<FS_RuntimeInventory>`（每条目 FName 8B + int32 4B = 0x0C 字节），数组前两项通常为：

| 数组索引 | FName | 含义 |
|----------|-------|------|
| [0] | Gold | 金币 |
| [1] | Soul | 灵魂 |

通过解引用 runtimeInventory.Data 指针可直接用 CE 指针链读取：

**非局内路径** (CanyonGameInstance → playerProgress → runtimeInventory):
```
"FarFarWest-Win64-Shipping.exe"+0x92CCD20
  +0x0228     // → GameInstance
  +0x03E0     // [GameInstance+0x0218+0x01C8] → runtimeInventory.Data 指针
  +0x08/0x14  // Data+0x08=金币([0].amount), Data+0x14=灵魂([1].amount)
```

**局内路径** (PlayerState → playerProgressReplicated → runtimeInventory):
```
"FarFarWest-Win64-Shipping.exe"+0x92CCD20
  +0x0228 → +0x0038 → +0x0000 → +0x0030 → +0x0358 → +0x0960
  // → PlayerState
  +0x05C8     // [PlayerState+0x0400+0x01C8] → runtimeInventory.Data 指针
  +0x08/0x14  // Data+0x08=金币, Data+0x14=灵魂
```

| # | 用途 | 偏移序列 | CE 类型 | 说明 |
|---|------|----------|---------|------|
| 77 | [非局内] 金币数量 | +0x03E0, +0x08 | 4 Bytes | runtimeInventory[0].amount (假设Gold在[0]) |
| 78 | [非局内] 灵魂数量 | +0x03E0, +0x14 | 4 Bytes | runtimeInventory[1].amount (假设Soul在[1]) |
| 79 | [局内] 金币数量 | +0x05C8, +0x08, +0x0960, +0x0358, +0x0030, +0x0000, +0x0038, +0x0228 | 4 Bytes | PlayerState路径 |
| 80 | [局内] 灵魂数量 | +0x05C8, +0x14, +0x0960, +0x0358, +0x0030, +0x0000, +0x0038, +0x0228 | 4 Bytes | PlayerState路径 |
| 81 | [非局内] 人物XP | +0x0228, +0x03E0, +0x44 | 4 Bytes | runtimeInventory[5].amount |
| 82 | [局内] 人物XP | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0960, +0x05C8, +0x44 | 4 Bytes | PlayerState路径 |
> **注意**: 数组索引假设 [0]=Gold, [1]=Soul。若运行时值不对，可在 CE 中调整最后一个 Offset（每项+0x0C 步进）。
>
> **XP 说明**:
> - **人物XP** (ID 81-82): 存储在 `runtimeInventory[5].amount`（+0x44），可用指针链直接读取。
> - **技能总XP**: 存储在 `spellXp`（`TMap<E_DamageCategory, int32>`，FS_PlayerProgress+0x0048）中，TMap 内部用 TSparseArray 无法用固定指针链读取。
> - 等级由 `F_CalculateXpLevel(totalXp)` 动态计算，无法用指针链读取。

### 6.3 持久化背包 — TArray 遍历参考

`runtimeInventory` 是一个 `TArray<FS_RuntimeInventory>`，如需遍历完整背包内容，需用 Lua 脚本：

**Lua 脚本 — 读取金币/灵魂数量**:

```lua
-- 读取 runtimeInventory 中的金币/灵魂数量
-- 路径: GWorld → GameInstance → playerProgress → runtimeInventory

local gworld = readPointer("FarFarWest-Win64-Shipping.exe+0x92CCD20")
if not gworld or gworld == 0 then return end

local gameInstance = readPointer(gworld + 0x0228)
if not gameInstance or gameInstance == 0 then return end

-- playerProgress 在 CanyonGameInstance+0x0218
local playerProgressAddr = gameInstance + 0x0218

-- runtimeInventory 在 playerProgress+0x01C8
-- TArray: [Data(8)] [Count(4)] [Max(4)]
local invData = readPointer(playerProgressAddr + 0x01C8)
local invCount = readInteger(playerProgressAddr + 0x01D0)

-- FS_RuntimeInventory 大小 = 0x0C (FName 8B + int32 4B)
local ENTRY_SIZE = 0x0C

if invData and invCount and invCount > 0 then
    for i = 0, invCount - 1 do
        local entry = invData + i * ENTRY_SIZE
        -- FName: ComparisonIndex (int32) at +0x0000
        local nameIdx = readInteger(entry)
        local amount = readInteger(entry + 0x0008)
        -- FName 到字符串需要查 FNamePool，这里用索引推断
        -- 常见索引需要运行时对比确认
        print(string.format("Inventory[%d] FNameIdx=%d Amount=%d", i, nameIdx or 0, amount or 0))
    end
end

-- 另一条路径: 从 PlayerState 读取
-- ABP_PlayerState_C+0x0400 = playerProgressReplicated
-- +0x01C8 = runtimeInventory
-- 同样结构
```

---

## 7. 武器解锁 / 技能等级

### 7.1 武器解锁判断

游戏不存储显式的 `unlockedWeapons` 布尔列表。武器是否解锁通过 `runtimeInventory` 中是否存在对应 FName 条目来判断：

- `F_GetAmountFromInventory(Progress, "WeaponName")` → 检查是否包含且 amount > 0
- 如果返回 `contains = true`，则该武器已解锁

> 函数 `BPFL_PlayerDatas_C::F_GetAmountFromInventory` 是遍历 runtimeInventory 查找匹配 FName 的条目。

### 7.2 武器技能等级

`itemsUpgrades` (FS_PlayerProgress+0x0128) 为 `TMap<FName, FS_ItemTweaks>`，每个武器对应一个 FS_ItemTweaks，内部 `TMap<FName, int32>` 存储各项属性等级。

同样，`spellXp` (FS_PlayerProgress+0x0048) 为 `TMap<E_DamageCategory, int32>`，存储各元素技能的 XP 值，等级由 `F_CalculateXpLevel` 动态计算。

### 7.3 Lua 脚本 — 读取 spellXp 和 itemsUpgrades

```lua
-- 读取 spellXp (TMap<E_DamageCategory, int32>)
-- 路径: PlayerState+0x0400+0x0048

local function ReadTMapInt32(mapAddr)
    -- UE5 TMap layout: [Pairs(0x00)] [Count(0x08)] [Max(0x0C)]
    -- 实际: TSet<TPair<Key,Value>> 内部
    -- 简化: 读取 count 后遍历 pairs
    local count = readInteger(mapAddr + 0x08)
    -- TMap 内部使用 TSparseArray，布局复杂
    -- 需根据实际运行时结构分析
    return count
end

-- 注意: UE5 TMap 内部使用 TSparseArray + FScriptMapHelper
-- 内存布局非连续数组，无法简单遍历
-- 建议通过 GObjects 查找对应 SaveGame 对象来读取
```

> **重要**: UE5 的 `TMap` 内部使用 `TSparseArray`，内存布局不连续（有空洞），无法像 TArray 那样用固定索引访问。读取 TMap 内容需要 Lua 脚本解析其内部结构，或通过游戏函数间接调用。

---

## 8. 非局内路径 — CanyonGameInstance（大厅/菜单/持久化数据）

> **核心区别**: 本节路径**不需要进入对局**即可访问，适用于大厅、菜单等场景。与第 3-6 节的局内路径（通过 Pawn/PlayerState）不同，非局内路径通过 `CanyonGameInstance` 直接访问持久化进度。

### 8.1 访问路径

```
"FarFarWest-Win64-Shipping.exe"+0x92CCD20
  +0x0228     // UWorld → OwningGameInstance → UBP_CanyonGameInstance_C*
```

到达 **CanyonGameInstance** 后，直接追加偏移访问内联字段，**无需额外解引用**。

### 8.2 CanyonGameInstance 直属属性

在 CanyonGameInstance 指针链末尾追加以下偏移：

| 属性 | 追加偏移 | CE 类型 | 说明 |
|------|----------|---------|------|
| **runtimeDebugEnabled** | `+0x04A1` | **1 Byte** | **调试模式开关** (设1开启) |
| enableOnlineFeatures | `+0x04A2` | 1 Byte | 在线功能开关 |
| shouldPlayComebackCinematic | `+0x04A0` | 1 Byte | 播放回归过场 |
| **currentStormIntensity** | `+0x04A8` | **Double (8)** | **风暴强度** |
| sessionStarted | `+0x05D0` | 1 Byte | 会话已启动 |
| isGameTime | `+0x05B8` | 1 Byte | 是否游戏时间 |
| particleQuality | `+0x06C8` | 4 Bytes | 粒子质量 |
| playerColors | `+0x06E8` | 4 Bytes | 玩家颜色数 |

### 8.3 playerProgress（持久化进度，内联结构体）

CanyonGameInstance+0x0218 处为内联 `FS_PlayerProgress`（0x258 字节），**无需解引用**，直接追加偏移：

| 属性 | 计算偏移 | CE 类型 | 说明 |
|------|----------|---------|------|
| **selectedDifficulty** | `+0x022C` | **4 Bytes** | **当前难度** (0x0218+0x0014) |
| **unlockedDifficulties** | `+0x0300` | **4 Bytes** | **已解锁难度** (0x0218+0x00E8) |
| **specialWeaponElement** | `+0x0234` | **1 Byte** | **武器元素** (0x0218+0x001C) |
| selectedPlayerSkin | `+0x021C` | 8 Bytes | 角色皮肤 FName (0x0218+0x0004) |
| selectedMountSkin | `+0x0224` | 8 Bytes | 坐骑皮肤 FName (0x0218+0x000C) |
| selectedEmoteA | `+0x0304` | 8 Bytes | 表情A FName (0x0218+0x00EC) |
| selectedEmoteB | `+0x030C` | 8 Bytes | 表情B FName (0x0218+0x00F4) |
| selectedEmoteC | `+0x0314` | 8 Bytes | 表情C FName (0x0218+0x00FC) |
| selectedEmoteD | `+0x031C` | 8 Bytes | 表情D FName (0x0218+0x0104) |
| **selectedItemA** | `+0x0324` | **8 Bytes** | **主武器 FName** (0x0218+0x010C) |
| **selectedItemB** | `+0x032C` | **8 Bytes** | **副武器 FName** (0x0218+0x0114) |
| **selectedItemUtility** | `+0x0334` | **8 Bytes** | **工具 FName** (0x0218+0x011C) |
| selectedSpellA | `+0x0238` | 8 Bytes | 技能A UClass* (0x0218+0x0020) |
| selectedSpellB | `+0x0240` | 8 Bytes | 技能B UClass* (0x0218+0x0028) |
| selectedSpellC | `+0x0248` | 8 Bytes | 技能C UClass* (0x0218+0x0030) |
| **runtimeInventory** | `+0x03E0` | — (TArray) | **背包 (金币/灵魂)** (0x0218+0x01C8) 需Lua遍历 |
| **spellXp** | `+0x0260` | — (TMap) | **法术经验** (0x0218+0x0048) 需Lua遍历 |
| itemsUpgrades | `+0x0340` | — (TMap) | 武器升级 (0x0218+0x0128) 需Lua遍历 |
| challenges | `+0x03F0` | — (TMap) | 挑战进度 (0x0218+0x01D8) 需Lua遍历 |

### 8.4 playerItemsLocal（本地装备，内联结构体）

CanyonGameInstance+0x01E0 处为内联 `FS_PlayerSelectedItems`（0x38 字节），**无需解引用**：

| 属性 | 计算偏移 | CE 类型 | 说明 |
|------|----------|---------|------|
| wepA | `+0x01E0` | 8 Bytes | 主武器 FName |
| wepB | `+0x01E8` | 8 Bytes | 副武器 FName |
| grenade | `+0x01F0` | 8 Bytes | 手雷 FName |
| spellA | `+0x01F8` | 8 Bytes | 技能A UClass* |
| spellB | `+0x0200` | 8 Bytes | 技能B UClass* |
| spellC | `+0x0208` | 8 Bytes | 技能C UClass* |
| element | `+0x0210` | 1 Byte | 武器元素 E_DamageCategory |

### 8.5 CE 指针链 — 非局内路径速查

| # | 用途 | 偏移序列 (从上到下) | 类型 |
|---|------|---------------------|------|
| 49 | 调试模式开关 | +0x0228, +0x04A1 | Byte |
| 50 | 风暴强度 | +0x0228, +0x04A8 | Double |
| 51 | 当前难度 | +0x0228, +0x022C | 4 Bytes |
| 52 | 已解锁难度 | +0x0228, +0x0300 | 4 Bytes |
| 53 | 武器元素 | +0x0228, +0x0234 | Byte |
| 54 | 主武器选择 | +0x0228, +0x0324 | 8 Bytes |
| 55 | 副武器选择 | +0x0228, +0x032C | 8 Bytes |
| 56 | 工具选择 | +0x0228, +0x0334 | 8 Bytes |
| 57 | 角色皮肤 | +0x0228, +0x021C | 8 Bytes |
| 58 | 坐骑皮肤 | +0x0228, +0x0224 | 8 Bytes |
| 59 | 粒子质量 | +0x0228, +0x06C8 | 4 Bytes |
| 77 | [非局内] 金币数量 | +0x0228, +0x03E0, +0x08 | 4 Bytes | runtimeInventory[0] |
| 78 | [非局内] 灵魂数量 | +0x0228, +0x03E0, +0x14 | 4 Bytes | runtimeInventory[1] |


### 8.6 Lua 脚本 — 非局内背包遍历

```lua
-- 非局内路径读取 runtimeInventory 中的金币/灵魂数量
-- 路径: GWorld → GameInstance → playerProgress → runtimeInventory
-- 适用于大厅/菜单场景，无需进入对局

local gworld = readPointer("FarFarWest-Win64-Shipping.exe+0x92CCD20")
if not gworld or gworld == 0 then return end

local gameInstance = readPointer(gworld + 0x0228)
if not gameInstance or gameInstance == 0 then return end

-- playerProgress 在 CanyonGameInstance+0x0218 (内联，无需解引用)
local pp = gameInstance + 0x0218

-- runtimeInventory 在 playerProgress+0x01C8 (TArray<FS_RuntimeInventory>)
-- TArray layout: [Data(8)] [Count(4)] [Max(4)]
local invData = readPointer(pp + 0x01C8)
local invCount = readInteger(pp + 0x01D0)

local ENTRY_SIZE = 0x0C  -- FS_RuntimeInventory: FName(8) + int32(4)

local goldAmount = 0
local soulAmount = 0

if invData and invCount and invCount > 0 then
    for i = 0, invCount - 1 do
        local entry = invData + i * ENTRY_SIZE
        local nameIdx = readInteger(entry)        -- FName.ComparisonIndex
        local amount  = readInteger(entry + 0x0008)
        if amount then
            -- FName 索引需要与运行时对比确认
            -- 通常金币和灵魂的 FName 索引是固定的
            -- 可先打印所有条目确认索引值
            print(string.format("  [%d] FNameIdx=%d  Amount=%d", i, nameIdx or -1, amount))
        end
    end
end

-- 也从 PlayerState 路径读取 (仅限局内)
-- PlayerState+0x0400 = playerProgressReplicated
-- +0x01C8 = runtimeInventory (同样结构)
```

---

## 9. 移动系统 — CharacterMovementComponent（跳跃/速度/重力）

> 通过 Pawn → CharacterMovementComponent 访问 UE5 原生移动属性，可修改跳跃高度、行走速度、重力等。

### 9.1 访问路径

在 Pawn 指针链末尾追加 `+0x0338` 解引用到 CharacterMovementComponent，再追加字段偏移：

**完整前缀**:
```
"FarFarWest-Win64-Shipping.exe"+0x92CCD20
  +0x0228     // UWorld → OwningGameInstance
  +0x0038     // UGameInstance → LocalPlayers.Data
  +0x0000     // [0] → 第一个 ULocalPlayer*
  +0x0030     // ULocalPlayer → PlayerController
  +0x0358     // APlayerController → AcknowledgedPawn
  +0x0338     // ACharacter → CharacterMovement (解引用)
```

此处读出的是 **UCharacterMovementComponent\*** 指针，继续追加：

### 9.2 核心移动属性

| 属性 | 追加偏移 | CE 类型 | 说明 | 推荐修改值 |
|------|----------|---------|------|-----------|
| **GravityScale** | `+0x01A8` | **Float** | **重力缩放** (默认1.0) | 0.5=轻功, 0=无重力 |
| **JumpZVelocity** | `+0x01B0` | **Float** | **跳跃初速度** (控制跳跃高度) | 2000=超高跳 |
| JumpOffJumpZFactor | `+0x01B4` | Float | 跳离跳跃Z因子 | — |
| MaxStepHeight | `+0x01AC` | Float | 最大台阶高度 | 100=无台阶阻挡 |
| WalkableFloorAngle | `+0x01D4` | Float | 可行走角度 | 90=可走墙壁 |
| GroundFriction | `+0x0244` | Float | 地面摩擦力 | 0=冰面滑行 |
| **MaxWalkSpeed** | `+0x0288` | **Float** | **最大行走速度** | 2000=极速 |
| MaxWalkSpeedCrouched | `+0x028C` | Float | 蹲下最大速度 | — |
| MaxSwimSpeed | `+0x0290` | Float | 最大游泳速度 | — |
| **MaxFlySpeed** | `+0x0294` | **Float** | **最大飞行速度** | 2000=快速飞行 |
| MaxCustomMovementSpeed | `+0x0298` | Float | 自定义移动速度 | — |
| **MaxAcceleration** | `+0x029C` | **Float** | **最大加速度** | 9999=瞬间加速 |
| MinAnalogWalkSpeed | `+0x02A0` | Float | 最小模拟行走速度 | — |
| BrakingDecelerationWalking | `+0x02B0` | Float | 行走刹车减速度 | 0=不减速 |
| BrakingDecelerationFalling | `+0x02B4` | Float | 下落刹车减速度 | — |
| BrakingDecelerationFlying | `+0x02BC` | Float | 飞行刹车减速度 | — |
| **AirControl** | `+0x02C0` | **Float** | **空中控制力** (0~1+) | 1.0=完全空中控制 |
| AirControlBoostMultiplier | `+0x02C4` | Float | 空中控制增强倍数 | — |
| FallingLateralFriction | `+0x02CC` | Float | 下落横向摩擦 | — |
| CrouchedHalfHeight | `+0x02D0` | Float | 蹲下半高 | — |
| RotationRate | `+0x02E0` | — (FRotator) | 旋转速率 (3×Float) | — |
| Mass | `+0x0310` | Float | 质量 | — |
| **bCheatFlying** | `+0x0565 bit4` | **Bit** | **飞行作弊** | 设1=自由飞行 |

### 9.3 ACharacter 跳跃属性（Pawn 直属，无需经过 CharMoveComp）

在 Pawn 指针链末尾直接追加：

| 属性 | 追加偏移 | CE 类型 | 说明 |
|------|----------|---------|------|
| **JumpMaxCount** | `+0x0464` | **4 Bytes** | **最大跳跃次数** (默认1, 设2=二段跳, 99=无限跳) |
| JumpCurrentCount | `+0x0468` | 4 Bytes | 当前跳跃计数 |
| JumpKeyHoldTime | `+0x0454` | Float | 跳跃键按住时间 |
| JumpMaxHoldTime | `+0x0460` | Float | 跳跃最大按住时间 |
| bPressedJump | `+0x0450 bit3` | Bit | 跳跃键按下标志 |

### 9.4 CE 指针链 — 移动系统速查

| # | 用途 | 偏移序列 (从上到下) | 类型 |
|---|------|---------------------|------|
| 60 | 重力缩放 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0338, +0x01A8 | Float |
| 61 | 跳跃初速度 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0338, +0x01B0 | Float |
| 62 | 最大行走速度 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0338, +0x0288 | Float |
| 63 | 最大飞行速度 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0338, +0x0294 | Float |
| 64 | 最大加速度 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0338, +0x029C | Float |
| 65 | 空中控制力 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0338, +0x02C0 | Float |
| 66 | 行走刹车减速 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0338, +0x02B0 | Float |
| 67 | 最大台阶高度 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0338, +0x01AC | Float |
| 68 | 地面摩擦力 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0338, +0x0244 | Float |
| 69 | 最大跳跃次数 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0464 | 4 Bytes |
| 70 | 最大游泳速度 | +0x0228, +0x0038, +0x0000, +0x0030, +0x0358, +0x0338, +0x0290 | Float |

### 9.5 常用修改组合

| 效果 | 修改方案 |
|------|----------|
| **二段跳** | JumpMaxCount=2, JumpZVelocity=800 |
| **超级跳** | JumpZVelocity=2000, GravityScale=0.8 |
| **月球重力** | GravityScale=0.3 |
| **无重力飞行** | GravityScale=0, bCheatFlying=1, MaxFlySpeed=2000 |
| **极速奔跑** | MaxWalkSpeed=2000, MaxAcceleration=9999, BrakingDecelerationWalking=0 |
| **完全空中控制** | AirControl=1.0, FallingLateralFriction=0 |
| **冰面滑行** | GroundFriction=0, BrakingDecelerationWalking=0 |
| **穿墙跳** | MaxStepHeight=999, WalkableFloorAngle=90 |

---

## 10. 敌人属性（需遍历 Actors 数组）

敌人无法用固定指针链访问，需遍历 `UWorld → PersistentLevel → Actors[]` 并按类型筛选。

### 9.1 遍历路径

```
"FarFarWest-Win64-Shipping.exe"+0x92CCD20          // GWorld → UWorld*
  +0x0030                     // UWorld → PersistentLevel (ULevel*)
  +0x00A0                     // ULevel → Actors.Data (TArray<AActor*> Data指针)
  +0x????                     // [i] × 8 → 第 i 个 AActor*
```

> 第 i 个元素的偏移 = `i × 0x0008`（64 位指针大小）

### 9.2 敌人属性偏移（以 ABP_Enemy_C 基址为准）

| 属性 | 偏移 | CE 类型 | 说明 |
|------|------|---------|------|
| **health** | `+0x06B0` | **Double** | **当前血量** |
| **isInvincible** | `+0x08BC` | **Byte** | **无敌** |
| aiState | `+0x06B8` | 4 Bytes | AI 状态枚举 |
| detectRadius | `+0x06A8` | Float | 检测半径 |
| runSpeed | `+0x06E0` | Float | 奔跑速度 |
| walkSpeed | `+0x06E4` | Float | 行走速度 |
| defaultHealRuntime | `+0x0730` | Double | 默认运行时血量 |
| damageBoost | `+0x0808` | Double | 伤害增益 |
| **isBoss** | `+0x0828` | **Byte** | **是否 Boss** |
| currentDamageCap | `+0x0878` | Double | 伤害上限 |
| slowMultiplier | `+0x0888` | Double | 减速乘数 |
| currentSpeed | `+0x0890` | Double | 当前速度 |
| isConsideredAsFlyingEnemy | `+0x08A9` | Byte | 飞行敌人 |
| spawnLoc | `+0x0778` | — (FVector) | 生成位置 (3×Float) |

### 9.3 CE 遍历 Actors 的 Lua 脚本示例

```lua
-- FarFarWest: 遍历敌人并显示血量
local gworld = readPointer("FarFarWest-Win64-Shipping.exe+0x92CCD20")
if not gworld or gworld == 0 then return end

local level = readPointer(gworld + 0x0030)
if not level or level == 0 then return end

local actorsData = readPointer(level + 0x00A0)
local actorsCount = readInteger(level + 0x00A8)

if not actorsData or actorsCount == 0 then return end

for i = 0, actorsCount - 1 do
    local actor = readPointer(actorsData + i * 8)
    if actor and actor ~= 0 then
        -- 读取 UObject 的 UClass 来判断类型
        -- UObject+0x0010 = UClassPrivate
        local objClass = readPointer(actor + 0x0010)
        if objClass and objClass ~= 0 then
            -- 可通过类名筛选 ABP_Enemy_C
            local health = readDouble(actor + 0x06B0)
            local isBoss = readBytes(actor + 0x0828)
            if health and health > 0 then
                print(string.format("Actor[%d] HP=%.1f Boss=%d", i, health, isBoss or 0))
            end
        end
    end
end
```

---

## 11. 遍历所有玩家（通过 GameState → PlayerArray）

### 10.1 路径

```
"FarFarWest-Win64-Shipping.exe"+0x92CCD20          // GWorld → UWorld*
  +0x01B0                     // UWorld → GameState (AGameStateBase*)
  +0x02C8                     // GameState → PlayerArray.Data (TArray<APlayerState*> Data指针)
  +0x????                     // [i] × 8 → 第 i 个 APlayerState*
```

### 10.2 PlayerArray 中每个玩家可读属性

与本地玩家 PlayerState 字段一致（见 3.3 节），偏移相同：

| 属性 | 偏移 | CE 类型 |
|------|------|---------|
| 当前血量 | `+0x0658` | Double |
| 最大血量 | `+0x0660` | Double |
| 无敌 | `+0x08E0` | Byte |
| 主武器当前弹匣 | `+0x03BC` | 4 Bytes |
| 副武器当前弹匣 | `+0x03D0` | 4 Bytes |
| 手雷数量 | `+0x03E0` | 4 Bytes |

---

## 12. CE 添加指针链操作指南

1. 打开 CE，附加到游戏进程
2. 点击 **"Add Address Manually"**
3. 勾选 **"Pointer"** 复选框
4. **Base Address** 栏输入: `"FarFarWest-Win64-Shipping.exe"+0x92CCD20`
5. **Offset(s)** 栏从上到下依次填写偏移值（对应指针链从第一步到最后一步）
6. **Type** 选择对应数据类型：
   - `int32` → **4 Bytes**
   - `double` → **Double (8 Bytes)**
   - `bool` → **1 Byte**
   - `float` → **Float (4 Bytes)**
7. 点击 OK

> **注意**: 如果指针链显示 `??`，说明中间某层指针为空（游戏尚未初始化或当前不在对局中），进入对局后刷新即可。

---

## 13. 一键导入 — CE 地址列表文件

CT 表已提取为独立文件 **`FarFarWest.CT`**，双击即可用 Cheat Engine 打开，所有 68 个指针链自动加载。

> CT 文件包含 82 个条目：局内属性 1-44，非局内路径 45-59，移动系统 60-76，持久化金币/灵魂/人物XP 77-82。
