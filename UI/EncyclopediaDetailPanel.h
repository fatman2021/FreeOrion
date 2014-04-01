// -*- C++ -*-
#ifndef _ENCYCLOPEDIA_DETAIL_PANEL_H_
#define _ENCYCLOPEDIA_DETAIL_PANEL_H_

#include "CUIWnd.h"

class Planet;
class Tech;
class PartType;
class HullType;
class BuildingType;
class Special;
class Species;
class FieldType;
class UniverseObject;
class Empire;
class ShipDesign;
class CUIMultiEdit;
class GraphControl;
template <class T> class TemporaryPtr;
namespace GG {
    class TextControl;
    class StaticGraphic;
    class MultiEdit;
}

/** UI class that displays in-game encyclopedic information about game
  * content.  Tech, PartType, HullType, BuildingType, ShipDesign, etc. */
class EncyclopediaDetailPanel : public CUIWnd {
public:
    //! \name Structors //!@{
    EncyclopediaDetailPanel(GG::X w, GG::Y h);
    //!@}

    /** \name Mutators */ //@{
    virtual void    SizeMove(const GG::Pt& ul, const GG::Pt& lr);
    virtual void    Render();

    /** Need to redefine this so that icons and name can be put at the top of
      * the Wnd, rather than being restricted to the client area of a CUIWnd */
    virtual GG::Pt  ClientUpperLeft() const;

    void            AddItem(const std::string& type, const std::string& name); // adds a new item to m_items
    void            PopItem();
    void            ClearItems();
    int             GetItemsSize() { return m_items.size(); }

    void            SetText(const std::string& text, bool lookup_in_stringtable = true);
    void            SetPlanet(int planet_id);
    void            SetCombatLog(int log_id);
    void            SetItem(TemporaryPtr<const Planet> planet);
    void            SetTech(const std::string& tech_name);
    void            SetItem(const Tech* tech);
    void            SetPartType(const std::string& part_name);
    void            SetItem(const PartType* part);
    void            SetHullType(const std::string& hull_name);
    void            SetItem(const HullType* hull_type);
    void            SetBuildingType(const std::string& building_name);
    void            SetItem(const BuildingType* building_type);
    void            SetSpecial(const std::string& special_name);
    void            SetItem(const Special* special);
    void            SetSpecies(const std::string& species_name);
    void            SetItem(const Species* species);
    void            SetFieldType(const std::string& field_type_name);
    void            SetItem(const FieldType* field_type);
    void            SetObject(int object_id);
    void            SetObject(const std::string& object_id);
    void            SetItem(TemporaryPtr<const UniverseObject> obj);
    void            SetEmpire(int empire_id);
    void            SetEmpire(const std::string& empire_id);
    void            SetItem(const Empire* empire);
    void            SetDesign(int design_id);
    void            SetDesign(const std::string& design_id);
    void            SetItem(const ShipDesign* design);
    void            SetIncompleteDesign(boost::weak_ptr<const ShipDesign> incomplete_design);
    void            SetGraph(const std::string& graph_id);
    void            SetIndex();

    void            Refresh();
    void            OnIndex();
    void            OnBack();
    void            OnNext();
    //@}

    mutable boost::signals2::signal<void ()> ClosingSignal;

private:
    void            DoLayout();

    virtual void    CloseClicked();

    void            HandleLinkClick(const std::string& link_type, const std::string& data);
    void            HandleLinkDoubleClick(const std::string& link_type, const std::string& data);

    static std::list<std::pair <std::string, std::string> >             m_items;    // stores all items which have been observed in the past
                                                                                    // .first == item type; .second == item.name
    static std::list<std::pair <std::string, std::string> >::iterator   m_items_it; // stores actual position within m_items
    boost::weak_ptr<const ShipDesign>                                   m_incomplete_design;

    GG::TextControl*    m_name_text;        // name
    GG::TextControl*    m_cost_text;        // cost and time to build or research
    GG::TextControl*    m_summary_text;     // general purpose item
    GG::MultiEdit*      m_description_box;  // detailed and lengthy description
    GG::StaticGraphic*  m_icon;
    GG::StaticGraphic*  m_other_icon;
    GG::Button*         m_index_button;
    GG::Button*         m_back_button;
    GG::Button*         m_next_button;

    GraphControl*       m_graph;
};


#endif
