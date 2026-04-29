# Far Far West — UE5 SDK 核心基址 & 偏移手册

> **引擎版本**: UE 5.7 (Build 5.7.3-50162420)  
> **游戏**: Far Far West  

---

## 1. 全局入口地址

| 符号 | 偏移量 | 说明 |
|------|--------|------|
| **GObjects** | `0x9141F60` | 全局对象数组，遍历游戏内所有 UObject（玩家、武器、载具等） |
| **GWorld** | `0x92CCD20` | 游戏世界指针，可获取 PersistentLevel 及所有 Actors |
| **ProcessEvent** | `0x13FCEE0` | 事件处理函数，调用游戏内部函数（开火、传送、修改属性等）的关键接口 |

---

## 2. 核心类层级与静态信息

| 类名 | 继承自 | 实例大小 |
|------|--------|----------|
| `ABP_Player_C` | `ACharacter` | `0x0B70` |
| `ABP_PlayerState_C` | `APlayerState` | `0x09B0` |
| `ABP_Enemy_C` | `ACharacter` | `0x08E0` |
| `ABP_PlayerGhost_C` | `ACharacter` | — |

---

## 3. ABP_Player_C — 玩家角色 (大小 0x0B70)

### 3.1 位置 / 移动

| 偏移 | 类型 | 属性名 | 说明 |
|------|------|--------|------|
| `0x06E0` | FTransform | `clientTransform` | 网络同步位置/旋转 |
| `0x0748` | FVector2D | `iaMove` | 移动输入 |
| `0x0758` | FVector2D | `moveInputsReplicated` | 网络同步移动输入 |
| `0x0768` | FVector2D | `iaLook` | 视角输入 |
| `0x0778` | FRotator | `cameraRotation` | 摄像机旋转 |
| `0x0848` | FVector | `receivedPlayerLocation` | 网络同步位置 |
| `0x09A8` | double | `buffMoveSpeed` | 增益移动速度 |

### 3.2 战斗 / 状态

| 偏移 | 类型 | 属性名 | 说明 |
|------|------|--------|------|
| `0x07A8` | bool | `isStunned` | 被击晕 |
| `0x07C8` | int32 | `killCombo` | 连杀数 |
| `0x07E8` | bool | `isRolling` | 翻滚中 |
| `0x0840` | int32 | `maxDash` | 最大冲刺次数 |
| `0x0844` | int32 | `currentDashLeft` | 剩余冲刺次数 |
| `0x0891` | bool | `invincibleFrame` | 无敌帧 |
| `0x08D8` | double | `stability` | 瞄准稳定性 |
| `0x08E0` | double | `weaponRecoil` | 武器后坐力 |
| `0x0940` | double | `dashCooldown` | 冲刺冷却 |
| `0x0950` | ABP_GameState_C* | `GameState` | 游戏状态引用 |
| **`0x0960`** | **ABP_PlayerState_C*** | **`localPlayerState`** | **玩家状态引用 (血量/弹药在此)** |
| `0x0958` | bool | `canUseWeapons` | 能否使用武器 |
| `0x0A2A` | bool | `spawnProtectionInvincible` | 出生保护无敌 |
| `0x0A4C` | int32 | `jumpsLeft` | 剩余跳跃次数 |
| `0x0B31` | bool | `tempoInvicible` | 临时无敌 |

---

## 4. ABP_PlayerState_C — 玩家状态 (大小 0x09B0) ⭐ 最关键

### 4.1 血量 / 生命

| 偏移 | 类型 | 属性名 | 说明 |
|------|------|--------|------|
| **`0x0658`** | **double** | **`playerHeal`** | **当前血量 (Net同步)** |
| **`0x0660`** | **double** | **`playerMaxHeal`** | **最大血量 (Net同步)** |
| `0x0668` | double | `playerSize` | 玩家尺寸 (Net同步) |
| **`0x08E0`** | **bool** | **`invincible`** | **无敌模式 (Net同步)** |

### 4.2 武器 / 弹药

| 偏移 | 类型 | 属性名 | 说明 |
|------|------|--------|------|
| `0x0380` | FS_PlayerSelectedItems | `selectedPlayerItemsServer` | 装备武器/技能 (Net同步) |
| `0x03B8` | FS_PlayerRuntimeStats | `playerRuntimeStats` | 弹药/技能冷却 |
| `0x0670` | UClass* | `CurrentItem` | 当前物品 |
| `0x08A8` | double | `tweakCooldownReduction` | 冷却缩减 |
| `0x08E8` | UClass* | `spellARuntime` | 技能A |
| `0x08F0` | UClass* | `spellBRuntime` | 技能B |
| `0x08F8` | UClass* | `spellCRuntime` | 技能C |

---

## 5. 子结构体偏移

### 5.1 FS_PlayerRuntimeStats（位于 `ABP_PlayerState_C+0x03B8`）

| 偏移 | 类型 | 属性名 | 说明 |
|------|------|--------|------|
| `+0x0000` | FS_ItemDatas | `datasWepA` | 主武器弹药 |
| `+0x0014` | FS_ItemDatas | `datasWepB` | 副武器弹药 |
| `+0x0028` | int32 | `amountGrenades` | 手雷数量 |
| `+0x0030` | double | `cooldownSpellA` | 技能A冷却 |
| `+0x0038` | double | `cooldownSpellB` | 技能B冷却 |
| `+0x0040` | double | `cooldownSpellC` | 技能C冷却 |

### 5.2 FS_ItemDatas（0x14 字节）

| 偏移 | 类型 | 属性名 |
|------|------|--------|
| `+0x0000` | bool | `isInitialized` |
| `+0x0004` | int32 | `currentMagazineAmmos` |
| `+0x0008` | int32 | `maxMagazineAmmos` |
| `+0x000C` | int32 | `currentTotalAmmos` |
| `+0x0010` | int32 | `maxTotalAmmos` |

> **弹药绝对偏移示例**（从 PlayerState 实例基址算起）：
> - 主武器当前弹匣: `0x03B8 + 0x0000 + 0x0004` = `0x03BC`
> - 副武器总弹药: `0x03B8 + 0x0014 + 0x000C` = `0x03D8`

---

## 6. ABP_Enemy_C — 敌人 (大小 0x08E0)

| 偏移 | 类型 | 属性名 | 说明 |
|------|------|--------|------|
| `0x06A0` | AActor* | `attackTarget` | 攻击目标 (Net同步) |
| `0x06A8` | float | `detectRadius` | 检测半径 |
| **`0x06B0`** | **double** | **`health`** | **当前血量 (Net同步)** |
| `0x06B8` | E_AIState | `aiState` | AI状态 |
| `0x06E0` | float | `runSpeed` | 奔跑速度 |
| `0x06E4` | float | `walkSpeed` | 行走速度 |
| `0x0730` | double | `defaultHealRuntime` | 默认运行时血量 (Net同步) |
| `0x0778` | FVector | `spawnLoc` | 生成位置 |
| `0x0808` | double | `damageBoost` | 伤害增益 |
| `0x0828` | bool | `isBoss` | 是否Boss |
| `0x0878` | double | `currentDamageCap` | 伤害上限 (Net同步) |
| `0x0888` | double | `slowMultiplier` | 减速乘数 |
| `0x0890` | double | `currentSpeed` | 当前速度 |
| `0x08A9` | bool | `isConsideredAsFlyingEnemy` | 飞行敌人 |
| **`0x08BC`** | **bool** | **`isInvincible`** | **无敌 (Net同步)** |

---

## 7. 运行时访问路径

### 7.1 本地玩家

```
GWorld (0x92CCD20)
  └─> UGameInstance
       └─> ULocalPlayer
            └─> APlayerController
                 └─> Pawn ────────────────────── ABP_Player_C
                      ├─ +0x06E0  clientTransform   (位置/旋转)
                      ├─ +0x0778  cameraRotation    (摄像机旋转)
                      ├─ +0x0844  currentDashLeft   (剩余冲刺)
                      └─ +0x0960  localPlayerState  ── ABP_PlayerState_C
                           ├─ +0x0658  playerHeal       (当前血量)
                           ├─ +0x0660  playerMaxHeal    (最大血量)
                           ├─ +0x08E0  invincible       (无敌)
                           └─ +0x03B8  playerRuntimeStats
                                ├─ +0x0000  datasWepA   (主武器弹药)
                                ├─ +0x0014  datasWepB   (副武器弹药)
                                ├─ +0x0028  amountGrenades (手雷)
                                ├─ +0x0030  cooldownSpellA
                                ├─ +0x0038  cooldownSpellB
                                └─ +0x0040  cooldownSpellC
```

### 7.2 遍历所有玩家

```
GWorld (0x92CCD20)
  └─> GameState
       └─> PlayerArray[]
            └─> ABP_PlayerState_C  (每个玩家一份)
```

### 7.3 遍历所有敌人

```
GWorld (0x92CCD20)
  └─> PersistentLevel
       └─> Actors[]
            └─> ABP_Enemy_C
                 ├─ +0x06B0  health        (敌人血量)
                 ├─ +0x08BC  isInvincible  (敌人无敌)
                 ├─ +0x06B8  aiState       (AI状态)
                 └─ +0x0828  isBoss        (是否Boss)
```

### 7.4 通过 GObjects 查找特定对象

```
GObjects (0x9141F60)
  └─> 遍历对象数组
       └─> 通过 UClass 筛选:
            - ABP_Player_C       → 玩家实例
            - ABP_PlayerState_C  → 玩家状态
            - ABP_Enemy_C        → 敌人实例
```

---

## 8. 常用偏移速查

| 用途 | 访问链 | 最终偏移 |
|------|--------|----------|
| 本地玩家位置 | Pawn->clientTransform | `Pawn + 0x06E0` |
| 当前血量 | PlayerState->playerHeal | `Pawn + 0x0960 → + 0x0658` |
| 最大血量 | PlayerState->playerMaxHeal | `Pawn + 0x0960 → + 0x0660` |
| 无敌开关 | PlayerState->invincible | `Pawn + 0x0960 → + 0x08E0` |
| 主武器弹匣 | PlayerState->runtimeStats->datasWepA.currentMagazineAmmos | `Pawn + 0x0960 → + 0x03B8 + 0x0004` |
| 主武器总弹药 | PlayerState->runtimeStats->datasWepA.currentTotalAmmos | `Pawn + 0x0960 → + 0x03B8 + 0x000C` |
| 副武器弹匣 | PlayerState->runtimeStats->datasWepB.currentMagazineAmmos | `Pawn + 0x0960 → + 0x03B8 + 0x0014 + 0x0004` |
| 手雷数量 | PlayerState->runtimeStats->amountGrenades | `Pawn + 0x0960 → + 0x03B8 + 0x0028` |
| 敌人血量 | Enemy->health | `Enemy + 0x06B0` |
| 敌人无敌 | Enemy->isInvincible | `Enemy + 0x08BC` |
| 冲刺剩余 | Pawn->currentDashLeft | `Pawn + 0x0844` |
| 连杀数 | Pawn->killCombo | `Pawn + 0x07C8` |
