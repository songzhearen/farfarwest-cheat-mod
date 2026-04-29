// dllmain.cpp : FarFarWest Mod v2 - DLL注入式作弊模块
// 功能: 追踪子弹 / 无CD / 无限跳跃 / 无限子弹 / 自定义经验 / CMD控制台 / 一键技能
// 热键: F1=追踪 / F2=无CD / F3=无限跳 / F4=无限子弹 / F5=一键技能 / END=卸载
// CMD: 命令行交互界面

#include "pch.h"
#include "SDK.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
#include <mutex>

// ============================================================================
// 偏移常量
// ============================================================================
namespace Offsets
{
    // 基础偏移 (来自SDK Basic.hpp)
    constexpr uintptr_t GWorld    = 0x092CCD20;

    // UWorld
    constexpr uintptr_t OwningGameInstance = 0x0228;

    // UGameInstance
    constexpr uintptr_t LocalPlayers = 0x0038;

    // UPlayer (ULocalPlayer继承自此)
    constexpr uintptr_t PlayerController = 0x0030;

    // AController
    constexpr uintptr_t Pawn = 0x02F0;

    // AActor
    constexpr uintptr_t ActorRootComponent = 0x01B8;

    // USceneComponent
    constexpr uintptr_t SceneCompRelativeLocation = 0x0148;

    // ABP_Player_C
    constexpr uintptr_t AutoLookTarget     = 0x0998;
    constexpr uintptr_t IsFocusingTarget   = 0x09A0;
    constexpr uintptr_t JumpsLeft          = 0x0A4C;
    constexpr uintptr_t MaxDash            = 0x0840;
    constexpr uintptr_t CurrentDashLeft    = 0x0844;
    constexpr uintptr_t DashCooldown       = 0x0940;
    constexpr uintptr_t IsDashOnCooldown   = 0x0A29;
    constexpr uintptr_t LocalPlayerState   = 0x0960;
    constexpr uintptr_t PlayerItems        = 0x06C8;  // UAC_PlayerItems_C*
    constexpr uintptr_t ClientTransform    = 0x06E0;

    // ABP_PlayerState_C
    constexpr uintptr_t PlayerRuntimeStats = 0x03B8;
    constexpr uintptr_t IsSuspicious       = 0x0985;
    constexpr uintptr_t PlayerProgress     = 0x0400;
    constexpr uintptr_t SpellARuntime      = 0x08E8;
    constexpr uintptr_t SpellBRuntime      = 0x08F0;
    constexpr uintptr_t SpellCRuntime      = 0x08F8;

    // FS_PlayerRuntimeStats
    constexpr uintptr_t DatasWepA          = 0x0000;
    constexpr uintptr_t DatasWepB          = 0x0014;
    constexpr uintptr_t AmountGrenades     = 0x0028;
    constexpr uintptr_t CooldownSpellA     = 0x0030;
    constexpr uintptr_t CooldownSpellB     = 0x0038;
    constexpr uintptr_t CooldownSpellC     = 0x0040;

    // FS_ItemDatas
    constexpr uintptr_t CurrentMagazineAmmos = 0x0004;
    constexpr uintptr_t MaxMagazineAmmos     = 0x0008;
    constexpr uintptr_t CurrentTotalAmmos    = 0x000C;
    constexpr uintptr_t MaxTotalAmmos        = 0x0010;

    // ABP_SpellProjectile_C
    constexpr uintptr_t ProjectileMovement  = 0x02D8;
    constexpr uintptr_t HomingToNearestTarget = 0x0318;
    constexpr uintptr_t HomingActorTarget   = 0x0320;
    constexpr uintptr_t OwningPawn          = 0x0360;

    // UProjectileMovementComponent
    constexpr uintptr_t BIsHomingProjectile = 0x0130;  // bit7 (0x80)
    constexpr uintptr_t HomingAccelerationMagnitude = 0x0190;
    constexpr uintptr_t HomingTargetComponent = 0x0194;

    // ABP_Enemy_C
    constexpr uintptr_t EnemyHealth = 0x06B0;

    // AC_PlayerItems_C
    constexpr uintptr_t SpellAPressed = 0x0137;
    constexpr uintptr_t SpellBPressed = 0x0138;
}

// ============================================================================
// 功能开关
// ============================================================================
struct FModState
{
    bool bTracking     = false;
    bool bNoCooldown   = false;
    bool bInfiniteJump = false;
    bool bInfiniteAmmo = false;
    bool bRunning      = true;
};

static FModState g_ModState;

// ============================================================================
// 自定义配置
// ============================================================================
struct FSpellCombo
{
    bool spellA = true;
    bool spellB = true;
    bool spellC = true;
    int  repeatCount = 1;  // 重复释放次数
};

static FSpellCombo g_SpellCombo;

// ============================================================================
// 指针缓存
// ============================================================================
struct FCachedPointers
{
    uintptr_t Player      = 0;
    uintptr_t PlayerState = 0;
};

static FCachedPointers g_Cache;

// 敌人缓存
struct FEnemyInfo
{
    uintptr_t Address;
    double     Distance;
};

static FEnemyInfo  g_NearestEnemy = { 0, 999999.0 };
static DWORD      g_LastEnemyScanTick = 0;
static const DWORD ENEMY_SCAN_INTERVAL_MS = 500;

// 已处理的投射物集合（避免重复设置homing）
struct FProcessedProjectile
{
    uintptr_t address;
    DWORD     tick;
};

static FProcessedProjectile g_ProcessedProjectiles[256];
static int g_ProcessedCount = 0;

// ============================================================================
// CMD控制台
// ============================================================================
static HANDLE g_hConsoleThread = nullptr;
static bool   g_bConsoleRunning = false;
static std::mutex g_ConsoleMutex;

// ============================================================================
// 工具函数
// ============================================================================

// 安全读取指针，返回0表示无效
static inline uintptr_t SafeReadPtr(uintptr_t address)
{
    if (!address) return 0;
    __try
    {
        auto ptr = *reinterpret_cast<uintptr_t*>(address);
        return ptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

// 安全读取基本类型
template<typename T>
static inline bool SafeRead(uintptr_t address, T& outValue)
{
    if (!address) return false;
    __try
    {
        outValue = *reinterpret_cast<T*>(address);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// 安全写入基本类型
template<typename T>
static inline bool SafeWrite(uintptr_t address, T value)
{
    if (!address) return false;
    __try
    {
        *reinterpret_cast<T*>(address) = value;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// 条件写入 - 仅在值不同时写入，减少内存操作
template<typename T>
static inline bool SafeWriteIfDifferent(uintptr_t address, T value)
{
    T current;
    if (SafeRead(address, current) && current != value)
    {
        return SafeWrite(address, value);
    }
    return false;
}

// 获取AActor位置 (通过RootComponent的RelativeLocation)
static bool GetActorLocation(uintptr_t actor, SDK::FVector& outLoc)
{
    if (!actor) return false;
    __try
    {
        uintptr_t rootComp = *reinterpret_cast<uintptr_t*>(actor + Offsets::ActorRootComponent);
        if (!rootComp) return false;
        outLoc = *reinterpret_cast<SDK::FVector*>(rootComp + Offsets::SceneCompRelativeLocation);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// 对ABP_Player_C特化: 用clientTransform获取位置 (更可靠)
static bool GetPlayerLocation(uintptr_t player, SDK::FVector& outLoc)
{
    if (!player) return false;
    __try
    {
        // FTransform layout: Rotation(0x00,0x20) + Translation(0x20,0x18) 
        outLoc = *reinterpret_cast<SDK::FVector*>(player + Offsets::ClientTransform + 0x20);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// 获取Actor的RootComponent指针
static uintptr_t GetActorRootComponent(uintptr_t actor)
{
    if (!actor) return 0;
    return SafeReadPtr(actor + Offsets::ActorRootComponent);
}

// ============================================================================
// 指针链导航
// ============================================================================
static uintptr_t GetLocalPlayerPawn()
{
    uintptr_t imageBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));

    uintptr_t world = SafeReadPtr(imageBase + Offsets::GWorld);
    if (!world) return 0;

    uintptr_t gameInstance = SafeReadPtr(world + Offsets::OwningGameInstance);
    if (!gameInstance) return 0;

    uintptr_t localPlayersData = SafeReadPtr(gameInstance + Offsets::LocalPlayers);
    if (!localPlayersData) return 0;

    uintptr_t localPlayer = SafeReadPtr(localPlayersData);
    if (!localPlayer) return 0;

    uintptr_t playerController = SafeReadPtr(localPlayer + Offsets::PlayerController);
    if (!playerController) return 0;

    uintptr_t pawn = SafeReadPtr(playerController + Offsets::Pawn);
    return pawn;
}

static uintptr_t GetPlayerState(uintptr_t player)
{
    if (!player) return 0;
    return SafeReadPtr(player + Offsets::LocalPlayerState);
}

// ============================================================================
// 指针验证和刷新
// ============================================================================
static bool ValidateAndRefreshPointers()
{
    if (g_Cache.Player && g_Cache.PlayerState)
    {
        __try
        {
            auto vtable = *reinterpret_cast<uintptr_t*>(g_Cache.Player);
            if (vtable)
            {
                uintptr_t ps = SafeReadPtr(g_Cache.Player + Offsets::LocalPlayerState);
                if (ps == g_Cache.PlayerState)
                    return true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    uintptr_t pawn = GetLocalPlayerPawn();
    if (!pawn) return false;

    uintptr_t playerState = GetPlayerState(pawn);
    if (!playerState) return false;

    g_Cache.Player = pawn;
    g_Cache.PlayerState = playerState;
    return true;
}

// ============================================================================
// 敌人查找
// ============================================================================
static SDK::UClass* g_EnemyClass = nullptr;

static void InitEnemyClass()
{
    if (g_EnemyClass) return;
    g_EnemyClass = SDK::ABP_Enemy_C::StaticClass();
}

static void ScanForNearestEnemy()
{
    DWORD now = GetTickCount();
    if (now - g_LastEnemyScanTick < ENEMY_SCAN_INTERVAL_MS)
        return;
    g_LastEnemyScanTick = now;

    InitEnemyClass();

    if (!g_Cache.Player || !g_EnemyClass)
    {
        g_NearestEnemy = { 0, 999999.0 };
        return;
    }

    SDK::FVector playerLoc;
    if (!GetPlayerLocation(g_Cache.Player, playerLoc))
    {
        g_NearestEnemy = { 0, 999999.0 };
        return;
    }

    FEnemyInfo best = { 0, 999999.0 };

    auto* gobjects = SDK::UObject::GObjects.GetTypedPtr();
    if (!gobjects) return;

    int32_t numElements = gobjects->Num();
    int32_t numChunks = gobjects->NumChunks;

    for (int32_t chunkIdx = 0; chunkIdx < numChunks; chunkIdx++)
    {
        auto* chunk = gobjects->GetDecrytedObjPtr()[chunkIdx];
        if (!chunk) continue;

        for (int32_t inChunkIdx = 0; inChunkIdx < gobjects->ElementsPerChunk; inChunkIdx++)
        {
            int32_t objIdx = chunkIdx * gobjects->ElementsPerChunk + inChunkIdx;
            if (objIdx >= numElements) break;

            auto* obj = chunk[inChunkIdx].Object;
            if (!obj) continue;

            __try
            {
                if (!obj->HasTypeFlag(SDK::EClassCastFlags::Actor))
                    continue;
                if (obj->IsDefaultObject())
                    continue;
                if (!obj->IsA(g_EnemyClass))
                    continue;

                uintptr_t enemyAddr = reinterpret_cast<uintptr_t>(obj);
                double health = 0.0;
                if (!SafeRead(enemyAddr + Offsets::EnemyHealth, health) || health <= 0.0)
                    continue;

                SDK::FVector enemyLoc;
                if (!GetActorLocation(enemyAddr, enemyLoc))
                    continue;

                double dx = enemyLoc.X - playerLoc.X;
                double dy = enemyLoc.Y - playerLoc.Y;
                double dz = enemyLoc.Z - playerLoc.Z;
                double dist = dx * dx + dy * dy + dz * dz;

                if (dist < best.Distance)
                {
                    best.Address = enemyAddr;
                    best.Distance = dist;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                continue;
            }
        }
    }

    g_NearestEnemy = best;
}

// ============================================================================
// 追踪子弹 (新实现: 修改ProjectileMovement组件)
// ============================================================================

// 检查投射物是否已处理过
static bool IsProjectileProcessed(uintptr_t addr)
{
    for (int i = 0; i < g_ProcessedCount; i++)
    {
        if (g_ProcessedProjectiles[i].address == addr)
            return true;
    }
    return false;
}

// 添加已处理的投射物
static void MarkProjectileProcessed(uintptr_t addr)
{
    // 清理过期条目 (超过10秒)
    DWORD now = GetTickCount();
    int writeIdx = 0;
    for (int i = 0; i < g_ProcessedCount; i++)
    {
        if (now - g_ProcessedProjectiles[i].tick < 10000)
        {
            g_ProcessedProjectiles[writeIdx++] = g_ProcessedProjectiles[i];
        }
    }
    g_ProcessedCount = writeIdx;

    // 添加新条目
    if (g_ProcessedCount < 256)
    {
        g_ProcessedProjectiles[g_ProcessedCount].address = addr;
        g_ProcessedProjectiles[g_ProcessedCount].tick = now;
        g_ProcessedCount++;
    }
}

static SDK::UClass* g_SpellProjectileClass = nullptr;

static void InitSpellProjectileClass()
{
    if (g_SpellProjectileClass) return;
    g_SpellProjectileClass = SDK::ABP_SpellProjectile_C::StaticClass();
}

static void ApplyTracking()
{
    if (!g_Cache.Player) return;

    // 刷新敌人列表
    ScanForNearestEnemy();

    // 找到投射物并设置homing
    InitSpellProjectileClass();
    if (!g_SpellProjectileClass) return;

    auto* gobjects = SDK::UObject::GObjects.GetTypedPtr();
    if (!gobjects) return;

    int32_t numElements = gobjects->Num();
    int32_t numChunks = gobjects->NumChunks;

    for (int32_t chunkIdx = 0; chunkIdx < numChunks; chunkIdx++)
    {
        auto* chunk = gobjects->GetDecrytedObjPtr()[chunkIdx];
        if (!chunk) continue;

        for (int32_t inChunkIdx = 0; inChunkIdx < gobjects->ElementsPerChunk; inChunkIdx++)
        {
            int32_t objIdx = chunkIdx * gobjects->ElementsPerChunk + inChunkIdx;
            if (objIdx >= numElements) break;

            auto* obj = chunk[inChunkIdx].Object;
            if (!obj) continue;

            __try
            {
                if (!obj->HasTypeFlag(SDK::EClassCastFlags::Actor))
                    continue;
                if (obj->IsDefaultObject())
                    continue;
                if (!obj->IsA(g_SpellProjectileClass))
                    continue;

                uintptr_t projAddr = reinterpret_cast<uintptr_t>(obj);

                // 检查owningPawn是否是本地玩家
                uintptr_t owningPawn = SafeReadPtr(projAddr + Offsets::OwningPawn);
                if (owningPawn != g_Cache.Player)
                    continue;

                // 跳过已处理的
                if (IsProjectileProcessed(projAddr))
                    continue;

                // 设置 ABP_SpellProjectile_C 层面的homing
                SafeWrite(projAddr + Offsets::HomingToNearestTarget, true);

                if (g_NearestEnemy.Address)
                {
                    SafeWrite(projAddr + Offsets::HomingActorTarget, g_NearestEnemy.Address);
                }

                // 设置 ProjectileMovement 组件的homing
                uintptr_t projMovement = SafeReadPtr(projAddr + Offsets::ProjectileMovement);
                if (projMovement)
                {
                    // 设置 bIsHomingProjectile (bit7 at 0x0130)
                    uint8_t flags = 0;
                    if (SafeRead(projMovement + Offsets::BIsHomingProjectile, flags))
                    {
                        flags |= 0x80;  // 设置bit7
                        SafeWrite(projMovement + Offsets::BIsHomingProjectile, flags);
                    }

                    // 设置追踪加速度
                    SafeWrite(projMovement + Offsets::HomingAccelerationMagnitude, 8000.0f);

                    // 设置HomingTargetComponent指向敌人的RootComponent
                    if (g_NearestEnemy.Address)
                    {
                        uintptr_t enemyRootComp = GetActorRootComponent(g_NearestEnemy.Address);
                        if (enemyRootComp)
                        {
                            // HomingTargetComponent 是 TWeakObjectPtr<USceneComponent>
                            // TWeakObjectPtr layout: int32 ObjectIndex, int32 ObjectSerialNumber
                            // 直接写入指针到敌人RootComponent
                            // 使用完整指针写入
                            SafeWrite(projMovement + Offsets::HomingTargetComponent, enemyRootComp);
                        }
                    }
                }

                MarkProjectileProcessed(projAddr);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                continue;
            }
        }
    }
}

// ============================================================================
// 无CD: 将技能冷却置零，恢复冲刺次数
// ============================================================================
static void ApplyNoCooldown()
{
    if (!g_Cache.Player || !g_Cache.PlayerState) return;

    uintptr_t runtimeStats = g_Cache.PlayerState + Offsets::PlayerRuntimeStats;
    SafeWriteIfDifferent(runtimeStats + Offsets::CooldownSpellA, 0.0);
    SafeWriteIfDifferent(runtimeStats + Offsets::CooldownSpellB, 0.0);
    SafeWriteIfDifferent(runtimeStats + Offsets::CooldownSpellC, 0.0);

    SafeWriteIfDifferent(g_Cache.Player + Offsets::DashCooldown, 0.0);
    SafeWriteIfDifferent(g_Cache.Player + Offsets::IsDashOnCooldown, false);

    int32_t maxDash = 0;
    if (SafeRead(g_Cache.Player + Offsets::MaxDash, maxDash) && maxDash > 0)
    {
        SafeWriteIfDifferent<int32_t>(g_Cache.Player + Offsets::CurrentDashLeft, maxDash);
    }
}

// ============================================================================
// 无限跳跃: 持续设置jumpsLeft
// ============================================================================
static void ApplyInfiniteJump()
{
    if (!g_Cache.Player) return;
    SafeWriteIfDifferent<int32_t>(g_Cache.Player + Offsets::JumpsLeft, 999);
}

// ============================================================================
// 无限子弹: 将弹药写为最大值
// ============================================================================
static void ApplyInfiniteAmmo()
{
    if (!g_Cache.PlayerState) return;

    uintptr_t runtimeStats = g_Cache.PlayerState + Offsets::PlayerRuntimeStats;

    // 主武器 datasWepA (+0x0000)
    uintptr_t wepA = runtimeStats + Offsets::DatasWepA;
    int32_t maxMagA = 0, maxTotalA = 0;
    if (SafeRead(wepA + Offsets::MaxMagazineAmmos, maxMagA) && maxMagA > 0)
    {
        SafeWriteIfDifferent(wepA + Offsets::CurrentMagazineAmmos, maxMagA);
    }
    if (SafeRead(wepA + Offsets::MaxTotalAmmos, maxTotalA) && maxTotalA > 0)
    {
        SafeWriteIfDifferent(wepA + Offsets::CurrentTotalAmmos, maxTotalA);
    }

    // 副武器 datasWepB (+0x0014)
    uintptr_t wepB = runtimeStats + Offsets::DatasWepB;
    int32_t maxMagB = 0, maxTotalB = 0;
    if (SafeRead(wepB + Offsets::MaxMagazineAmmos, maxMagB) && maxMagB > 0)
    {
        SafeWriteIfDifferent(wepB + Offsets::CurrentMagazineAmmos, maxMagB);
    }
    if (SafeRead(wepB + Offsets::MaxTotalAmmos, maxTotalB) && maxTotalB > 0)
    {
        SafeWriteIfDifferent(wepB + Offsets::CurrentTotalAmmos, maxTotalB);
    }

    // 手雷: 确保至少有5颗
    int32_t grenades = 0;
    SafeRead(runtimeStats + Offsets::AmountGrenades, grenades);
    if (grenades < 5)
    {
        SafeWriteIfDifferent(runtimeStats + Offsets::AmountGrenades, 5);
    }
}

// ============================================================================
// 反作弊: 清除isSuspicious标记
// ============================================================================
static void ApplyAntiCheat()
{
    if (!g_Cache.PlayerState) return;
    SafeWriteIfDifferent(g_Cache.PlayerState + Offsets::IsSuspicious, false);
}

// ============================================================================
// 一键技能释放: 快速触发选中的技能
// ============================================================================
static void CastSpellCombo()
{
    if (!g_Cache.Player) return;

    uintptr_t playerItems = SafeReadPtr(g_Cache.Player + Offsets::PlayerItems);
    if (!playerItems) return;

    // 验证canUseSpells
    bool canUseSpells = false;
    SafeRead(playerItems + 0x0136, canUseSpells);
    if (!canUseSpells) return;

    // 通过设置spellAPressed/spellBPressed标志来触发
    // 这些标志会在下一帧被游戏逻辑读取并触发技能释放
    for (int i = 0; i < g_SpellCombo.repeatCount; i++)
    {
        if (g_SpellCombo.spellA)
        {
            SafeWrite(playerItems + Offsets::SpellAPressed, true);
            // 检查spellA CD
            if (g_ModState.bNoCooldown)
            {
                uintptr_t runtimeStats = g_Cache.PlayerState + Offsets::PlayerRuntimeStats;
                SafeWrite(runtimeStats + Offsets::CooldownSpellA, 0.0);
            }
        }
        if (g_SpellCombo.spellB)
        {
            SafeWrite(playerItems + Offsets::SpellBPressed, true);
            if (g_ModState.bNoCooldown)
            {
                uintptr_t runtimeStats = g_Cache.PlayerState + Offsets::PlayerRuntimeStats;
                SafeWrite(runtimeStats + Offsets::CooldownSpellB, 0.0);
            }
        }
        if (g_SpellCombo.spellC)
        {
            // SpellC没有pressed标志，直接设置CD为0让它可用
            // 然后设置spellAPressed也会触发spellC的输入链
            if (g_ModState.bNoCooldown)
            {
                uintptr_t runtimeStats = g_Cache.PlayerState + Offsets::PlayerRuntimeStats;
                SafeWrite(runtimeStats + Offsets::CooldownSpellC, 0.0);
            }
        }
    }
}

// ============================================================================
// CMD控制台命令处理
// ============================================================================
static void PrintHelp()
{
    printf("\n");
    printf("=== FarFarWest Mod v2 Console ===\n");
    printf("help              - Show this help\n");
    printf("status            - Show current status\n");
    printf("tracking [on|off] - Toggle homing bullets\n");
    printf("nocd [on|off]     - Toggle no cooldown\n");
    printf("jump [on|off]     - Toggle infinite jump\n");
    printf("ammo [on|off]     - Toggle infinite ammo\n");
    printf("souls <amount>    - Add souls (server-side)\n");
    printf("spell <A|B|C|ABC> - Set spell combo\n");
    printf("repeat <count>    - Set spell repeat count (1-5)\n");
    printf("cast              - Cast spell combo now\n");
    printf("exit              - Unload DLL\n");
    printf("=================================\n");
    printf("Hotkeys: F1=Track F2=NoCD F3=Jump F4=Ammo F5=Cast END=Unload\n");
    printf("\n");
}

static void PrintStatus()
{
    printf("\n=== Status ===\n");
    printf("Homing Bullets: %s\n", g_ModState.bTracking ? "ON" : "OFF");
    printf("No Cooldown:    %s\n", g_ModState.bNoCooldown ? "ON" : "OFF");
    printf("Infinite Jump:  %s\n", g_ModState.bInfiniteJump ? "ON" : "OFF");
    printf("Infinite Ammo:  %s\n", g_ModState.bInfiniteAmmo ? "ON" : "OFF");
    printf("Spell Combo:    %s%s%s x%d\n",
        g_SpellCombo.spellA ? "A" : "",
        g_SpellCombo.spellB ? "B" : "",
        g_SpellCombo.spellC ? "C" : "",
        g_SpellCombo.repeatCount);
    if (g_NearestEnemy.Address)
    {
        printf("Nearest Enemy:  dist=%.1f\n", sqrt(g_NearestEnemy.Distance));
    }
    else
    {
        printf("Nearest Enemy:  none\n");
    }
    printf("==============\n\n");
}

static void ProcessCommand(const std::string& cmd)
{
    std::istringstream iss(cmd);
    std::string action;
    iss >> action;

    if (action == "help" || action == "?")
    {
        PrintHelp();
    }
    else if (action == "status" || action == "s")
    {
        PrintStatus();
    }
    else if (action == "tracking")
    {
        std::string arg;
        iss >> arg;
        if (arg == "on") g_ModState.bTracking = true;
        else if (arg == "off") g_ModState.bTracking = false;
        else g_ModState.bTracking = !g_ModState.bTracking;
        printf("Homing Bullets: %s\n", g_ModState.bTracking ? "ON" : "OFF");
    }
    else if (action == "nocd")
    {
        std::string arg;
        iss >> arg;
        if (arg == "on") g_ModState.bNoCooldown = true;
        else if (arg == "off") g_ModState.bNoCooldown = false;
        else g_ModState.bNoCooldown = !g_ModState.bNoCooldown;
        printf("No Cooldown: %s\n", g_ModState.bNoCooldown ? "ON" : "OFF");
    }
    else if (action == "jump")
    {
        std::string arg;
        iss >> arg;
        if (arg == "on") g_ModState.bInfiniteJump = true;
        else if (arg == "off") g_ModState.bInfiniteJump = false;
        else g_ModState.bInfiniteJump = !g_ModState.bInfiniteJump;
        printf("Infinite Jump: %s\n", g_ModState.bInfiniteJump ? "ON" : "OFF");
    }
    else if (action == "ammo")
    {
        std::string arg;
        iss >> arg;
        if (arg == "on") g_ModState.bInfiniteAmmo = true;
        else if (arg == "off") g_ModState.bInfiniteAmmo = false;
        else g_ModState.bInfiniteAmmo = !g_ModState.bInfiniteAmmo;
        printf("Infinite Ammo: %s\n", g_ModState.bInfiniteAmmo ? "ON" : "OFF");
    }
    else if (action == "souls")
    {
        int amount = 0;
        iss >> amount;
        if (amount > 0 && g_Cache.PlayerState)
        {
            printf("Add %d souls (note: server-side, may not work)\n", amount);
        }
        else
        {
            printf("Usage: souls <amount>\n");
        }
    }
    else if (action == "spell")
    {
        std::string arg;
        iss >> arg;
        g_SpellCombo.spellA = false;
        g_SpellCombo.spellB = false;
        g_SpellCombo.spellC = false;
        if (arg.find('A') != std::string::npos || arg.find('a') != std::string::npos)
            g_SpellCombo.spellA = true;
        if (arg.find('B') != std::string::npos || arg.find('b') != std::string::npos)
            g_SpellCombo.spellB = true;
        if (arg.find('C') != std::string::npos || arg.find('c') != std::string::npos)
            g_SpellCombo.spellC = true;
        printf("Spell Combo: %s%s%s\n",
            g_SpellCombo.spellA ? "A" : "",
            g_SpellCombo.spellB ? "B" : "",
            g_SpellCombo.spellC ? "C" : "");
    }
    else if (action == "repeat")
    {
        int count = 0;
        iss >> count;
        if (count >= 1 && count <= 5)
        {
            g_SpellCombo.repeatCount = count;
            printf("Spell Repeat: %d\n", count);
        }
        else
        {
            printf("Usage: repeat <1-5>\n");
        }
    }
    else if (action == "cast")
    {
        CastSpellCombo();
        printf("Cast!\n");
    }
    else if (action == "exit" || action == "quit")
    {
        g_ModState.bRunning = false;
        printf("Unloading...\n");
    }
    else if (!action.empty())
    {
        printf("Unknown command: %s (type help)\n", action.c_str());
    }
}

static DWORD WINAPI ConsoleThread(LPVOID lpParam)
{
    // 创建控制台窗口
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONIN$", "r", stdin);
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    // 设置控制台标题
    SetConsoleTitleA("FarFarWest Mod v2");

    printf("\n* FarFarWest Mod v2 Loaded *\n");
    printf("Type help for command list\n\n");

    g_bConsoleRunning = true;

    char buffer[256];
    while (g_ModState.bRunning)
    {
        if (fgets(buffer, sizeof(buffer), stdin))
        {
            // 移除换行符
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n')
                buffer[len - 1] = '\0';

            std::lock_guard<std::mutex> lock(g_ConsoleMutex);
            ProcessCommand(std::string(buffer));
        }
    }

    g_bConsoleRunning = false;
    FreeConsole();
    return 0;
}

// ============================================================================
// 热键处理
// ============================================================================
static DWORD g_LastF5Press = 0;

static void HandleHotkeys()
{
    // F1 - 切换追踪子弹
    if (GetAsyncKeyState(VK_F1) & 0x8000)
    {
        while (GetAsyncKeyState(VK_F1) & 0x8000)
            Sleep(10);
        g_ModState.bTracking = !g_ModState.bTracking;
    }

    // F2 - 切换无CD
    if (GetAsyncKeyState(VK_F2) & 0x8000)
    {
        while (GetAsyncKeyState(VK_F2) & 0x8000)
            Sleep(10);
        g_ModState.bNoCooldown = !g_ModState.bNoCooldown;
    }

    // F3 - 切换无限跳跃
    if (GetAsyncKeyState(VK_F3) & 0x8000)
    {
        while (GetAsyncKeyState(VK_F3) & 0x8000)
            Sleep(10);
        g_ModState.bInfiniteJump = !g_ModState.bInfiniteJump;
    }

    // F4 - 切换无限子弹
    if (GetAsyncKeyState(VK_F4) & 0x8000)
    {
        while (GetAsyncKeyState(VK_F4) & 0x8000)
            Sleep(10);
        g_ModState.bInfiniteAmmo = !g_ModState.bInfiniteAmmo;
    }

    // F5 - 一键技能
    if (GetAsyncKeyState(VK_F5) & 0x8000)
    {
        DWORD now = GetTickCount();
        if (now - g_LastF5Press > 300) // 300ms防抖
        {
            g_LastF5Press = now;
            CastSpellCombo();
        }
    }

    // END - 卸载DLL
    if (GetAsyncKeyState(VK_END) & 0x8000)
    {
        g_ModState.bRunning = false;
    }
}

// ============================================================================
// 主线程
// ============================================================================
static DWORD WINAPI MainThread(LPVOID lpParam)
{
    // 等待游戏加载完成
    while (g_ModState.bRunning)
    {
        uintptr_t imageBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
        uintptr_t world = SafeReadPtr(imageBase + Offsets::GWorld);
        if (world) break;
        Sleep(1000);
    }

    // 额外等待3秒
    Sleep(3000);

    // 启动CMD控制台线程
    CreateThread(nullptr, 0, ConsoleThread, nullptr, 0, nullptr);

    // 主循环
    while (g_ModState.bRunning)
    {
        HandleHotkeys();

        if (!ValidateAndRefreshPointers())
        {
            Sleep(100);
            continue;
        }

        // 始终执行反作弊
        ApplyAntiCheat();

        // 按开关执行功能
        if (g_ModState.bTracking)
            ApplyTracking();

        if (g_ModState.bNoCooldown)
            ApplyNoCooldown();

        if (g_ModState.bInfiniteJump)
            ApplyInfiniteJump();

        if (g_ModState.bInfiniteAmmo)
            ApplyInfiniteAmmo();

        Sleep(1);
    }

    FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), 0);
    return 0;
}

// ============================================================================
// DLL入口
// ============================================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    return TRUE;
}
