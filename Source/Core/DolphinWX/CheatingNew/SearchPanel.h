// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/event.h>
#include <wx/gauge.h>
#include <wx/object.h>
#include <wx/panel.h>
#include <wx/srchctrl.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/window.h>

namespace Cheats
{
class SearchResultsModel;

class SearchPanel final : public wxPanel
{
public:
  explicit SearchPanel(wxWindow* parent, wxWindowID id = wxID_ANY,
                       const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                       long style = wxTAB_TRAVERSAL, const wxString& name = wxPanelNameStr);
  ~SearchPanel();

private:
  void CreateGUI();
  void UpdateSearchResultsStatus();

  void OnReset(wxCommandEvent& event);
  void OnNewResults(wxCommandEvent& event);
  void OnItemActivated(wxDataViewEvent& event);
  void OnRefresh(wxTimerEvent& event);
  void OnSearch(wxCommandEvent& event);
  void OnCancel(wxCommandEvent& event);
  void OnSearchProgress(wxCommandEvent& event);

  wxObjectDataPtr<SearchResultsModel> m_search_results_model;

  wxStaticText* m_search_results_status;
  wxDataViewCtrl* m_search_results;
  wxSearchCtrl* m_value_search_box;
  wxGauge* m_search_progress;
  wxChoice* m_value_type;

  wxTimer m_refresh_timer;
};

}  // namespace Cheats
