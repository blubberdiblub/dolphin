// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/CheatingNew/SearchPanel.h"

#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/event.h>
#include <wx/font.h>
#include <wx/gauge.h>
#include <wx/gdicmn.h>
#include <wx/object.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/srchctrl.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/timer.h>
#include <wx/window.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "DolphinWX/CheatingNew/SearchResultsModel.h"
#include "DolphinWX/CheatingNew/Utils.h"
#include "DolphinWX/WxUtils.h"

wxDEFINE_EVENT(DOLPHIN_EVT_CHEATS_NEW_SEARCH_RESULTS, wxCommandEvent);
wxDEFINE_EVENT(DOLPHIN_EVT_CHEATS_UPDATE_SEARCH_PROGRESS, wxCommandEvent);

namespace Cheats
{
SearchPanel::SearchPanel(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
                         long style, const wxString& name)
    : wxPanel(parent, id, pos, size, style, name)
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): creating", __FUNCTION__);

  CreateGUI();
}

SearchPanel::~SearchPanel()
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): destructing", __FUNCTION__);

  // FIXME: Make the DataViewCtrl the container of the timer.
  m_refresh_timer.Stop();
  m_refresh_timer.SetOwner(nullptr);

  // Associating a new model decrements the reference count of the previous one,
  // which is supposed to reach a reference count of 0, causing its destruction.
  // (DataViewCtrl should be the only entity holding a reference to the model.)
  m_search_results->AssociateModel(nullptr);
}

void SearchPanel::CreateGUI()
{
  wxSize spacing = FromDIP(wxSize(6, 6));

  auto* const reset_button =
      new wxButton(this, wxID_ANY, _("Reset"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  reset_button->Bind(wxEVT_BUTTON, &SearchPanel::OnReset, this);

  m_search_results_status =
      new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                       wxALIGN_CENTER_HORIZONTAL | wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_END);

  auto* const search_results_header_sizer = new wxBoxSizer(wxHORIZONTAL);
  search_results_header_sizer->Add(reset_button, wxSizerFlags(0).Expand());
  search_results_header_sizer->AddSpacer(spacing.GetWidth());
  search_results_header_sizer->Add(m_search_results_status, wxSizerFlags(1).CenterVertical());

  wxFont monospaced_font = GetFont();
  monospaced_font.SetFamily(wxFONTFAMILY_TELETYPE);
  m_search_results = new wxDataViewCtrl(this, wxID_ANY);
  m_search_results->SetFont(monospaced_font);

  m_search_results_model = new SearchResultsModel(
      [this]() { QueueEvent(new wxCommandEvent(DOLPHIN_EVT_CHEATS_NEW_SEARCH_RESULTS)); });
  m_search_results->AssociateModel(m_search_results_model.get());
  Bind(DOLPHIN_EVT_CHEATS_NEW_SEARCH_RESULTS, &SearchPanel::OnNewResults, this);

  m_search_results->AppendColumn(new wxDataViewColumn(
      "Address", new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_ACTIVATABLE, wxALIGN_CENTER),
      0, wxDVC_DEFAULT_WIDTH, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));
  m_search_results->AppendTextColumn("Value", 1, wxDATAVIEW_CELL_EDITABLE, -1, wxALIGN_RIGHT,
                                     wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
  m_search_results->AppendTextColumn("Old Value", 2, wxDATAVIEW_CELL_ACTIVATABLE, -1, wxALIGN_RIGHT,
                                     wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
  m_search_results->Bind(wxEVT_TIMER, &SearchPanel::OnRefresh, this);

  auto* const search_view_sizer = new wxBoxSizer(wxVERTICAL);
  search_view_sizer->Add(search_results_header_sizer, wxSizerFlags(0).Expand());
  search_view_sizer->Add(m_search_results, wxSizerFlags(1).Expand());

  m_value_search_box = new wxSearchCtrl(this, wxID_ANY);
  m_value_search_box->ShowSearchButton(true);
  auto* const search_button =
      new wxButton(this, wxID_ANY, _("Search"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  search_button->SetDefault();
  search_button->Bind(wxEVT_BUTTON, &SearchPanel::OnSearch, this);

  auto* const cancel_button =
      new wxButton(this, wxID_ANY, _("Cancel"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  cancel_button->Bind(wxEVT_BUTTON, &SearchPanel::OnCancel, this);

  auto* const search_line_sizer = new wxBoxSizer(wxHORIZONTAL);
  search_line_sizer->Add(m_value_search_box, wxSizerFlags(1).Expand());
  search_line_sizer->Add(search_button, wxSizerFlags(0).Expand());
  search_line_sizer->Add(cancel_button, wxSizerFlags(0).Expand());

  m_search_progress = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 8));
  Bind(DOLPHIN_EVT_CHEATS_UPDATE_SEARCH_PROGRESS, &SearchPanel::OnSearchProgress, this);

  {
    const auto tmp_choices = GetFriendlyMemoryItemTypeNames();
    wxArrayString choices;
    choices.Alloc(tmp_choices.size());
    for (auto it = tmp_choices.cbegin() + 1; it != tmp_choices.cend(); ++it)
    {
      choices.Add(StrToWxStr(*it));
    }

    m_value_type = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
    m_value_type->SetSelection(0);
  }

  auto* const search_controls_sizer = new wxBoxSizer(wxVERTICAL);
  search_controls_sizer->Add(search_line_sizer, wxSizerFlags(0).Expand());
  search_controls_sizer->Add(m_search_progress, wxSizerFlags(0).Expand());
  search_controls_sizer->AddSpacer(spacing.GetHeight());
  search_controls_sizer->Add(m_value_type, wxSizerFlags(0));

  auto* const search_sizer = new wxBoxSizer(wxHORIZONTAL);
  search_sizer->Add(search_view_sizer, wxSizerFlags(1).Expand());
  search_sizer->AddSpacer(spacing.GetWidth());
  search_sizer->Add(search_controls_sizer, wxSizerFlags(0));

  SetSizer(search_sizer);
  SetMinClientSize(search_sizer->ComputeFittingClientSize(this));

  UpdateSearchResultsStatus();

  m_refresh_timer.SetOwner(m_search_results);
  m_refresh_timer.Start(33);  // TODO: find a good compromise and make the frequency configurable
}

void SearchPanel::UpdateSearchResultsStatus()
{
  auto count = m_search_results_model->GetCount();
  wxString label;

  if (count == 0)
  {
    label = _("no results");
  }
  else if (count > SearchResultsModel::MAX_ROWS)
  {
    label = _("too many results, excessive ones hidden");
  }
  else
  {
    label.Printf(_("%d results"), count);
  }

  m_search_results_status->SetLabel(std::move(label));
}

void SearchPanel::OnReset(wxCommandEvent& WXUNUSED(event))
{
  m_search_results_model->ClearResults();
}

void SearchPanel::OnNewResults(wxCommandEvent& WXUNUSED(event))
{
  if (m_search_results_model->NewResults())
  {
    UpdateSearchResultsStatus();
  };
}

void SearchPanel::OnRefresh(wxTimerEvent& WXUNUSED(event))
{
  // DEBUG_LOG(ACTIONREPLAY, "%s(): refreshing search results", __FUNCTION__);
  m_search_results->Refresh();
}

void SearchPanel::OnSearch(wxCommandEvent& WXUNUSED(event))
{
  MemoryItemType type = static_cast<MemoryItemType>(m_value_type->GetSelection() + 1);
  if (!IsValidMemoryItemType(type))
  {
    ERROR_LOG(ACTIONREPLAY, "%s(): unhandled item type selection", __FUNCTION__);
    return;
  }

  MemoryItem item = ParseMemoryItem(WxStrToStr(m_value_search_box->GetValue()), type);
  if (!IsValidMemoryItem(item))
  {
    DEBUG_LOG(ACTIONREPLAY, "%s(): invalid search item \"%s\"", __FUNCTION__,
              static_cast<const char*>(m_value_search_box->GetValue().ToUTF8()));
    return;
  }

  DEBUG_LOG(ACTIONREPLAY, "%s(): searching for %s", __FUNCTION__,
            FormatMemoryItem(item).value_or("???").c_str());
  auto retval = m_search_results_model->SearchItem(item, [this](int progress) {
    auto* const event = new wxCommandEvent(DOLPHIN_EVT_CHEATS_UPDATE_SEARCH_PROGRESS);
    event->SetInt(progress);
    QueueEvent(event);
  });
  DEBUG_LOG(ACTIONREPLAY, "%s(): result = %d", __FUNCTION__, (int)retval);
}

void SearchPanel::OnCancel(wxCommandEvent& WXUNUSED(event))
{
  m_search_results_model->CancelSearch();
}

void SearchPanel::OnSearchProgress(wxCommandEvent& event)
{
  m_search_progress->SetValue(event.GetInt());
}

}  // namespace Cheats
