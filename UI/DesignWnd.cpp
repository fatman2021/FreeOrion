#include "DesignWnd.h"

#include "../util/i18n.h"
#include "../util/Logger.h"
#include "../util/Order.h"
#include "../util/OptionsDB.h"
#include "../util/ScopedTimer.h"
#include "ClientUI.h"
#include "CUIWnd.h"
#include "CUIControls.h"
#include "EncyclopediaDetailPanel.h"
#include "InfoPanels.h"
#include "Sound.h"
#include "../Empire/Empire.h"
#include "../client/human/HumanClientApp.h"
#include "../universe/UniverseObject.h"
#include "../universe/ShipDesign.h"

#include <GG/DrawUtil.h>
#include <GG/StaticGraphic.h>
#include <GG/TabWnd.h>

#include <boost/cast.hpp>
#include <boost/function.hpp>
#include <boost/timer.hpp>

#include <algorithm>


namespace {
    const std::string   PART_CONTROL_DROP_TYPE_STRING = "Part Control";
    const std::string   EMPTY_STRING = "";
    const GG::Y         BASES_LIST_BOX_ROW_HEIGHT(100);
    const GG::X         PART_CONTROL_WIDTH(64);
    const GG::Y         PART_CONTROL_HEIGHT(64);
    const GG::X         SLOT_CONTROL_WIDTH(72);
    const GG::Y         SLOT_CONTROL_HEIGHT(72);
    const int           PAD(3);

    /** Returns texture with which to render a SlotControl, depending on \a slot_type. */
    boost::shared_ptr<GG::Texture>  SlotBackgroundTexture(ShipSlotType slot_type) {
        if (slot_type == SL_EXTERNAL)
            return ClientUI::GetTexture(ClientUI::ArtDir() / "icons" / "ship_parts" / "external_slot.png", true);
        else if (slot_type == SL_INTERNAL)
            return ClientUI::GetTexture(ClientUI::ArtDir() / "icons" / "ship_parts" / "internal_slot.png", true);
        else if (slot_type == SL_CORE)
            return ClientUI::GetTexture(ClientUI::ArtDir() / "icons" / "ship_parts" / "core_slot.png", true);
        else
            return ClientUI::GetTexture(ClientUI::ArtDir() / "misc" / "missing.png", true);
    }

    /** Returns background texture with which to render a PartControl, depending on the
      * types of slot that the indicated \a part can be put into. */
    boost::shared_ptr<GG::Texture>  PartBackgroundTexture(const PartType* part) {
        if (part) {
            bool ex = part->CanMountInSlotType(SL_EXTERNAL);
            bool in = part->CanMountInSlotType(SL_INTERNAL);
            bool co = part->CanMountInSlotType(SL_CORE);

            if (ex && in)
                return ClientUI::GetTexture(ClientUI::ArtDir() / "icons" / "ship_parts" / "independent_part.png", true);
            else if (ex)
                return ClientUI::GetTexture(ClientUI::ArtDir() / "icons" / "ship_parts" / "external_part.png", true);
            else if (in)
                return ClientUI::GetTexture(ClientUI::ArtDir() / "icons" / "ship_parts" / "internal_part.png", true);
            else if (co)
                return ClientUI::GetTexture(ClientUI::ArtDir() / "icons" / "ship_parts" / "core_part.png", true);
        }
        return ClientUI::GetTexture(ClientUI::ArtDir() / "misc" / "missing.png", true);
    }

    float getMainStat(ShipPartClass spclass, PartTypeStats spstats)  {
        switch (spclass) {
            case PC_SHORT_RANGE:
            case PC_POINT_DEFENSE: {
                const DirectFireStats& stats = boost::get<DirectFireStats>(spstats);
                return stats.m_damage; //stats.m_ROF stats.m_range
                break;
            }
            case PC_MISSILES: {
                const LRStats& stats = boost::get<LRStats>(spstats);
                return stats.m_damage; //stats.m_ROF stats.m_range stats.m_speed stats.m_stealth stats.m_structure stats.m_capacity
                break;
            }
            case PC_FIGHTERS: {
                const FighterStats& stats = boost::get<FighterStats>(spstats);
                return stats.m_anti_ship_damage; //stats.m_anti_fighter_damage stats.m_launch_rate stats.m_fighter_weapon_range stats.m_speed stats.m_stealth stats.m_structure stats.m_detection stats.m_capacity
                break;
            }
            case PC_SHIELD:
            case PC_DETECTION:
            case PC_STEALTH:
            case PC_FUEL:
            case PC_COLONY:
            case PC_ARMOUR:
            case PC_BATTLE_SPEED:
            case PC_STARLANE_SPEED:
                return boost::get<float>(spstats);
                break;
            case PC_GENERAL:
            case PC_BOMBARD:
            default:
                return 0.0f;
        }
    }
    typedef std::map<std::pair<ShipPartClass,ShipSlotType>, std::vector<const PartType* > > partGroupsType;
}

//////////////////////////////////////////////////
// PartControl                                  //
//////////////////////////////////////////////////
/** UI representation of a ship part.  Displayed in the PartPalette, and can be
  * dragged onto SlotControls to add parts to the design. */
class PartControl : public GG::Control {
public:
    /** \name Structors */ //@{
    PartControl(const PartType* part);
    //@}

    /** \name Accessors */ //@{
    const PartType*     Part() const { return m_part; }
    const std::string&  PartName() const { return m_part ? m_part->Name() : EMPTY_STRING; }
    //@}

    /** \name Mutators */ //@{
    virtual void        Render();
    virtual void        LClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    virtual void        LDoubleClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);
    //@}

    mutable boost::signals2::signal<void (const PartType*)> ClickedSignal;
    mutable boost::signals2::signal<void (const PartType*)> DoubleClickedSignal;
private:
    GG::StaticGraphic*  m_icon;
    GG::StaticGraphic*  m_background;
    const PartType*     m_part;
};

PartControl::PartControl(const PartType* part) :
    GG::Control(GG::X0, GG::Y0, SLOT_CONTROL_WIDTH, SLOT_CONTROL_HEIGHT, GG::INTERACTIVE),
    m_icon(0),
    m_background(0),
    m_part(part)
{
    if (m_part) {
        m_background = new GG::StaticGraphic(GG::X0, GG::Y0, SLOT_CONTROL_WIDTH, SLOT_CONTROL_HEIGHT,
                                             PartBackgroundTexture(m_part),
                                             GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
        m_background->Show();
        AttachChild(m_background);


        // position of part image centred within part control.  control is size of a slot, but the
        // part image is smaller
        GG::X part_left = (Width() - PART_CONTROL_WIDTH) / 2;
        GG::Y part_top = (Height() - PART_CONTROL_HEIGHT) / 2;

        //Logger().debugStream() << "PartControl::PartControl this: " << this << " part: " << part << " named: " << (part ? part->Name() : "no part");
        m_icon = new GG::StaticGraphic(part_left, part_top, PART_CONTROL_WIDTH, PART_CONTROL_HEIGHT,
                                       ClientUI::PartIcon(m_part->Name()),
                                       GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
        m_icon->Show();
        AttachChild(m_icon);


        SetDragDropDataType(PART_CONTROL_DROP_TYPE_STRING);


        //Logger().debugStream() << "PartControl::PartControl part name: " << m_part->Name();
        SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));
        SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
            new IconTextBrowseWnd(ClientUI::PartIcon(m_part->Name()),
                                  UserString(m_part->Name()),
                                  UserString(m_part->Description()))
                                 )
                        );
    }
}

void PartControl::Render() {}

void PartControl::LClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys) {
    ClickedSignal(m_part);
}

void PartControl::LDoubleClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys)
{ DoubleClickedSignal(m_part); }


//////////////////////////////////////////////////
// PartsListBox                                 //
//////////////////////////////////////////////////
/** Arrangement of PartControls that can be dragged onto SlotControls */
class PartsListBox : public CUIListBox {
public:
    class PartsListBoxRow : public CUIListBox::Row {
    public:
        PartsListBoxRow(GG::X w, GG::Y h);
        virtual void    ChildrenDraggedAway(const std::vector<GG::Wnd*>& wnds, const GG::Wnd* destination);
    };

    /** \name Structors */ //@{
    PartsListBox(GG::X x, GG::Y y, GG::X w, GG::Y h);
    //@}

    /** \name Accessors */ //@{
    const std::set<ShipPartClass>&  GetClassesShown() const;
    const std::set<ShipSlotType>&   GetSlotTypesShown() const;
    const std::pair<bool, bool>&    GetAvailabilitiesShown() const; // .first -> available items; .second -> unavailable items
    //@}

    /** \name Mutators */ //@{
    virtual void    SizeMove(const GG::Pt& ul, const GG::Pt& lr);

    partGroupsType  GroupAvailableDisplayableParts(const Empire* empire);
    void            CullSuperfluousParts(std::vector<const PartType* >& thisGroup, ShipPartClass pclass, int empire_id, int loc_id);
    void            Populate();

    void            ShowClass(ShipPartClass part_class, bool refresh_list = true);
    void            ShowAllClasses(bool refresh_list = true);
    void            HideClass(ShipPartClass part_class, bool refresh_list = true);
    void            HideAllClasses(bool refresh_list = true);

    void            ShowSlotType(ShipSlotType slot_type, bool refresh_list = true);
    void            HideSlotType(ShipSlotType slot_type, bool refresh_list = true);

    void            ShowAvailability(bool available, bool refresh_list = true);
    void            HideAvailability(bool available, bool refresh_list = true);
    //@}

    mutable boost::signals2::signal<void (const PartType*)> PartTypeClickedSignal;
    mutable boost::signals2::signal<void (const PartType*)> PartTypeDoubleClickedSignal;

private:
    std::set<ShipPartClass> m_part_classes_shown;   // which part classes should be shown
    std::set<ShipSlotType>  m_slot_types_shown;     // which slot types of parts to be shown.  parts must be mountable on at least one of these slot types to be shown
    std::pair<bool, bool>   m_availabilities_shown; // first indicates whether available parts should be shown.  second indicates whether unavailable parts should be shown

    int                     m_previous_num_columns;
};

PartsListBox::PartsListBoxRow::PartsListBoxRow(GG::X w, GG::Y h) :
    CUIListBox::Row(w, h, "")    // drag_drop_data_type = "" implies not draggable row
{}

void PartsListBox::PartsListBoxRow::ChildrenDraggedAway(const std::vector<GG::Wnd*>& wnds, const GG::Wnd* destination) {
    if (wnds.empty())
        return;
    const GG::Wnd* wnd = wnds.front();  // should only be one wnd in list due because PartControls can only be dragged one-at-a-time
    const GG::Control* control = dynamic_cast<const GG::Control*>(wnd);
    if (!control)
        return;

    GG::Control* dragged_control = 0;

    // find control in row
    unsigned int i = -1;
    for (i = 0; i < size(); ++i) {
        dragged_control = (*this)[i];
        if (dragged_control == control)
            break;
        else
            dragged_control = 0;
    }

    if (!dragged_control)
        return;

    PartControl* part_control = dynamic_cast<PartControl*>(dragged_control);
    const PartType* part_type = 0;
    if (part_control)
        part_type = part_control->Part();

    RemoveCell(i);  // Wnd that accepts drop takes ownership of dragged-away control

    if (part_type) {
        part_control = new PartControl(part_type);
        const PartsListBox* parent = dynamic_cast<const PartsListBox*>(Parent());
        if (parent) {
            GG::Connect(part_control->ClickedSignal,        parent->PartTypeClickedSignal);
            GG::Connect(part_control->DoubleClickedSignal,  parent->PartTypeDoubleClickedSignal);
        }
        SetCell(i, part_control);
    }
}

PartsListBox::PartsListBox(GG::X x, GG::Y y, GG::X w, GG::Y h) :
    CUIListBox(x, y, w, h),
    m_part_classes_shown(),
    m_slot_types_shown(),
    m_availabilities_shown(std::make_pair(false, false)),
    m_previous_num_columns(-1)
{ SetStyle(GG::LIST_NOSEL); }

const std::set<ShipPartClass>& PartsListBox::GetClassesShown() const
{ return m_part_classes_shown; }

const std::set<ShipSlotType>& PartsListBox::GetSlotTypesShown() const {
    return m_slot_types_shown;
}

const std::pair<bool, bool>& PartsListBox::GetAvailabilitiesShown() const
{ return m_availabilities_shown; }

void PartsListBox::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    GG::Pt old_size = GG::Wnd::Size();

    // maybe later do something interesting with docking
    CUIListBox::SizeMove(ul, lr);

    if (Visible() && old_size != GG::Wnd::Size()) {
        // determine how many columns can fit in the box now...
        const GG::X TOTAL_WIDTH = Size().x - ClientUI::ScrollWidth();
        const int NUM_COLUMNS = std::max(1, Value(TOTAL_WIDTH / (SLOT_CONTROL_WIDTH + GG::X(PAD))));

        if (NUM_COLUMNS != m_previous_num_columns)
            Populate();
    }
}

partGroupsType PartsListBox::GroupAvailableDisplayableParts(const Empire* empire) {
    const PartTypeManager& manager = GetPartTypeManager();
    partGroupsType partGroups;
    // loop through all possible parts
    for (PartTypeManager::iterator part_it = manager.begin(); part_it != manager.end(); ++part_it) {
        const PartType* part = part_it->second;
        if (!part->Producible())
            continue;

        // check whether this part should be shown in list
        ShipPartClass part_class = part->Class();
        if (m_part_classes_shown.find(part_class) == m_part_classes_shown.end())
            continue;   // part of this class is not requested to be shown

        bool part_available = empire ? empire->ShipPartAvailable(part->Name()) : true;
        if (!(part_available && m_availabilities_shown.first) && !(!part_available && m_availabilities_shown.second))
            continue;   // part is available but available parts shouldn't be shown, or part isn't available and not available parts shouldn't be shown
            
        for (std::set<ShipSlotType>::const_iterator it = m_slot_types_shown.begin(); it != m_slot_types_shown.end(); ++it) {
            if (part->CanMountInSlotType(*it)) {
                partGroups[ std::make_pair<ShipPartClass,ShipSlotType>(part_class, *it) ].push_back( part );
            }
        }
    }
    return partGroups;
}

void PartsListBox::CullSuperfluousParts(std::vector<const PartType* >& thisGroup,
                                        ShipPartClass pclass, int empire_id, int loc_id)
{
    /// This is not merely a check for obsolescence; see PartsListBox::Populate for more info
    for (std::vector<const PartType* >::iterator part_it = thisGroup.begin(); part_it != thisGroup.end(); ++part_it) {
        const PartType* checkPart = *part_it;
        for (std::vector<const PartType* >::iterator check_it = thisGroup.begin(); check_it != thisGroup.end(); ++check_it ) {
            const PartType* refPart = *check_it;
            if ( (getMainStat(pclass, checkPart->Stats()) < getMainStat(pclass, refPart->Stats()) ) &&
                ( checkPart->ProductionCost(empire_id, loc_id) >= refPart->ProductionCost(empire_id, loc_id) ) &&
                ( checkPart->ProductionTime(empire_id, loc_id) >= refPart->ProductionTime(empire_id, loc_id) ) ) {
                thisGroup.erase(part_it--);
                break;
            }
        }
    }
}

void PartsListBox::Populate() {
    ScopedTimer scoped_timer("PartsListBox::Populate");

    const GG::X TOTAL_WIDTH = ClientWidth() - ClientUI::ScrollWidth();
    const int NUM_COLUMNS = std::max(1, Value(TOTAL_WIDTH / (SLOT_CONTROL_WIDTH + GG::X(PAD))));

    int empire_id = HumanClientApp::GetApp()->EmpireID();
    const Empire* empire = Empires().Lookup(empire_id);  // may be 0

    int cur_col = NUM_COLUMNS;
    PartsListBoxRow* cur_row = 0;

    // remove parts currently in rows of listbox
    Clear();
    
    /** 
     * The Parts are first filtered for availability to this empire and according to the current 
     * selections of which part classes are to be displayed.  Then, in order to eliminate presentation
     * of clearly suboptimal parts, such as Mass Driver I when Mass Driver II is available at the same
     * cost & build time, some orgnization, paring and sorting of parts is done. The previously 
     * filtered parts are grouped according to (class, slot).  Within each group, parts are compared 
     * and pared for display; only parts within the same group may suppress display of each other.
     * The paring is (currently) done on the basis of main stat, construction cost, and construction 
     * time. If two parts have the same class and slot, and one has a lower main stat but also a lower
     * cost, they will both be presented; if one has a higher main stat and is at least as good on cost
     * and time, it will suppress the other.  
     * 
     * An example of one of the more subtle possible results is that if a part class had multiple parts
     * with different but overlapping MountableSlotType patterns, then a part with two possible slot
     * types might be rendered superfluous for the first slot type by a first other part, be rendered
     * superfluous for the second slot type by a second other part, even if neither of the latter two
     * parts would be considered to individually render the former part obsolete.
     */    

    /// filter parts by availability and current designation of classes for display; group according to (class, slot)
    partGroupsType partGroups = GroupAvailableDisplayableParts(empire);
    
    // get empire id and location to use for cost and time comparisons
    int loc_id = INVALID_OBJECT_ID;
    if (empire) {
        TemporaryPtr<const UniverseObject> location = GetUniverseObject(empire->CapitalID());
        loc_id = location ? location->ID() : INVALID_OBJECT_ID;
    }

    // if the empire id is not ALL_EMPIRES, and unavailable parts are not to be displayed, cull Parts for display
    if ( empire_id != ALL_EMPIRES && !m_availabilities_shown.second  ) {
        for (partGroupsType::iterator group_it=partGroups.begin(); group_it != partGroups.end(); group_it++) {
            ShipPartClass pclass = group_it->first.first;
            // currently, only cull ShortRange Weapons, though the culling code is more broadly applicable. 
            if ( pclass == PC_SHORT_RANGE )
                CullSuperfluousParts(group_it->second, pclass, empire_id, loc_id);
        }
    }
    
    // now sort the parts within each group according to main stat, via weak sorting in a multimap
    // also, if a part was in multiple groups due to being compatible with multiple slot types, ensure it is only displayed once
    std::set<const PartType* > alreadyAdded;
    for (partGroupsType::iterator group_it=partGroups.begin(); group_it != partGroups.end(); group_it++) {
        std::vector<const PartType* > thisGroup = group_it->second;
        ShipPartClass pclass = group_it->first.first;
        std::multimap<double, const PartType*> sortedGroup;
        for (std::vector<const PartType* >::iterator part_it = thisGroup.begin(); part_it != thisGroup.end(); ++part_it) {
            const PartType* part = *part_it;
            if (alreadyAdded.find(part) != alreadyAdded.end())
                continue;
            alreadyAdded.insert(part);
            sortedGroup.insert(std::make_pair<double, const PartType*>(getMainStat(pclass, part->Stats()), part));
        }

        // take the sorted parts and make UI elements (technically rows) for the PartsListBox
        for (std::multimap<double, const PartType*>::iterator sorted_it = sortedGroup.begin(); sorted_it != sortedGroup.end(); ++sorted_it) {
            const PartType* part = sorted_it->second;
            // check if current row is full, and make a new row if necessary
            if (cur_col >= NUM_COLUMNS) {
                if (cur_row)
                    Insert(cur_row);
                cur_col = 0;
                cur_row = new PartsListBoxRow(TOTAL_WIDTH, SLOT_CONTROL_HEIGHT + GG::Y(PAD));
            }
            ++cur_col;

            // make new part control and add to row
            PartControl* control = new PartControl(part);
            GG::Connect(control->ClickedSignal,         PartsListBox::PartTypeClickedSignal);
            GG::Connect(control->DoubleClickedSignal,   PartsListBox::PartTypeDoubleClickedSignal);
            cur_row->push_back(control);
        }
    }
    // add any incomplete rows
    if (cur_row)
        Insert(cur_row);

    // keep track of how many columns are present now
    m_previous_num_columns = NUM_COLUMNS;
}

void PartsListBox::ShowClass(ShipPartClass part_class, bool refresh_list) {
    if (m_part_classes_shown.find(part_class) == m_part_classes_shown.end()) {
        m_part_classes_shown.insert(part_class);
        if (refresh_list)
            Populate();
    }
}

void PartsListBox::ShowAllClasses(bool refresh_list) {
    for (ShipPartClass part_class = ShipPartClass(0); part_class != NUM_SHIP_PART_CLASSES; part_class = ShipPartClass(part_class + 1))
        m_part_classes_shown.insert(part_class);
    if (refresh_list)
        Populate();
}

void PartsListBox::HideClass(ShipPartClass part_class, bool refresh_list) {
    std::set<ShipPartClass>::iterator it = m_part_classes_shown.find(part_class);
    if (it != m_part_classes_shown.end()) {
        m_part_classes_shown.erase(it);
        if (refresh_list)
            Populate();
    }
}

void PartsListBox::HideAllClasses(bool refresh_list) {
    m_part_classes_shown.clear();
    if (refresh_list)
        Populate();
}

void PartsListBox::ShowSlotType(ShipSlotType slot_type, bool refresh_list) {
    if (m_slot_types_shown.find(slot_type) == m_slot_types_shown.end()) {
        m_slot_types_shown.insert(slot_type);
        if (refresh_list)
            Populate();
    }
}

void PartsListBox::HideSlotType(ShipSlotType slot_type, bool refresh_list) {
    std::set<ShipSlotType>::iterator it = m_slot_types_shown.find(slot_type);
    if (it != m_slot_types_shown.end()) {
        m_slot_types_shown.erase(it);
        if (refresh_list)
            Populate();
    }
}

void PartsListBox::ShowAvailability(bool available, bool refresh_list) {
    if (available) {
        if (!m_availabilities_shown.first) {
            m_availabilities_shown.first = true;
            if (refresh_list)
                Populate();
        }
    } else {
        if (!m_availabilities_shown.second) {
            m_availabilities_shown.second = true;
            if (refresh_list)
                Populate();
        }
    }
}

void PartsListBox::HideAvailability(bool available, bool refresh_list) {
    if (available) {
        if (m_availabilities_shown.first) {
            m_availabilities_shown.first = false;
            if (refresh_list)
                Populate();
        }
    } else {
        if (m_availabilities_shown.second) {
            m_availabilities_shown.second = false;
            if (refresh_list)
                Populate();
        }
    }
}


//////////////////////////////////////////////////
// DesignWnd::PartPalette                       //
//////////////////////////////////////////////////
/** Contains graphical list of PartControl which can be dragged and dropped
  * onto slots to assign parts to those slots */
class DesignWnd::PartPalette : public CUIWnd {
public:
    /** \name Structors */ //@{
    PartPalette(GG::X w, GG::Y h);
    //@}

    /** \name Mutators */ //@{
    virtual void    SizeMove(const GG::Pt& ul, const GG::Pt& lr);

    void            ShowClass(ShipPartClass part_class, bool refresh_list = true);
    void            ShowAllClasses(bool refresh_list = true);
    void            HideClass(ShipPartClass part_class, bool refresh_list = true);
    void            HideAllClasses(bool refresh_list = true);
    void            ToggleClass(ShipPartClass part_class, bool refresh_list = true);
    void            ToggleAllClasses(bool refresh_list = true);

    void            ShowSlotType(ShipSlotType slot_type, bool refresh_list = true);
    void            HideSlotType(ShipSlotType slot_type, bool refresh_list = true);
    void            ToggleSlotType(ShipSlotType slot_type, bool refresh_list = true);

    void            ShowAvailability(bool available, bool refresh_list = true);
    void            HideAvailability(bool available, bool refresh_list = true);
    void            ToggleAvailability(bool available, bool refresh_list = true);

    void            Reset();
    //@}

    mutable boost::signals2::signal<void (const PartType*)> PartTypeClickedSignal;
    mutable boost::signals2::signal<void (const PartType*)> PartTypeDoubleClickedSignal;

private:
    void            DoLayout();

    PartsListBox*   m_parts_list;

    std::map<ShipPartClass, CUIButton*> m_class_buttons;
    std::map<ShipSlotType, CUIButton*>  m_slot_type_buttons;
    std::pair<CUIButton*, CUIButton*>   m_availability_buttons;
};

DesignWnd::PartPalette::PartPalette(GG::X w, GG::Y h) :
    CUIWnd(UserString("DESIGN_WND_PART_PALETTE_TITLE"), GG::X0, GG::Y0, w, h, GG::ONTOP | GG::INTERACTIVE | GG::DRAGABLE | GG::RESIZABLE),
    m_parts_list(0)
{
    //TempUISoundDisabler sound_disabler;     // should be redundant with disabler in DesignWnd::DesignWnd.  uncomment if this is not the case
    SetChildClippingMode(ClipToClient);

    m_parts_list = new PartsListBox(GG::X0, GG::Y0, GG::X(10), GG::Y(10));
    AttachChild(m_parts_list);
    GG::Connect(m_parts_list->PartTypeClickedSignal,        PartTypeClickedSignal);
    GG::Connect(m_parts_list->PartTypeDoubleClickedSignal,  PartTypeDoubleClickedSignal);

    const PartTypeManager& part_manager = GetPartTypeManager();

    // class buttons
    for (ShipPartClass part_class = ShipPartClass(0); part_class != NUM_SHIP_PART_CLASSES; part_class = ShipPartClass(part_class + 1)) {
        // are there any parts of this class?
        bool part_of_this_class_exists = false;
        for (PartTypeManager::iterator part_it = part_manager.begin(); part_it != part_manager.end(); ++part_it) {
            if (const PartType* part = part_it->second) {
                if (part->Class() == part_class) {
                    part_of_this_class_exists = true;
                    break;
                }
            }
        }
        if (!part_of_this_class_exists)
            continue;

        m_class_buttons[part_class] = (new CUIButton(UserString(boost::lexical_cast<std::string>(part_class)), GG::X(10), GG::Y(10), GG::X(10)));
        AttachChild(m_class_buttons[part_class]);
        GG::Connect(m_class_buttons[part_class]->LeftClickedSignal,
                    boost::bind(&DesignWnd::PartPalette::ToggleClass, this, part_class, true));
    }

    //// slot type buttons
    //for (ShipSlotType slot_type = ShipSlotType(0); slot_type != NUM_SHIP_SLOT_TYPES; slot_type = ShipSlotType(slot_type + 1)) {
    //    m_slot_type_buttons[slot_type] = (new CUIButton(UserString(boost::lexical_cast<std::string>(slot_type)), GG::X(10), GG::Y(10), GG::X(10)));
    //    AttachChild(m_slot_type_buttons[slot_type]);
    //    GG::Connect(m_slot_type_buttons[slot_type]->ClickedSignal,
    //                boost::bind(&DesignWnd::PartPalette::ToggleSlotType, this, slot_type, true));
    //}

    //// availability buttons
    //CUIButton* button = new CUIButton(UserString("PRODUCTION_WND_AVAILABILITY_AVAILABLE"), GG::X(10), GG::Y(10), GG::X(10));
    //m_availability_buttons.first = button;
    //AttachChild(button);
    //GG::Connect(button->ClickedSignal,
    //            boost::bind(&DesignWnd::PartPalette::ToggleAvailability, this, true, true));
    //button = new CUIButton(UserString("PRODUCTION_WND_AVAILABILITY_UNAVAILABLE"), GG::X(10), GG::Y(10), GG::X(10));
    //m_availability_buttons.second = button;
    //AttachChild(button);
    //GG::Connect(button->ClickedSignal, 
    //            boost::bind(&DesignWnd::PartPalette::ToggleAvailability, this, false, true));

    // default to showing nothing
    ShowAllClasses(false);
    ShowAvailability(true, false);
    for (ShipSlotType slot_type = ShipSlotType(0); slot_type != NUM_SHIP_SLOT_TYPES; slot_type = ShipSlotType(slot_type + 1)) {
        m_parts_list->ShowSlotType(slot_type, false);
        //m_slot_type_buttons[slot_type]->MarkSelectedGray();
    }
    DoLayout();
}

void DesignWnd::PartPalette::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    CUIWnd::SizeMove(ul, lr);
    DoLayout();
}

void DesignWnd::PartPalette::DoLayout() {
    const int PTS = ClientUI::Pts();
    const GG::X PTS_WIDE(PTS/2);         // guess at how wide per character the font needs
    const GG::Y  BUTTON_HEIGHT(PTS*3/2);
    const int BUTTON_SEPARATION = 3;    // vertical or horizontal sepration between adjacent buttons
    const int BUTTON_EDGE_PAD = 2;      // distance from edges of control to buttons
    const GG::X RIGHT_EDGE_PAD(8);       // to account for border of CUIWnd

    const GG::X USABLE_WIDTH = std::max(ClientWidth() - RIGHT_EDGE_PAD, GG::X1);   // space in which to fit buttons
    const int GUESSTIMATE_NUM_CHARS_IN_BUTTON_LABEL = 14;                   // rough guesstimate... avoid overly long part class names
    const GG::X MIN_BUTTON_WIDTH = PTS_WIDE*GUESSTIMATE_NUM_CHARS_IN_BUTTON_LABEL;
    const int MAX_BUTTONS_PER_ROW = std::max(Value(USABLE_WIDTH / (MIN_BUTTON_WIDTH + BUTTON_SEPARATION)), 1);

    const int NUM_CLASS_BUTTONS = std::max(1, static_cast<int>(m_class_buttons.size()));
    const int NUM_SLOT_TYPE_BUTTONS = 0;//std::max(1, static_cast<int>(m_slot_type_buttons.size()));
    const int NUM_AVAILABILITY_BUTTONS = 0;//2;
    const int NUM_NON_CLASS_BUTTONS = NUM_SLOT_TYPE_BUTTONS + NUM_AVAILABILITY_BUTTONS;

    // determine whether to put non-class buttons (availability and slot type) in one column or two.
    // -> if class buttons fill up fewer rows than (the non-class buttons in one column), split the
    //    non-class buttons into two columns
    int num_non_class_buttons_per_row = 0;//1;
    if (NUM_CLASS_BUTTONS < NUM_NON_CLASS_BUTTONS*(MAX_BUTTONS_PER_ROW - num_non_class_buttons_per_row))
        num_non_class_buttons_per_row = 2;

    const int MAX_CLASS_BUTTONS_PER_ROW = std::max(1, MAX_BUTTONS_PER_ROW - num_non_class_buttons_per_row);

    const int NUM_CLASS_BUTTON_ROWS = static_cast<int>(std::ceil(static_cast<float>(NUM_CLASS_BUTTONS) / MAX_CLASS_BUTTONS_PER_ROW));
    const int NUM_CLASS_BUTTONS_PER_ROW = static_cast<int>(std::ceil(static_cast<float>(NUM_CLASS_BUTTONS) / NUM_CLASS_BUTTON_ROWS));

    const int TOTAL_BUTTONS_PER_ROW = NUM_CLASS_BUTTONS_PER_ROW + num_non_class_buttons_per_row;

    const GG::X BUTTON_WIDTH = (USABLE_WIDTH - (TOTAL_BUTTONS_PER_ROW - 1)*BUTTON_SEPARATION) / TOTAL_BUTTONS_PER_ROW;

    const GG::X COL_OFFSET = BUTTON_WIDTH + BUTTON_SEPARATION;    // horizontal distance between each column of buttons
    const GG::Y ROW_OFFSET = BUTTON_HEIGHT + BUTTON_SEPARATION;   // vertical distance between each row of buttons

    // place class buttons
    int col = NUM_CLASS_BUTTONS_PER_ROW, row = -1;
    for (std::map<ShipPartClass, CUIButton*>::iterator it = m_class_buttons.begin(); it != m_class_buttons.end(); ++it) {
        if (col >= NUM_CLASS_BUTTONS_PER_ROW) {
            col = 0;
            ++row;
        }
        GG::Pt ul(BUTTON_EDGE_PAD + col*COL_OFFSET, BUTTON_EDGE_PAD + row*ROW_OFFSET);
        GG::Pt lr = ul + GG::Pt(BUTTON_WIDTH, BUTTON_HEIGHT);
        it->second->SizeMove(ul, lr);
        ++col;
    }

    // place parts list.  note: assuming at least as many rows of class buttons as availability buttons, as should
    //                          be the case given how num_non_class_buttons_per_row is determined
    m_parts_list->SizeMove(GG::Pt(GG::X0, BUTTON_EDGE_PAD + ROW_OFFSET*(row + 1)), ClientSize() - GG::Pt(GG::X(BUTTON_SEPARATION), GG::Y(BUTTON_SEPARATION)));

    //// place slot type buttons
    //col = NUM_CLASS_BUTTONS_PER_ROW;
    //row = 0;
    //for (std::map<ShipSlotType, CUIButton*>::iterator it = m_slot_type_buttons.begin(); it != m_slot_type_buttons.end(); ++it) {
    //    GG::Pt ul(BUTTON_EDGE_PAD + col*COL_OFFSET, BUTTON_EDGE_PAD + row*ROW_OFFSET);
    //    GG::Pt lr = ul + GG::Pt(BUTTON_WIDTH, BUTTON_HEIGHT);
    //    it->second->SizeMove(ul, lr);
    //    ++row;
    //}

    //// place availability buttons
    //if (num_non_class_buttons_per_row > 1) {
    //    ++col;
    //    row = 0;
    //}
    //GG::Pt ul(BUTTON_EDGE_PAD + col*COL_OFFSET, BUTTON_EDGE_PAD + row*ROW_OFFSET);
    //GG::Pt lr = ul + GG::Pt(BUTTON_WIDTH, BUTTON_HEIGHT);
    //m_availability_buttons.first->SizeMove(ul, lr);
    //++row;
    //ul = GG::Pt(BUTTON_EDGE_PAD + col*COL_OFFSET, BUTTON_EDGE_PAD + row*ROW_OFFSET);
    //lr = ul + GG::Pt(BUTTON_WIDTH, BUTTON_HEIGHT);
    //m_availability_buttons.second->SizeMove(ul, lr);
}

void DesignWnd::PartPalette::ShowClass(ShipPartClass part_class, bool refresh_list) {
    if (part_class >= ShipPartClass(0) && part_class < NUM_SHIP_PART_CLASSES) {
        m_parts_list->ShowClass(part_class, refresh_list);
        m_class_buttons[part_class]->MarkSelectedGray();
    } else {
        throw std::invalid_argument("PartPalette::ShowClass was passed an invalid ShipPartClass");
    }
}

void DesignWnd::PartPalette::ShowAllClasses(bool refresh_list) {
    m_parts_list->ShowAllClasses(refresh_list);
    for (std::map<ShipPartClass, CUIButton*>::iterator it = m_class_buttons.begin(); it != m_class_buttons.end(); ++it)
        it->second->MarkSelectedGray();
}

void DesignWnd::PartPalette::HideClass(ShipPartClass part_class, bool refresh_list) {
    if (part_class >= ShipPartClass(0) && part_class < NUM_SHIP_PART_CLASSES) {
        m_parts_list->HideClass(part_class, refresh_list);
        m_class_buttons[part_class]->MarkNotSelected();
    } else {
        throw std::invalid_argument("PartPalette::HideClass was passed an invalid ShipPartClass");
    }
}

void DesignWnd::PartPalette::HideAllClasses(bool refresh_list) {
    m_parts_list->HideAllClasses(refresh_list);
    for (std::map<ShipPartClass, CUIButton*>::iterator it = m_class_buttons.begin(); it != m_class_buttons.end(); ++it)
        it->second->MarkNotSelected();
}

void DesignWnd::PartPalette::ToggleClass(ShipPartClass part_class, bool refresh_list) {
    if (part_class >= ShipPartClass(0) && part_class < NUM_SHIP_PART_CLASSES) {
        const std::set<ShipPartClass>& classes_shown = m_parts_list->GetClassesShown();
        if (classes_shown.find(part_class) == classes_shown.end())
            ShowClass(part_class, refresh_list);
        else
            HideClass(part_class, refresh_list);
    } else {
        throw std::invalid_argument("PartPalette::ToggleClass was passed an invalid ShipPartClass");
    } 
}

void DesignWnd::PartPalette::ToggleAllClasses(bool refresh_list)
{
    const std::set<ShipPartClass>& classes_shown = m_parts_list->GetClassesShown();
    if (classes_shown.size() == NUM_SHIP_PART_CLASSES)
        HideAllClasses(refresh_list);
    else
        ShowAllClasses(refresh_list);
}

void DesignWnd::PartPalette::ShowSlotType(ShipSlotType slot_type, bool refresh_list) {
    if (slot_type >= ShipSlotType(0) && slot_type < NUM_SHIP_SLOT_TYPES) {
        m_parts_list->ShowSlotType(slot_type, refresh_list);
        //m_slot_type_buttons[slot_type]->MarkSelectedGray();
    } else {
        throw std::invalid_argument("PartPalette::ShowSlotType was passed an invalid ShipSlotType");
    }
}

void DesignWnd::PartPalette::HideSlotType(ShipSlotType slot_type, bool refresh_list) {
    if (slot_type >= ShipSlotType(0) && slot_type < NUM_SHIP_SLOT_TYPES) {
        m_parts_list->HideSlotType(slot_type, refresh_list);
        //m_slot_type_buttons[slot_type]->MarkNotSelected();
    } else {
        throw std::invalid_argument("PartPalette::HideSlotType was passed an invalid ShipSlotType");
    }
}

void DesignWnd::PartPalette::ToggleSlotType(ShipSlotType slot_type, bool refresh_list) {
    if (slot_type >= ShipSlotType(0) && slot_type < NUM_SHIP_SLOT_TYPES) {
        const std::set<ShipSlotType>& slot_types_shown = m_parts_list->GetSlotTypesShown();
        if (slot_types_shown.find(slot_type) == slot_types_shown.end())
            ShowSlotType(slot_type, refresh_list);
        else
            HideSlotType(slot_type, refresh_list);
    } else {
        throw std::invalid_argument("PartPalette::ToggleSlotType was passed an invalid ShipSlotType");
    }
}

void DesignWnd::PartPalette::ShowAvailability(bool available, bool refresh_list) {
    m_parts_list->ShowAvailability(available, refresh_list);
    //if (available)
    //    m_availability_buttons.first->MarkSelectedGray();
    //else
    //    m_availability_buttons.second->MarkSelectedGray();
}

void DesignWnd::PartPalette::HideAvailability(bool available, bool refresh_list) {
    m_parts_list->HideAvailability(available, refresh_list);
    //if (available)
    //    m_availability_buttons.first->MarkNotSelected();
    //else
    //    m_availability_buttons.second->MarkNotSelected();
}

void DesignWnd::PartPalette::ToggleAvailability(bool available, bool refresh_list) {
    const std::pair<bool, bool>& avail_shown = m_parts_list->GetAvailabilitiesShown();
    if (available) {
        if (avail_shown.first)
            HideAvailability(true, refresh_list);
        else
            ShowAvailability(true, refresh_list);
    } else {
        if (avail_shown.second)
            HideAvailability(false, refresh_list);
        else
            ShowAvailability(false, refresh_list);
    }
}

void DesignWnd::PartPalette::Reset()
{ m_parts_list->Populate(); }


//////////////////////////////////////////////////
// BasesListBox                                  //
//////////////////////////////////////////////////
/** List of starting points for designs, such as empty hulls, existing designs
  * kept by this empire or seen elsewhere in the universe, design template
  * scripts or saved (on disk) designs from previous games. */
class BasesListBox : public CUIListBox {
public:
    /** \name Structors */ //@{
    BasesListBox(GG::X x, GG::Y y, GG::X w, GG::Y h);
    //@}

    /** \name Accessors */ //@{
    const std::pair<bool, bool>&    GetAvailabilitiesShown() const;
    //@}

    /** \name Mutators */ //@{
    virtual void                    SizeMove(const GG::Pt& ul, const GG::Pt& lr);

    void                            SetEmpireShown(int empire_id, bool refresh_list = true);

    virtual void                    Populate();

    void                            ShowEmptyHulls(bool refresh_list = true);
    void                            ShowCompletedDesigns(bool refresh_list = true);

    void                            ShowAvailability(bool available, bool refresh_list = true);
    void                            HideAvailability(bool available, bool refresh_list = true);
    //@}

    /** an existing complete design that is known to this empire
        was selected (double-clicked) */
    mutable boost::signals2::signal<void (int)> DesignSelectedSignal;
    /** a hull and a set of parts (which may be empty) was selected
        (double-clicked) */
    mutable boost::signals2::signal<void (const std::string&, const std::vector<std::string>&)>
                                    DesignComponentsSelectedSignal;
    /** a completed design was browsed (clicked once) */
    mutable boost::signals2::signal<void (const ShipDesign*)> DesignBrowsedSignal;
    /** a hull was browsed (clicked once) */
    mutable boost::signals2::signal<void (const HullType*)> HullBrowsedSignal;
    /** a complete design was right-clicked (once) */
    mutable boost::signals2::signal<void (const ShipDesign*)> DesignRightClickedSignal;

private:
    void                            BaseDoubleClicked(GG::ListBox::iterator it);
    void                            BaseLeftClicked(GG::ListBox::iterator it, const GG::Pt& pt);
    void                            BaseRightClicked(GG::ListBox::iterator it, const GG::Pt& pt);

    GG::Pt                          ListRowSize();

    void                            PopulateWithEmptyHulls();
    void                            PopulateWithCompletedDesigns();

    int                             m_empire_id_shown;
    std::pair<bool, bool>           m_availabilities_shown;             // first indicates whether available parts should be shown.  second indicates whether unavailable parts should be shown
    bool                            m_showing_empty_hulls, m_showing_completed_designs;

    std::set<int>                   m_designs_in_list;
    std::set<std::string>           m_hulls_in_list;

    boost::signals2::connection     m_empire_designs_changed_signal;

    class BasesListBoxRow : public CUIListBox::Row {
    public:
        BasesListBoxRow(GG::X w, GG::Y h);
        virtual void                    Render();
        virtual void                    SizeMove(const GG::Pt& ul, const GG::Pt& lr);
    };

    class HullAndPartsListBoxRow : public BasesListBoxRow {
    public:
        class HullPanel : public GG::Control {
        public:
            HullPanel(GG::X w, GG::Y h, const std::string& hull);
            virtual void                    SizeMove(const GG::Pt& ul, const GG::Pt& lr);
            virtual void                    Render() {}
        private:
            GG::StaticGraphic*              m_graphic;
            GG::TextControl*                m_name;
            GG::TextControl*                m_cost_and_build_time;
        };
        HullAndPartsListBoxRow(GG::X w, GG::Y h, const std::string& hull, const std::vector<std::string>& parts);
        const std::string&              Hull() const { return m_hull; }
        const std::vector<std::string>& Parts() const { return m_parts; }
    private:
        std::string                     m_hull;
        std::vector<std::string>        m_parts;
    };

    class CompletedDesignListBoxRow : public BasesListBoxRow {
    public:
        CompletedDesignListBoxRow(GG::X w, GG::Y h, int design_id);
        int                             DesignID() const { return m_design_id; }
//        void                            Update() {
//            ShipDesignPanel* panel = dynamic_cast<ShipDesignPanel*>(this->operator[](0));
//        }
    private:
        int                             m_design_id;
    };
};

BasesListBox::BasesListBoxRow::BasesListBoxRow(GG::X w, GG::Y h) :
    CUIListBox::Row(w, h, "BasesListBoxRow")
{}

void BasesListBox::BasesListBoxRow::Render() {
    GG::Pt ul = UpperLeft();
    GG::Pt lr = LowerRight();
    GG::FlatRectangle(ul, lr, ClientUI::WndColor(), GG::CLR_WHITE, 1);
}

void BasesListBox::BasesListBoxRow::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    const GG::Pt old_size = Size();
    CUIListBox::Row::SizeMove(ul, lr);
    if (!empty() && old_size != Size())
        at(0)->Resize(Size());
}

BasesListBox::HullAndPartsListBoxRow::HullPanel::HullPanel(GG::X w, GG::Y h, const std::string& hull) :
    GG::Control(GG::X0, GG::Y0, w, h, GG::Flags<GG::WndFlag>()),
    m_graphic(0),
    m_name(0),
    m_cost_and_build_time(0)
{
    m_graphic = new GG::StaticGraphic(GG::X0, GG::Y0, w, h, ClientUI::HullIcon(hull), GG::GRAPHIC_PROPSCALE | GG::GRAPHIC_FITGRAPHIC);
    AttachChild(m_graphic);
    m_name = new GG::TextControl(GG::X0, GG::Y0, UserString(hull), ClientUI::GetFont(), ClientUI::TextColor(), GG::FORMAT_NONE);
    AttachChild(m_name);
}

void BasesListBox::HullAndPartsListBoxRow::HullPanel::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    GG::Control::SizeMove(ul, lr);
    if (m_graphic)
        m_graphic->Resize(Size());
    if (m_name)
        m_name->Resize(GG::Pt(Width(), m_name->Height()));
    if (m_cost_and_build_time) {
        //m_cost_and_build_time->???
    }
}

BasesListBox::HullAndPartsListBoxRow::HullAndPartsListBoxRow(GG::X w, GG::Y h, const std::string& hull,
                                                             const std::vector<std::string>& parts) :
    BasesListBoxRow(w, h),
    m_hull(hull),
    m_parts(parts)
{
    if (m_parts.empty()) {
        // contents are just a hull
        push_back(new HullPanel(w, h, m_hull));
    } else {
        // contents are a hull and parts  TODO: make a HullAndPartsPanel
        push_back(new GG::StaticGraphic(GG::X0, GG::Y0, w, h, ClientUI::HullTexture(hull), GG::GRAPHIC_PROPSCALE | GG::GRAPHIC_FITGRAPHIC));
    }
}

BasesListBox::CompletedDesignListBoxRow::CompletedDesignListBoxRow(GG::X w, GG::Y h, int design_id) :
    BasesListBoxRow(w, h),
    m_design_id(design_id)
{
    std::string hull;
    if (const ShipDesign* ship_design = GetShipDesign(design_id))
        hull = ship_design->Hull();
    push_back(new ShipDesignPanel(w, h, design_id));
}

BasesListBox::BasesListBox(GG::X x, GG::Y y, GG::X w, GG::Y h) :
    CUIListBox(x, y, w, h),
    m_empire_id_shown(ALL_EMPIRES),
    m_availabilities_shown(std::make_pair(false, false)),
    m_showing_empty_hulls(false),
    m_showing_completed_designs(false)
{
    GG::Connect(DoubleClickedSignal,    &BasesListBox::BaseDoubleClicked,   this);
    GG::Connect(LeftClickedSignal,      &BasesListBox::BaseLeftClicked,     this);
    GG::Connect(RightClickedSignal,     &BasesListBox::BaseRightClicked,    this);
}

const std::pair<bool, bool>& BasesListBox::GetAvailabilitiesShown() const
{ return m_availabilities_shown; }

void BasesListBox::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    const GG::Pt old_size = Size();
    CUIListBox::SizeMove(ul, lr);
    if (old_size != Size()) {
        const GG::Pt row_size = ListRowSize();
        for (GG::ListBox::iterator it = begin(); it != end(); ++it)
            (*it)->Resize(row_size);
    }
}

void BasesListBox::SetEmpireShown(int empire_id, bool refresh_list) {
    m_empire_id_shown = empire_id;

    // disconnect old signal
    m_empire_designs_changed_signal.disconnect();

    // connect signal to update this list if the empire's designs change
    if (const Empire* empire = Empires().Lookup(m_empire_id_shown))
        m_empire_designs_changed_signal = GG::Connect(empire->ShipDesignsChangedSignal, &BasesListBox::Populate,    this);

    if (refresh_list)
        Populate();
}

void BasesListBox::Populate() {
    //// abort of not visible to see results
    //if (!Empty() && !Visible())
    //    return;

    Logger().debugStream() << "BasesListBox::Populate";

    // populate list as appropriate for types of bases shown
    if (m_showing_empty_hulls)
        PopulateWithEmptyHulls();

    if (m_showing_completed_designs)
        PopulateWithCompletedDesigns();
}

GG::Pt BasesListBox::ListRowSize()
{ return GG::Pt(Width() - ClientUI::ScrollWidth() - 5, BASES_LIST_BOX_ROW_HEIGHT); }

void BasesListBox::PopulateWithEmptyHulls() {
    ScopedTimer scoped_timer("BasesListBox::PopulateWithEmptyHulls");

    const bool showing_available = m_availabilities_shown.first;
    const bool showing_unavailable = m_availabilities_shown.second;
    Logger().debugStream() << "BasesListBox::PopulateWithEmptyHulls showing available (t, f):  " << showing_available << ", " << showing_unavailable;
    const Empire* empire = Empires().Lookup(m_empire_id_shown); // may return 0
    Logger().debugStream() << "BasesListBox::PopulateWithEmptyHulls m_empire_id_shown: " << m_empire_id_shown;

    //Logger().debugStream() << "... hulls in list: ";
    //for (std::set<std::string>::const_iterator it = m_hulls_in_list.begin(); it != m_hulls_in_list.end(); ++it)
    //    Logger().debugStream() << "... ... hull: " << *it;

    // loop through all hulls, determining if they need to be added to list
    std::set<std::string> hulls_to_add;
    std::set<std::string> hulls_to_remove;

    const HullTypeManager& manager = GetHullTypeManager();
    for (HullTypeManager::iterator it = manager.begin(); it != manager.end(); ++it) {
        const std::string& hull_name = it->first;

        const HullType* hull_type = manager.GetHullType(hull_name);
        if (!hull_type || !hull_type->Producible())
            continue;

        // add or retain in list 1) all hulls if no empire is specified, or 
        //                       2) hulls of appropriate availablility for set empire
        if (!empire ||
            (showing_available && empire->ShipHullAvailable(hull_name)) ||
            (showing_unavailable && !empire->ShipHullAvailable(hull_name)))
        {
            // add or retain hull in list
            if (m_hulls_in_list.find(hull_name) == m_hulls_in_list.end())
                hulls_to_add.insert(hull_name);
        } else {
            // remove or don't add hull to list
            if (m_hulls_in_list.find(hull_name) != m_hulls_in_list.end())
                hulls_to_remove.insert(hull_name);
        }
    }

    //Logger().debugStream() << "... hulls to remove from list: ";
    //for (std::set<std::string>::const_iterator it = hulls_to_remove.begin(); it != hulls_to_remove.end(); ++it)
    //    Logger().debugStream() << "... ... hull: " << *it;
    //Logger().debugStream() << "... hulls to add to list: ";
    //for (std::set<std::string>::const_iterator it = hulls_to_add.begin(); it != hulls_to_add.end(); ++it)
    //    Logger().debugStream() << "... ... hull: " << *it;


    // loop through list, removing rows as appropriate
    for (iterator it = begin(); it != end(); ) {
        iterator temp_it = it++;
        //Logger().debugStream() << " row index: " << i;
        if (const HullAndPartsListBoxRow* row = dynamic_cast<const HullAndPartsListBoxRow*>(*temp_it)) {
            const std::string& current_row_hull = row->Hull();
            //Logger().debugStream() << " current row hull: " << current_row_hull;
            if (hulls_to_remove.find(current_row_hull) != hulls_to_remove.end()) {
                //Logger().debugStream() << " ... removing";
                m_hulls_in_list.erase(current_row_hull);    // erase from set before deleting row, so as to not invalidate current_row_hull reference to deleted row's member string
                delete Erase(temp_it);
            }
        }
    }

    // loop through hulls to add, adding to list
    std::vector<std::string> empty_parts_vec;
    const GG::Pt row_size = ListRowSize();
    for (std::set<std::string>::const_iterator it = hulls_to_add.begin(); it != hulls_to_add.end(); ++it) {
        const std::string& hull_name = *it;
        HullAndPartsListBoxRow* row = new HullAndPartsListBoxRow(row_size.x, row_size.y, hull_name, empty_parts_vec);
        Insert(row);
        row->Resize(row_size);

        m_hulls_in_list.insert(hull_name);
    }
}

void BasesListBox::PopulateWithCompletedDesigns() {
    ScopedTimer scoped_timer("BasesListBox::PopulateWithCompletedDesigns");

    const bool showing_available = m_availabilities_shown.first;
    const bool showing_unavailable = m_availabilities_shown.second;
    const Empire* empire = Empires().Lookup(m_empire_id_shown); // may return 0
    const Universe& universe = GetUniverse();

    Logger().debugStream() << "BasesListBox::PopulateWithCompletedDesigns for empire " << m_empire_id_shown;

    // remove preexisting rows
    Clear();
    const GG::Pt row_size = ListRowSize();

    if (empire) {
        // add rows for designs this empire is keeping
        for (Empire::ShipDesignItr it = empire->ShipDesignBegin(); it != empire->ShipDesignEnd(); ++it) {
            int design_id = *it;
            bool available = empire->ShipDesignAvailable(design_id);
            const ShipDesign* design = GetShipDesign(design_id);
            if (!design || !design->Producible())
                continue;
            if ((available && showing_available) || (!available && showing_unavailable)) {
                CompletedDesignListBoxRow* row = new CompletedDesignListBoxRow(row_size.x, row_size.y, design_id);
                Insert(row);
                row->Resize(row_size);
            }
        }
    } else if (showing_available) {
        // add all known / existing designs
        for (Universe::ship_design_iterator it = universe.beginShipDesigns(); it != universe.endShipDesigns(); ++it) {
            int design_id = it->first;
            const ShipDesign* design = it->second;
            if (!design->Producible())
                continue;
            CompletedDesignListBoxRow* row = new CompletedDesignListBoxRow(row_size.x, row_size.y, design_id);
            Insert(row);
            row->Resize(row_size);
        }
    }
}

void BasesListBox::BaseLeftClicked(GG::ListBox::iterator it, const GG::Pt& pt) {
    // determine type of row that was clicked, and emit appropriate signal

    CompletedDesignListBoxRow* design_row = dynamic_cast<CompletedDesignListBoxRow*>(*it);
    if (design_row) {
        int id = design_row->DesignID();
        const ShipDesign* design = GetShipDesign(id);
        if (design)
            DesignBrowsedSignal(design);
    }

    HullAndPartsListBoxRow* box_row = dynamic_cast<HullAndPartsListBoxRow*>(*it);
    if (box_row) {
        const std::string& hull_name = box_row->Hull();
        const HullType* hull_type = GetHullType(hull_name);
        const std::vector<std::string>& parts = box_row->Parts();
        if (hull_type && parts.empty())
            HullBrowsedSignal(hull_type);
    }
}

void BasesListBox::BaseRightClicked(GG::ListBox::iterator it, const GG::Pt& pt) {
    // determine type of row that was clicked, and emit appropriate signal

    CompletedDesignListBoxRow* design_row = dynamic_cast<CompletedDesignListBoxRow*>(*it);
    if (design_row) {
        int design_id = design_row->DesignID();
        const ShipDesign* design = GetShipDesign(design_id);
        if (design)
            DesignRightClickedSignal(design);
        // TODO: Subsequent code assumes we have a design, so we may want to do something about that...

        int empire_id = HumanClientApp::GetApp()->EmpireID();

        Logger().debugStream() << "BasesListBox::BaseRightClicked on design id : " << design_id;

        // create popup menu with a commands in it
        GG::MenuItem menu_contents;
        menu_contents.next_level.push_back(GG::MenuItem(UserString("DESIGN_DELETE"), 1, false, false));
        if (design->DesignedByEmpire() == empire_id) {
            menu_contents.next_level.push_back(GG::MenuItem(UserString("DESIGN_RENAME"), 2, false, false));
        }
        GG::PopupMenu popup(pt.x, pt.y, ClientUI::GetFont(), menu_contents, ClientUI::TextColor(),
                            ClientUI::WndOuterBorderColor(), ClientUI::WndColor(), ClientUI::EditHiliteColor());
        if (popup.Run()) {
            switch (popup.MenuID()) {

            case 1: { // delete design
                HumanClientApp::GetApp()->Orders().IssueOrder(
                    OrderPtr(new ShipDesignOrder(empire_id, design_id, true)));
                break;
            }
            case 2: { // rename design
                CUIEditWnd edit_wnd(GG::X(350), UserString("DESIGN_ENTER_NEW_DESIGN_NAME"), design->Name());
                edit_wnd.Run();
                const std::string& result = edit_wnd.Result();
                if (result != "" && result != design->Name()) {
                    HumanClientApp::GetApp()->Orders().IssueOrder(
                        OrderPtr(new ShipDesignOrder(empire_id, design_id, result)));
                    ShipDesignPanel* design_panel = dynamic_cast<ShipDesignPanel*>((*design_row)[0]);
                    design_panel->Update();
                }
                break;
            }

            default:
                break;
            }
        }
    }
}

void BasesListBox::BaseDoubleClicked(GG::ListBox::iterator it) {
    // determine type of row that was clicked, and emit appropriate signal

    HullAndPartsListBoxRow* hp_row = dynamic_cast<HullAndPartsListBoxRow*>(*it);
    if (hp_row) {
        DesignComponentsSelectedSignal(hp_row->Hull(), hp_row->Parts());
        return;
    }

    CompletedDesignListBoxRow* cd_row = dynamic_cast<CompletedDesignListBoxRow*>(*it);
    if (cd_row) {
        DesignSelectedSignal(cd_row->DesignID());
        return;
    }
}

void BasesListBox::ShowEmptyHulls(bool refresh_list) {
    m_showing_empty_hulls = true;
    m_showing_completed_designs = false;
    if (refresh_list)
        Populate();
}

void BasesListBox::ShowCompletedDesigns(bool refresh_list) {
    m_showing_empty_hulls = false;
    m_showing_completed_designs = true;
    if (refresh_list)
        Populate();
}

void BasesListBox::ShowAvailability(bool available, bool refresh_list) {
    if (available) {
        if (!m_availabilities_shown.first) {
            m_availabilities_shown.first = true;
            if (refresh_list)
                Populate();
        }
    } else {
        if (!m_availabilities_shown.second) {
            m_availabilities_shown.second = true;
            if (refresh_list)
                Populate();
        }
    }
}

void BasesListBox::HideAvailability(bool available, bool refresh_list) {
    if (available) {
        if (m_availabilities_shown.first) {
            m_availabilities_shown.first = false;
            if (refresh_list)
                Populate();
        }
    } else {
        if (m_availabilities_shown.second) {
            m_availabilities_shown.second = false;
            if (refresh_list)
                Populate();
        }
    }
}


//////////////////////////////////////////////////
// DesignWnd::BaseSelector                      //
//////////////////////////////////////////////////
class DesignWnd::BaseSelector : public CUIWnd {
public:
    /** \name Structors */ //@{
    BaseSelector(GG::X w, GG::Y h);
    //@}

    /** \name Mutators */ //@{
    virtual void    SizeMove(const GG::Pt& ul, const GG::Pt& lr);
    void            Reset();
    void            ToggleAvailability(bool available, bool refresh_list);
    void            SetEmpireShown(int empire_id, bool refresh_list);
    void            ShowAvailability(bool available, bool refresh_list);
    void            HideAvailability(bool available, bool refresh_list);
    //@}

    /** an existing complete design that is known to this empire
        was selected (double-clicked) */
    mutable boost::signals2::signal<void (int)> DesignSelectedSignal;

    /** a hull and a set of parts (which may be empty) was
        selected (double-clicked) */
    mutable boost::signals2::signal<void (const std::string&, const std::vector<std::string>&)>
                    DesignComponentsSelectedSignal;

    /** a complete design was browsed (clicked once) */
    mutable boost::signals2::signal<void (const ShipDesign*)>
                    DesignBrowsedSignal;

    /** a hull was browsed (clicked once) */
    mutable boost::signals2::signal<void (const HullType*)>
                    HullBrowsedSignal;
private:
    void            DoLayout();
    void            WndSelected(std::size_t index);

    GG::TabWnd*                         m_tabs;
    BasesListBox*                       m_hulls_list;           // empty hulls on which a new design can be based
    BasesListBox*                       m_designs_list;         // designs this empire has created or learned how to make
    std::pair<CUIButton*, CUIButton*>   m_availability_buttons;
};

DesignWnd::BaseSelector::BaseSelector(GG::X w, GG::Y h) :
    CUIWnd(UserString("DESIGN_WND_STARTS"), GG::X0, GG::Y0, w, h, GG::INTERACTIVE | GG::RESIZABLE | GG::ONTOP | GG::DRAGABLE),
    m_tabs(0),
    m_hulls_list(0),
    m_designs_list(0)
{
    CUIButton* button = new CUIButton(UserString("PRODUCTION_WND_AVAILABILITY_AVAILABLE"), GG::X(10), GG::Y(10), GG::X(10));
    m_availability_buttons.first = button;
    AttachChild(button);
    GG::Connect(button->LeftClickedSignal,
                boost::bind(&DesignWnd::BaseSelector::ToggleAvailability, this, true, true));

    button = new CUIButton(UserString("PRODUCTION_WND_AVAILABILITY_UNAVAILABLE"), GG::X(10), GG::Y(10), GG::X(10));
    m_availability_buttons.second = button;
    AttachChild(button);
    GG::Connect(button->LeftClickedSignal,
                boost::bind(&DesignWnd::BaseSelector::ToggleAvailability, this, false, true));

    m_tabs = new GG::TabWnd(GG::X(5), GG::Y(2), GG::X(10), GG::Y(10), ClientUI::GetFont(), ClientUI::WndColor(), ClientUI::TextColor(), GG::TAB_BAR_DETACHED, GG::INTERACTIVE);
    GG::Connect(m_tabs->WndChangedSignal,                       &DesignWnd::BaseSelector::WndSelected,      this);
    AttachChild(m_tabs);

    m_hulls_list = new BasesListBox(GG::X0, GG::Y0, GG::X(10), GG::Y(10));
    m_tabs->AddWnd(m_hulls_list, UserString("DESIGN_WND_HULLS"));
    m_hulls_list->ShowEmptyHulls(false);
    GG::Connect(m_hulls_list->DesignComponentsSelectedSignal,   DesignWnd::BaseSelector::DesignComponentsSelectedSignal);
    GG::Connect(m_hulls_list->HullBrowsedSignal,                DesignWnd::BaseSelector::HullBrowsedSignal);

    m_designs_list = new BasesListBox(GG::X0, GG::Y0, GG::X(10), GG::Y(10));
    m_tabs->AddWnd(m_designs_list, UserString("DESIGN_WND_FINISHED_DESIGNS"));
    m_designs_list->ShowCompletedDesigns(false);
    GG::Connect(m_designs_list->DesignSelectedSignal,           DesignWnd::BaseSelector::DesignSelectedSignal);
    GG::Connect(m_designs_list->DesignBrowsedSignal,            DesignWnd::BaseSelector::DesignBrowsedSignal);

    ////m_saved_designs_list = new CUIListBox(GG::X0, GG::Y0, GG::X(10), GG::X(10));
    //m_tabs->AddWnd(new GG::TextControl(GG::X0, GG::Y0, GG::X(30), GG::Y(20), UserString("DESIGN_NO_PART"),
    //                                   ClientUI::GetFont(),
    //                                   ClientUI::TextColor()),
    //               UserString("DESIGN_WND_SAVED_DESIGNS"));

    ////m_templates_list = new CUIListBox(GG::X0, GG::Y0, GG::X(10), GG::X(10));
    //m_tabs->AddWnd(new GG::TextControl(GG::X0, GG::Y0, GG::X(30), GG::Y(20), UserString("DESIGN_NO_PART"),
    //                                   ClientUI::GetFont(),
    //                                   ClientUI::TextColor()),
    //               UserString("DESIGN_WND_TEMPLATES"));

    DoLayout();
    ShowAvailability(true, false);   // default to showing available unavailable bases.
}

void DesignWnd::BaseSelector::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    const GG::Pt old_size = Size();
    CUIWnd::SizeMove(ul, lr);
    if (old_size != Size())
        DoLayout();
}

void DesignWnd::BaseSelector::Reset() {
    ScopedTimer scoped_timer("BaseSelector::Reset");

    const int empire_id = HumanClientApp::GetApp()->EmpireID();
    SetEmpireShown(empire_id, false);

    if (!m_tabs)
        return;

    if (GG::Wnd* wnd = m_tabs->CurrentWnd()) {
        if (BasesListBox* base_box = dynamic_cast<BasesListBox*>(wnd))
            base_box->Populate();
    }
}

void DesignWnd::BaseSelector::SetEmpireShown(int empire_id, bool refresh_list) {
    if (m_hulls_list)
        m_hulls_list->SetEmpireShown(empire_id, refresh_list);
    if (m_designs_list)
        m_designs_list->SetEmpireShown(empire_id, refresh_list);
}

void DesignWnd::BaseSelector::ShowAvailability(bool available, bool refresh_list) {
    if (m_hulls_list)
        m_hulls_list->ShowAvailability(available, refresh_list);
    if (m_designs_list)
        m_designs_list->ShowAvailability(available, refresh_list);
    if (available)
        m_availability_buttons.first->MarkSelectedGray();
    else
        m_availability_buttons.second->MarkSelectedGray();
}

void DesignWnd::BaseSelector::HideAvailability(bool available, bool refresh_list) {
    if (m_hulls_list)
        m_hulls_list->HideAvailability(available, refresh_list);
    if (m_designs_list)
        m_designs_list->HideAvailability(available, refresh_list);
    if (available)
        m_availability_buttons.first->MarkNotSelected();
    else
        m_availability_buttons.second->MarkNotSelected();
}

void DesignWnd::BaseSelector::ToggleAvailability(bool available, bool refresh_list) {
    const std::pair<bool, bool>& avail_shown = m_hulls_list->GetAvailabilitiesShown();
    if (available) {
        if (avail_shown.first)
            HideAvailability(true, refresh_list);
        else
            ShowAvailability(true, refresh_list);
    } else {
        if (avail_shown.second)
            HideAvailability(false, refresh_list);
        else
            ShowAvailability(false, refresh_list);
    }
}

void DesignWnd::BaseSelector::DoLayout() {
    const GG::X LEFT_PAD(5);
    const GG::Y TOP_PAD(2);
    const GG::X AVAILABLE_WIDTH = ClientWidth() - 2*LEFT_PAD;
    const int BUTTON_SEPARATION = 3;
    const GG::X BUTTON_WIDTH = (AVAILABLE_WIDTH - BUTTON_SEPARATION) / 2;
    const int PTS = ClientUI::Pts();
    const GG::Y BUTTON_HEIGHT(PTS * 2);

    GG::Y top(TOP_PAD);
    GG::X left(LEFT_PAD);

    m_availability_buttons.first->SizeMove(GG::Pt(left, top), GG::Pt(left + BUTTON_WIDTH, top + BUTTON_HEIGHT));
    left = left + BUTTON_WIDTH + BUTTON_SEPARATION;
    m_availability_buttons.second->SizeMove(GG::Pt(left, top), GG::Pt(left + BUTTON_WIDTH, top + BUTTON_HEIGHT));
    left = LEFT_PAD;
    top = top + BUTTON_HEIGHT + BUTTON_SEPARATION;

    if (!m_tabs)
        return;
    m_tabs->SizeMove(GG::Pt(left, top), ClientSize() - GG::Pt(LEFT_PAD, TOP_PAD));
}

void DesignWnd::BaseSelector::WndSelected(std::size_t index)
{ Reset(); }

//////////////////////////////////////////////////
// SlotControl                                  //
//////////////////////////////////////////////////
/** UI representation and drop-target for slots of a design.  PartControl may
  * be dropped into slots to add the corresponding parts to the ShipDesign, or
  * the part may be set programmatically with SetPart(). */
class SlotControl : public GG::Control {
public:
    /** \name Structors */ //@{
    SlotControl();
    SlotControl(double x, double y, ShipSlotType slot_type);
    //@}

    /** \name Accessors */ //@{
    virtual void    DropsAcceptable(DropsAcceptableIter first, DropsAcceptableIter last, const GG::Pt& pt) const;

    ShipSlotType    SlotType() const;
    double          XPositionFraction() const;
    double          YPositionFraction() const;
    const PartType* GetPart() const;
    //@}

    /** \name Mutators */ //@{
    virtual void    StartingChildDragDrop(const GG::Wnd* wnd, const GG::Pt& offset);
    virtual void    CancellingChildDragDrop(const std::vector<const GG::Wnd*>& wnds);
    virtual void    AcceptDrops(const std::vector<GG::Wnd*>& wnds, const GG::Pt& pt);
    virtual void    ChildrenDraggedAway(const std::vector<GG::Wnd*>& wnds, const GG::Wnd* destination);
    virtual void    DragDropEnter(const GG::Pt& pt, const std::map<GG::Wnd*, GG::Pt>& drag_drop_wnds, GG::Flags<GG::ModKey> mod_keys);
    virtual void    DragDropLeave();

    virtual void    Render();
    void            Highlight(bool actually = true);

    void            SetPart(const std::string& part_name);  //!< used to programmatically set the PartControl in this slot.  Does not emit signal
    void            SetPart(const PartType* part_type = 0); //!< used to programmatically set the PartControl in this slot.  Does not emit signal
    //@}

    /** emitted when the contents of a slot are altered by the dragging
      * a PartControl in or out of the slot.  signal should be caught and the
      * slot contents set using SetPart accordingly */
    mutable boost::signals2::signal<void (const PartType*)> SlotContentsAlteredSignal;

    mutable boost::signals2::signal<void (const PartType*)> PartTypeClickedSignal;

private:
    /** emits SlotContentsAlteredSignal with PartType* = 0.  needed because
      * boost::signals2::signal is noncopyable, so boost::bind can't be used
      * to bind the parameter 0 to SlotContentsAlteredSignal::operator() */
    void            EmitNullSlotContentsAlteredSignal();

    bool                m_highlighted;
    ShipSlotType        m_slot_type;
    double              m_x_position_fraction, m_y_position_fraction;   //!< position on hull image where slot should be shown, as a fraction of that image's size
    PartControl*        m_part_control;
    GG::StaticGraphic*  m_background;
};

SlotControl::SlotControl() :
    GG::Control(GG::X0, GG::Y0, SLOT_CONTROL_WIDTH, SLOT_CONTROL_HEIGHT, GG::INTERACTIVE),
    m_highlighted(false),
    m_slot_type(INVALID_SHIP_SLOT_TYPE),
    m_x_position_fraction(0.4),
    m_y_position_fraction(0.4),
    m_part_control(0),
    m_background(0)
{ SetDragDropDataType(""); }

SlotControl::SlotControl(double x, double y, ShipSlotType slot_type) :
    GG::Control(GG::X0, GG::Y0, SLOT_CONTROL_WIDTH, SLOT_CONTROL_HEIGHT, GG::INTERACTIVE),
    m_highlighted(false),
    m_slot_type(slot_type),
    m_x_position_fraction(x),
    m_y_position_fraction(y),
    m_part_control(0),
    m_background(0)
{
    SetDragDropDataType("");
    m_background = new GG::StaticGraphic(GG::X0, GG::Y0, SLOT_CONTROL_WIDTH, SLOT_CONTROL_HEIGHT,
                                         SlotBackgroundTexture(m_slot_type),
                                         GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
    m_background->Show();
    AttachChild(m_background);

    SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));

    // set up empty slot tool tip
    std::string title_text;
    if (slot_type == SL_EXTERNAL)
        title_text = UserString("SL_EXTERNAL");
    else if (slot_type == SL_INTERNAL)
        title_text = UserString("SL_INTERNAL");
    else if (slot_type == SL_CORE)
        title_text = UserString("SL_CORE");

    SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
        new IconTextBrowseWnd(SlotBackgroundTexture(m_slot_type), title_text, UserString("SL_TOOLTIP_DESC"))));
}

void SlotControl::DropsAcceptable(DropsAcceptableIter first, DropsAcceptableIter last, const GG::Pt& pt) const {
    bool acceptable_part_found = false;
    for (DropsAcceptableIter it = first; it != last; ++it) {
        if (!acceptable_part_found && it->first->DragDropDataType() == PART_CONTROL_DROP_TYPE_STRING) {
            const PartControl* part_control = boost::polymorphic_downcast<const PartControl*>(it->first);
            const PartType* part_type = part_control->Part();
            if (part_type &&
                part_type->CanMountInSlotType(m_slot_type) &&
                part_control != m_part_control) {
                it->second = true;
                acceptable_part_found = true;
            } else {
                it->second = false;
            }
        } else {
            it->second = false;
        }
    }
}

ShipSlotType SlotControl::SlotType() const
{ return m_slot_type; }

double SlotControl::XPositionFraction() const
{ return m_x_position_fraction; }

double SlotControl::YPositionFraction() const
{ return m_y_position_fraction; }

const PartType* SlotControl::GetPart() const {
    if (m_part_control)
        return m_part_control->Part();
    else
        return 0;
}

void SlotControl::StartingChildDragDrop(const GG::Wnd* wnd, const GG::Pt& offset) {
    if (!m_part_control)
        return;

    const PartControl* control = dynamic_cast<const PartControl*>(wnd);
    if (!control)
        return;

    if (control == m_part_control)
        m_part_control->Hide();
}

void SlotControl::CancellingChildDragDrop(const std::vector<const GG::Wnd*>& wnds) {
    if (!m_part_control)
        return;

    for (std::vector<const GG::Wnd*>::const_iterator it = wnds.begin(); it != wnds.end(); ++it) {
        const PartControl* control = dynamic_cast<const PartControl*>(*it);
        if (!control)
            continue;

        if (control == m_part_control)
            m_part_control->Show();
    }
}

void SlotControl::AcceptDrops(const std::vector<GG::Wnd*>& wnds, const GG::Pt& pt) {
    assert(wnds.size() == 1);

    const GG::Wnd* wnd = *(wnds.begin());
    const PartControl* control = boost::polymorphic_downcast<const PartControl*>(wnd);
    const PartType* part_type = control->Part();

    delete control;

    //Logger().debugStream() << "SlotControl::AcceptDrops part_type: " << (part_type ? part_type->Name() : "no part");
    SlotContentsAlteredSignal(part_type);
}

void SlotControl::ChildrenDraggedAway(const std::vector<GG::Wnd*>& wnds, const GG::Wnd* destination) {
    if (wnds.empty())
        return;
    const GG::Wnd* wnd = wnds.front();
    const PartControl* part_control = dynamic_cast<const PartControl*>(wnd);
    if (part_control != m_part_control)
        return;
    m_part_control = 0; // SlotContentsAlteredSignal is connected to this->SetPart, which will delete m_part_control if it is not null.  The drop-accepting Wnd is responsible for deleting the accepted Wnd, so setting m_part_control = 0 here prevents this->SetPart from deleting it prematurely
    SlotContentsAlteredSignal(0);
}

void SlotControl::DragDropEnter(const GG::Pt& pt, const std::map<GG::Wnd*, GG::Pt>& drag_drop_wnds, GG::Flags<GG::ModKey> mod_keys)
{}

void SlotControl::DragDropLeave() {
    if (m_part_control)
        m_part_control->Hide();
}

void SlotControl::Render()
{}

void SlotControl::Highlight(bool actually)
{ m_highlighted = actually; }

void SlotControl::SetPart(const std::string& part_name)
{ SetPart(GetPartType(part_name)); }

void SlotControl::SetPart(const PartType* part_type) {
    // remove existing part control, if any
    if (m_part_control) {
        delete m_part_control;
        m_part_control = 0;
    }

    // create new part control for passed in part_type
    if (part_type) {
        m_part_control = new PartControl(part_type);
        AttachChild(m_part_control);

        // single click shows encyclopedia data
        GG::Connect(m_part_control->ClickedSignal, PartTypeClickedSignal);

        // double click clears slot
        GG::Connect(m_part_control->DoubleClickedSignal,
                    boost::bind(&SlotControl::EmitNullSlotContentsAlteredSignal, this));
        SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));

        // set part occupying slot's tool tip to say slot type
        std::string title_text;
        if (m_slot_type == SL_EXTERNAL)
            title_text = UserString("SL_EXTERNAL");
        else if (m_slot_type == SL_INTERNAL)
            title_text = UserString("SL_INTERNAL");
        else if (m_slot_type == SL_CORE)
            title_text = UserString("SL_CORE");

        m_part_control->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
            new IconTextBrowseWnd(ClientUI::PartIcon(part_type->Name()),
                                  UserString(part_type->Name()) + " (" + title_text + ")",
                                  UserString(part_type->Description()))));
    }
}

void SlotControl::EmitNullSlotContentsAlteredSignal()
{ SlotContentsAlteredSignal(0); }


//////////////////////////////////////////////////
// DesignWnd::MainPanel                         //
//////////////////////////////////////////////////
class DesignWnd::MainPanel : public CUIWnd {
public:
    /** \name Structors */ //@{
    MainPanel(GG::X w, GG::Y h);
    //@}

    /** \name Accessors */ //@{
    const std::vector<std::string>      Parts() const;              //!< returns vector of names of parts in slots of current shown design.  empty slots are represented with empty stri
    const std::string&                  Hull() const;               //!< returns name of hull of current shown design
    const std::string&                  DesignName() const;         //!< returns name currently entered for design
    const std::string&                  DesignDescription() const;  //!< returns description currently entered for design

    boost::shared_ptr<const ShipDesign> GetIncompleteDesign() const;                                //!< returns a pointer to the design currently being modified (if any).  may return an empty pointer if not currently modifying a design.
    int                                 GetCompleteDesignID() const;                                //!< returns ID of complete design currently being shown in this panel.  returns ShipDesign::INVALID_DESIGN_ID if not showing a complete design
    static std::string                  dummy;
    bool                                CurrentDesignIsRegistered(std::string& design_name = dummy);//!< returns true iff a design with the same hull and parts is already registered with thsi empire; if so, also populates design_name with the name of that design
    //@}

    /** \name Mutators */ //@{
    virtual void    LClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys);

    virtual void    SizeMove(const GG::Pt& ul, const GG::Pt& lr);
    void            Sanitize();

    void            SetPart(const std::string& part_name, unsigned int slot);   //!< puts specified part in specified slot.  does nothing if slot is out of range of available slots for current hull
    void            SetPart(const PartType* part, unsigned int slot);
    void            SetParts(const std::vector<std::string>& parts);            //!< puts specified parts in slots.  attempts to put each part into the slot corresponding to its place in the passed vector.  if a part cannot be placed, it is ignored.  more parts than there are slots available are ignored, and slots for which there are insufficient parts in the passed vector are unmodified

    /** Attempts to add the specified part to the design, if possible.  will
      * first attempt to add part to an empty slot of the appropriate type, and
      * if no appropriate slots are available, may or may not move other parts
      * around within the design to open up a compatible slot in which to add
      * this part (and then add it).  may also do nothing. */
    void            AddPart(const PartType* part);
    bool            CanPartBeAdded(const PartType* part);

    void            ClearParts();                                               //!< removes all parts from design.  hull is not altered
    void            SetHull(const std::string& hull_name);                      //!< sets the design hull to the specified hull, displaying appropriate background image and creating appropriate SlotControls
    void            SetHull(const HullType* hull);
    void            SetDesign(const ShipDesign* ship_design);                   //!< sets the displayed design by setting the appropriate hull and parts
    void            SetDesign(int design_id);                                   //!< sets the displayed design by setting the appropriate hull and parts
    void            SetDesignComponents(const std::string& hull, const std::vector<std::string>& parts);//!< sets design hull and parts to those specified

    void            HighlightSlotType(std::vector<ShipSlotType>& slot_types);   //!< renders slots of the indicated types differently, perhaps to indicate that that those slots can be drop targets for a particular part?
    void            RegisterCompletedDesignDump(std::string design_dump);       
    void            ReregisterDesigns();                                        //!< resets m_completed_designs and reregisters all current empire designs.
                                                                                //!< Is used w/r/t m_confirm_button status
    //@}

    /** emitted when the design is changed (by adding or removing parts, not
      * name or description changes) */
    mutable boost::signals2::signal<void ()> DesignChangedSignal;

    /** propegates signals from contained SlotControls that signal that a part
      * has been clicked */
    mutable boost::signals2::signal<void (const PartType*)> PartTypeClickedSignal;

    /** emitted when the user clicks the m_confirm_button to add the new
      * design to the player's empire */
    mutable boost::signals2::signal<void ()> DesignConfirmedSignal;

    /** emitted when the user clicks on the background of this main panel and
      * a completed design is showing */
    mutable boost::signals2::signal<void (int)> CompleteDesignClickedSignal;

private:
    // disambiguate overloaded SetPart function, because otherwise boost::bind wouldn't be able to tell them apart
    typedef void (DesignWnd::MainPanel::*SetPartFuncPtrType)(const PartType* part, unsigned int slot);
    static SetPartFuncPtrType const s_set_part_func_ptr;

    void            Populate();                         //!< creates and places SlotControls for current hull
    void            DoLayout();                         //!< positions buttons, text entry boxes and SlotControls
    void            DesignChanged();                    //!< responds to the design being changed
    void            RefreshIncompleteDesign() const;
    std::string     GetCleanDesignDump(const ShipDesign* ship_design); //!< similar to ship design dump but without 'lookup_strings', icon and model entries

    bool            AddPartEmptySlot(const PartType* part, int slot_number);                            //!< Adds part to slot number
    bool            AddPartWithSwapping(const PartType* part, std::pair<int, int> swap_and_empty_slot); //!< Swaps part in slot # pair.first to slot # pair.second, adds given part to slot # pair.first
    int             FindEmptySlotForPart(const PartType* part);                                         //!< Determines if a part can be added to any empty slot, returns the slot index if possible, otherwise -1   
    
    void            EditedSignal(const std::string& new_name);                                ///< Triggered when m_design_name's AfterTextChangedSignal fires. Used for basic name validation.

    std::pair<int, int> FindSlotForPartWithSwapping(const PartType* part);                              //!< Determines if a part can be added to a slot with swapping, returns a pair containing the slot to swap and an empty slot, otherwise a pair with -1
                                                                                                        //!< This function only tries to find a way to add the given part by swapping a part already in a slot to an empty slot
                                                                                                        //!< If theres an open slot that the given part could go into but all of the occupied slots contain parts that can't swap into the open slot
                                                                                                        //!< This function will indicate that it could not add the part, even though adding the part is possible
    const HullType*                         m_hull;
    std::vector<SlotControl*>               m_slots;
    int                                     m_complete_design_id;
    mutable boost::shared_ptr<ShipDesign>   m_incomplete_design;
    std::set<std::string>                   m_completed_design_dump_strings;

    GG::StaticGraphic*  m_background_image;
    GG::TextControl*    m_design_name_label;
    GG::Edit*           m_design_name;
    GG::TextControl*    m_design_description_label;
    GG::Edit*           m_design_description;
    GG::Button*         m_confirm_button;
    GG::Button*         m_clear_button;
    boost::signals2::connection      m_empire_designs_changed_signal;
};

// static
DesignWnd::MainPanel::SetPartFuncPtrType const DesignWnd::MainPanel::s_set_part_func_ptr = &DesignWnd::MainPanel::SetPart;

DesignWnd::MainPanel::MainPanel(GG::X w, GG::Y h) :
    CUIWnd(UserString("DESIGN_WND_MAIN_PANEL_TITLE"), GG::X0, GG::Y0, w, h, GG::INTERACTIVE | GG::DRAGABLE | GG::RESIZABLE),
    m_hull(0),
    m_slots(),
    m_complete_design_id(ShipDesign::INVALID_DESIGN_ID),
    m_incomplete_design(),
    m_completed_design_dump_strings(),
    m_background_image(0),
    m_design_name_label(0),
    m_design_name(0),
    m_design_description_label(0),
    m_design_description(0),
    m_confirm_button(0),
    m_clear_button(0)
{
    SetChildClippingMode(ClipToClient);

    boost::shared_ptr<GG::Font> font = ClientUI::GetFont();

    m_design_name_label = new GG::TextControl(GG::X0, GG::Y0, GG::X(10), GG::Y(10), UserString("DESIGN_WND_DESIGN_NAME"), font,
                                              ClientUI::TextColor(), GG::FORMAT_RIGHT | GG::FORMAT_VCENTER,
                                              GG::INTERACTIVE | GG::ONTOP);
    AttachChild(m_design_name_label);

    m_design_name = new CUIEdit(GG::X0, GG::Y0, GG::X(10), UserString("DESIGN_NAME_DEFAULT"), font, ClientUI::CtrlBorderColor(),
                                ClientUI::TextColor(), ClientUI::CtrlColor(), GG::INTERACTIVE | GG::ONTOP);
    AttachChild(m_design_name);
    GG::Connect(m_design_name->EditedSignal, &DesignWnd::MainPanel::EditedSignal, this);

    m_design_description_label = new GG::TextControl(GG::X0, GG::Y0, GG::X(10), GG::Y(10), UserString("DESIGN_WND_DESIGN_DESCRIPTION"), font,
                                                     ClientUI::TextColor(), GG::FORMAT_RIGHT | GG::FORMAT_VCENTER,
                                                     GG::INTERACTIVE | GG::ONTOP);
    AttachChild(m_design_description_label);

    m_design_description = new CUIEdit(GG::X0, GG::Y0, GG::X(10), UserString("DESIGN_DESCRIPTION_DEFAULT"), font, ClientUI::CtrlBorderColor(),
                                       ClientUI::TextColor(), ClientUI::CtrlColor(), GG::INTERACTIVE | GG::ONTOP);
    AttachChild(m_design_description);

    m_confirm_button = new CUIButton(UserString("DESIGN_WND_CONFIRM"), GG::X0, GG::Y0, GG::X(10), font, ClientUI::CtrlColor(),
                                     ClientUI::CtrlBorderColor(), 1, ClientUI::TextColor(), GG::INTERACTIVE | GG::ONTOP);
    AttachChild(m_confirm_button);
    GG::Connect(m_confirm_button->LeftClickedSignal, DesignConfirmedSignal);
    m_confirm_button->SetBrowseModeTime(GetOptionsDB().Get<int>("UI.tooltip-delay"));

    m_clear_button = new CUIButton(UserString("DESIGN_WND_CLEAR"), GG::X0, GG::Y0, GG::X(10), font, ClientUI::CtrlColor(),
                                   ClientUI::CtrlBorderColor(), 1, ClientUI::TextColor(), GG::INTERACTIVE | GG::ONTOP);
    AttachChild(m_clear_button);
    GG::Connect(m_clear_button->LeftClickedSignal, &DesignWnd::MainPanel::ClearParts, this);


    GG::Connect(this->DesignChangedSignal, &DesignWnd::MainPanel::DesignChanged, this);
    DesignChanged(); // Initialize components that rely on the current state of the design.
}

const std::vector<std::string> DesignWnd::MainPanel::Parts() const {
    std::vector<std::string> retval;
    for (std::vector<SlotControl*>::const_iterator it = m_slots.begin(); it != m_slots.end(); ++it) {
        const PartType* part_type = (*it)->GetPart();
        if (part_type)
            retval.push_back(part_type->Name());
        else
            retval.push_back("");
    }
    return retval;
}

const std::string& DesignWnd::MainPanel::Hull() const {
    if (m_hull)
        return m_hull->Name();
    else
        return EMPTY_STRING;
}

const std::string& DesignWnd::MainPanel::DesignName() const {
    if (m_design_name)
        return m_design_name->Text();
    else
        return EMPTY_STRING;
}

const std::string& DesignWnd::MainPanel::DesignDescription() const {
    if (m_design_description)
        return m_design_description->Text();
    else
        return EMPTY_STRING;
}

boost::shared_ptr<const ShipDesign> DesignWnd::MainPanel::GetIncompleteDesign() const {
    RefreshIncompleteDesign();
    return m_incomplete_design;
}

int DesignWnd::MainPanel::GetCompleteDesignID() const
{ return m_complete_design_id; }

bool DesignWnd::MainPanel::CurrentDesignIsRegistered(std::string& design_name) {
    int empire_id = HumanClientApp::GetApp()->EmpireID();
    const Empire* empire = Empires().Lookup(empire_id); // Had better not return 0 if we're designing a ship.
    if (!empire) {
        Logger().errorStream() << "DesignWnd::MainPanel::CurrentDesignIsRegistered couldn't get the current empire.";
        return false;
    }

    if (boost::shared_ptr<const ShipDesign> cur_design = GetIncompleteDesign()) {
        for (Empire::ShipDesignItr it = empire->ShipDesignBegin();
             it != empire->ShipDesignEnd(); ++it)
        {
            const ShipDesign& ship_design = *GetShipDesign(*it);
            if (ship_design == *cur_design.get()) {
                design_name = ship_design.Name();
                return true;
            }
        }
    }
    return false;
}

void DesignWnd::MainPanel::LClick(const GG::Pt& pt, GG::Flags<GG::ModKey> mod_keys) {
    if (m_incomplete_design.get())
        DesignChangedSignal();
    else if (m_complete_design_id != ShipDesign::INVALID_DESIGN_ID)
        CompleteDesignClickedSignal(m_complete_design_id);
    CUIWnd::LClick(pt, mod_keys);
}

void DesignWnd::MainPanel::SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
    CUIWnd::SizeMove(ul, lr);
    DoLayout();
}

void DesignWnd::MainPanel::ReregisterDesigns() {
    Logger().debugStream() << "DesignWnd::MainPanel::ReregisterDesigns";
    m_completed_design_dump_strings.clear();
    int empire_id = HumanClientApp::GetApp()->EmpireID();
    const Empire* empire = Empires().Lookup(empire_id); // may return 0
    if (empire) {
        for (Empire::ShipDesignItr it = empire->ShipDesignBegin(); it != empire->ShipDesignEnd(); ++it) {
            if (const ShipDesign* design = GetShipDesign(*it)) {
                std::string dump_str = GetCleanDesignDump(design);
                m_completed_design_dump_strings.insert(dump_str); //no need to validate
            }
        }
    }
}

void DesignWnd::MainPanel::Sanitize() {
    SetHull(0);
    m_design_name->SetText(UserString("DESIGN_NAME_DEFAULT"));
    m_design_description->SetText(UserString("DESIGN_DESCRIPTION_DEFAULT"));
    // disconnect old empire design signal
    m_empire_designs_changed_signal.disconnect();
    // connect signal to update this list if the empire's designs change
    int empire_id = HumanClientApp::GetApp()->EmpireID();
    if (const Empire* empire = Empires().Lookup(empire_id))
        m_empire_designs_changed_signal = GG::Connect(empire->ShipDesignsChangedSignal, &MainPanel::ReregisterDesigns,    this); // not apparent if this is working, but in typical use is unnecessary
    ReregisterDesigns();
}

void DesignWnd::MainPanel::SetPart(const std::string& part_name, unsigned int slot)
{ SetPart(GetPartType(part_name), slot); }

void DesignWnd::MainPanel::SetPart(const PartType* part, unsigned int slot) {
    //Logger().debugStream() << "DesignWnd::MainPanel::SetPart(" << (part ? part->Name() : "no part") << ", slot " << slot << ")";
    if (slot > m_slots.size()) {
        Logger().errorStream() << "DesignWnd::MainPanel::SetPart specified nonexistant slot";
        return;
    }
    m_slots[slot]->SetPart(part);
    DesignChangedSignal();
}

void DesignWnd::MainPanel::SetParts(const std::vector<std::string>& parts) {
    unsigned int num_parts = std::min(parts.size(), m_slots.size());
    for (unsigned int i = 0; i < num_parts; ++i)
        m_slots[i]->SetPart(parts[i]);
    DesignChangedSignal();
}

void DesignWnd::MainPanel::AddPart(const PartType* part) {
    if (!AddPartEmptySlot(part, FindEmptySlotForPart(part))) {
        if (!AddPartWithSwapping(part, FindSlotForPartWithSwapping(part)))
            Logger().debugStream() << "DesignWnd::MainPanel::AddPart(" << (part ? part->Name() : "no part") << ") couldn't find a slot for the part";
    }      
}

bool DesignWnd::MainPanel::CanPartBeAdded(const PartType* part) {
    std::pair<int, int> swap_result = FindSlotForPartWithSwapping(part);
    return (FindEmptySlotForPart(part) >= 0 || (swap_result.first >= 0 && swap_result.second >= 0));
}

bool DesignWnd::MainPanel::AddPartEmptySlot(const PartType* part, int slot_number) {
    if (!part || slot_number < 0)
        return false;
    SetPart(part, slot_number);
    return true;
}

bool DesignWnd::MainPanel::AddPartWithSwapping(const PartType* part, std::pair<int, int> swap_and_empty_slot) {
    if (!part || swap_and_empty_slot.first < 0 || swap_and_empty_slot.second < 0)
        return false;
    SetPart(m_slots[swap_and_empty_slot.first]->GetPart(), swap_and_empty_slot.second);         // Move the flexible part to the first open spot
    SetPart(part, swap_and_empty_slot.first);                                                   // Move ourself into the newly opened slot
    return true;
}

int DesignWnd::MainPanel::FindEmptySlotForPart(const PartType* part) {
    int result = -1;
    if (!part)
        return result;

    for (unsigned int i = 0; i < m_slots.size(); ++i) {             // scan through slots to find one that can mount part
        const ShipSlotType slot_type = m_slots[i]->SlotType();
        const PartType* part_type = m_slots[i]->GetPart();          // check if this slot is empty

        if (!part_type && part->CanMountInSlotType(slot_type)) {    // ... and if the part can mount here
            result = i;
            return result;
        }
    }
    return result;
}

void DesignWnd::MainPanel::EditedSignal(const std::string& new_name) {
    DesignChangedSignal();  // Check whether the confirmation button should be enabled or disabled each time the name changes.
}

std::pair<int, int> DesignWnd::MainPanel::FindSlotForPartWithSwapping(const PartType* part) {
    // result.first = swap_slot, result.second = empty_slot
    std::pair<int, int> result = std::make_pair(-1, -1);

    // TODO: allow for swapping of core slot parts
    if (!part || (!part->CanMountInSlotType(SL_EXTERNAL) && !part->CanMountInSlotType(SL_INTERNAL)))
        return result;

    for (unsigned int i = 0; i < m_slots.size(); ++i) {             // scan through slots to find one that can mount part
        const ShipSlotType slot_type = m_slots[i]->SlotType();
        const PartType* part_type = m_slots[i]->GetPart();          // check if this slot is empty

        if (!part_type)                                             // record an empty slot index
            result.second = i;

        // Find a slot that has a part that can be mounted in both slots
        // And that we are compatible with
        if (part_type &&
            part_type->CanMountInSlotType(SL_EXTERNAL) &&
            part_type->CanMountInSlotType(SL_INTERNAL) &&
            part->CanMountInSlotType(slot_type))
        { result.first = i; }
    }
    return result;
}

void DesignWnd::MainPanel::ClearParts() {
    for (unsigned int i = 0; i < m_slots.size(); ++i)
        m_slots[i]->SetPart(0);
    DesignChangedSignal();
}

void DesignWnd::MainPanel::SetHull(const std::string& hull_name)
{ SetHull(GetHullType(hull_name)); }

void DesignWnd::MainPanel::SetHull(const HullType* hull) {
    m_hull = hull;
    DeleteChild(m_background_image);
    m_background_image = 0;
    if (m_hull) {
        boost::shared_ptr<GG::Texture> texture = ClientUI::HullTexture(hull->Name());
        m_background_image = new GG::StaticGraphic(GG::X0, GG::Y0, GG::X1, GG::Y1, texture, GG::GRAPHIC_PROPSCALE | GG::GRAPHIC_FITGRAPHIC);
        AttachChild(m_background_image);
        MoveChildDown(m_background_image);
    }
    Populate();
    DoLayout();
    if (hull)
        DesignChangedSignal();
}

void DesignWnd::MainPanel::SetDesign(const ShipDesign* ship_design) {
    m_incomplete_design.reset();

    if (!ship_design) {
        SetHull(0);
        return;
    }

    m_complete_design_id = ship_design->ID();

    m_design_name->SetText(ship_design->Name());
    m_design_description->SetText(ship_design->Description());

    const HullType* hull_type = ship_design->GetHull();
    SetHull(hull_type);

    const std::vector<std::string>& parts_vec = ship_design->Parts();
    for (unsigned int i = 0; i < parts_vec.size() && i < m_slots.size(); ++i)
        m_slots[i]->SetPart(GetPartType(parts_vec[i]));
    DesignChangedSignal();
}

void DesignWnd::MainPanel::SetDesign(int design_id)
{ SetDesign(GetShipDesign(design_id)); }

void DesignWnd::MainPanel::SetDesignComponents(const std::string& hull, const std::vector<std::string>& parts) {
    SetHull(hull);
    SetParts(parts);
}

void DesignWnd::MainPanel::HighlightSlotType(std::vector<ShipSlotType>& slot_types) {
    for (std::vector<SlotControl*>::iterator it = m_slots.begin(); it != m_slots.end(); ++it) {
        SlotControl* control = *it;
        ShipSlotType slot_type = control->SlotType();
        if (std::find(slot_types.begin(), slot_types.end(), slot_type) != slot_types.end())
            control->Highlight(true);
        else
            control->Highlight(false);
    }
}

void DesignWnd::MainPanel::Populate(){
    for (std::vector<SlotControl*>::iterator it = m_slots.begin(); it != m_slots.end(); ++it)
        delete *it;
    m_slots.clear();

    if (!m_hull)
        return;

    const std::vector<HullType::Slot>& hull_slots = m_hull->Slots();

    for (std::vector<HullType::Slot>::size_type i = 0; i != hull_slots.size(); ++i) {
        const HullType::Slot& slot = hull_slots[i];
        SlotControl* slot_control = new SlotControl(slot.x, slot.y, slot.type);
        m_slots.push_back(slot_control);
        AttachChild(slot_control);
        boost::function<void (const PartType*)> set_part_func =
            boost::bind(DesignWnd::MainPanel::s_set_part_func_ptr, this, _1, i);
        GG::Connect(slot_control->SlotContentsAlteredSignal, set_part_func);
        GG::Connect(slot_control->PartTypeClickedSignal, PartTypeClickedSignal);
    }
}

void DesignWnd::MainPanel::DoLayout() {
    // position labels and text edit boxes for name and description and buttons to clear and confirm design

    const int PTS = ClientUI::Pts();
    const GG::X PTS_WIDE(PTS / 2);           // guess at how wide per character the font needs
    const GG::Y BUTTON_HEIGHT(PTS * 2);
    const GG::X LABEL_WIDTH = PTS_WIDE * 15;
    const int PAD = 6;
    const int GUESSTIMATE_NUM_CHARS_IN_BUTTON_TEXT = 25;    // rough guesstimate... avoid overly long part class names
    const GG::X BUTTON_WIDTH = PTS_WIDE*GUESSTIMATE_NUM_CHARS_IN_BUTTON_TEXT;

    GG::X edit_right = ClientWidth();
    GG::Y edit_height = BUTTON_HEIGHT;
    GG::X confirm_right = ClientWidth() - PAD;

    if (m_confirm_button) {
        GG::Pt lr = GG::Pt(confirm_right, BUTTON_HEIGHT) + GG::Pt(GG::X0, GG::Y(PAD));
        GG::Pt ul = lr - GG::Pt(BUTTON_WIDTH, BUTTON_HEIGHT);
        m_confirm_button->SizeMove(ul, lr);
        edit_right = ul.x - PAD;
    }
    if (m_clear_button) {
        GG::Pt lr = ClientSize() + GG::Pt(-GG::X(PAD), -GG::Y(PAD));
        GG::Pt ul = lr - GG::Pt(BUTTON_WIDTH, BUTTON_HEIGHT);
        m_clear_button->SizeMove(ul, lr);
    }

    if (m_design_name)
        edit_height = m_design_name->Height();

    GG::X x(PAD);
    GG::Y y(PAD);
    if (m_design_name_label) {
        GG::Pt ul = GG::Pt(x, y);
        GG::Pt lr = ul + GG::Pt(LABEL_WIDTH, edit_height);
        m_design_name_label->SizeMove(ul, lr);
        x = lr.x + PAD;
    }
    if (m_design_name) {
        GG::Pt ul = GG::Pt(x, y);
        GG::Pt lr = GG::Pt(edit_right, y + edit_height);
        m_design_name->SizeMove(ul, lr);
        x = lr.x + PAD;
    }

    x = GG::X(PAD);
    y += (edit_height + PAD);

    if (m_design_description_label) {
        GG::Pt ul = GG::Pt(x, y);
        GG::Pt lr = ul + GG::Pt(LABEL_WIDTH, edit_height);
        m_design_description_label->SizeMove(ul, lr);
        x = lr.x + PAD;
    }
    if (m_design_description) {
        GG::Pt ul = GG::Pt(x, y);
        GG::Pt lr = GG::Pt(confirm_right, y + edit_height);
        m_design_description->SizeMove(ul, lr);
        x = lr.x + PAD;
    }

    y += (edit_height);

    // place background image of hull
    GG::Rect background_rect = GG::Rect(GG::Pt(GG::X0, y), ClientLowerRight());

    if (m_background_image) {
        GG::Pt ul = GG::Pt(GG::X0, y);
        GG::Pt lr = ClientSize();
        m_background_image->SizeMove(ul, lr);
        background_rect = m_background_image->RenderedArea();
    }

    // place slot controls over image of hull
    for (std::vector<SlotControl*>::iterator it = m_slots.begin(); it != m_slots.end(); ++it) {
        SlotControl* slot = *it;
        GG::X x(background_rect.Left() - slot->Width()/2 - ClientUpperLeft().x + slot->XPositionFraction() * background_rect.Width());
        GG::Y y(background_rect.Top() - slot->Height()/2 - ClientUpperLeft().y + slot->YPositionFraction() * background_rect.Height());
        slot->MoveTo(GG::Pt(x, y));
    }
}

void DesignWnd::MainPanel::DesignChanged() {
    m_confirm_button->ClearBrowseInfoWnd();

    m_complete_design_id = ShipDesign::INVALID_DESIGN_ID;
    int client_empire_id = HumanClientApp::GetApp()->EmpireID();
    std::string design_name;

    if (!m_hull) {
        m_confirm_button->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
            new TextBrowseWnd(UserString("DESIGN_INVALID"), UserString("DESIGN_INV_NO_HULL"))));

        m_confirm_button->Disable(true);
    }
    else if (client_empire_id == ALL_EMPIRES) {
        m_confirm_button->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
            new TextBrowseWnd(UserString("DESIGN_INVALID"), UserString("DESIGN_INV_MODERATOR"))));

        m_confirm_button->Disable(true);
    }
    else if (m_design_name->Text().empty()) { // Whitespace probably shouldn't be OK either.
        m_confirm_button->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
            new TextBrowseWnd(UserString("DESIGN_INVALID"), UserString("DESIGN_INV_NO_NAME"))));

        m_confirm_button->Disable(true);
    }
    else if (!ShipDesign::ValidDesign(m_hull->Name(), Parts())) {
        // I have no idea how this would happen, so I'm not going to display a tooltip for it. ~ Bigjoe5
        m_confirm_button->Disable(true);
    }
    else if (CurrentDesignIsRegistered(design_name)) {
        m_confirm_button->SetBrowseInfoWnd(boost::shared_ptr<GG::BrowseInfoWnd>(
            new TextBrowseWnd(UserString("DESIGN_KNOWN"),
            boost::io::str(FlexibleFormat(UserString("DESIGN_KNOWN_DETAIL")) % design_name))));

        m_confirm_button->Disable(true);
    }
    else {
        m_confirm_button->Disable(false);
    }
}

std::string DesignWnd::MainPanel::GetCleanDesignDump(const ShipDesign* ship_design) {
    std::string retval = "ShipDesign\n";
    retval += ship_design->Name() + "\"\n";
    retval += ship_design->Hull() + "\"\n";
    const std::vector<std::string> part_list = ship_design->Parts();
    for (std::vector<std::string>::const_iterator it = part_list.begin(); it != part_list.end(); ++it) {
        retval += "\"" + *it + "\"\n";
    }
    return retval; 
}

void DesignWnd::MainPanel::RefreshIncompleteDesign() const {
    if (ShipDesign* design = m_incomplete_design.get()) {
        if (design->Hull() ==           this->Hull() &&
            design->Name() ==           this->DesignName() &&
            design->Description() ==    this->DesignDescription() &&
            design->Parts() ==          this->Parts())
        {
            // nothing has changed, so don't need to update
            return;
        }
    }

    // assemble and check info for new design
    const std::string& hull =           this->Hull();
    std::vector<std::string> parts =    this->Parts();

    if (!ShipDesign::ValidDesign(hull, parts)) {
        Logger().errorStream() << "DesignWnd::MainPanel::RefreshIncompleteDesign attempting to create an invalid design.";
        m_incomplete_design.reset();
        return;
    }

    // make sure name isn't blank.  TODO: prevent duplicate names?
    std::string name = this->DesignName();
    if (name.empty())
        name = UserString("DESIGN_NAME_DEFAULT");

    const std::string& description = this->DesignDescription();

    const std::string& icon = m_hull ? m_hull->Icon() : EMPTY_STRING;

    // update stored design
    try {
        m_incomplete_design.reset(new ShipDesign(name, description, CurrentTurn(), ClientApp::GetApp()->EmpireID(),
                                                 hull, parts, icon, ""));
    } catch (const std::exception& e) {
        // had a weird crash in the above call a few times, but I can't seem to
        // replicate it now.  hopefully catching any exception here will
        // prevent crashes and instead just cause the incomplete design details
        // to not update when expected.
        Logger().errorStream() << "DesignWnd::MainPanel::RefreshIncompleteDesign caught exception: " << e.what();
    }
}

//////////////////////////////////////////////////
// DesignWnd                                    //
//////////////////////////////////////////////////
DesignWnd::DesignWnd(GG::X w, GG::Y h) :
    GG::Wnd(GG::X0, GG::Y0, w, h, GG::ONTOP | GG::INTERACTIVE),
    m_detail_panel(0),
    m_base_selector(0),
    m_part_palette(0),
    m_main_panel(0)
{
    Sound::TempUISoundDisabler sound_disabler;
    SetChildClippingMode(ClipToClient);

    GG::X base_selector_width(300);
    GG::X most_panels_left = base_selector_width;
    GG::X most_panels_width = ClientWidth() - most_panels_left;
    GG::Y detail_top = GG::Y0;
    GG::Y detail_height(200);
    GG::Y main_top = detail_top + detail_height;
    GG::Y part_palette_height(160);
    GG::Y part_palette_top = ClientHeight() - part_palette_height;
    GG::Y main_height = part_palette_top - main_top;

    m_detail_panel = new EncyclopediaDetailPanel(most_panels_width, detail_height);
    AttachChild(m_detail_panel);
    m_detail_panel->MoveTo(GG::Pt(most_panels_left, detail_top));

    m_main_panel = new MainPanel(most_panels_width, main_height);
    AttachChild(m_main_panel);
    GG::Connect(m_main_panel->PartTypeClickedSignal,            static_cast<void (EncyclopediaDetailPanel::*)(const PartType*)>(&EncyclopediaDetailPanel::SetItem),  m_detail_panel);
    GG::Connect(m_main_panel->DesignConfirmedSignal,            &DesignWnd::AddDesign,              this);
    GG::Connect(m_main_panel->DesignChangedSignal,              boost::bind(&EncyclopediaDetailPanel::SetIncompleteDesign,
                                                                            m_detail_panel,
                                                                            boost::bind(&DesignWnd::MainPanel::GetIncompleteDesign,
                                                                                        m_main_panel)));
    GG::Connect(m_main_panel->CompleteDesignClickedSignal,      static_cast<void (EncyclopediaDetailPanel::*)(int)>(&EncyclopediaDetailPanel::SetDesign),m_detail_panel);
    m_main_panel->MoveTo(GG::Pt(most_panels_left, main_top));
    m_main_panel->Sanitize();

    m_part_palette = new PartPalette(most_panels_width, part_palette_height);
    AttachChild(m_part_palette);
    GG::Connect(m_part_palette->PartTypeClickedSignal,          static_cast<void (EncyclopediaDetailPanel::*)(const PartType*)>(&EncyclopediaDetailPanel::SetItem),  m_detail_panel);
    GG::Connect(m_part_palette->PartTypeDoubleClickedSignal,    &DesignWnd::MainPanel::AddPart,     m_main_panel);
    m_part_palette->MoveTo(GG::Pt(most_panels_left, part_palette_top));

    m_base_selector = new BaseSelector(base_selector_width, ClientHeight());
    AttachChild(m_base_selector);
    GG::Connect(m_base_selector->DesignSelectedSignal,          static_cast<void (MainPanel::*)(int)>(&MainPanel::SetDesign),              m_main_panel);
    GG::Connect(m_base_selector->DesignComponentsSelectedSignal,&MainPanel::SetDesignComponents,    m_main_panel);
    GG::Connect(m_base_selector->DesignBrowsedSignal,           static_cast<void (EncyclopediaDetailPanel::*)(const ShipDesign*)>(&EncyclopediaDetailPanel::SetItem),  m_detail_panel);
    GG::Connect(m_base_selector->HullBrowsedSignal,             static_cast<void (EncyclopediaDetailPanel::*)(const HullType*)>(&EncyclopediaDetailPanel::SetItem),  m_detail_panel);
    m_base_selector->MoveTo(GG::Pt());
}

void DesignWnd::Reset() {
    m_part_palette->Reset();
    m_base_selector->Reset();
    m_detail_panel->Refresh();
    m_main_panel->Sanitize();
}

void DesignWnd::Sanitize()
{ m_main_panel->Sanitize(); }

void DesignWnd::Render() {
    GG::Pt ul = UpperLeft();
    GG::Pt lr = LowerRight();

    // use GL to draw the lines
    glDisable(GL_TEXTURE_2D);

    // draw background
    glBegin(GL_POLYGON);
        glColor(ClientUI::WndColor());
        glVertex(ul.x, ul.y);
        glVertex(lr.x, ul.y);
        glVertex(lr.x, lr.y);
        glVertex(ul.x, lr.y);
        glVertex(ul.x, ul.y);
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

void DesignWnd::ShowPartTypeInEncyclopedia(const std::string& part_type)
{ m_detail_panel->SetPartType(part_type); }

void DesignWnd::ShowHullTypeInEncyclopedia(const std::string& hull_type)
{ m_detail_panel->SetHullType(hull_type); }

void DesignWnd::ShowShipDesignInEncyclopedia(int design_id)
{ m_detail_panel->SetDesign(design_id); }

void DesignWnd::AddDesign() {
    int empire_id = HumanClientApp::GetApp()->EmpireID();
    const Empire* empire = Empires().Lookup(empire_id);
    if (!empire) return;

    std::vector<std::string> parts = m_main_panel->Parts();
    const std::string& hull_name = m_main_panel->Hull();

    if (!ShipDesign::ValidDesign(hull_name, parts)) {
        Logger().errorStream() << "DesignWnd::AddDesign tried to add an invalid ShipDesign";
        return;
    }

    // make sure name isn't blank.  TODO: prevent duplicate names?
    std::string name = m_main_panel->DesignName();
    if (name == "")
        name = UserString("DESIGN_NAME_DEFAULT");

    const std::string& description = m_main_panel->DesignDescription();

    std::string icon = "ship_hulls/generic_hull.png";
    if (const HullType* hull = GetHullType(hull_name))
        icon = hull->Icon();

    // create design from stuff chosen in UI
    ShipDesign design(name, description, CurrentTurn(), ClientApp::GetApp()->EmpireID(),
                      hull_name, parts, icon, "some model");

    int new_design_id = HumanClientApp::GetApp()->GetNewDesignID();
    HumanClientApp::GetApp()->Orders().IssueOrder(
        OrderPtr(new ShipDesignOrder(empire_id, new_design_id, design)));
    m_main_panel->ReregisterDesigns();
    m_main_panel->DesignChangedSignal();

    Logger().debugStream() << "Added new design: " << design.Name();
}

void DesignWnd::EnableOrderIssuing(bool enable/* = true*/)
{}
