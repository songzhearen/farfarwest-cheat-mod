// dllmain.cpp : 无限火力 Mod v0.3.0 - Web UI 控制界面
// 功能: 无CD / 无限跳跃 / 无限子弹 / 移速修改 / 传送到玩家 / 传送点
// 控制: localhost:1145 网页界面 + 热键
// by songzhearen

#include "pch.h"

#define MOD_VERSION "0.3.0"
#define GITHUB_REPO "songzhearen/farfarwest-cheat-mod"

#include "SDK.hpp"
#include "SDK/Engine_parameters.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
#include <atomic>
#include <cmath>
#include <mutex>

#include <winhttp.h>
#pragma comment(lib, "ws2_32.lib")

#include "httplib.h"
#include "web_ui.h"

// ============================================================================
// GitHub 更新检查 (使用 WinHTTP，无需额外依赖)
// ============================================================================
static std::string CheckGitHubUpdate()
{
    static std::string cachedResult;
    static DWORD lastCheck = 0;
    DWORD now = GetTickCount();
    if (now - lastCheck < 300000 && !cachedResult.empty())
        return cachedResult;
    lastCheck = now;

    HMODULE hWinHttp = LoadLibraryW(L"winhttp.dll");
    if (!hWinHttp) { cachedResult = "{\"error\":\"no winhttp\"}"; return cachedResult; }

    auto pOpen = (decltype(&WinHttpOpen))GetProcAddress(hWinHttp, "WinHttpOpen");
    auto pConnect = (decltype(&WinHttpConnect))GetProcAddress(hWinHttp, "WinHttpConnect");
    auto pOpenReq = (decltype(&WinHttpOpenRequest))GetProcAddress(hWinHttp, "WinHttpOpenRequest");
    auto pSendReq = (decltype(&WinHttpSendRequest))GetProcAddress(hWinHttp, "WinHttpSendRequest");
    auto pRecvResp = (decltype(&WinHttpReceiveResponse))GetProcAddress(hWinHttp, "WinHttpReceiveResponse");
    auto pReadData = (decltype(&WinHttpReadData))GetProcAddress(hWinHttp, "WinHttpReadData");
    auto pCloseH = (decltype(&WinHttpCloseHandle))GetProcAddress(hWinHttp, "WinHttpCloseHandle");
    auto pSetOpt = (decltype(&WinHttpSetOption))GetProcAddress(hWinHttp, "WinHttpSetOption");

    if (!pOpen || !pConnect || !pOpenReq || !pSendReq || !pRecvResp || !pReadData || !pCloseH || !pSetOpt)
    {
        FreeLibrary(hWinHttp);
        cachedResult = "{\"error\":\"winhttp api\"}";
        return cachedResult;
    }

    std::string result;
    HINTERNET hSession = pOpen(L"FarFarWestMod/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) { FreeLibrary(hWinHttp); cachedResult = "{\"error\":\"session\"}"; return cachedResult; }

    HINTERNET hConnect = pConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { pCloseH(hSession); FreeLibrary(hWinHttp); cachedResult = "{\"error\":\"connect\"}"; return cachedResult; }

    HINTERNET hRequest = pOpenReq(hConnect, L"GET",
        L"/repos/songzhearen/farfarwest-cheat-mod/releases/latest",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { pCloseH(hConnect); pCloseH(hSession); FreeLibrary(hWinHttp); cachedResult = "{\"error\":\"request\"}"; return cachedResult; }

    DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                     SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    pSetOpt(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));

    BOOL ok = pSendReq(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = pRecvResp(hRequest, nullptr);

    if (ok)
    {
        std::string body;
        DWORD bytesRead = 0;
        char buf[4096];
        while (pReadData(hRequest, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0)
        {
            buf[bytesRead] = '\0';
            body.append(buf, bytesRead);
            bytesRead = 0;
        }

        size_t tagPos = body.find("\"tag_name\":\"");
        size_t urlPos = body.find("\"html_url\":\"");
        if (tagPos != std::string::npos)
        {
            tagPos += 12;
            size_t tagEnd = body.find('"', tagPos);
            std::string tag = body.substr(tagPos, tagEnd - tagPos);

            std::string url;
            if (urlPos != std::string::npos)
            {
                urlPos += 12;
                size_t urlEnd = body.find('"', urlPos);
                url = body.substr(urlPos, urlEnd - urlPos);
            }

            bool hasUpdate = (tag != MOD_VERSION);
            result = "{\"hasUpdate\":" + std::string(hasUpdate ? "true" : "false") +
                     ",\"latest\":\"" + tag + "\",\"current\":\"" MOD_VERSION "\"";
            if (!url.empty()) result += ",\"url\":\"" + url + "\"";
            result += "}";
        }
        else
        {
            result = "{\"hasUpdate\":false,\"current\":\"" MOD_VERSION "\",\"note\":\"no releases\"}";
        }
    }
    else
    {
        result = "{\"hasUpdate\":false,\"current\":\"" MOD_VERSION "\",\"note\":\"request failed\"}";
    }

    pCloseH(hRequest);
    pCloseH(hConnect);
    pCloseH(hSession);
    FreeLibrary(hWinHttp);

    cachedResult = result;
    return result;
}

// ============================================================================
// 偏移常量
// ============================================================================
namespace Offsets
{
    constexpr uintptr_t GWorld              = 0x092CFDA0;
    constexpr uintptr_t OwningGameInstance  = 0x0228;
    constexpr uintptr_t LocalPlayers        = 0x0038;
    constexpr uintptr_t PlayerController    = 0x0030;
    constexpr uintptr_t Pawn                = 0x02F0;
    constexpr uintptr_t ActorRootComponent  = 0x01B8;
    constexpr uintptr_t SceneCompRelativeLocation = 0x0148;

    // ABP_Player_C
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
    constexpr uintptr_t MaxWalkSpeed        = 0x0288;
    constexpr uintptr_t MaxWalkSpeedCrouched = 0x028C;
    constexpr uintptr_t MaxAcceleration     = 0x029C;

    // ABP_PlayerState_C
    constexpr uintptr_t PlayerRuntimeStats  = 0x03B8;
    constexpr uintptr_t IsSuspicious        = 0x0975;

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
}

// ============================================================================
// 功能开关
// ============================================================================
struct FModState
{
    std::atomic<bool> bNoCooldown{false};
    std::atomic<bool> bInfiniteJump{false};
    std::atomic<bool> bInfiniteAmmo{false};
    std::atomic<bool> bSpeedHack{false};
    std::atomic<bool> bRunning{true};
};

static FModState g_ModState;

static std::atomic<float> g_TargetSpeed{0.0f};
static std::atomic<float> g_RealSpeed{0.0f};
static std::atomic<int32_t> g_CustomAmmo{0};  // 0=使用最大值(原行为), >0=自定义数量

// ============================================================================
// 指针缓存
// ============================================================================
struct FCachedPointers
{
    uintptr_t Player      = 0;
    uintptr_t PlayerState = 0;
};

static FCachedPointers g_Cache;
static uintptr_t g_LastKnownWorld = 0;

namespace UEObjOffsets
{
    constexpr uintptr_t VTable  = 0x0000;
    constexpr uintptr_t Flags   = 0x0008;
    constexpr uintptr_t Class   = 0x0010;
}

constexpr int32_t OBJ_FLAG_BeginDestroyed  = 0x00008000;
constexpr int32_t OBJ_FLAG_FinishDestroyed = 0x00010000;

// ProcessEvent vtable 索引
constexpr int PROCESS_EVENT_INDEX = 0x4C;

// ============================================================================
// 玩家列表 (带缓存，HTTP线程直接读取不阻塞)
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
static std::mutex g_PlayerMutex;

// 传送请求 (从HTTP线程写入，主线程执行)
static std::atomic<int> g_TeleportRequest{-1};

// K2_SetActorLocation UFunction 缓存
static SDK::UFunction* g_SetActorLocationFunc = nullptr;

// 传送点 (最多5个)
static SDK::FVector g_TeleportPoints[5];
static bool g_TeleportPointActive[5] = {false, false, false, false, false};
static std::atomic<int> g_TeleportPointRequest{-1};   // 请求传送到点
static std::atomic<int> g_SavePointRequest{-1};       // 请求保存当前位

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

// ProcessEvent 调用封装
using ProcessEventFn = void(*)(SDK::UObject* obj, SDK::UFunction* func, void* params);
static void SafeProcessEvent(SDK::UObject* obj, SDK::UFunction* func, void* params)
{
    if (!obj || !func) return;
    uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
    uintptr_t vtable = SafeReadPtr(objAddr);
    if (!vtable) return;
    ProcessEventFn pe = reinterpret_cast<ProcessEventFn>(
        SafeReadPtr(vtable + PROCESS_EVENT_INDEX * sizeof(uintptr_t)));
    if (!pe) return;
    __try { pe(obj, func, params); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// 查找 UFunction
static SDK::UFunction* FindUFunctionByName(const char* targetName)
{
    auto* gobjects = SDK::UObject::GObjects.GetTypedPtr();
    if (!gobjects) return nullptr;
    int32_t num = 0;
    SafeRead(reinterpret_cast<uintptr_t>(gobjects) + 0x14, num);
    for (int32_t i = 0; i < num; i++)
    {
        auto* uobj = SDK::UObject::GObjects->GetByIndex(i);
        if (!uobj) continue;
        if (!uobj->HasTypeFlag(SDK::EClassCastFlags::Function)) continue;
        if (uobj->GetFullName().find(targetName) != std::string::npos)
            return static_cast<SDK::UFunction*>(uobj);
    }
    return nullptr;
}

// K2_SetActorLocation 参数结构 (匹配SDK布局)
struct FHitResult_Simple
{
    int32_t FaceIndex;           // 0x00
    float Time;                  // 0x04
    float Distance;              // 0x08
    SDK::FVector Location;       // 0x0C (3 floats)
    SDK::FVector ImpactPoint;    // 0x18
    SDK::FVector Normal;         // 0x24
    SDK::FVector ImpactNormal;   // 0x30
    uint8_t _pad0[0x78];         // PhysMat, HitActor, HitComponent, HitBoneName, etc.
};

struct FK2_SetActorLocationParams
{
    SDK::FVector NewLocation;         // 0x00 (Input)
    bool bSweep;                      // 0x0C
    uint8_t _pad0[3];
    FHitResult_Simple SweepHitResult; // 0x10
    bool bTeleport;                   // 0xA0
    uint8_t _pad1[3];
    bool ReturnValue;                 // 0xA4
};

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
    {
        std::lock_guard<std::mutex> lock(g_PlayerMutex);
        g_AllPlayerCount = 0;
    }
    g_LastPlayerScanTick = 0;
    g_SetActorLocationFunc = nullptr;
    // 切换地图时清除所有传送点
    for (int i = 0; i < 5; i++)
    {
        g_TeleportPointActive[i] = false;
        g_TeleportPoints[i] = {};
    }
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
// 扫描所有玩家
// ============================================================================
static SDK::UClass* TryPlayerStaticClass()
{
    __try { return SDK::ABP_Player_C::StaticClass(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static int DoPlayerScan(FPlayerInfo* outPlayers, int maxCount)
{
    if (!g_Cache.Player) return 0;

    auto* gobjects = SDK::UObject::GObjects.GetTypedPtr();
    if (!gobjects) return 0;

    int32_t numElements = 0, numChunks = 0;
    SafeRead(reinterpret_cast<uintptr_t>(gobjects) + 0x14, numElements);
    SafeRead(reinterpret_cast<uintptr_t>(gobjects) + 0x1C, numChunks);
    if (numElements <= 0) return 0;

    auto** chunks = SafeGetGObjectsChunks(gobjects);
    if (!chunks) return 0;

    static SDK::UClass* bpPlayerClass = nullptr;
    if (!bpPlayerClass) bpPlayerClass = TryPlayerStaticClass();
    if (!bpPlayerClass) return 0;

    int count = 0;

    for (int32_t chunkIdx = 0; chunkIdx < numChunks && count < maxCount; chunkIdx++)
    {
        auto* chunk = chunks[chunkIdx];
        if (!chunk) continue;
        for (int32_t inChunkIdx = 0; inChunkIdx < gobjects->ElementsPerChunk && count < maxCount; inChunkIdx++)
        {
            int32_t objIdx = chunkIdx * gobjects->ElementsPerChunk + inChunkIdx;
            if (objIdx >= numElements) break;
            auto* obj = chunk[inChunkIdx].Object;
            if (!obj) continue;
            if (!SafeHasTypeFlag(obj, SDK::EClassCastFlags::Actor)) continue;
            if (SafeIsDefaultObject(obj)) continue;

            uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
            if (objAddr == g_Cache.Player) continue;
            if (IsUObjectDestroyed(objAddr)) continue;

            if (!SafeIsA(obj, bpPlayerClass)) continue;

            SDK::FVector loc;
            if (!GetPlayerLocation(objAddr, loc)) continue;

            outPlayers[count].Address = objAddr;
            outPlayers[count].Location = loc;
            count++;
        }
    }
    return count;
}

static void ScanAllPlayers()
{
    DWORD now = GetTickCount();
    if (now - g_LastPlayerScanTick < PLAYER_SCAN_INTERVAL_MS) return;
    g_LastPlayerScanTick = now;

    FPlayerInfo tempPlayers[32];
    int count = DoPlayerScan(tempPlayers, 32);
    {
        std::lock_guard<std::mutex> lock(g_PlayerMutex);
        memcpy(g_AllPlayers, tempPlayers, sizeof(FPlayerInfo) * count);
        g_AllPlayerCount = count;
    }
}

// ============================================================================
// 传送到指定玩家 (通过 ProcessEvent 调用 K2_SetActorLocation)
// ============================================================================
static void TeleportToPlayer(int index)
{
    if (index < 0 || index >= g_AllPlayerCount) return;
    if (!g_Cache.Player) return;

    // 初始化 K2_SetActorLocation UFunction (只查找一次)
    if (!g_SetActorLocationFunc)
        g_SetActorLocationFunc = FindUFunctionByName("K2_SetActorLocation");

    if (!g_SetActorLocationFunc) return;

    SDK::FVector targetLoc = g_AllPlayers[index].Location;
    targetLoc.X += 150.0;
    targetLoc.Z += 60.0;

    FK2_SetActorLocationParams params;
    memset(&params, 0, sizeof(params));
    params.NewLocation = targetLoc;
    params.bSweep = false;
    params.bTeleport = true;
    params.ReturnValue = false;

    SafeProcessEvent(
        reinterpret_cast<SDK::UObject*>(g_Cache.Player),
        g_SetActorLocationFunc,
        &params);
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
    int32_t custom = g_CustomAmmo.load();
    uintptr_t runtimeStats = g_Cache.PlayerState + Offsets::PlayerRuntimeStats;
    uintptr_t wepA = runtimeStats + Offsets::DatasWepA;
    int32_t maxMagA = 0, maxTotalA = 0;
    if (SafeRead(wepA + Offsets::MaxMagazineAmmos, maxMagA) && maxMagA > 0)
        SafeWriteIfDifferent(wepA + Offsets::CurrentMagazineAmmos, custom > 0 ? custom : maxMagA);
    if (SafeRead(wepA + Offsets::MaxTotalAmmos, maxTotalA) && maxTotalA > 0)
        SafeWriteIfDifferent(wepA + Offsets::CurrentTotalAmmos, custom > 0 ? custom : maxTotalA);

    uintptr_t wepB = runtimeStats + Offsets::DatasWepB;
    int32_t maxMagB = 0, maxTotalB = 0;
    if (SafeRead(wepB + Offsets::MaxMagazineAmmos, maxMagB) && maxMagB > 0)
        SafeWriteIfDifferent(wepB + Offsets::CurrentMagazineAmmos, custom > 0 ? custom : maxMagB);
    if (SafeRead(wepB + Offsets::MaxTotalAmmos, maxTotalB) && maxTotalB > 0)
        SafeWriteIfDifferent(wepB + Offsets::CurrentTotalAmmos, custom > 0 ? custom : maxTotalB);

    int32_t grenades = 0;
    SafeRead(runtimeStats + Offsets::AmountGrenades, grenades);
    if (grenades < 5)
        SafeWriteIfDifferent(runtimeStats + Offsets::AmountGrenades, custom > 0 ? custom : 5);
}

// 读取真实值
static void ReadRealValues()
{
    if (!g_Cache.Player) return;
    uintptr_t charMoveComp = SafeReadPtr(g_Cache.Player + Offsets::CharMoveComp);
    if (!charMoveComp) return;

    float speed = 0.0f;
    if (SafeRead(charMoveComp + Offsets::MaxWalkSpeed, speed))
        g_RealSpeed = speed;
}

// 移速修改: 每帧强制写入绝对值
static void ApplySpeedHack()
{
    if (!g_Cache.Player) return;
    float target = g_TargetSpeed.load();
    if (target <= 0.0f) return;

    uintptr_t charMoveComp = SafeReadPtr(g_Cache.Player + Offsets::CharMoveComp);
    if (!charMoveComp) return;

    // 使用 SafeWrite (无条件写入) 代替 SafeWriteIfDifferent
    // 因为游戏可能每帧重置这些值
    SafeWrite<float>(charMoveComp + Offsets::MaxWalkSpeed, target);
    SafeWrite<float>(charMoveComp + Offsets::MaxWalkSpeedCrouched, target * 0.5f);
    SafeWrite<float>(charMoveComp + Offsets::MaxAcceleration, 4096.0f);
}

// 保存当前位置为传送点
static void SaveTeleportPoint(int index)
{
    if (index < 0 || index >= 5) return;
    if (!g_Cache.Player) return;
    SDK::FVector loc;
    if (!GetPlayerLocation(g_Cache.Player, loc)) return;
    g_TeleportPoints[index] = loc;
    g_TeleportPointActive[index] = true;
}

// 传送到已保存的传送点
static void TeleportToPoint(int index)
{
    if (index < 0 || index >= 5) return;
    if (!g_TeleportPointActive[index]) return;
    if (!g_Cache.Player) return;

    if (!g_SetActorLocationFunc)
        g_SetActorLocationFunc = FindUFunctionByName("K2_SetActorLocation");
    if (!g_SetActorLocationFunc) return;

    SDK::FVector targetLoc = g_TeleportPoints[index];
    targetLoc.Z += 60.0;

    FK2_SetActorLocationParams params;
    memset(&params, 0, sizeof(params));
    params.NewLocation = targetLoc;
    params.bSweep = false;
    params.bTeleport = true;
    params.ReturnValue = false;

    SafeProcessEvent(
        reinterpret_cast<SDK::UObject*>(g_Cache.Player),
        g_SetActorLocationFunc,
        &params);
}

// 删除传送点
static void DeleteTeleportPoint(int index)
{
    if (index < 0 || index >= 5) return;
    g_TeleportPointActive[index] = false;
    g_TeleportPoints[index] = {};
}

// 处理传送请求 (从HTTP线程发起，主线程执行)
static void ProcessTeleportRequest()
{
    // 传送到玩家
    int idx = g_TeleportRequest.exchange(-1);
    if (idx >= 0)
        TeleportToPlayer(idx);

    // 传送到保存的点
    int pointIdx = g_TeleportPointRequest.exchange(-1);
    if (pointIdx >= 0)
        TeleportToPoint(pointIdx);

    // 保存传送点
    int saveIdx = g_SavePointRequest.exchange(-1);
    if (saveIdx >= 0)
        SaveTeleportPoint(saveIdx);
}

// ============================================================================
// 构建JSON状态
// ============================================================================
static std::string BuildStatusJson()
{
    std::string json = "{";
    json += "\"version\":\"" MOD_VERSION "\"";
    json += ",\"nocd\":" + std::string(g_ModState.bNoCooldown.load() ? "true" : "false");
    json += ",\"jump\":" + std::string(g_ModState.bInfiniteJump.load() ? "true" : "false");
    json += ",\"ammo\":" + std::string(g_ModState.bInfiniteAmmo.load() ? "true" : "false");
    json += ",\"speed\":" + std::string(g_ModState.bSpeedHack.load() ? "true" : "false");
    json += ",\"targetSpeed\":" + std::to_string(g_TargetSpeed.load());
    json += ",\"customAmmo\":" + std::to_string(g_CustomAmmo.load());
    json += ",\"realSpeed\":" + std::to_string(g_RealSpeed.load());

    json += ",\"points\":[";
    for (int i = 0; i < 5; i++)
    {
        if (i > 0) json += ",";
        if (g_TeleportPointActive[i])
        {
            json += "{\"active\":true";
            json += ",\"x\":" + std::to_string(g_TeleportPoints[i].X);
            json += ",\"y\":" + std::to_string(g_TeleportPoints[i].Y);
            json += ",\"z\":" + std::to_string(g_TeleportPoints[i].Z) + "}";
        }
        else
        {
            json += "{\"active\":false}";
        }
    }
    json += "]";

    json += ",\"players\":[";
    {
        std::lock_guard<std::mutex> lock(g_PlayerMutex);
        for (int i = 0; i < g_AllPlayerCount; i++)
        {
            if (i > 0) json += ",";
            json += "{\"x\":" + std::to_string(g_AllPlayers[i].Location.X);
            json += ",\"y\":" + std::to_string(g_AllPlayers[i].Location.Y);
            json += ",\"z\":" + std::to_string(g_AllPlayers[i].Location.Z) + "}";
        }
    }
    json += "]";

    json += "}";
    return json;
}

// ============================================================================
// 简易JSON解析
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
        res.set_content(BuildStatusJson(), "application/json");
    });

    svr.Post("/api/toggle/:feature", [](const httplib::Request& req, httplib::Response& res) {
        auto it = req.path_params.find("feature");
        if (it == req.path_params.end()) { res.set_content("{\"error\":\"missing\"}", "application/json"); return; }
        std::string f = it->second;
        bool newState = false;
        if (f == "nocd") { g_ModState.bNoCooldown = !g_ModState.bNoCooldown.load(); newState = g_ModState.bNoCooldown; }
        else if (f == "jump") { g_ModState.bInfiniteJump = !g_ModState.bInfiniteJump.load(); newState = g_ModState.bInfiniteJump; }
        else if (f == "ammo") { g_ModState.bInfiniteAmmo = !g_ModState.bInfiniteAmmo.load(); newState = g_ModState.bInfiniteAmmo; }
        else if (f == "speed") { g_ModState.bSpeedHack = !g_ModState.bSpeedHack.load(); newState = g_ModState.bSpeedHack; }
        else { res.set_content("{\"error\":\"unknown\"}", "application/json"); return; }
        res.set_content("{\"ok\":true,\"state\":" + std::string(newState ? "true" : "false") + "}", "application/json");
    });

    svr.Post("/api/speed", [](const httplib::Request& req, httplib::Response& res) {
        float val = ParseJsonFloat(req.body, "speed");
        if (val < 0) val = 0;
        if (val > 5000) val = 5000;
        g_TargetSpeed = val;
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Post("/api/ammo", [](const httplib::Request& req, httplib::Response& res) {
        int val = ParseJsonInt(req.body, "ammo");
        if (val < 0) val = 0;
        if (val > 99999) val = 99999;
        g_CustomAmmo = val;
        res.set_content("{\"ok\":true}", "application/json");
    });

    // 传送请求: 设置原子标志，主线程执行实际传送
    svr.Post("/api/teleport", [](const httplib::Request& req, httplib::Response& res) {
        int idx = ParseJsonInt(req.body, "index");
        g_TeleportRequest = idx;
        res.set_content("{\"ok\":true}", "application/json");
    });

    // 保存传送点
    svr.Post("/api/savepoint", [](const httplib::Request& req, httplib::Response& res) {
        int idx = ParseJsonInt(req.body, "index");
        if (idx < 0 || idx >= 5) { res.set_content("{\"error\":\"invalid index\"}", "application/json"); return; }
        g_SavePointRequest = idx;
        res.set_content("{\"ok\":true}", "application/json");
    });

    // 传送到已保存的点
    svr.Post("/api/gotopoint", [](const httplib::Request& req, httplib::Response& res) {
        int idx = ParseJsonInt(req.body, "index");
        if (idx < 0 || idx >= 5) { res.set_content("{\"error\":\"invalid index\"}", "application/json"); return; }
        g_TeleportPointRequest = idx;
        res.set_content("{\"ok\":true}", "application/json");
    });

    // 删除传送点
    svr.Post("/api/delpoint", [](const httplib::Request& req, httplib::Response& res) {
        int idx = ParseJsonInt(req.body, "index");
        if (idx < 0 || idx >= 5) { res.set_content("{\"error\":\"invalid index\"}", "application/json"); return; }
        DeleteTeleportPoint(idx);
        res.set_content("{\"ok\":true}", "application/json");
    });

    svr.Get("/api/update", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(CheckGitHubUpdate(), "application/json");
    });

    svr.listen("0.0.0.0", 1145);
}

// ============================================================================
// 热键处理
// ============================================================================
static bool g_PrevF1 = false;
static bool g_PrevF2 = false;
static bool g_PrevF3 = false;
static bool g_PrevF4 = false;
static bool g_PrevEnd = false;

static void HandleHotkeys()
{
    {
        bool cur = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (cur && !g_PrevF1) g_ModState.bNoCooldown = !g_ModState.bNoCooldown.load();
        g_PrevF1 = cur;
    }
    {
        bool cur = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
        if (cur && !g_PrevF2) g_ModState.bInfiniteJump = !g_ModState.bInfiniteJump.load();
        g_PrevF2 = cur;
    }
    {
        bool cur = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
        if (cur && !g_PrevF3) g_ModState.bInfiniteAmmo = !g_ModState.bInfiniteAmmo.load();
        g_PrevF3 = cur;
    }
    {
        bool cur = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
        if (cur && !g_PrevF4) g_ModState.bSpeedHack = !g_ModState.bSpeedHack.load();
        g_PrevF4 = cur;
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
        ReadRealValues();

        if (g_ModState.bNoCooldown)    ApplyNoCooldown();
        if (g_ModState.bInfiniteJump)  ApplyInfiniteJump();
        if (g_ModState.bInfiniteAmmo)  ApplyInfiniteAmmo();
        if (g_ModState.bSpeedHack)     ApplySpeedHack();

        ProcessTeleportRequest();
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
