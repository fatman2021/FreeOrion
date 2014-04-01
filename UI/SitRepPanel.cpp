#include "SitRepPanel.h"

#include "CUIControls.h"
#include "LinkText.h"
#include "Sound.h"
#include "../client/human/HumanClientApp.h"
#include "../Empire/Empire.h"
#include "../util/i18n.h"
#include "../util/Logger.h"
#include "../util/OptionsDB.h"
#include "../util/SitRepEntry.h"
#include "../universe/ShipDesign.h"

#include <GG/DrawUtil.h>

#include <boost/lexical_cast.hpp>


namespace {
    /** Adds options related to SitRepPanel to Options DB. */
    void AddOptions(OptionsDB& db)
    { db.Add("verbose-sitrep", UserStringNop("OPTIONS_DB_VERBOSE_SITREP_DESC"),  false,  Validator<bool>()); }
    bool temp_bool = RegisterOptions(&AddOptions);

    void HandleLinkClick(const std::string& link_type, const std::string& data) {
        using boost::lexical_cast;
        try {
            if (link_type == VarText::PLANET_ID_TAG) {
                ClientUI::GetClientUI()->ZoomToPlanet(lexical_cast<int>(data));
            } else if (link_type == VarText::SYSTEM_ID_TAG) {
                ClientUI::GetClientUI()->ZoomToSystem(lexical_cast<int>(data));
            } else if (link_type == VarText::FLEET_ID_TAG) {
                ClientUI::GetClientUI()->ZoomToFleet(lexical_cast<int>(data));
            } else if (link_type == VarText::SHIP_ID_TAG) {
                ClientUI::GetClientUI()->ZoomToShip(lexical_cast<int>(data));
            } else if (link_type == VarText::BUILDING_ID_TAG) {
                ClientUI::GetClientUI()->ZoomToBuilding(lexical_cast<int>(data));

            } else if (link_type == VarText::COMBAT_ID_TAG) {
                ClientUI::GetClientUI()->ZoomToCombatLog(lexical_cast<int>(data));

            } else if (link_type == VarText::EMPIRE_ID_TAG) {
                ClientUI::GetClientUI()->ZoomToEmpire(lexical_cast<int>(data));
            } else if (link_type == VarText::DESIGN_ID_TAG) {
                ClientUI::GetClientUI()->ZoomToShipDesign(lexical_cast<int>(data));
            } else if (link_type == VarText::PREDEFINED_DESIGN_TAG) {
                if (const ShipDesign* design = GetPredefinedShipDesign(data))
                    ClientUI::GetClientUI()->ZoomToShipDesign(design->ID());

            } else if (link_type == VarText::TECH_TAG) {
                ClientUI::GetClientUI()->ZoomToTech(data);
            } else if (link_type == VarText::BUILDING_TYPE_TAG) {
                ClientUI::GetClientUI()->ZoomToBuildingType(data);
            } else if (link_type == VarText::SPECIAL_TAG) {
                ClientUI::GetClientUI()->ZoomToSpecial(data);
            } else if (link_type == VarText::SHIP_HULL_TAG) {
                ClientUI::GetClientUI()->ZoomToShipHull(data);
            } else if (link_type == VarText::SHIP_PART_TAG) {
                ClientUI::GetClientUI()->ZoomToShipPart(data);
            } else if (link_type == VarText::SPECIES_TAG) {
                ClientUI::GetClientUI()->ZoomToSpecies(data);

            } else if (link_type == TextLinker::ENCYCLOPEDIA_TAG) {
                ClientUI::GetClientUI()->ZoomToEncyclopediaEntry(data);
            }
        } catch (const boost::bad_lexical_cast&) {
            Logger().errorStream() << "SitrepPanel.cpp HandleLinkClick caught lexical cast exception for link type: " << link_type << " and data: " << data;
        }
    }

    std::vector<std::string> OrderedSitrepTemplateStrings() {
        // determine sitrep order
        std::istringstream template_stream(UserString("SITREP_PRIORITY_ORDER"));
        std::vector<std::string> sitrep_order;
        std::copy(std::istream_iterator<std::string>(template_stream),
                  std::istream_iterator<std::string>(),
                  std::back_inserter<std::vector<std::string> >(sitrep_order));
        return sitrep_order;
    }

    std::vector<std::string> AllSitRepTemplateStrings() {
        std::set<std::string> template_set;
        std::vector<std::string> retval;
        Empire *empire = HumanClientApp::GetApp()->Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
        if (!empire)
            return retval;
        for (Empire::SitRepItr sitrep_it = empire->SitRepBegin();
             sitrep_it != empire->SitRepEnd(); ++sitrep_it)
        { template_set.insert(sitrep_it->GetTemplateString()); }
        std::vector<std::string> ordered_templates = OrderedSitrepTemplateStrings();
        // only use those ordered templates actually in the current set of sitrep templates
        for (std::vector<std::string>::iterator it = ordered_templates.begin(); 
             it!=ordered_templates.end(); it++) 
        {
            if ( (template_set.find(*it) != template_set.end()) && 
                 (std::find(retval.begin(), retval.end(), *it) == retval.end()) )
            { retval.push_back(*it); }
        }
        //now add the current templates that did not have a specified order
        for (std::set<std::string>::iterator it = template_set.begin(); it!= template_set.end(); it++)
            if (std::find(retval.begin(), retval.end(), *it) == retval.end())
                retval.push_back(*it);
        return retval;
    }

    //////////////////////////////////
    // SitRepDataPanel
    //////////////////////////////////
    class SitRepDataPanel : public GG::Control {
    public:
        SitRepDataPanel(GG::X w, GG::Y h, const SitRepEntry& sitrep) :
            Control(GG::X0, GG::Y0, w, h, GG::Flags<GG::WndFlag>()),
            m_initialized(false),
            m_sitrep_entry(sitrep),
            m_icon(0),
            m_link_text(0)
        {
            SetChildClippingMode(ClipToClient);
        }

        virtual void        Render() {
            if (!m_initialized)
                Init();
            GG::Clr background_clr = this->Disabled() ? ClientUI::WndColor() : ClientUI::CtrlColor();
            GG::FlatRectangle(UpperLeft(), LowerRight(), background_clr, ClientUI::WndOuterBorderColor(), 1u);
        }

        virtual void        SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
            const GG::Pt old_size = Size();
            GG::Control::SizeMove(ul, lr);
            if (old_size != Size())
                DoLayout();
        }

        const SitRepEntry&  GetSitRepEntry() const { return m_sitrep_entry; }

    private:
        void            DoLayout() {
            if (!m_initialized)
                return;
            const GG::X ICON_WIDTH(Value(ClientHeight()));

            GG::X left(GG::X0);
            GG::Y bottom(ClientHeight());

            m_icon->SizeMove(GG::Pt(left, GG::Y0), GG::Pt(left + ICON_WIDTH, bottom));
            left += ICON_WIDTH + GG::X(3);

            m_link_text->SizeMove(GG::Pt(left, GG::Y0), GG::Pt(ClientWidth(), bottom));
        }

        void            Init() {
            if (m_initialized)
                return;
            m_initialized = true;

            std::string icon_texture = (m_sitrep_entry.GetIcon().empty() ?
                "/icons/sitrep/generic.png" : m_sitrep_entry.GetIcon());
            boost::shared_ptr<GG::Texture> icon = ClientUI::GetTexture(
                ClientUI::ArtDir() / icon_texture, true);
            m_icon = new GG::StaticGraphic(GG::X0, GG::Y0, GG::X(Value(Height())), Height(), icon,
                                           GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
            AttachChild(m_icon);

            m_link_text = new LinkText(m_icon->Width(), GG::Y0, Width() - m_icon->Width(),
                                       m_sitrep_entry.GetText() + " ", ClientUI::GetFont(),
                                       GG::FORMAT_LEFT | GG::FORMAT_VCENTER, ClientUI::TextColor());
            AttachChild(m_link_text);

            GG::Connect(m_link_text->LinkClickedSignal,       &HandleLinkClick);
            GG::Connect(m_link_text->LinkDoubleClickedSignal, &HandleLinkClick);
            GG::Connect(m_link_text->LinkRightClickedSignal,  &HandleLinkClick);

            DoLayout();
        }

        bool                m_initialized;

        SitRepEntry         m_sitrep_entry;
        GG::StaticGraphic*  m_icon;
        LinkText*           m_link_text;
    };

    ////////////////////////////////////////////////
    // SitRepRow
    ////////////////////////////////////////////////
    /** A ListBox::Row subclass used to display SitReps. */
    class SitRepRow : public GG::ListBox::Row {
    public:
        SitRepRow(GG::X w, GG::Y h, const SitRepEntry& sitrep) :
            GG::ListBox::Row(w, h, ""),
            m_panel(0)
        {
            SetName("SitRepRow");
            SetChildClippingMode(ClipToClient);
            SetDragDropDataType("SitRepRow");
            m_panel = new SitRepDataPanel(w, h, sitrep);
            push_back(m_panel);
        }

        void SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
            const GG::Pt old_size = Size();
            GG::ListBox::Row::SizeMove(ul, lr);
            if (!empty() && old_size != Size() && m_panel)
                m_panel->Resize(Size());
        }

        const SitRepEntry&  GetSitRepEntry() const { return m_panel->GetSitRepEntry(); }

    private:
        SitRepDataPanel*    m_panel;
    };
}

SitRepPanel::SitRepPanel(GG::X x, GG::Y y, GG::X w, GG::Y h) :
    CUIWnd(UserString("SITREP_PANEL_TITLE"), x, y, w, h, GG::ONTOP | GG::INTERACTIVE | GG::DRAGABLE | GG::RESIZABLE | CLOSABLE),
    m_sitreps_lb(0),
    m_prev_turn_button(0),
    m_next_turn_button(0),
    m_last_turn_button(0),
    m_showing_turn(INVALID_GAME_TURN)
{
    Sound::TempUISoundDisabler sound_disabler;
    SetChildClippingMode(DontClip);

    m_sitreps_lb = new CUIListBox(GG::X0, GG::Y0, GG::X1, GG::Y1);
    m_sitreps_lb->SetStyle(GG::LIST_NOSORT | GG::LIST_NOSEL);
    AttachChild(m_sitreps_lb);

    m_prev_turn_button = new CUIButton(UserString("BACK"), GG::X0, GG::Y0, GG::X(20));
    AttachChild(m_prev_turn_button);
    m_next_turn_button = new CUIButton(UserString("NEXT"), GG::X0, GG::Y0, GG::X(20));
    AttachChild(m_next_turn_button);
    m_last_turn_button = new CUIButton(UserString("LAST"), GG::X0, GG::Y0, GG::X(20));
    AttachChild(m_last_turn_button);
    m_filter_button = new CUIButton(UserString("FILTERS"), GG::X0, GG::Y0, GG::X(30));
    AttachChild(m_filter_button);

    GG::Connect(m_prev_turn_button->LeftClickedSignal,  &SitRepPanel::PrevClicked,          this);
    GG::Connect(m_next_turn_button->LeftClickedSignal,  &SitRepPanel::NextClicked,          this);
    GG::Connect(m_last_turn_button->LeftClickedSignal,  &SitRepPanel::LastClicked,          this);
    GG::Connect(m_filter_button->LeftClickedSignal,     &SitRepPanel::FilterClicked,        this);

    DoLayout();
    Hide();
}

void SitRepPanel::DoLayout() {
    GG::X BUTTON_WIDTH(ClientUI::Pts()*4);
    GG::Y BUTTON_HEIGHT = m_last_turn_button->Height();
    int PAD(3);

    GG::Pt button_ul(ClientWidth() - GG::X(INNER_BORDER_ANGLE_OFFSET) - BUTTON_WIDTH,
                     ClientHeight() - BUTTON_HEIGHT);

    m_last_turn_button->SizeMove(button_ul, button_ul + GG::Pt(BUTTON_WIDTH, BUTTON_HEIGHT));
    button_ul -= GG::Pt(BUTTON_WIDTH + GG::X(PAD), GG::Y0);
    m_next_turn_button->SizeMove(button_ul, button_ul + GG::Pt(BUTTON_WIDTH, BUTTON_HEIGHT));
    button_ul -= GG::Pt(BUTTON_WIDTH + GG::X(PAD), GG::Y0);
    m_prev_turn_button->SizeMove(button_ul, button_ul + GG::Pt(BUTTON_WIDTH, BUTTON_HEIGHT));
    button_ul -= GG::Pt(BUTTON_WIDTH + GG::X(PAD), GG::Y0);

    m_sitreps_lb->SizeMove(GG::Pt(GG::X0, GG::Y0), GG::Pt(ClientWidth() - 1, button_ul.y));

    m_filter_button->SizeMove(GG::Pt(GG::X0, button_ul.y), GG::Pt(BUTTON_WIDTH*2, button_ul.y + BUTTON_HEIGHT));

    SetMinSize(GG::Pt(6*BUTTON_WIDTH, 6*BUTTON_HEIGHT));
}

void SitRepPanel::KeyPress(GG::Key key, boost::uint32_t key_code_point,
                            GG::Flags<GG::ModKey> mod_keys)
{
    switch (key) {
    case GG::GGK_RETURN:
    case GG::GGK_KP_ENTER:
    case GG::GGK_ESCAPE:
    case GG::GGK_F2: {
        Hide();
        break;
    }
    default:
        break;
    }
}

void SitRepPanel::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    GG::Pt old_size = GG::Wnd::Size();

    CUIWnd::SizeMove(ul, lr);

    if (old_size != GG::Wnd::Size()) {
        DoLayout();
        Update();
    }
}

void SitRepPanel::CloseClicked()
{ ClosingSignal(); }

void SitRepPanel::PrevClicked() {
    m_showing_turn = std::max(1, m_showing_turn - 1);
    Update();
}

void SitRepPanel::NextClicked() {
    m_showing_turn = std::min(CurrentTurn(), std::max(1, m_showing_turn + 1));
    Update();
}

void SitRepPanel::LastClicked() {
    m_showing_turn = CurrentTurn();
    Update();
}

void SitRepPanel::FilterClicked() {
    std::vector<std::string> all_templates = AllSitRepTemplateStrings();

    std::map<int, std::string> menu_index_templates;
    std::map<int, bool> menu_index_checked;
    int index = 1;
    bool all_checked = true;
    int ALL_INDEX = 9999;

    GG::MenuItem menu_contents;
    for (std::vector<std::string>::const_iterator it = all_templates.begin();
         it != all_templates.end(); ++it, ++index)
    {
        menu_index_templates[index] = *it;
        bool check = true;
        if (m_hidden_sitrep_templates.find(*it) != m_hidden_sitrep_templates.end()) {
            check = false;
            all_checked = false;
        }
        menu_index_checked[index] = check;
        const std::string& menu_label = UserString(*it + "_LABEL");
        menu_contents.next_level.push_back(GG::MenuItem(menu_label, index, false, check));
    }
    menu_contents.next_level.push_back(GG::MenuItem((all_checked ? UserString("NONE") : UserString("ALL")),
                                       ALL_INDEX, false, false));

    GG::PopupMenu popup(m_filter_button->UpperLeft().x, m_filter_button->LowerRight().y,
                        ClientUI::GetFont(), menu_contents, ClientUI::TextColor(),
                        ClientUI::WndOuterBorderColor(), ClientUI::WndColor(), ClientUI::EditHiliteColor());
    if (!popup.Run())
        return;
    int selected_menu_item = popup.MenuID();
    if (selected_menu_item == 0)
        return; // nothing was selected

    if (selected_menu_item == ALL_INDEX) {
        // select / deselect all templates
        if (all_checked) {
            // deselect all
            for (std::vector<std::string>::const_iterator it = all_templates.begin();
                 it != all_templates.end(); ++it, ++index)
            { m_hidden_sitrep_templates.insert(*it); }
        } else {
            // select all
            m_hidden_sitrep_templates.clear();
        }
    } else {
        // select / deselect the chosen template
        const std::string& selected_template_string = menu_index_templates[selected_menu_item];
        if (menu_index_checked[selected_menu_item]) {
            // disable showing this template string
            m_hidden_sitrep_templates.insert(selected_template_string);
        } else {
            // re-enabled showing this template string
            m_hidden_sitrep_templates.erase(selected_template_string);
        }
    }
    Update();
}

void SitRepPanel::GetTurnSitrepsFromEmpire(std::list<SitRepEntry>& append_list, int empire_id) const {
    // get id(s) of empires to get sitreps for
    std::vector<int> empire_ids;
    if (empire_id != ALL_EMPIRES) {
        if (Empires().Lookup(empire_id))
            empire_ids.push_back(empire_id);
    } else {
        for (EmpireManager::iterator it = Empires().begin(); it != Empires().end(); ++it)
            empire_ids.push_back(it->first);
    }

    for (std::vector<int>::const_iterator it = empire_ids.begin(); it != empire_ids.end(); ++it) {
        const Empire* empire = Empires().Lookup(*it);
        if (!empire)
            continue;

        // loop through sitreps and add to current list
        for (Empire::SitRepItr sitrep_it = empire->SitRepBegin();
             sitrep_it != empire->SitRepEnd(); ++sitrep_it)
        {
            if (!GetOptionsDB().Get<bool>("verbose-sitrep")) {
                if (!sitrep_it->Validate())
                    continue;
            }
            if (m_showing_turn != INVALID_GAME_TURN && m_showing_turn != sitrep_it->GetTurn())
                continue;
            if (m_hidden_sitrep_templates.find(sitrep_it->GetTemplateString()) != m_hidden_sitrep_templates.end())
                continue;
            append_list.push_back(*sitrep_it);
        }
    }
}

void SitRepPanel::Update() {
    Logger().debugStream() << "SitRepPanel::Update()";
    m_sitreps_lb->Clear();

    if (m_showing_turn == INVALID_GAME_TURN)
        this->SetName(UserString("SITREP_PANEL_TITLE"));
    else
        this->SetName(boost::io::str(FlexibleFormat(UserString("SITREP_PANEL_TITLE_TURN")) % m_showing_turn));


    // get sitrep entries for this client's player empire, or for all empires
    // if this client is an observer or moderator.
    // todo: double check that no-empire players are actually moderator or
    //       observers, instead of just passing the client empire id.
    std::list<SitRepEntry> currentTurnSitreps;
    GetTurnSitrepsFromEmpire(currentTurnSitreps, HumanClientApp::GetApp()->EmpireID());


    // order sitreps for display
    std::vector<SitRepEntry> orderedSitreps;
    std::vector<std::string> ordered_templates = OrderedSitrepTemplateStrings();
    for (std::vector<std::string>::const_iterator template_it = ordered_templates.begin();
         template_it != ordered_templates.end(); ++template_it)
    {
        for (std::list<SitRepEntry>::iterator sitrep_it = currentTurnSitreps.begin();
             sitrep_it != currentTurnSitreps.end(); sitrep_it++)
        {
            if (sitrep_it->GetTemplateString() == *template_it) {
                //Logger().debugStream() << "saving into orderedSitreps -  sitrep of template "<<*template_it<<" with full string "<< sitrep_it->GetText();
                orderedSitreps.push_back(*sitrep_it);
                //Logger().debugStream()<< "deleting above sitrep from currentTurnSitreps";
                sitrep_it = --currentTurnSitreps.erase(sitrep_it);
            }
        }
    }

    // copy remaining unordered sitreps
    for (std::list<SitRepEntry>::iterator sitrep_it = currentTurnSitreps.begin();
         sitrep_it != currentTurnSitreps.end(); sitrep_it++)
    { orderedSitreps.push_back(*sitrep_it); }

    // create UI rows for all sitrps
    GG::X width = m_sitreps_lb->Width() - 8;
    for (std::vector<SitRepEntry>::iterator sitrep_it = orderedSitreps.begin();
         sitrep_it != orderedSitreps.end(); sitrep_it++)
    { m_sitreps_lb->Insert(new SitRepRow(width, GG::Y(ClientUI::Pts()*2), *sitrep_it)); }

    // if at first turn, disable back button
    if (CurrentTurn() >= 1 && m_showing_turn > 1) {
        m_prev_turn_button->Disable(false);
    } else {
        m_prev_turn_button->Disable();
    }

    // if at current turn, disable forward button
    if (CurrentTurn() >= 1 && m_showing_turn < CurrentTurn()) {
        m_next_turn_button->Disable(false);
        m_last_turn_button->Disable(false);
    } else {
        m_next_turn_button->Disable();
        m_last_turn_button->Disable();
    }
}

void SitRepPanel::ShowSitRepsForTurn(int turn)
{ m_showing_turn = turn; }

void SitRepPanel::SetHiddenSitRepTemplates(const std::set<std::string>& templates) {
    std::set<std::string> old_hidden_sitrep_templates = m_hidden_sitrep_templates;
    m_hidden_sitrep_templates = templates;
    if (old_hidden_sitrep_templates != m_hidden_sitrep_templates)
        Update();
}

int SitRepPanel::NumVisibleSitrepsThisTurn() const {
    int count = 0;

    int client_empire_id = HumanClientApp::GetApp()->EmpireID();

    // which empires to count sitreps for?
    std::vector<int> empire_ids;
    if (HumanClientApp::GetApp()->Empires().Lookup(client_empire_id)) {
        empire_ids.push_back(client_empire_id);
    } else {
        for (EmpireManager::const_iterator it = Empires().begin(); it != Empires().end(); ++it)
            empire_ids.push_back(it->first);
    }

    for (std::vector<int>::const_iterator empire_it = empire_ids.begin();
         empire_it != empire_ids.end(); ++empire_it)
    {
        const Empire* empire = HumanClientApp::GetApp()->Empires().Lookup(*empire_it);
        if (!empire)
            continue;

        for (Empire::SitRepItr sitrep_it = empire->SitRepBegin(); sitrep_it != empire->SitRepEnd(); ++sitrep_it) {
            if (CurrentTurn() != sitrep_it->GetTurn())
                continue;
            if (m_hidden_sitrep_templates.find(sitrep_it->GetTemplateString()) != m_hidden_sitrep_templates.end())
                continue;
            ++count;
        }
    }
    return count;
}
