// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <optional>

#include <wx/dataview.h>
#include <wx/string.h>
#include <wx/variant.h>

#include "DolphinWX/CheatingNew/Finder.h"
#include "DolphinWX/CheatingNew/Utils.h"

namespace Cheats
{
class SearchResultsModel final : public wxDataViewVirtualListModel
{
public:
  typedef Finder::NewResultsCallback NewResultsCallback;
  typedef std::optional<NewResultsCallback> OptionalNewResultsCallback;
  typedef Finder::SearchProgressCallback SearchProgressCallback;
  typedef std::optional<SearchProgressCallback> OptionalSearchProgressCallback;
  typedef Finder::Status Status;

  explicit SearchResultsModel(
      const OptionalNewResultsCallback& new_results_callback = std::nullopt);
  ~SearchResultsModel();

  Status SearchItem(const MemoryItem& item,
                    const OptionalSearchProgressCallback& search_progress_callback = std::nullopt);
  bool NewResults();
  void CancelSearch();
  void ClearResults();

  virtual unsigned int GetColumnCount() const;
  virtual wxString GetColumnType(unsigned int col) const;

  virtual bool GetAttrByRow(unsigned int row, unsigned int col, wxDataViewItemAttr& attr) const;
  virtual bool IsEnabledByRow(unsigned int row, unsigned int col) const;
  virtual void GetValueByRow(wxVariant& variant, unsigned int row, unsigned int col) const;
  virtual bool SetValueByRow(const wxVariant& variant, unsigned int row, unsigned int col);

  // wxDataViewVirtualListModel is not truly virtual. It probably stores a mapping between rows and
  // IDs that it likely generates itself. If you use a substantially larger number of rows and
  // replace all of them at once (which we do), it will hog the event system for several seconds or
  // minutes, blocking some events and letting others through (it's likely prioritized).
  static constexpr std::size_t MAX_ROWS = 9999;

private:
  mutable Finder m_finder;
};

}  // namespace Cheats
