// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/CheatingNew/CheatsPanel.h"

#include <cstdint>

#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/object.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/timer.h>
#include <wx/window.h>

#include "Common/Logging/Log.h"

#include "DolphinWX/CheatingNew/CheatsTreeModel.h"
#include "DolphinWX/CheatingNew/Utils.h"
#include "DolphinWX/WxUtils.h"

wxDEFINE_EVENT(DOLPHIN_EVT_CHEATS_ADD_ENTRY, wxCommandEvent);

namespace Cheats
{
CheatsPanel::CheatsPanel(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
                         long style, const wxString& name)
    : wxPanel(parent, id, pos, size, style, name)
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): creating", __FUNCTION__);

  CreateGUI();

  m_refresh_timer.Start(100);  // TODO: find a good compromise and make the frequency configurable
}

CheatsPanel::~CheatsPanel()
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): destructing", __FUNCTION__);

  // FIXME: Make the DataViewCtrl the container of the timer.
  m_refresh_timer.Stop();
  m_refresh_timer.SetOwner(nullptr);

  // Associating a new model decrements the reference count of the previous one,
  // which is supposed to reach a reference count of 0, causing its destruction.
  // (DataViewCtrl should be the only entity holding a reference to the model.)
  m_cheats_tree->AssociateModel(nullptr);
}

void CheatsPanel::CreateGUI()
{
  // wxSize spacing = FromDIP(wxSize(6, 6));

  m_cheats_tree_model = new CheatsTreeModel();

  m_cheats_tree = new wxDataViewCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                     wxDV_MULTIPLE | wxDV_ROW_LINES);
  m_cheats_tree->AssociateModel(m_cheats_tree_model.get());

  m_cheats_tree->AppendColumn(new wxDataViewColumn(
      _("Name"), new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_EDITABLE, wxALIGN_LEFT), 0,
      160, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));
  m_cheats_tree->AppendColumn(new wxDataViewColumn(
      _("Address"),
      new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_ACTIVATABLE, wxALIGN_RIGHT), 2, 80,
      wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));

  {
    const auto tmp_choices = GetMemoryItemTypeNames();
    wxArrayString choices;
    choices.Alloc(tmp_choices.size());
    for (auto choice : tmp_choices)
    {
      choices.Add(StrToWxStr(choice));
    }

    m_cheats_tree->AppendColumn(new wxDataViewColumn(
        _("Type"), new wxDataViewChoiceRenderer(choices, wxDATAVIEW_CELL_EDITABLE, wxALIGN_CENTER),
        3, 60, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE));
  }

  m_cheats_tree->AppendColumn(new wxDataViewColumn(
      _("L."), new wxDataViewToggleRenderer("bool", wxDATAVIEW_CELL_ACTIVATABLE, wxALIGN_CENTER), 5,
      20, wxALIGN_CENTER, 0));
  m_cheats_tree->AppendColumn(new wxDataViewColumn(
      _("Value"), new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_EDITABLE, wxALIGN_RIGHT), 4,
      150, wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE));
  m_cheats_tree->AppendColumn(new wxDataViewColumn(
      _("Description"),
      new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_EDITABLE, wxALIGN_LEFT), 1, 120,
      wxALIGN_CENTER, wxDATAVIEW_COL_RESIZABLE));

  m_cheats_tree->Bind(wxEVT_DATAVIEW_ITEM_BEGIN_DRAG, &CheatsPanel::OnBeginDrag, this);
  m_cheats_tree->Bind(wxEVT_DATAVIEW_ITEM_DROP_POSSIBLE, &CheatsPanel::OnDropPossible, this);
  m_cheats_tree->Bind(wxEVT_DATAVIEW_ITEM_DROP, &CheatsPanel::OnDrop, this);
  m_cheats_tree->EnableDragSource(wxDF_UNICODETEXT);
  m_cheats_tree->EnableDropTarget(wxDF_UNICODETEXT);

  m_cheats_tree->Bind(wxEVT_TIMER, &CheatsPanel::OnRefresh, this);
  m_refresh_timer.SetOwner(m_cheats_tree);

  auto* const delete_button =
      new wxButton(this, wxID_ANY, _("Delete"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
  delete_button->Bind(wxEVT_BUTTON, &CheatsPanel::OnDelete, this);

  auto* const control_sizer = new wxBoxSizer(wxHORIZONTAL);
  control_sizer->Add(delete_button, wxSizerFlags(0).Expand());

  auto* const cheats_sizer = new wxBoxSizer(wxVERTICAL);
  cheats_sizer->Add(m_cheats_tree, wxSizerFlags(1).Expand());
  cheats_sizer->Add(control_sizer, wxSizerFlags(0));

  SetSizer(cheats_sizer);
  SetMinClientSize(cheats_sizer->ComputeFittingClientSize(this));
}

void CheatsPanel::OnAddEntry(wxCommandEvent& event)
{
  m_cheats_tree_model->AddEntry(event.GetExtraLong(), static_cast<MemoryItemType>(event.GetInt()));
}

void CheatsPanel::OnBeginDrag(wxDataViewEvent& event)
{
  auto item = event.GetItem();

  auto* data_obj = new wxTextDataObject{};
  data_obj->SetText("foobar");
  event.SetDataObject(data_obj);
  event.SetClientData(item.GetID());

  event.SetDragFlags(wxDrag_AllowMove);
  DEBUG_LOG(ACTIONREPLAY, "%s(): item = %lu", __FUNCTION__,
            reinterpret_cast<uintptr_t>(item.GetID()));
}

void CheatsPanel::OnDropPossible(wxDataViewEvent& event)
{
  auto item = event.GetItem();
  auto key = event.GetClientData();

  DEBUG_LOG(ACTIONREPLAY, "%s(): item = %lu, effect = %d, key = %lu", __FUNCTION__,
            reinterpret_cast<uintptr_t>(item.GetID()), event.GetDropEffect(),
            reinterpret_cast<uintptr_t>(key));
}

void CheatsPanel::OnDrop(wxDataViewEvent& event)
{
  auto item = event.GetItem();
  auto key = event.GetClientData();

  DEBUG_LOG(ACTIONREPLAY, "%s(): item = %lu, effect = %d, key = %lu", __FUNCTION__,
            reinterpret_cast<uintptr_t>(item.GetID()), event.GetDropEffect(),
            reinterpret_cast<uintptr_t>(key));
}

void CheatsPanel::OnRefresh(wxTimerEvent& WXUNUSED(event))
{
  // DEBUG_LOG(ACTIONREPLAY, "%s(): refreshing search results", __FUNCTION__);
  m_cheats_tree->Refresh();
}

void CheatsPanel::OnDelete(wxCommandEvent& WXUNUSED(event))
{
  if (!m_cheats_tree->HasSelection())
    return;

  m_cheats_tree_model->DeleteEntry(m_cheats_tree->GetSelection());
}

}  // namespace Cheats
