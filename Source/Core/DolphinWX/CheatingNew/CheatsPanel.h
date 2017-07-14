// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/panel.h>
#include <wx/timer.h>

class wxDataViewCtrl;

namespace Cheats
{
class CheatsTreeModel;

class CheatsPanel final : public wxPanel
{
public:
  explicit CheatsPanel(wxWindow* parent, wxWindowID id = wxID_ANY,
                       const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                       long style = wxTAB_TRAVERSAL, const wxString& name = wxPanelNameStr);
  ~CheatsPanel();

private:
  void CreateGUI();

  void OnRefresh(wxTimerEvent& event);

  wxObjectDataPtr<CheatsTreeModel> m_cheats_tree_model;

  wxDataViewCtrl* m_cheats_tree;

  wxTimer m_refresh_timer;
};

}  // namespace Cheats
