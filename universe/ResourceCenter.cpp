#include "ResourceCenter.h"

#include "../util/Directories.h"
#include "../util/Logger.h"
#include "../util/OptionsDB.h"
#include "../Empire/Empire.h"
#include "Fleet.h"
#include "Planet.h"
#include "ShipDesign.h"
#include "System.h"
#include "Building.h"

#include <stdexcept>

namespace {
    static const std::string EMPTY_STRING;
}

ResourceCenter::ResourceCenter() :
    m_focus(),
    m_last_turn_focus_changed(BEFORE_FIRST_TURN)
{}

ResourceCenter::~ResourceCenter()
{}

ResourceCenter::ResourceCenter(const ResourceCenter& rhs) :
    m_focus(rhs.m_focus),
    m_last_turn_focus_changed(rhs.m_last_turn_focus_changed)
{}

void ResourceCenter::Copy(TemporaryPtr<const ResourceCenter> copied_object, Visibility vis) {
    if (copied_object == this)
        return;
    if (!copied_object) {
        Logger().errorStream() << "ResourceCenter::Copy passed a null object";
        return;
    }

    if (vis >= VIS_PARTIAL_VISIBILITY) {
        this->m_focus = copied_object->m_focus;
        this->m_last_turn_focus_changed = copied_object->m_last_turn_focus_changed;
    }
}

void ResourceCenter::Init() {
    //Logger().debugStream() << "ResourceCenter::Init";
    AddMeter(METER_INDUSTRY);
    AddMeter(METER_RESEARCH);
    AddMeter(METER_TRADE);
    AddMeter(METER_CONSTRUCTION);
    AddMeter(METER_TARGET_INDUSTRY);
    AddMeter(METER_TARGET_RESEARCH);
    AddMeter(METER_TARGET_TRADE);
    AddMeter(METER_TARGET_CONSTRUCTION);
    m_focus.clear();
    m_last_turn_focus_changed = INVALID_GAME_TURN;
}

const std::string& ResourceCenter::Focus() const
{ return m_focus; }

int ResourceCenter::TurnsSinceFocusChange() const {
    if (m_last_turn_focus_changed == INVALID_GAME_TURN)
        return 0;
    int current_turn = CurrentTurn();
    if (current_turn == INVALID_GAME_TURN)
        return 0;
    return current_turn - m_last_turn_focus_changed;
}

std::vector<std::string> ResourceCenter::AvailableFoci() const
{ return std::vector<std::string>(); }

const std::string& ResourceCenter::FocusIcon(const std::string& focus_name) const
{ return EMPTY_STRING; }

std::string ResourceCenter::Dump() const {
    std::stringstream os;
    os << "ResourceCenter focus: " << m_focus << " last changed on turn: " << m_last_turn_focus_changed;
    return os.str();
}

float ResourceCenter::ResourceCenterNextTurnMeterValue(MeterType type) const {
    const Meter* meter = GetMeter(type);
    if (!meter) {
        throw std::invalid_argument("ResourceCenter::ResourceCenterNextTurnMeterValue passed meter type that the ResourceCenter does not have.");
    }
    float current_meter_value = meter->Current();

    MeterType target_meter_type = INVALID_METER_TYPE;
    switch (type) {
    case METER_TARGET_INDUSTRY:
    case METER_TARGET_RESEARCH:
    case METER_TARGET_TRADE:
    case METER_TARGET_CONSTRUCTION:
        return current_meter_value;
        break;
    case METER_INDUSTRY:    target_meter_type = METER_TARGET_INDUSTRY;      break;
    case METER_RESEARCH:    target_meter_type = METER_TARGET_RESEARCH;      break;
    case METER_TRADE:       target_meter_type = METER_TARGET_TRADE;         break;
    case METER_CONSTRUCTION:target_meter_type = METER_TARGET_CONSTRUCTION;  break;
    default:
        Logger().errorStream() << "ResourceCenter::ResourceCenterNextTurnMeterValue dealing with invalid meter type";
        return 0.0f;
    }

    const Meter* target_meter = GetMeter(target_meter_type);
    if (!target_meter) {
        throw std::runtime_error("ResourceCenter::ResourceCenterNextTurnMeterValue dealing with invalid meter type");
    }
    float target_meter_value = target_meter->Current();

    // currently meter growth is one per turn.
    if (target_meter_value > current_meter_value)
        return std::min(current_meter_value + 1.0f, target_meter_value);
    else if (target_meter_value < current_meter_value)
        return std::max(target_meter_value, current_meter_value - 1.0f);
    else
        return current_meter_value;
}

void ResourceCenter::SetFocus(const std::string& focus) {
    if (focus.empty()) {
        ClearFocus();
        return;
    }
    std::vector<std::string> avail_foci = AvailableFoci();
    if (std::find(avail_foci.begin(), avail_foci.end(), focus) != avail_foci.end()) {
        m_focus = focus;
        m_last_turn_focus_changed = CurrentTurn();
        ResourceCenterChangedSignal();
        return;
    }
    Logger().errorStream() << "ResourceCenter::SetFocus Exploiter!-- unavailable focus " << focus << " attempted to be set for object w/ dump string: " << Dump();
}

void ResourceCenter::ClearFocus() {
    m_focus.clear();
    m_last_turn_focus_changed = CurrentTurn();
    ResourceCenterChangedSignal();
}

void ResourceCenter::ResourceCenterResetTargetMaxUnpairedMeters() {
    GetMeter(METER_TARGET_INDUSTRY)->ResetCurrent();
    GetMeter(METER_TARGET_RESEARCH)->ResetCurrent();
    GetMeter(METER_TARGET_TRADE)->ResetCurrent();
    GetMeter(METER_TARGET_CONSTRUCTION)->ResetCurrent();
}

void ResourceCenter::ResourceCenterPopGrowthProductionResearchPhase() {
    GetMeter(METER_INDUSTRY)->SetCurrent(ResourceCenterNextTurnMeterValue(METER_INDUSTRY));
    GetMeter(METER_RESEARCH)->SetCurrent(ResourceCenterNextTurnMeterValue(METER_RESEARCH));
    GetMeter(METER_TRADE)->SetCurrent(ResourceCenterNextTurnMeterValue(METER_TRADE));
    GetMeter(METER_CONSTRUCTION)->SetCurrent(ResourceCenterNextTurnMeterValue(METER_CONSTRUCTION));
}

void ResourceCenter::ResourceCenterClampMeters() {
    GetMeter(METER_TARGET_INDUSTRY)->ClampCurrentToRange();
    GetMeter(METER_TARGET_RESEARCH)->ClampCurrentToRange();
    GetMeter(METER_TARGET_TRADE)->ClampCurrentToRange();
    GetMeter(METER_TARGET_CONSTRUCTION)->ClampCurrentToRange();

    GetMeter(METER_INDUSTRY)->ClampCurrentToRange();
    GetMeter(METER_RESEARCH)->ClampCurrentToRange();
    GetMeter(METER_TRADE)->ClampCurrentToRange();
    GetMeter(METER_CONSTRUCTION)->ClampCurrentToRange();
}

void ResourceCenter::Reset() {
    m_focus.clear();
    m_last_turn_focus_changed = INVALID_GAME_TURN;

    GetMeter(METER_INDUSTRY)->Reset();
    GetMeter(METER_RESEARCH)->Reset();
    GetMeter(METER_TRADE)->Reset();
    GetMeter(METER_CONSTRUCTION)->Reset();

    GetMeter(METER_TARGET_INDUSTRY)->Reset();
    GetMeter(METER_TARGET_RESEARCH)->Reset();
    GetMeter(METER_TARGET_TRADE)->Reset();
    GetMeter(METER_TARGET_CONSTRUCTION)->Reset();
}
