// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/CheatingNew/Utils.h"

#include <cstddef>
#include <istream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"

namespace Cheats
{
const char* TYPE_NAMES[] = {"Unspecified", "Byte", "Short", "Long", "Quad"};
const char* FRIENDLY_TYPE_NAMES[] = {"Unspecified", "Byte (8 bit)", "Short (16 bit)",
                                     "Long (32 bit)", "Quad (64 bit)"};

std::vector<std::string> GetMemoryItemTypeNames()
{
  return std::vector<std::string>(std::cbegin(TYPE_NAMES), std::cend(TYPE_NAMES));
}

std::vector<std::string> GetFriendlyMemoryItemTypeNames()
{
  return std::vector<std::string>(std::cbegin(FRIENDLY_TYPE_NAMES), std::cend(FRIENDLY_TYPE_NAMES));
}

MemoryItemType GetMemoryItemTypeForName(const std::string& name)
{
  const char* name_str = name.c_str();

  for (const char*& it : TYPE_NAMES)
  {
    if (strcasecmp(it, name_str) == 0)
      return static_cast<MemoryItemType>(&it - std::cbegin(TYPE_NAMES));
  }

  return MemoryItemType::UNSPECIFIED;
}

std::optional<std::string> GetNameForMemoryItemType(MemoryItemType type)
{
  if (!IsValidMemoryItemType(type))
    return std::nullopt;

  return std::string(TYPE_NAMES[static_cast<std::size_t>(type)]);
}

std::optional<std::string> GetFriendlyNameForMemoryItemType(MemoryItemType type)
{
  if (!IsValidMemoryItemType(type))
    return std::nullopt;

  return std::string(FRIENDLY_TYPE_NAMES[static_cast<std::size_t>(type)]);
}

const std::string GetMemoryItemTypeName(const MemoryItem& item)
{
  return std::string(TYPE_NAMES[item.index()]);
}

const std::string GetFriendlyMemoryItemTypeName(const MemoryItem& item)
{
  return std::string(FRIENDLY_TYPE_NAMES[item.index()]);
}

MemorySize GetMemoryItemTypeSize(MemoryItemType type)
{
  switch (type)
  {
  case MemoryItemType::U8:
    return sizeof(u8);
  case MemoryItemType::U16:
    return sizeof(u16);
  case MemoryItemType::U32:
    return sizeof(u32);
  case MemoryItemType::U64:
    return sizeof(u64);
  default:
    return 0;
  }
}

MemorySize GetMemoryItemSize(const MemoryItem& item)
{
  if (!IsValidMemoryItem(item))
    return std::get<UnspecifiedItemType>(item).size;

  return GetMemoryItemTypeSize(GetMemoryItemType(item));
}

MemorySize GetMemoryItemAlignment(const MemoryItem& item)
{
  // if (std::holds_alternative<u32>(item) || std::holds_alternative<u64>(item))
  //   return 4;

  // if (std::holds_alternative<u16>(item))
  //  return 2;

  return 1;
}

MemoryItem MakeMemoryItemFromType(MemoryItemType type)
{
  switch (type)
  {
  case MemoryItemType::U8:
    return static_cast<u8>(0);
  case MemoryItemType::U16:
    return static_cast<u16>(0);
  case MemoryItemType::U32:
    return static_cast<u32>(0);
  case MemoryItemType::U64:
    return static_cast<u64>(0);
  default:
    return UnspecifiedItem;
  }
}

MemoryItem MakeMemoryItemFromTypeName(const std::string& name)
{
  return MakeMemoryItemFromType(GetMemoryItemTypeForName(name));
}

bool IsAddressRangeValid(Address address, MemorySize size, AddressTranslation translation)
{
  // TODO: prefix with _dbg
  _assert_msg_(ACTIONREPLAY, size != 0, "size of 0 not supported as of now due to ambiguity");

  if (!Memory::IsInitialized())
    return false;

  return PowerPC::HostIsRAMRange(address, size, translation);
}

MemoryItem PeekMemoryItem(Address address, MemoryItemType type, AddressTranslation translation)
{
  if (!IsValidMemoryItemType(type))
    return UnspecifiedItem;

  if (!Memory::IsInitialized())
    return UnspecifiedItem;

  switch (type)
  {
  case MemoryItemType::U8:
  {
    u8 buffer;
    if (PowerPC::HostReadRAM(&buffer, address, sizeof(buffer),
                             PowerPC::AddressTranslationType::DATA))
      return buffer;

    break;
  }
  case MemoryItemType::U16:
  {
    u16 buffer;
    if (PowerPC::HostReadRAM(&buffer, address, sizeof(buffer),
                             PowerPC::AddressTranslationType::DATA))
      return Common::swap16(buffer);

    break;
  }
  case MemoryItemType::U32:
  {
    u32 buffer;
    if (PowerPC::HostReadRAM(&buffer, address, sizeof(buffer),
                             PowerPC::AddressTranslationType::DATA))
      return Common::swap32(buffer);

    break;
  }
  case MemoryItemType::U64:
  {
    u64 buffer;
    if (PowerPC::HostReadRAM(&buffer, address, sizeof(buffer),
                             PowerPC::AddressTranslationType::DATA))
      return Common::swap64(buffer);

    break;
  }
  default:;  // unhandled types fall through
  }

  return UnspecifiedItem;
}

bool PokeMemoryItem(Address address, const MemoryItem& item, AddressTranslation translation)
{
  if (!IsValidMemoryItem(item))
    return false;

  if (!Memory::IsInitialized())
    return false;

  if (auto* u8_ptr = std::get_if<u8>(&item))
  {
    return PowerPC::HostWriteRAM(address, u8_ptr, sizeof(*u8_ptr),
                                 PowerPC::AddressTranslationType::DATA);
  }

  if (auto* u16_ptr = std::get_if<u16>(&item))
  {
    auto data = Common::swap16(*u16_ptr);
    return PowerPC::HostWriteRAM(address, &data, sizeof(data),
                                 PowerPC::AddressTranslationType::DATA);
  }

  if (auto* u32_ptr = std::get_if<u32>(&item))
  {
    auto data = Common::swap32(*u32_ptr);
    return PowerPC::HostWriteRAM(address, &data, sizeof(data),
                                 PowerPC::AddressTranslationType::DATA);
  }

  if (auto* u64_ptr = std::get_if<u64>(&item))
  {
    auto data = Common::swap64(*u64_ptr);
    return PowerPC::HostWriteRAM(address, &data, sizeof(data),
                                 PowerPC::AddressTranslationType::DATA);
  }

  return false;
}

std::optional<std::string> FormatMemoryItem(const MemoryItem& item)
{
  if (std::holds_alternative<u8>(item) || std::holds_alternative<u16>(item) ||
      std::holds_alternative<u32>(item) || std::holds_alternative<u64>(item))
  {
    if (auto* u8_ptr = std::get_if<u8>(&item))
      return std::to_string(*u8_ptr);

    if (auto* u16_ptr = std::get_if<u16>(&item))
      return std::to_string(*u16_ptr);

    if (auto* u32_ptr = std::get_if<u32>(&item))
      return std::to_string(*u32_ptr);

    if (auto* u64_ptr = std::get_if<u64>(&item))
      return std::to_string(*u64_ptr);
  }

  return std::nullopt;
}

MemoryItem ParseMemoryItem(std::istream& is, MemoryItemType type)
{
  if (type == MemoryItemType::U8 || type == MemoryItemType::U16 || type == MemoryItemType::U32 ||
      type == MemoryItemType::U64)
  {
    unsigned long long primitive;

    if (!(is >> primitive))
      return UnspecifiedItem;

    switch (type)
    {
    case MemoryItemType::U8:
      return static_cast<u8>(primitive);
    case MemoryItemType::U16:
      return static_cast<u16>(primitive);
    case MemoryItemType::U32:
      return static_cast<u32>(primitive);
    case MemoryItemType::U64:
      return static_cast<u64>(primitive);
    default:
      ERROR_LOG(ACTIONREPLAY, "%s(): unhandled integral type", __FUNCTION__);
    }
  }

  return UnspecifiedItem;
}

MemoryItem ParseMemoryItem(const std::string& str, MemoryItemType type)
{
  std::istringstream iss{str};
  MemoryItem item = ParseMemoryItem(iss, type);
  if (!IsValidMemoryItem(item))
    return UnspecifiedItem;

  if (iss.get() != decltype(iss)::traits_type::eof())
    return UnspecifiedItem;

  return item;
}

}  // namespace Cheats
