// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Common/CommonTypes.h"

#include "Core/PowerPC/PowerPC.h"

namespace Cheats
{
struct UnspecifiedItemType
{
  u32 size;
};

typedef std::variant<UnspecifiedItemType, u8, u16, u32, u64> MemoryItem;
typedef u32 MemorySize, Address;
typedef std::optional<Address> MaybeAddress;

typedef PowerPC::AddressTranslationType AddressTranslation;

enum class MemoryItemType : u8  // TODO: try other underlying data types
{
  UNSPECIFIED = MemoryItem(UnspecifiedItemType{}).index(),
  U8 = MemoryItem(static_cast<u8>(0)).index(),
  U16 = MemoryItem(static_cast<u16>(0)).index(),
  U32 = MemoryItem(static_cast<u32>(0)).index(),
  U64 = MemoryItem(static_cast<u64>(0)).index(),
};

const UnspecifiedItemType UnspecifiedItem{0};

struct SearchResult
{
  Address address;
  MemoryItem current;
  MemoryItem previous;
};

std::vector<std::string> GetMemoryItemTypeNames();
std::vector<std::string> GetFriendlyMemoryItemTypeNames();
MemoryItemType GetMemoryItemTypeForName(const std::string& name);
std::optional<std::string> GetNameForMemoryItemType(MemoryItemType type);
std::optional<std::string> GetFriendlyNameForMemoryItemType(MemoryItemType type);

inline bool IsValidMemoryItemType(MemoryItemType type)
{
  return type != MemoryItemType::UNSPECIFIED &&
         static_cast<std::size_t>(type) < std::variant_size<MemoryItem>::value;
}

inline bool IsValidMemoryItem(const MemoryItem& item)
{
  return !std::holds_alternative<UnspecifiedItemType>(item);
}

inline MemoryItemType GetMemoryItemType(const MemoryItem& item)
{
  return static_cast<MemoryItemType>(item.index());
}

const std::string GetMemoryItemTypeName(const MemoryItem& item);
const std::string GetFriendlyMemoryItemTypeName(const MemoryItem& item);

MemorySize GetMemoryItemTypeSize(MemoryItemType type);
MemorySize GetMemoryItemSize(const MemoryItem& item);
MemorySize GetMemoryItemAlignment(const MemoryItem& item);

MemoryItem MakeMemoryItemFromType(MemoryItemType type);
MemoryItem MakeMemoryItemFromTypeName(const std::string& name);

bool IsAddressRangeValid(Address address, MemorySize size, AddressTranslation translation);
MemoryItem PeekMemoryItem(Address address, MemoryItemType type, AddressTranslation translation);
bool PokeMemoryItem(Address address, const MemoryItem& item, AddressTranslation translation);

std::optional<std::string> FormatMemoryItem(const MemoryItem& item);
MemoryItem ParseMemoryItem(const std::string& str, MemoryItemType type);

}  // namespace Cheats
