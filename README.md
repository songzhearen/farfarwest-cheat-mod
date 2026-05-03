# FarFarWest Mod — 无限火力

《Far Far West》游戏的 DLL 注入 Mod，提供 Web 控制界面。

## 功能

- **无冷却** — 技能无 CD
- **无限跳跃** — 空中无限连跳
- **无限弹药** — 子弹不减
- **移速修改** — 自定义移动速度
- **传送到玩家** — 一键传送到其他玩家位置
- **传送点** — 保存/传送至自定义坐标

## 使用方式

1. 编译项目，生成 DLL
2. 将 DLL 注入 `FarFarWest-Win64-Shipping.exe`
3. 浏览器访问 `http://localhost:1145` 打开控制界面

## 构建环境

- Visual Studio 2022
- Windows SDK 10.0
- 目标平台：x64 / Win32

## 项目结构

```
mod/
├── dllmain.cpp        # 主逻辑（偏移定义、功能实现、HTTP 服务）
├── web_ui.h           # Web 控制界面 HTML
├── httplib.h          # cpp-httplib（第三方 HTTP 库）
├── pch.h / pch.cpp    # 预编译头
├── framework.h        # Windows 框架头文件
├── sdk_impl.cpp       # SDK 实现
├── mod.vcxproj        # VS 项目文件
└── CppSDK/            # UE5 SDK 头文件（自动生成）
```
##注入器
https://github.com/songzhearen/ffwinject
## 作者

songzhearen
