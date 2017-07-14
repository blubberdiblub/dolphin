// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/frame.h>

namespace Cheats
{
class ManagerWindow final : public wxFrame
{
public:
  explicit ManagerWindow(wxWindow* parent, wxWindowID id = wxID_ANY,
                         const wxString& title = _("Cheat Manager"),
                         const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                         long style = wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT,
                         const wxString& name = wxFrameNameStr);
  ~ManagerWindow();
  void UpdateGUI();

private:
  void CreateGUI();
};

}  // namespace Cheats
