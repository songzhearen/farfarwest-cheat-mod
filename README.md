# FarFarWest Mod — 无限火力

《Far Far West》游戏的 DLL 注入 Mod，提供 Web 控制界面。

GitHub: https://github.com/songzhearen/farfarwest-cheat-mod

## 功能

- **无冷却** — 技能无 CD
- **无限跳跃** — 空中无限连跳
- **无限弹药** — 子弹不减
- **移速修改** — 自定义移动速度
- **传送到玩家** — 一键传送到其他玩家位置
- **传送点** — 保存/传送至自定义坐标

## 使用方式

1. 编译项目，生成 DLL。注入器：https://github.com/songzhearen/ffwinject
2. 将 DLL 注入 `FarFarWest-Win64-Shipping.exe`
3. 浏览器访问 `http://localhost:1145` 打开控制界面

## 构建环境

- Visual Studio 2022
- Windows SDK 10.0
- 目标平台：x64 / Win32

## SDK 依赖

本项目依赖 UE5 SDK 进行编译。SDK 文件不随项目发布，需自行生成：

1. 使用 [Dumper-7](https://github.com/Encryqed/Dumper-7) 对游戏进行 SDK Dump
2. 将生成的 SDK 文件放入 `mod/CppSDK/` 目录
3. 确保 `CppSDK/SDK.hpp` 及相关头文件存在后即可编译

## 项目结构

```
mod/
├── dllmain.cpp        # 主逻辑（偏移定义、功能实现、HTTP 服务、版本检测）
├── web_ui.h           # Web 控制界面 HTML
├── httplib.h          # cpp-httplib（第三方 HTTP 库）
├── pch.h / pch.cpp    # 预编译头
├── framework.h        # Windows 框架头文件
├── sdk_impl.cpp       # SDK 实现
├── mod.vcxproj        # VS 项目文件
└── CppSDK/            # UE5 SDK 头文件（需使用 Dumper-7 自行生成）
```

## 作者

songzhearen
