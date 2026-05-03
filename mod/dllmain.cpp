// dllmain.cpp : FarFarWest Mod v3 - Web UI 控制界面
// 功能: 无CD / 无限跳跃 / 无限子弹 / 移速修改 / 跳跃高度 / 自动瞄准 / 传送到玩家
// 控制: localhost:1145 网页界面 + 热键

#include "pch.h"
#include "SDK.hpp"
#include "SDK/Engine_parameters.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
#include <atomic>
#include <cmath>

#pragma comment(lib, "ws2_32.lib")

// cpp-httplib: 关闭异常以兼容SEH
#define CPPHTTPLIB_NO_EXCEPTIONS
#include "httplib.h"
#include "web_ui.h"

// ============================================================================
// 偏移常量
// ============================================================================
namespace Offsets
{
    constexpr uintptr_t GWorld              = 0x092CCD20;
    constexpr uintptr_t OwningGameInstance  = 0x0228;
    constexpr uintptr_t LocalPlayers        = 0x0038;
    constexpr uintptr_t PlayerController    = 0x0030;
    constexpr uintptr_t Pawn                = 0x02F0;
    constexpr uintptr_t ActorRootComponent  = 0x01B8;
    constexpr uintptr_t SceneCompRelativeLocation = 0x0148;

    // ABP_Player_C
    constexpr uintptr_t AutoLookTarget      = 0x0998;
    constexpr uintptr_t IsFocusingTarget    = 0x09A0;
    constexpr uintptr_t JumpsLeft           = 0x0A4C;
    constexpr uintptr_t MaxDash             = 0x0840;
    constexpr uintptr_t CurrentDashLeft     = 0x0844;
    constexpr uintptr_t DashCooldown        = 0x0940;
    constexpr uintptr_t IsDashOnCooldown    = 0x0A29;
    constexpr uintptr_t LocalPlayerState    = 0x0960;
    constexpr uintptr_t ClientTransform     = 0x06E0;
    constexpr uintptr_t CharMoveComp        = 0x0338;
    constexpr uintptr_t BuffMoveSpeed       = 0x09A8;

    // CharacterMovementComponent
    constexpr uintptr_t JumpZVelocity       = 0x01B0;
    constexpr uintptr_t MaxWalkSpeed        = 0x0288;
    constexpr uintptr_t MaxWalkSpeedCrouched = 0x028C;
    constexpr uintptr_t MaxAcceleration     = 0x029C;

    // ABP_PlayerState_C
    constexpr uintptr_t PlayerRuntimeStats  = 0x03B8;
    constexpr uintptr_t IsSuspicious        = 0x0985;

    // FS_PlayerRuntimeStats
    constexpr uintptr_t DatasWepA           = 0x0000;
    constexpr uintptr_t DatasWepB           = 0x0014;
    constexpr uintptr_t AmountGrenades      = 0x0028;
    constexpr uintptr_t CooldownSpellA      = 0x0030;
    constexpr uintptr_t CooldownSpellB      = 0x0038;
    constexpr uintptr_t CooldownSpellC      = 0x0040;

    // FS_ItemDatas
    constexpr uintptr_t CurrentMagazineAmmos = 0x0004;
    constexpr uintptr_t MaxMagazineAmmos     = 0x0008;
    constexpr uintptr_t CurrentTotalAmmos    = 0x000C;
    constexpr uintptr_t MaxTotalAmmos        = 0x0010;

    // ABP_Enemy_C
    constexpr uintptr_t EnemyHealth = 0x06B0;
}

// ============================================================================
// 功能开关 (atomic 保证Web服务器线程安全)
// ============================================================================
struct FModState
{
    std::atomic<bool> bNoCooldown{false};
    std::atomic<bool> bInfiniteJump{false};
    std::atomic<bool> bInfiniteAmmo{false};
    std::atomic<bool> bSpeedHack{false};
    std::atomic<bool> bAutoAim{false};
    std::atomic<bool> bRunning{true};
};

static FModState g_ModState;
static std::atomic<float> g_SpeedMultiplier{1.0f};
static std::atomic<float> g_JumpHeight{0.0f};

// ============================================================================
// 指针缓存
// ============================================================================
struct FCachedPointers
{
    uintptr_t Player      = 0;
    uintptr_t PlayerState = 0;
};

static FCachedPointers g_Cache;

struct FEnemyInfo
{
    uintptr_t Address;
    double     Distance;
};

static FEnemyInfo  g_NearestEnemy = { 0, 999999.0 };
static DWORD      g_LastEnemyScanTick = 0;
static const DWORD ENEMY_SCAN_INTERVAL_MS = 500;

static uintptr_t g_LastKnownWorld = 0;

namespace UEObjOffsets
{
    constexpr uintptr_t VTable  = 0x0000;
    constexpr uintptr_t Flags   = 0x0008;
    constexpr uintptr_t Index   = 0x000C;
    constexpr uintptr_t Class   = 0x0010;
}

constexpr int32_t OBJ_FLAG_BeginDestroyed  = 0x00008000;
constexpr int32_t OBJ_FLAG_FinishDestroyed = 0x00010000;

static SDK::UClass* g_EnemyClass = nullptr;

// ============================================================================
// 玩家列表 (用于传送)
// ============================================================================
struct FPlayerInfo
{
    uintptr_t    Address;
    SDK::FVector Location;
};

static FPlayerInfo g_AllPlayers[32];
static int g_AllPlayerCount = 0;
static DWORD g_LastPlayerScanTick = 0;
static const DWORD PLAYER_SCAN_INTERVAL_MS = 2000;

// ============================================================================
// 工具函数
// ============================================================================
static inline uintptr_t SafeReadPtr(uintptr_t address)
{
    if (!address) return 0;
    __try { return *reinterpret_cast<uintptr_t*>(address); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

template<typename T>
static inline bool SafeRead(uintptr_t address, T& outValue)
{
    if (!address) return false;
    __try { outValue = *reinterpret_cast<T*>(address); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

template<typename T>
static inline bool SafeWrite(uintptr_t address, T value)
{
    if (!address) return false;
    __try { *reinterpret_cast<T*>(address) = value; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

template<typename T>
static inline bool SafeWriteIfDifferent(uintptr_t address, T value)
{
    T current;
    if (SafeRead(address, current) && current != value)
        return SafeWrite(address, value);
    return false;
}

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
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool GetPlayerLocation(uintptr_t player, SDK::FVector& outLoc)
{
    if (!player) return false;
    __try
    {
        outLoc = *reinterpret_cast<SDK::FVector*>(player + Offsets::ClientTransform + 0x20);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static uintptr_t GetLocalPlayerPawn()
{
    __try
    {
        uintptr_t imageBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
        if (!imageBase) return 0;
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
        return SafeReadPtr(playerController + Offsets::Pawn);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static uintptr_t GetPlayerState(uintptr_t player)
{
    if (!player) return 0;
    return SafeReadPtr(player + Offsets::LocalPlayerState);
}

static bool IsUObjectDestroyed(uintptr_t objAddr)
{
    if (!objAddr) return true;
    __try
    {
        int32_t flags = *reinterpret_cast<int32_t*>(objAddr + UEObjOffsets::Flags);
        if (flags & (OBJ_FLAG_BeginDestroyed | OBJ_FLAG_FinishDestroyed)) return true;
        uintptr_t cls = *reinterpret_cast<uintptr_t*>(objAddr + UEObjOffsets::Class);
        if (!cls) return true;
        return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return true; }
}

static bool SafeIsA(SDK::UObject* obj, SDK::UClass* targetClass)
{
    if (!obj || !targetClass) return false;
    __try
    {
        uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
        if (IsUObjectDestroyed(objAddr)) return false;
        auto vtable = *reinterpret_cast<uintptr_t*>(objAddr);
        if (!vtable) return false;
        return obj->IsA(targetClass);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeHasTypeFlag(SDK::UObject* obj, SDK::EClassCastFlags flags)
{
    if (!obj) return false;
    __try
    {
        uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
        if (IsUObjectDestroyed(objAddr)) return false;
        auto vtable = *reinterpret_cast<uintptr_t*>(objAddr);
        if (!vtable) return false;
        return obj->HasTypeFlag(flags);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeIsDefaultObject(SDK::UObject* obj)
{
    if (!obj) return true;
    __try
    {
        uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
        if (IsUObjectDestroyed(objAddr)) return true;
        return obj->IsDefaultObject();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return true; }
}

static SDK::FUObjectItem** SafeGetGObjectsChunks(SDK::TUObjectArray* gobjects)
{
    if (!gobjects) return nullptr;
    __try { return gobjects->GetDecrytedObjPtr(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

// ============================================================================
// 指针验证和刷新
// ============================================================================
static bool HasWorldChanged()
{
    uintptr_t imageBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
    uintptr_t currentWorld = SafeReadPtr(imageBase + Offsets::GWorld);
    if (currentWorld != g_LastKnownWorld)
    {
        if (currentWorld == 0) return true;
        g_LastKnownWorld = currentWorld;
        return true;
    }
    return false;
}

static void OnLevelTransition()
{
    g_NearestEnemy = { 0, 999999.0 };
    g_AllPlayerCount = 0;
    g_LastEnemyScanTick = 0;
    g_LastPlayerScanTick = 0;
    g_EnemyClass = nullptr;
}

static bool ValidateAndRefreshPointers()
{
    bool worldChanged = HasWorldChanged();
    if (worldChanged)
    {
        g_Cache.Player = 0;
        g_Cache.PlayerState = 0;
        OnLevelTransition();
    }

    if (g_Cache.Player && g_Cache.PlayerState)
    {
        if (IsUObjectDestroyed(g_Cache.Player) || IsUObjectDestroyed(g_Cache.PlayerState))
        {
            g_Cache.Player = 0;
            g_Cache.PlayerState = 0;
        }
        else
        {
            uintptr_t ps = SafeReadPtr(g_Cache.Player + Offsets::LocalPlayerState);
            if (ps == g_Cache.PlayerState) return true;
            if (ps) { g_Cache.PlayerState = ps; return true; }
            g_Cache.Player = 0;
            g_Cache.PlayerState = 0;
        }
    }

    uintptr_t pawn = GetLocalPlayerPawn();
    if (!pawn) return false;
    if (IsUObjectDestroyed(pawn)) return false;
    uintptr_t playerState = GetPlayerState(pawn);
    if (!playerState) return false;
    if (IsUObjectDestroyed(playerState)) return false;

    g_Cache.Player = pawn;
    g_Cache.PlayerState = playerState;
    return true;
}

// ============================================================================
// 敌人查找
// ============================================================================
static SDK::UClass* TryEnemyStaticClass()
{
    __try { return SDK::ABP_Enemy_C::StaticClass(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static SDK::UClass* TryFindClass(const std::string& fullName)
{
    __try { return SDK::UObject::FindClass(fullName); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static SDK::UClass* ScanForEnemyClassInGObjects()
{
    for (int32_t i = 0; i < SDK::UObject::GObjects->Num(); i++)
    {
        auto* obj = SDK::UObject::GObjects->GetByIndex(i);
        if (!obj) continue;
        uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
        uintptr_t clsAddr = SafeReadPtr(objAddr + UEObjOffsets::Class);
        if (!clsAddr) continue;
        SDK::EClassCastFlags castFlags;
        if (!SafeRead(clsAddr + 0xD8, castFlags)) continue;
        if (!(castFlags & SDK::EClassCastFlags::Class)) continue;
        SDK::FName clsFName;
        if (!SafeRead(clsAddr + 0x18, clsFName)) continue;
        std::string name = clsFName.GetRawString();
        if (name.find("BP_Enemy_C") != std::string::npos)
            return static_cast<SDK::UClass*>(obj);
    }
    return nullptr;
}

static void InitEnemyClass()
{
    if (g_EnemyClass) return;
    g_EnemyClass = TryEnemyStaticClass();
    if (!g_EnemyClass)
    {
        std::string fullName = "BlueprintGeneratedClass BP_Enemy.BP_Enemy_C";
        g_EnemyClass = TryFindClass(fullName);
    }
    if (!g_EnemyClass)
        g_EnemyClass = ScanForEnemyClassInGObjects();
}

static void ScanForNearestEnemy()
{
    DWORD now = GetTickCount();
    if (now - g_LastEnemyScanTick < ENEMY_SCAN_INTERVAL_MS) return;
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

    int32_t numElements = 0, numChunks = 0;
    SafeRead(reinterpret_cast<uintptr_t>(gobjects) + 0x14, numElements);
    SafeRead(reinterpret_cast<uintptr_t>(gobjects) + 0x1C, numChunks);
    if (numElements <= 0) return;

    auto** chunks = SafeGetGObjectsChunks(gobjects);
    if (!chunks) return;

    for (int32_t chunkIdx = 0; chunkIdx < numChunks; chunkIdx++)
    {
        auto* chunk = chunks[chunkIdx];
        if (!chunk) continue;
        for (int32_t inChunkIdx = 0; inChunkIdx < gobjects->ElementsPerChunk; inChunkIdx++)
        {
            int32_t objIdx = chunkIdx * gobjects->ElementsPerChunk + inChunkIdx;
            if (objIdx >= numElements) break;
            auto* obj = chunk[inChunkIdx].Object;
            if (!obj) continue;
            if (!SafeHasTypeFlag(obj, SDK::EClassCastFlags::Actor)) continue;
            if (SafeIsDefaultObject(obj)) continue;

            bool isEnemy = SafeIsA(obj, g_EnemyClass);
            if (!isEnemy && g_EnemyClass)
            {
                uintptr_t clsAddr = SafeReadPtr(reinterpret_cast<uintptr_t>(obj) + UEObjOffsets::Class);
                if (clsAddr)
                {
                    SDK::FName clsFName;
                    if (SafeRead(clsAddr + 0x18, clsFName))
                    {
                        std::string clsName = clsFName.GetRawString();
                        if (clsName.find("BP_Enemy_C") != std::string::npos || clsName.find("BP_Enemy") == 0)
                            isEnemy = true;
                    }
                }
            }
            if (!isEnemy) continue;

            uintptr_t enemyAddr = reinterpret_cast<uintptr_t>(obj);
            double health = 0.0;
            if (!SafeRead(enemyAddr + Offsets::EnemyHealth, health) || health <= 0.0) continue;
            if (IsUObjectDestroyed(enemyAddr)) continue;

            SDK::FVector enemyLoc;
            if (!GetActorLocation(enemyAddr, enemyLoc)) continue;

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
    }
    g_NearestEnemy = best;
}

// ============================================================================
// 扫描所有玩家 (用于传送)
// ============================================================================
static void ScanAllPlayers()
{
    DWORD now = GetTickCount();
    if (now - g_LastPlayerScanTick < PLAYER_SCAN_INTERVAL_MS) return;
    g_LastPlayerScanTick = now;

    if (!g_Cache.Player) return;

    auto* gobjects = SDK::UObject::GObjects.GetTypedPtr();
    if (!gobjects) return;

    int32_t numElements = 0, numChunks = 0;
    SafeRead(reinterpret_cast<uintptr_t>(gobjects) + 0x14, numElements);
    SafeRead(reinterpret_cast<uintptr_t>(gobjects) + 0x1C, numChunks);
    if (numElements <= 0) return;

    auto** chunks = SafeGetGObjectsChunks(gobjects);
    if (!chunks) return;

    static SDK::UClass* playerClass = nullptr;
    if (!playerClass) playerClass = TryEnemyStaticClass(); // placeholder, will use IsA

    // 直接用 ABP_Player_C::StaticClass
    static SDK::UClass* bpPlayerClass = nullptr;
    if (!bpPlayerClass)
    {
        __try { bpPlayerClass = SDK::ABP_Player_C::StaticClass(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { bpPlayerClass = nullptr; }
    }
    if (!bpPlayerClass) return;

    int count = 0;
    for (int32_t chunkIdx = 0; chunkIdx < numChunks && count < 32; chunkIdx++)
    {
        auto* chunk = chunks[chunkIdx];
        if (!chunk) continue;
        for (int32_t inChunkIdx = 0; inChunkIdx < gobjects->ElementsPerChunk && count < 32; inChunkIdx++)
        {
            int32_t objIdx = chunkIdx * gobjects->ElementsPerChunk + inChunkIdx;
            if (objIdx >= numElements) break;
            auto* obj = chunk[inChunkIdx].Object;
            if (!obj) continue;
            if (!SafeHasTypeFlag(obj, SDK::EClassCastFlags::Actor)) continue;
            if (SafeIsDefaultObject(obj)) continue;

            uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
            if (objAddr == g_Cache.Player) continue; // 排除自己
            if (IsUObjectDestroyed(objAddr)) continue;

            if (!SafeIsA(obj, bpPlayerClass)) continue;

            SDK::FVector loc;
            if (!GetPlayerLocation(objAddr, loc)) continue;

            g_AllPlayers[count].Address = objAddr;
            g_AllPlayers[count].Location = loc;
            count++;
        }
    }
    g_AllPlayerCount = count;
}

// ============================================================================
// 传送到指定玩家
// ============================================================================
static void TeleportToPlayer(int index)
{
    if (index < 0 || index >= g_AllPlayerCount) return;
    if (!g_Cache.Player) return;

    SDK::FVector targetLoc = g_AllPlayers[index].Location;
    // 偏移避免重叠
    targetLoc.X += 150.0;
    targetLoc.Z += 60.0;

    // 通过写入 RootComponent 的 RelativeLocation 实现传送
    uintptr_t rootComp = SafeReadPtr(g_Cache.Player + Offsets::ActorRootComponent);
    if (rootComp)
        SafeWrite(rootComp + Offsets::SceneCompRelativeLocation, targetLoc);

    // 同时写入 ClientTransform 的 Translation
    __try
    {
        *reinterpret_cast<SDK::FVector*>(g_Cache.Player + Offsets::ClientTransform + 0x20) = targetLoc;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================================
// 功能实现
// ============================================================================
static void ApplyAntiCheat()
{
    if (!g_Cache.PlayerState) return;
    SafeWriteIfDifferent(g_Cache.PlayerState + Offsets::IsSuspicious, false);
}

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
        SafeWriteIfDifferent<int32_t>(g_Cache.Player + Offsets::CurrentDashLeft, maxDash);
}

static void ApplyInfiniteJump()
{
    if (!g_Cache.Player) return;
    SafeWriteIfDifferent<int32_t>(g_Cache.Player + Offsets::JumpsLeft, 999);
}

static void ApplyInfiniteAmmo()
{
    if (!g_Cache.PlayerState) return;
    uintptr_t runtimeStats = g_Cache.PlayerState + Offsets::PlayerRuntimeStats;
    uintptr_t wepA = runtimeStats + Offsets::DatasWepA;
    int32_t maxMagA = 0, maxTotalA = 0;
    if (SafeRead(wepA + Offsets::MaxMagazineAmmos, maxMagA) && maxMagA > 0)
        SafeWriteIfDifferent(wepA + Offsets::CurrentMagazineAmmos, maxMagA);
    if (SafeRead(wepA + Offsets::MaxTotalAmmos, maxTotalA) && maxTotalA > 0)
        SafeWriteIfDifferent(wepA + Offsets::CurrentTotalAmmos, maxTotalA);

    uintptr_t wepB = runtimeStats + Offsets::DatasWepB;
    int32_t maxMagB = 0, maxTotalB = 0;
    if (SafeRead(wepB + Offsets::MaxMagazineAmmos, maxMagB) && maxMagB > 0)
        SafeWriteIfDifferent(wepB + Offsets::CurrentMagazineAmmos, maxMagB);
    if (SafeRead(wepB + Offsets::MaxTotalAmmos, maxTotalB) && maxTotalB > 0)
        SafeWriteIfDifferent(wepB + Offsets::CurrentTotalAmmos, maxTotalB);

    int32_t grenades = 0;
    SafeRead(runtimeStats + Offsets::AmountGrenades, grenades);
    if (grenades < 5)
        SafeWriteIfDifferent(runtimeStats + Offsets::AmountGrenades, 5);
}

static void ApplySpeedHack()
{
    if (!g_Cache.Player) return;
    float mult = g_SpeedMultiplier.load();
    uintptr_t charMoveComp = SafeReadPtr(g_Cache.Player + Offsets::CharMoveComp);
    if (!charMoveComp) return;
    SafeWriteIfDifferent<double>(g_Cache.Player + Offsets::BuffMoveSpeed, (double)mult);
    SafeWriteIfDifferent<float>(charMoveComp + Offsets::MaxWalkSpeed, 600.0f * mult);
    SafeWriteIfDifferent<float>(charMoveComp + Offsets::MaxWalkSpeedCrouched, 300.0f * mult);
    SafeWriteIfDifferent<float>(charMoveComp + Offsets::MaxAcceleration, 4096.0f * mult);
}

static void ApplyJumpHeight()
{
    if (!g_Cache.Player) return;
    float height = g_JumpHeight.load();
    if (height <= 0.0f) return;
    uintptr_t charMoveComp = SafeReadPtr(g_Cache.Player + Offsets::CharMoveComp);
    if (!charMoveComp) return;
    SafeWriteIfDifferent<float>(charMoveComp + Offsets::JumpZVelocity, height);
}

static void ApplyAutoAim()
{
    if (!g_Cache.Player || !g_NearestEnemy.Address) return;
    if (IsUObjectDestroyed(g_NearestEnemy.Address))
    {
        g_NearestEnemy = { 0, 999999.0 };
        return;
    }
    SafeWrite(g_Cache.Player + Offsets::AutoLookTarget, g_NearestEnemy.Address);
    SafeWrite(g_Cache.Player + Offsets::IsFocusingTarget, true);
}

// ============================================================================
// 构建JSON状态
// ============================================================================
static std::string BuildStatusJson()
{
    std::string json = "{";
    json += "\"nocd\":" + std::string(g_ModState.bNoCooldown.load() ? "true" : "false");
    json += ",\"jump\":" + std::string(g_ModState.bInfiniteJump.load() ? "true" : "false");
    json += ",\"ammo\":" + std::string(g_ModState.bInfiniteAmmo.load() ? "true" : "false");
    json += ",\"speed\":" + std::string(g_ModState.bSpeedHack.load() ? "true" : "false");
    json += ",\"aim\":" + std::string(g_ModState.bAutoAim.load() ? "true" : "false");
    json += ",\"speedMultiplier\":" + std::to_string(g_SpeedMultiplier.load());
    json += ",\"jumpHeight\":" + std::to_string(g_JumpHeight.load());

    json += ",\"players\":[";
    for (int i = 0; i < g_AllPlayerCount; i++)
    {
        if (i > 0) json += ",";
        json += "{\"x\":" + std::to_string(g_AllPlayers[i].Location.X);
        json += ",\"y\":" + std::to_string(g_AllPlayers[i].Location.Y);
        json += ",\"z\":" + std::to_string(g_AllPlayers[i].Location.Z) + "}";
    }
    json += "]";

    json += ",\"enemyDist\":" + std::to_string(
        g_NearestEnemy.Address ? sqrt(g_NearestEnemy.Distance) : 0.0);

    json += "}";
    return json;
}

// ============================================================================
// 简易JSON解析 (仅用于简单的key:value)
// ============================================================================
static float ParseJsonFloat(const std::string& body, const std::string& key)
{
    std::string search = "\"" + key + "\":";
    size_t pos = body.find(search);
    if (pos == std::string::npos) return 0.0f;
    pos += search.size();
    size_t end = body.find_first_of(",}", pos);
    if (end == std::string::npos) end = body.size();
    try { return std::stof(body.substr(pos, end - pos)); }
    catch (...) { return 0.0f; }
}

static int ParseJsonInt(const std::string& body, const std::string& key)
{
    std::string search = "\"" + key + "\":";
    size_t pos = body.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.size();
    size_t end = body.find_first_of(",}", pos);
    if (end == std::string::npos) end = body.size();
    try { return std::stoi(body.substr(pos, end - pos)); }
    catch (...) { return 0; }
}

// ============================================================================
// Web服务器线程
// ============================================================================
static void WebServerThread(LPVOID)
{
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(WEB_UI_HTML, sizeof(WEB_UI_HTML) - 1, "text/html; charset=utf-8");
    });

    svr.Get("/api/status", [](const httplib::Request&, httplib::Response& res) {
        std::string json = BuildStatusJson();
        res.set_content(json, "application/json");
    });

    svr.Post("/api/toggle/:feature", [](const httplib::Request& req, httplib::Response& res) {
        auto it = req.path_params.find("feature");
        if (it == req.path_params.end()) { res.set_content("{\"error\":\"missing feature\"}", "application/json"); return; }
        std::string f = it->second;
        bool newState = false;
        if (f == "nocd") { g_ModState.bNoCooldown = !g_ModState.bNoCooldown.load(); newState = g_ModState.bNoCooldown; }
        else if (f == "jump") { g_ModState.bInfiniteJump = !g_ModState.bInfiniteJump.load(); newState = g_ModState.bInfiniteJump; }
        else if (f == "ammo") { g_ModState.bInfiniteAmmo = !g_ModState.bInfiniteAmmo.load(); newState = g_ModState.bInfiniteAmmo; }
        else if (f == "speed") { g_ModState.bSpeedHack = !g_ModState.bSpeedHack.load(); newState = g_ModState.bSpeedHack; }
        else if (f == "aim") { g_ModState.bAutoAim = !g_ModState.bAutoAim.load(); newState = g_ModState.bAutoAim; }
        else { res.set_content("{\"error\":\"unknown feature\"}", "application/json"); return; }
        res.set_content("{\"ok\":true,\"state\":" + std::string(newState ? "true" : "false") + "}", "application/json");
    });

    svr.Post("/api/speed", [](const httplib::Request& req, httplib::Response& res) {
        float val = ParseJsonFloat(req.body, "multiplier");
        if (val < 0.5f) val = 0.5f;
        if (val > 10.0f) val = 10.0f;
        g_SpeedMultiplier = val;
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/api/jumpheight", [](const httplib::Request& req, httplib::Response& res) {
        float val = ParseJsonFloat(req.body, "height");
        if (val < 0) val = 0;
        if (val > 5000) val = 5000;
        g_JumpHeight = val;
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/api/teleport", [](const httplib::Request& req, httplib::Response& res) {
        int idx = ParseJsonInt(req.body, "index");
        TeleportToPlayer(idx);
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.listen("127.0.0.1", 1145);
}

// ============================================================================
// 热键处理
// ============================================================================
static bool g_PrevF1 = false;
static bool g_PrevF2 = false;
static bool g_PrevF3 = false;
static bool g_PrevF4 = false;
static bool g_PrevF5 = false;
static bool g_PrevEnd = false;

static void HandleHotkeys()
{
    {
        bool cur = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (cur && !g_PrevF1) g_ModState.bAutoAim = !g_ModState.bAutoAim.load();
        g_PrevF1 = cur;
    }
    {
        bool cur = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
        if (cur && !g_PrevF2) g_ModState.bNoCooldown = !g_ModState.bNoCooldown.load();
        g_PrevF2 = cur;
    }
    {
        bool cur = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
        if (cur && !g_PrevF3) g_ModState.bInfiniteJump = !g_ModState.bInfiniteJump.load();
        g_PrevF3 = cur;
    }
    {
        bool cur = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
        if (cur && !g_PrevF4) g_ModState.bInfiniteAmmo = !g_ModState.bInfiniteAmmo.load();
        g_PrevF4 = cur;
    }
    {
        bool cur = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        if (cur && !g_PrevF5) g_ModState.bSpeedHack = !g_ModState.bSpeedHack.load();
        g_PrevF5 = cur;
    }
    {
        bool cur = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
        if (cur && !g_PrevEnd) g_ModState.bRunning = false;
        g_PrevEnd = cur;
    }
}

// ============================================================================
// 主线程
// ============================================================================
static DWORD WINAPI MainThread(LPVOID lpParam)
{
    while (g_ModState.bRunning)
    {
        uintptr_t imageBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
        uintptr_t world = SafeReadPtr(imageBase + Offsets::GWorld);
        if (world) break;
        Sleep(1000);
    }

    Sleep(3000);

    {
        uintptr_t imageBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
        g_LastKnownWorld = SafeReadPtr(imageBase + Offsets::GWorld);
    }

    // 启动Web服务器线程
    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)WebServerThread, nullptr, 0, nullptr);

    int consecutiveFailures = 0;
    while (g_ModState.bRunning)
    {
        HandleHotkeys();

        if (!ValidateAndRefreshPointers())
        {
            consecutiveFailures++;
            DWORD waitMs = (consecutiveFailures < 10) ? 100 :
                           (consecutiveFailures < 30) ? 500 : 2000;
            Sleep(waitMs);
            continue;
        }
        consecutiveFailures = 0;

        ApplyAntiCheat();

        if (g_ModState.bAutoAim)
        {
            ScanForNearestEnemy();
            ApplyAutoAim();
        }

        if (g_ModState.bNoCooldown)    ApplyNoCooldown();
        if (g_ModState.bInfiniteJump)  ApplyInfiniteJump();
        if (g_ModState.bInfiniteAmmo)  ApplyInfiniteAmmo();
        if (g_ModState.bSpeedHack)     ApplySpeedHack();
        ApplyJumpHeight();

        ScanAllPlayers();

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
