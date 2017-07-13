// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/CheatingNew/SearchResultsModel.h"

#include <algorithm>
#include <variant>

#include <wx/dataview.h>
#include <wx/string.h>
#include <wx/variant.h>

#include "Common/Logging/Log.h"

#include "DolphinWX/CheatingNew/Finder.h"
#include "DolphinWX/CheatingNew/Utils.h"
#include "DolphinWX/WxUtils.h"

namespace Cheats
{
SearchResultsModel::SearchResultsModel(const OptionalNewResultsCallback& new_results_callback)
    : m_finder(new_results_callback)
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): creating", __FUNCTION__);
}

SearchResultsModel::~SearchResultsModel()
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): destructing", __FUNCTION__);
}

SearchResultsModel::Status
SearchResultsModel::SearchItem(const MemoryItem& item,
                               const OptionalSearchProgressCallback& search_progress_callback)
{
  return m_finder.SearchItem(item, search_progress_callback);
}

bool SearchResultsModel::NewResults()
{
  if (!m_finder.NewResults())
    return false;

  Reset(std::min(m_finder.GetResultCount(), MAX_ROWS + 1));
  return true;
}

void SearchResultsModel::CancelSearch()
{
  m_finder.CancelSearch();
}

void SearchResultsModel::ClearResults()
{
  m_finder.ClearResults();
}

unsigned int SearchResultsModel::GetColumnCount() const
{
  return 4;
}

wxString SearchResultsModel::GetColumnType(unsigned int col) const
{
  return (col <= 1) ? "long" : "string";
}

bool SearchResultsModel::GetAttrByRow(unsigned int row, unsigned int col,
                                      wxDataViewItemAttr& attr) const
{
  return false;
}

bool SearchResultsModel::IsEnabledByRow(unsigned int row, unsigned int col) const
{
  return row < MAX_ROWS;
}

void SearchResultsModel::GetValueByRow(wxVariant& variant, unsigned int row, unsigned int col) const
{
  variant.MakeNull();

  if (row >= MAX_ROWS)
  {
    if (col > 1)
    {
      variant = "...";
    }
    return;
  }

  switch (col)
  {
  case 0:
    if (auto address_opt = m_finder.GetAddress(row))
    {
      variant = static_cast<long>(*address_opt);
    }

    return;
  case 1:
  {
    auto type = m_finder.GetItemType(row);
    if (IsValidMemoryItemType(type))
    {
      variant = static_cast<long>(type);
    }

    return;
  }
  case 2:
  case 3:
  {
    MemoryItem item = (col == 2) ? m_finder.GetCurrentItem(row) : m_finder.GetPreviousItem(row);
    if (IsValidMemoryItem(item))
    {
      if (auto str_opt = FormatMemoryItem(item))
      {
        variant = StrToWxStr(*str_opt);
        return;
      }
    }

    break;
  }
  }

  variant = "<invalid>";
}

bool SearchResultsModel::SetValueByRow(const wxVariant& variant, unsigned int row, unsigned int col)
{
  if (col != 2)
    return false;

  if (row >= MAX_ROWS)
    return false;

  Address address;
  if (auto address_opt = m_finder.GetAddress(row))
  {
    address = *address_opt;
  }
  else
  {
    return false;
  }

  MemoryItemType type = m_finder.GetItemType(row);
  if (!IsValidMemoryItemType(type))
    return false;

  MemoryItem item = ParseMemoryItem(WxStrToStr(variant.GetString()), type);
  if (!IsValidMemoryItem(item))
    return false;

  DEBUG_LOG(ACTIONREPLAY, "%s(): 0x%X <- %s", __FUNCTION__, address,
            FormatMemoryItem(item).value_or("???").c_str());

  bool result = PokeMemoryItem(address, item, AddressTranslation::DATA);
  DEBUG_LOG(ACTIONREPLAY, "%s(): result = %s", __FUNCTION__, result ? "true" : "false");
  return result;
}

}  // namespace Cheats
