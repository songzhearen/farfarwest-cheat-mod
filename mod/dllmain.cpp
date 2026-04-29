// dllmain.cpp : FarFarWest Mod - DLL注入式作弊模块
// 功能: 100%追踪子弹 / 无CD / 无限跳跃
// 热键: F1=追踪 / F2=无CD / F3=无限跳 / END=卸载

#include "pch.h"
#include "SDK.hpp"

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

    // ABP_Player_C
    constexpr uintptr_t AutoLookTarget     = 0x0998;
    constexpr uintptr_t IsFocusingTarget   = 0x09A0;
    constexpr uintptr_t JumpsLeft          = 0x0A4C;
    constexpr uintptr_t MaxDash            = 0x0840;
    constexpr uintptr_t CurrentDashLeft    = 0x0844;
    constexpr uintptr_t DashCooldown       = 0x0940;
    constexpr uintptr_t IsDashOnCooldown   = 0x0A29;
    constexpr uintptr_t LocalPlayerState   = 0x0960;

    // ABP_PlayerState_C
    constexpr uintptr_t PlayerRuntimeStats = 0x03B8;
    constexpr uintptr_t IsSuspicious       = 0x0985;

    // FS_PlayerRuntimeStats
    constexpr uintptr_t CooldownSpellA = 0x0030;
    constexpr uintptr_t CooldownSpellB = 0x0038;
    constexpr uintptr_t CooldownSpellC = 0x0040;

    // ABP_Enemy_C
    constexpr uintptr_t EnemyHealth = 0x06B0;
}

// ============================================================================
// 功能开关
// ============================================================================
struct FModState
{
    bool bTracking     = false;
    bool bNoCooldown   = false;
    bool bInfiniteJump = false;
    bool bRunning      = true;
};

static FModState g_ModState;

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
        // AActor+0x01B8 = RootComponent (USceneComponent*) [SDK验证]
        uintptr_t rootComp = *reinterpret_cast<uintptr_t*>(actor + 0x01B8);
        if (!rootComp) return false;

        // USceneComponent+0x0148 = RelativeLocation (FVector) [SDK验证]
        outLoc = *reinterpret_cast<SDK::FVector*>(rootComp + 0x0148);
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
        // clientTransform is at +0x06E0, size 0x0060
        // UE5 FTransform layout: 
        //   Rotation (FQuat) at 0x00, size 0x20
        //   Translation (FVector) at 0x20, size 0x18 (but aligned to 0x20)
        //   Scale3D (FVector) at 0x40, size 0x18
        // Total = 0x58, padded to 0x60
        outLoc = *reinterpret_cast<SDK::FVector*>(player + 0x06E0 + 0x20);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
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

    // LocalPlayers is TArray<ULocalPlayer*>
    uintptr_t localPlayersData = SafeReadPtr(gameInstance + Offsets::LocalPlayers);
    if (!localPlayersData) return 0;

    // LocalPlayers[0]
    uintptr_t localPlayer = SafeReadPtr(localPlayersData);
    if (!localPlayer) return 0;

    // UPlayer -> PlayerController
    uintptr_t playerController = SafeReadPtr(localPlayer + Offsets::PlayerController);
    if (!playerController) return 0;

    // AController -> Pawn
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
    // 如果缓存的指针仍然有效，直接返回
    if (g_Cache.Player && g_Cache.PlayerState)
    {
        // 快速验证: 读取一个已知字段
        __try
        {
            // 验证Player指针 - 读取VTable应该非零
            auto vtable = *reinterpret_cast<uintptr_t*>(g_Cache.Player);
            if (vtable)
            {
                // 验证PlayerState指针
                uintptr_t ps = SafeReadPtr(g_Cache.Player + Offsets::LocalPlayerState);
                if (ps == g_Cache.PlayerState)
                    return true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            // 指针无效，需要刷新
        }
    }

    // 重新获取指针
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

// 初始化敌人类缓存 (仅执行一次)
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

    // 遍历GObjects查找ABP_Enemy_C实例
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
                // 快速Actor类型过滤
                if (!obj->HasTypeFlag(SDK::EClassCastFlags::Actor))
                    continue;

                // 跳过默认对象(CDO)
                if (obj->IsDefaultObject())
                    continue;

                // 使用SDK的IsA检查是否是ABP_Enemy_C或其子类
                // IsA(UClass*)会遍历类层级链，是O(depth)但depth很小
                if (!obj->IsA(g_EnemyClass))
                    continue;

                // 检查敌人是否存活(health > 0)
                uintptr_t enemyAddr = reinterpret_cast<uintptr_t>(obj);
                double health = 0.0;
                if (!SafeRead(enemyAddr + Offsets::EnemyHealth, health) || health <= 0.0)
                    continue;

                // 计算距离
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
// 功能实现
// ============================================================================

// 100%追踪子弹: 设置autoLookTarget为最近敌人，isFocusingTarget=true
static void ApplyTracking()
{
    if (!g_Cache.Player) return;

    // 刷新敌人列表
    ScanForNearestEnemy();

    if (g_NearestEnemy.Address)
    {
        // 设置autoLookTarget指向最近的敌人
        SafeWriteIfDifferent(g_Cache.Player + Offsets::AutoLookTarget, g_NearestEnemy.Address);
        SafeWriteIfDifferent(g_Cache.Player + Offsets::IsFocusingTarget, true);
    }
}

// 无CD: 将技能冷却置零，恢复冲刺次数
static void ApplyNoCooldown()
{
    if (!g_Cache.Player || !g_Cache.PlayerState) return;

    // 技能冷却置零
    uintptr_t runtimeStats = g_Cache.PlayerState + Offsets::PlayerRuntimeStats;
    SafeWriteIfDifferent(runtimeStats + Offsets::CooldownSpellA, 0.0);
    SafeWriteIfDifferent(runtimeStats + Offsets::CooldownSpellB, 0.0);
    SafeWriteIfDifferent(runtimeStats + Offsets::CooldownSpellC, 0.0);

    // 冲刺冷却置零
    SafeWriteIfDifferent(g_Cache.Player + Offsets::DashCooldown, 0.0);
    SafeWriteIfDifferent(g_Cache.Player + Offsets::IsDashOnCooldown, false);

    // 恢复冲刺次数
    int32_t maxDash = 0;
    if (SafeRead(g_Cache.Player + Offsets::MaxDash, maxDash) && maxDash > 0)
    {
        SafeWriteIfDifferent<int32_t>(g_Cache.Player + Offsets::CurrentDashLeft, maxDash);
    }
}

// 无限跳跃: 持续设置jumpsLeft
static void ApplyInfiniteJump()
{
    if (!g_Cache.Player) return;
    SafeWriteIfDifferent<int32_t>(g_Cache.Player + Offsets::JumpsLeft, 999);
}

// 反作弊: 清除isSuspicious标记
static void ApplyAntiCheat()
{
    if (!g_Cache.PlayerState) return;
    SafeWriteIfDifferent(g_Cache.PlayerState + Offsets::IsSuspicious, false);
}

// ============================================================================
// 热键处理
// ============================================================================
static void HandleHotkeys()
{
    // F1 - 切换追踪子弹
    if (GetAsyncKeyState(VK_F1) & 0x8000)
    {
        // 防抖: 等待按键释放
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
    // 等待游戏加载完成 (等待GWorld有效)
    while (g_ModState.bRunning)
    {
        uintptr_t imageBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
        uintptr_t world = SafeReadPtr(imageBase + Offsets::GWorld);
        if (world) break;
        Sleep(1000);
    }

    // 额外等待3秒，确保游戏完全初始化
    Sleep(3000);

    // 主循环
    while (g_ModState.bRunning)
    {
        HandleHotkeys();

        // 获取/验证指针
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

        Sleep(1); // 最小延迟，约1000Hz轮询
    }

    // DLL卸载前清理
    // 注意: 不需要恢复原始值，因为游戏会在下一帧覆盖

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
