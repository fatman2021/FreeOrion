// -*- C++ -*-
#ifndef _PlayerListWnd_h_
#define _PlayerListWnd_h_

#include "CUIWnd.h"
#include "../network/Message.h"
#include <GG/ListBox.h>

class PlayerListBox;

class PlayerListWnd : public CUIWnd {
public:
    //! \name Structors //@{
    PlayerListWnd(GG::X x, GG::Y y, GG::X w, GG::Y h);
    //@}

    //! \name Accessors //@{
    std::set<int>   SelectedPlayerIDs() const;
    //@}

    //! \name Mutators //@{
    void            HandlePlayerStatusUpdate(Message::PlayerStatus player_status, int about_player_id);
    void            Update();
    void            Refresh();
    void            Clear();

    void            SetSelectedPlayers(const std::set<int>& player_ids);

    virtual void    SizeMove(const GG::Pt& ul, const GG::Pt& lr);
    //@}

    mutable boost::signals2::signal<void ()>    SelectedPlayersChangedSignal;
    mutable boost::signals2::signal<void (int)> PlayerDoubleClickedSignal;
    mutable boost::signals2::signal<void ()>    ClosingSignal;

private:
    virtual void    CloseClicked();

    void            DoLayout();

    void            PlayerSelectionChanged(const GG::ListBox::SelectionSet& rows);
    void            PlayerDoubleClicked(GG::ListBox::iterator it);
    void            PlayerRightClicked(GG::ListBox::iterator it, const GG::Pt& pt);
    int             PlayerInRow(GG::ListBox::iterator it) const;

    PlayerListBox*  m_player_list;
};

#endif
