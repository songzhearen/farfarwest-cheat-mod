// sdk_impl.cpp : SDK必要函数的手动实现
// 避免编译SDK巨大的cpp文件，只实现本mod需要的函数

#include <Windows.h>
#include <string>

// SDK头文件
#include "CppSDK/UnrealContainers.hpp"
#include "CppSDK/SDK/Basic.hpp"
#include "CppSDK/SDK/CoreUObject_structs.hpp"
#include "CppSDK/SDK/CoreUObject_classes.hpp"

namespace SDK
{

// ============================================================================
// InSDKUtils
// ============================================================================
uintptr_t InSDKUtils::GetImageBase()
{
    return reinterpret_cast<uintptr_t>(GetModuleHandle(0));
}

// ============================================================================
// BasicFilesImpleUtils
// ============================================================================
namespace BasicFilesImpleUtils
{

class UClass* FindClassByName(const std::string& Name, bool bByFullName)
{
    return bByFullName ? UObject::FindClass(Name) : UObject::FindClassFast(Name);
}

class UClass* FindClassByFullName(const std::string& Name)
{
    return UObject::FindClass(Name);
}

std::string GetObjectName(class UClass* Class)
{
    return Class->GetName();
}

int32 GetObjectIndex(class UClass* Class)
{
    return Class->Index;
}

uint64 GetObjFNameAsUInt64(class UClass* Class)
{
    return *reinterpret_cast<uint64*>(&Class->Name);
}

class UObject* GetObjectByIndex(int32 Index)
{
    return UObject::GObjects->GetByIndex(Index);
}

UFunction* FindFunctionByFName(const FName* Name)
{
    for (int i = 0; i < UObject::GObjects->Num(); ++i)
    {
        UObject* Object = UObject::GObjects->GetByIndex(i);
        if (!Object) continue;
        if (Object->Name == *Name)
            return static_cast<UFunction*>(Object);
    }
    return nullptr;
}

FName StringToName(const wchar_t* Name)
{
    // Stub: 本mod不使用STATIC_NAME_IMPL宏
    static FName EmptyName;
    return EmptyName;
}

} // namespace BasicFilesImpleUtils

// ============================================================================
// GetStaticName (本mod不使用，提供stub)
// ============================================================================
const FName& GetStaticName(const wchar_t* Name, FName& StaticName)
{
    if (StaticName.IsNone())
        StaticName = BasicFilesImpleUtils::StringToName(Name);
    return StaticName;
}

// ============================================================================
// FWeakObjectPtr
// ============================================================================
class UObject* FWeakObjectPtr::Get() const
{
    return UObject::GObjects->GetByIndex(ObjectIndex);
}

class UObject* FWeakObjectPtr::operator->() const
{
    return UObject::GObjects->GetByIndex(ObjectIndex);
}

bool FWeakObjectPtr::operator==(const FWeakObjectPtr& Other) const
{
    return ObjectIndex == Other.ObjectIndex;
}

bool FWeakObjectPtr::operator!=(const FWeakObjectPtr& Other) const
{
    return ObjectIndex != Other.ObjectIndex;
}

bool FWeakObjectPtr::operator==(const class UObject* Other) const
{
    return ObjectIndex == Other->Index;
}

bool FWeakObjectPtr::operator!=(const class UObject* Other) const
{
    return ObjectIndex != Other->Index;
}

// ============================================================================
// UObject 关键函数
// ============================================================================
class UObject* UObject::FindObjectFastImpl(const std::string& Name, EClassCastFlags RequiredType)
{
    for (int i = 0; i < GObjects->Num(); ++i)
    {
        UObject* Object = GObjects->GetByIndex(i);
        if (!Object) continue;
        if (Object->HasTypeFlag(RequiredType) && Object->GetName() == Name)
            return Object;
    }
    return nullptr;
}

class UObject* UObject::FindObjectImpl(const std::string& FullName, EClassCastFlags RequiredType)
{
    for (int i = 0; i < GObjects->Num(); ++i)
    {
        UObject* Object = GObjects->GetByIndex(i);
        if (!Object) continue;
        if (Object->HasTypeFlag(RequiredType) && Object->GetFullName() == FullName)
            return Object;
    }
    return nullptr;
}

std::string UObject::GetFullName() const
{
    if (this && Class)
    {
        std::string Temp;
        for (UObject* NextOuter = Outer; NextOuter; NextOuter = NextOuter->Outer)
            Temp = NextOuter->GetName() + "." + Temp;

        std::string Name = Class->GetName();
        Name += " ";
        Name += Temp;
        Name += GetName();
        return Name;
    }
    return "None";
}

std::string UObject::GetName() const
{
    return this ? Name.ToString() : "None";
}

bool UObject::HasTypeFlag(EClassCastFlags TypeFlags) const
{
    return (Class->CastFlags & TypeFlags) != 0;
}

bool UObject::IsA(EClassCastFlags TypeFlags) const
{
    return (Class->CastFlags & TypeFlags) != 0;
}

bool UObject::IsA(const class FName& ClassName) const
{
    return Class->IsSubclassOf(ClassName);
}

bool UObject::IsA(const class UClass* TypeClass) const
{
    return Class->IsSubclassOf(TypeClass);
}

bool UObject::IsDefaultObject() const
{
    return (Flags & EObjectFlags::ClassDefaultObject) != 0;
}

void UObject::ExecuteUbergraph(int32 EntryPoint)
{
    // Stub - 本mod不使用ProcessEvent调用
}

// ============================================================================
// UStruct
// ============================================================================
bool UStruct::IsSubclassOf(const UStruct* Base) const
{
    if (!Base)
        return false;
    const int32 NumParentStructBasesInChainMinusOne = Base->BaseChain.NumStructBasesInChainMinusOne;
    return NumParentStructBasesInChainMinusOne <= BaseChain.NumStructBasesInChainMinusOne
        && BaseChain.StructBaseChainArray[NumParentStructBasesInChainMinusOne] == &Base->BaseChain;
}

bool UStruct::IsSubclassOf(const FName& BaseClassName) const
{
    if (BaseClassName.IsNone())
        return false;
    for (const UStruct* Struct = this; Struct; Struct = Struct->SuperStruct)
    {
        if (Struct->Name == BaseClassName)
            return true;
    }
    return false;
}

// ============================================================================
// UClass
// ============================================================================
class UFunction* UClass::GetFunction(const char* ClassName, const char* FuncName) const
{
    for (const UStruct* Clss = this; Clss; Clss = Clss->SuperStruct)
    {
        if (Clss->GetName() != ClassName)
            continue;
        for (UField* Field = Clss->Children; Field; Field = Field->Next)
        {
            if (Field->HasTypeFlag(EClassCastFlags::Function) && Field->GetName() == FuncName)
                return static_cast<class UFunction*>(Field);
        }
    }
    return nullptr;
}

} // namespace SDK
