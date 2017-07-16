// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include <wx/dataview.h>

#include "DolphinWX/CheatingNew/Utils.h"

namespace Cheats
{
class CheatsTreeModel final : public wxDataViewModel
{
public:
  explicit CheatsTreeModel();
  ~CheatsTreeModel();

  virtual unsigned int GetColumnCount() const;
  virtual wxString GetColumnType(unsigned int col) const;

  virtual bool IsEnabled(const wxDataViewItem& item, unsigned int col) const;
  virtual void GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const;
  virtual bool SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned int col);

  virtual wxDataViewItem GetParent(const wxDataViewItem& item) const;
  virtual bool HasContainerColumns(const wxDataViewItem& item) const;
  virtual bool IsContainer(const wxDataViewItem& item) const;
  virtual unsigned int GetChildren(const wxDataViewItem& item, wxDataViewItemArray& children) const;

  bool AddEntry(Address address, MemoryItemType type);
  bool DeleteEntry(const wxDataViewItem& entry);

private:
  struct Data
  {
    Address address;
    MemoryItemType type;
    MemoryItem content;
    bool locked;
  };

  struct Entry
  {
    void* parent;
    std::string name;
    std::string description;
    std::optional<Data> data;
  };

  uintptr_t m_current_key = 0;
  mutable std::unordered_map<void*, Entry> m_entries;
  std::unordered_multimap<void*, void*> m_children;

  void* GetNewKey();

  static bool ChangeDataLock(Data& data, bool lock);
  static bool ChangeDataType(Data& data, MemoryItemType type);
};

}  // namespace Cheats
