// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/CheatingNew/Manager.h"

#include <algorithm>

#include <wx/frame.h>
#include <wx/splitter.h>

#include "Common/Logging/Log.h"

#include "DolphinWX/CheatingNew/CheatsPanel.h"
#include "DolphinWX/CheatingNew/SearchPanel.h"

namespace Cheats
{
ManagerWindow::ManagerWindow(wxWindow* parent, wxWindowID id, const wxString& title,
                             const wxPoint& pos, const wxSize& size, long style,
                             const wxString& name)
    : wxFrame(parent, id, title, pos, size, style, name)
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): creating", __FUNCTION__);

  CreateGUI();
}

ManagerWindow::~ManagerWindow()
{
  DEBUG_LOG(ACTIONREPLAY, "%s(): destructing", __FUNCTION__);
}

void ManagerWindow::CreateGUI()
{
  auto* const splitter = new wxSplitterWindow(this, wxID_ANY);
  auto* const search_panel = new SearchPanel(splitter, wxID_ANY);
  auto* const cheats_panel = new CheatsPanel(splitter, wxID_ANY);
  Bind(DOLPHIN_EVT_CHEATS_ADD_ENTRY, &CheatsPanel::OnAddEntry, cheats_panel);

  splitter->SplitHorizontally(search_panel, cheats_panel);
  splitter->SetMinimumPaneSize(
      std::min(search_panel->GetMinHeight(), cheats_panel->GetMinHeight()));
  splitter->SetSashGravity(0.5);

  UpdateGUI();
  SetMinClientSize(splitter->GetEffectiveMinSize());
}

void ManagerWindow::UpdateGUI()
{
}

}  // namespace Cheats
