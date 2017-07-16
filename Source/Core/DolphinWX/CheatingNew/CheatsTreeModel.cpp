// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/CheatingNew/CheatsTreeModel.h"

#include <optional>
#include <string>
#include <variant>

#include <wx/dataview.h>
#include <wx/string.h>
#include <wx/variant.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "DolphinWX/CheatingNew/Utils.h"
#include "DolphinWX/WxUtils.h"

namespace Cheats
{
CheatsTreeModel::CheatsTreeModel()
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): creating", __FUNCTION__);

  void* key1 = reinterpret_cast<void*>(1);
  {
    const Entry entry{nullptr, "Just a Header", "Thing you can expand and collapse.", std::nullopt};
    m_entries.emplace(key1, std::move(entry));
    m_children.emplace(entry.parent, key1);
  }

  void* key2 = reinterpret_cast<void*>(2);
  {
    const Entry entry{nullptr, "Byte with Children", "",
                      Data{0x80001111U, MemoryItemType::U8, UnspecifiedItem, false}};
    m_entries.emplace(key2, std::move(entry));
    m_children.emplace(entry.parent, key2);
  }

  void* key3 = reinterpret_cast<void*>(3);
  {
    const Entry entry{key1, "Short", "",
                      Data{0x80002222U, MemoryItemType::U16, UnspecifiedItem, false}};
    m_entries.emplace(key3, std::move(entry));
    m_children.emplace(entry.parent, key3);
  }

  void* key4 = reinterpret_cast<void*>(4);
  {
    const Entry entry{key2, "Long", "",
                      Data{0x80004444U, MemoryItemType::U32, UnspecifiedItem, false}};
    m_entries.emplace(key4, std::move(entry));
    m_children.emplace(entry.parent, key4);
  }

  void* key5 = reinterpret_cast<void*>(5);
  {
    const Entry entry{key1, "Quad", "",
                      Data{0x80008888U, MemoryItemType::U64, UnspecifiedItem, false}};
    m_entries.emplace(key5, std::move(entry));
    m_children.emplace(entry.parent, key5);
  }
}

CheatsTreeModel::~CheatsTreeModel()
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): destructing", __FUNCTION__);
}

unsigned int CheatsTreeModel::GetColumnCount() const
{
  return 6;
}

wxString CheatsTreeModel::GetColumnType(unsigned int col) const
{
  return (col == 5) ? "bool" : "string";
}

bool CheatsTreeModel::IsEnabled(const wxDataViewItem& item, unsigned int col) const
{
  if (col >= 6)
    return false;

  auto it = m_entries.find(item.GetID());
  if (it == m_entries.end())
    return false;

  if (col <= 1)
    return true;

  const Entry& entry = it->second;
  if (!entry.data)
    return false;

  if (col == 2)
    return true;

  if (!IsValidMemoryItemType(entry.data->type))
    return false;

  return true;
}

void CheatsTreeModel::GetValue(wxVariant& variant, const wxDataViewItem& item,
                               unsigned int col) const
{
  auto it = m_entries.find(item.GetID());
  if (it != m_entries.end())
  {
    Entry& entry = it->second;

    variant.MakeNull();

    switch (col)
    {
    case 0:
      variant = StrToWxStr(entry.name);
      return;

    case 1:
      variant = StrToWxStr(entry.description);
      return;

    case 2:
      if (!entry.data)
        return;

      variant = wxString::Format("0x%X", entry.data->address);
      return;

    case 3:
      if (!entry.data)
        return;

      if (auto str_opt = GetNameForMemoryItemType(entry.data->type))
      {
        variant = StrToWxStr(*str_opt);
      }

      return;

    case 4:
      if (!entry.data)
        return;

      if (!IsValidMemoryItemType(entry.data->type))
        return;

      if (!entry.data->locked)
      {
        entry.data->content =
            PeekMemoryItem(entry.data->address, entry.data->type, AddressTranslation::DATA);
      }

      if (!IsValidMemoryItem(entry.data->content))
      {
        variant = "<invalid>";
        return;
      }

      if (auto str_opt = FormatMemoryItem(entry.data->content))
      {
        variant = StrToWxStr(*str_opt);
        return;
      }

      break;

    case 5:
      if (!entry.data)
        return;

      variant = entry.data->locked;
      return;
    }
  }

  variant = "???";
}

bool CheatsTreeModel::SetValue(const wxVariant& variant, const wxDataViewItem& item,
                               unsigned int col)
{
  auto it = m_entries.find(item.GetID());
  if (it == m_entries.end())
    return false;

  Entry& entry = it->second;

  switch (col)
  {
  case 0:
    entry.name = WxStrToStr(variant.GetString());
    return true;

  case 1:
    entry.description = WxStrToStr(variant.GetString());
    return true;

  case 2:
    if (!entry.data)
      return false;

    // TODO: implement setting address
    break;

  case 3:
    if (!entry.data)
      return false;

    return ChangeDataType(*entry.data, GetMemoryItemTypeForName(WxStrToStr(variant.GetString())));

  case 4:
    if (!entry.data)
      return false;

    if (!IsValidMemoryItemType(entry.data->type))
      return false;

    {
      auto content = ParseMemoryItem(WxStrToStr(variant.GetString()), entry.data->type);
      if (!IsValidMemoryItem(content))
        return false;

      if (!entry.data->locked &&
          !PokeMemoryItem(entry.data->address, content, AddressTranslation::DATA))
        return false;

      entry.data->content = content;
    }

    return true;

  case 5:
    if (!entry.data)
      return false;

    return ChangeDataLock(*entry.data, variant.GetBool());
  }

  return false;
}

wxDataViewItem CheatsTreeModel::GetParent(const wxDataViewItem& item) const
{
  auto it = m_entries.find(item.GetID());

  return wxDataViewItem((it != m_entries.end()) ? it->second.parent : nullptr);
}

bool CheatsTreeModel::HasContainerColumns(const wxDataViewItem& item) const
{
  return true;
}

bool CheatsTreeModel::IsContainer(const wxDataViewItem& item) const
{
  void* key = item.GetID();
  if (key == nullptr)
    return true;  // The invisible root element is always a container.

  if (m_children.find(key) != m_children.end())
    return true;

  auto it = m_entries.find(key);
  if (it == m_entries.end())
    return false;

  return !(it->second.data);  // If it has no data, it's a header and therefore a container.
}

unsigned int CheatsTreeModel::GetChildren(const wxDataViewItem& item,
                                          wxDataViewItemArray& children) const
{
  children.Empty();

  auto range = m_children.equal_range(item.GetID());
  if (range.first == range.second)
    return 0;

  for (auto it = range.first; it != range.second; ++it)
  {
    children.Add(wxDataViewItem(it->second));
  }

  return children.size();
}

bool CheatsTreeModel::DeleteEntry(const wxDataViewItem& entry)
{
  if (!entry.IsOk())
    return false;

  auto key = entry.GetID();
  auto it = m_entries.find(key);
  if (it == m_entries.end())
    return false;

  // FIXME: move children to parent

  auto parent = it->second.parent;
  m_entries.erase(it);

  ItemDeleted(wxDataViewItem{parent}, wxDataViewItem{key});
  return true;
}

bool CheatsTreeModel::ChangeDataLock(Data& data, bool lock)
{
  if (!IsValidMemoryItemType(data.type))
    return false;

  if (!lock)
  {
    data.locked = false;
    return true;
  }

  if (data.locked)
    return true;

  auto content = PeekMemoryItem(data.address, data.type, AddressTranslation::DATA);
  if (!IsValidMemoryItem(content))
    return false;

  data.locked = true;
  data.content = content;
  return true;
}

bool CheatsTreeModel::ChangeDataType(Data& data, MemoryItemType type)
{
  if (type == data.type)
    return true;

  data.content = MakeMemoryItemFromType(type);
  data.type = type;
  return true;
}

}  // namespace Cheats
