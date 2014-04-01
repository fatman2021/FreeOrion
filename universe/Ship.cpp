#include "Ship.h"

#include "../util/i18n.h"
#include "../util/Logger.h"
#include "../util/Random.h"
#include "Fleet.h"
#include "Predicates.h"
#include "ShipDesign.h"
#include "Species.h"
#include "Universe.h"
#include "../Empire/Empire.h"
#include "../Empire/EmpireManager.h"

class Species;
const Species* GetSpecies(const std::string& name);

Ship::Ship() :
    m_design_id(ShipDesign::INVALID_DESIGN_ID),
    m_fleet_id(INVALID_OBJECT_ID),
    m_ordered_scrapped(false),
    m_ordered_colonize_planet_id(INVALID_OBJECT_ID),
    m_ordered_invade_planet_id(INVALID_OBJECT_ID),
    m_ordered_bombard_planet_id(INVALID_OBJECT_ID),
    m_last_turn_active_in_combat(INVALID_GAME_TURN),
    m_produced_by_empire_id(ALL_EMPIRES)
{}

Ship::Ship(int empire_id, int design_id, const std::string& species_name,
           int produced_by_empire_id/* = ALL_EMPIRES*/) :
    m_design_id(design_id),
    m_fleet_id(INVALID_OBJECT_ID),
    m_ordered_scrapped(false),
    m_ordered_colonize_planet_id(INVALID_OBJECT_ID),
    m_ordered_invade_planet_id(INVALID_OBJECT_ID),
    m_ordered_bombard_planet_id(INVALID_OBJECT_ID),
    m_last_turn_active_in_combat(INVALID_GAME_TURN),
    m_species_name(species_name),
    m_produced_by_empire_id(produced_by_empire_id)
{
    if (!GetShipDesign(design_id))
        throw std::invalid_argument("Attempted to construct a Ship with an invalid design id");

    if (!m_species_name.empty() && !GetSpecies(m_species_name))
        Logger().debugStream() << "Ship created with invalid species name: " << m_species_name;

    SetOwner(empire_id);

    UniverseObject::Init();

    AddMeter(METER_FUEL);
    AddMeter(METER_MAX_FUEL);
    AddMeter(METER_SHIELD);
    AddMeter(METER_MAX_SHIELD);
    AddMeter(METER_DETECTION);
    AddMeter(METER_STRUCTURE);
    AddMeter(METER_MAX_STRUCTURE);
    AddMeter(METER_BATTLE_SPEED);
    AddMeter(METER_STARLANE_SPEED);

    const std::vector<std::string>& part_names = Design()->Parts();
    for (std::size_t i = 0; i < part_names.size(); ++i) {
        if (part_names[i] != "") {
            const PartType* part = GetPartType(part_names[i]);
            if (!part) {
                Logger().errorStream() << "Ship::Ship couldn't get part with name " << part_names[i];
                continue;
            }

            switch (part->Class()) {
            case PC_SHORT_RANGE:
            case PC_POINT_DEFENSE: {
                m_part_meters[std::make_pair(METER_DAMAGE,              part->Name())];
                m_part_meters[std::make_pair(METER_ROF,                 part->Name())];
                m_part_meters[std::make_pair(METER_RANGE,               part->Name())];
                break;
            }
            case PC_MISSILES: {
                std::pair<std::size_t, std::size_t>& part_missiles =
                    m_missiles[part_names[i]];
                ++part_missiles.first;
                part_missiles.second += boost::get<LRStats>(part->Stats()).m_capacity;
                m_part_meters[std::make_pair(METER_DAMAGE,              part->Name())];
                m_part_meters[std::make_pair(METER_ROF,                 part->Name())];
                m_part_meters[std::make_pair(METER_RANGE,               part->Name())];
                m_part_meters[std::make_pair(METER_SPEED,               part->Name())];
                m_part_meters[std::make_pair(METER_STEALTH,             part->Name())];
                m_part_meters[std::make_pair(METER_STRUCTURE,           part->Name())];
                m_part_meters[std::make_pair(METER_CAPACITY,            part->Name())];
                break;
            }
            case PC_FIGHTERS: {
                std::pair<std::size_t, std::size_t>& part_fighters =
                    m_fighters[part_names[i]];
                ++part_fighters.first;
                part_fighters.second += boost::get<FighterStats>(part->Stats()).m_capacity;
                m_part_meters[std::make_pair(METER_ANTI_SHIP_DAMAGE,    part->Name())];
                m_part_meters[std::make_pair(METER_ANTI_FIGHTER_DAMAGE, part->Name())];
                m_part_meters[std::make_pair(METER_LAUNCH_RATE,         part->Name())];
                m_part_meters[std::make_pair(METER_FIGHTER_WEAPON_RANGE,part->Name())];
                m_part_meters[std::make_pair(METER_SPEED,               part->Name())];
                m_part_meters[std::make_pair(METER_STEALTH,             part->Name())];
                m_part_meters[std::make_pair(METER_STRUCTURE,           part->Name())];
                m_part_meters[std::make_pair(METER_DETECTION,           part->Name())];
                m_part_meters[std::make_pair(METER_CAPACITY,            part->Name())];
                break;
            }
            default:
                break;
            }
        }
    }
}

Ship* Ship::Clone(int empire_id) const {
    Visibility vis = GetUniverse().GetObjectVisibilityByEmpire(this->ID(), empire_id);

    if (!(vis >= VIS_BASIC_VISIBILITY && vis <= VIS_FULL_VISIBILITY))
        return 0;

    Ship* retval = new Ship();
    retval->Copy(TemporaryFromThis(), empire_id);
    return retval;
}

void Ship::Copy(TemporaryPtr<const UniverseObject> copied_object, int empire_id) {
    if (copied_object == this)
        return;
    TemporaryPtr<const Ship> copied_ship = boost::dynamic_pointer_cast<const Ship>(copied_object);
    if (!copied_ship) {
        Logger().errorStream() << "Ship::Copy passed an object that wasn't a Ship";
        return;
    }

    int copied_object_id = copied_object->ID();
    Visibility vis = GetUniverse().GetObjectVisibilityByEmpire(copied_object_id, empire_id);
    std::set<std::string> visible_specials = GetUniverse().GetObjectVisibleSpecialsByEmpire(copied_object_id, empire_id);

    UniverseObject::Copy(copied_object, vis, visible_specials);;

    if (vis >= VIS_BASIC_VISIBILITY) {
        if (this->m_fleet_id != copied_ship->m_fleet_id) {
            // as with other containers, removal from the old container is triggered by the contained Object; removal from System is handled by UniverseObject::Copy
            if (TemporaryPtr<Fleet> oldFleet = GetFleet(this->m_fleet_id)) 
                oldFleet->RemoveShip(this->ID());
            this->m_fleet_id =              copied_ship->m_fleet_id; // as with other containers (Systems), actual insertion into fleet ships set is handled by the fleet
        }

        if (vis >= VIS_PARTIAL_VISIBILITY) {
            if (this->Unowned())
                this->m_name =              copied_ship->m_name;

            this->m_design_id =             copied_ship->m_design_id;
            this->m_fighters =              copied_ship->m_fighters;
            this->m_missiles =              copied_ship->m_missiles;
            for (PartMeterMap::const_iterator it = copied_ship->m_part_meters.begin();
                 it != copied_ship->m_part_meters.end(); ++it)
            { this->m_part_meters[it->first]; }
            this->m_species_name =          copied_ship->m_species_name;

            if (vis >= VIS_FULL_VISIBILITY) {
                this->m_ordered_scrapped =          copied_ship->m_ordered_scrapped;
                this->m_ordered_colonize_planet_id= copied_ship->m_ordered_colonize_planet_id;
                this->m_ordered_invade_planet_id  = copied_ship->m_ordered_invade_planet_id;
                this->m_ordered_bombard_planet_id = copied_ship->m_ordered_bombard_planet_id;
                this->m_last_turn_active_in_combat= copied_ship->m_last_turn_active_in_combat;
                this->m_part_meters =               copied_ship->m_part_meters;
                this->m_produced_by_empire_id =     copied_ship->m_produced_by_empire_id;
            }
        }
    }
}

std::set<std::string> Ship::Tags() const {
    std::set<std::string> retval;

    const ShipDesign* design = GetShipDesign(m_design_id);
    if (!design)
        return retval;

    const HullType* hull = ::GetHullType(design->Hull());
    if (!hull)
        return retval;
    retval.insert(hull->Tags().begin(), hull->Tags().end());

    const std::vector<std::string>& parts = design->Parts();
    if (parts.empty())
        return retval;

    for (std::vector<std::string>::const_iterator part_it = parts.begin(); part_it != parts.end(); ++part_it) {
        if (const PartType* part = GetPartType(*part_it)) {
            retval.insert(part->Tags().begin(), part->Tags().end());
        }
    }

    return retval;
}

bool Ship::HasTag(const std::string& name) const {
    const ShipDesign* design = GetShipDesign(m_design_id);
    if (design) {
        // check hull for tag
        const HullType* hull = ::GetHullType(design->Hull());
        if (hull && hull->Tags().count(name))
            return true;

        // check parts for tag
        const std::vector<std::string>& parts = design->Parts();
        for (std::vector<std::string>::const_iterator part_it = parts.begin(); part_it != parts.end(); ++part_it) {
            const PartType* part = GetPartType(*part_it);
            if (part && part->Tags().count(name))
                return true;
        }
    }
    // check species for tag
    const Species* species = GetSpecies(SpeciesName());
    if (species && species->Tags().count(name))
        return true;

    return false;
}

UniverseObjectType Ship::ObjectType() const
{ return OBJ_SHIP; }

bool Ship::ContainedBy(int object_id) const {
    return object_id != INVALID_OBJECT_ID
        && (    object_id == m_fleet_id
            ||  object_id == this->SystemID());
}

std::string Ship::Dump() const {
    std::stringstream os;
    os << UniverseObject::Dump();
    os << " design id: " << m_design_id
       << " fleet id: " << m_fleet_id
       << " species name: " << m_species_name
       << " produced by empire id: " << m_produced_by_empire_id
       << " fighters: ";
    //typedef std::map<std::string, std::pair<std::size_t, std::size_t> > ConsumablesMap;
    for (ConsumablesMap::const_iterator it = m_fighters.begin(); it != m_fighters.end();) {
        const std::string& part_name = it->first;
        int num_consumables_available = it->second.second;
        ++it;
        os << part_name << ": " << num_consumables_available << (it == m_fighters.end() ? "" : ", ");
    }
    os << " missiles: ";
    for (ConsumablesMap::const_iterator it = m_missiles.begin(); it != m_missiles.end();) {
        const std::string& part_name = it->first;
        int num_consumables_available = it->second.second;
        ++it;
        os << part_name << ": " << num_consumables_available << (it == m_missiles.end() ? "" : ", ");
    }
    //typedef std::map<std::pair<MeterType, std::string>, Meter> PartMeters;
    os << " part meters: ";
    for (PartMeterMap::const_iterator it = m_part_meters.begin(); it != m_part_meters.end();) {
        const std::string part_name = it->first.second;
        MeterType meter_type = it->first.first;
        const Meter& meter = it->second;
        ++it;
        os << UserString(part_name) << " "
           << UserString(EnumToString(meter_type))
           << ": " << meter.Current() << "  ";
    }
    return os.str();
}

const ShipDesign* Ship::Design() const
{ return GetShipDesign(m_design_id); }

bool Ship::IsMonster() const {
    const ShipDesign* design = Design();
    if (design)
        return design->IsMonster();
    else
        return false;
}

bool Ship::IsArmed() const {
    const ShipDesign* design = Design();
    if (design)
        return design->IsArmed();
    else
        return false;
}

bool Ship::CanColonize() const {
    if (m_species_name.empty())
        return false;
    const Species* species = GetSpecies(m_species_name);
    if (!species)
        return false;
    if (!species->CanColonize())
        return false;

    const ShipDesign* design = Design();
    if (!design)
        return false;
    if (!design->CanColonize())
        return false;

    return true;
}

bool Ship::HasTroops() const {
    const ShipDesign* design = Design();
    return design && design->HasTroops();
}

bool Ship::CanBombard() const {
    const ShipDesign* design = Design();
    return design && design->CanBombard();
}

float Ship::Speed() const
{ return CurrentMeterValue(METER_STARLANE_SPEED); }

const std::string& Ship::PublicName(int empire_id) const {
    // Disclose real ship name only to fleet owners. Rationale: a player who
    // doesn't know the design for a particular ship can easily guess it if the
    // ship's name is "Scout"
    // An exception is made for unowned monsters.
    if (GetUniverse().AllObjectsVisible() || empire_id == ALL_EMPIRES || OwnedBy(empire_id) || (IsMonster() && Owner() == ALL_EMPIRES))
        return Name();
    const ShipDesign* design = Design();
    if (design)
        return design->Name();
    else if (IsMonster())
        return UserString("SM_MONSTER");
    else if (!Unowned())
        return UserString("FW_FOREIGN_SHIP");
    else if (Unowned() && GetVisibility(empire_id) > VIS_NO_VISIBILITY)
        return UserString("FW_ROGUE_SHIP");
    else
        return UserString("OBJ_SHIP");
}

TemporaryPtr<const UniverseObject> Ship::Accept(UniverseObjectVisitor* visitor) const
{ return UniverseObject::Accept(this, visitor); }

TemporaryPtr<UniverseObject> Ship::Accept(UniverseObjectVisitor* visitor)
{ return UniverseObject::Accept(this, visitor); }

float Ship::NextTurnCurrentMeterValue(MeterType type) const {
    //if (type == METER_FUEL) {
    //    // todo: consider fleet movement or being stationary, which may partly replenish fuel
    //    // todo: consider fleet passing through or being in a supplied system, which replenishes fuel
    //} else if (type == METER_SUPPLY) {
    //    // todo: consider fleet passing through or being in a supplied system, which replenishes supplies
    //}
    if (type == METER_SHIELD) {
        if (m_last_turn_active_in_combat >= CurrentTurn())
            return std::max(0.0f,
                            std::min(UniverseObject::GetMeter(METER_SHIELD)->Current(),
                                     UniverseObject::GetMeter(METER_MAX_SHIELD)->Current()));
        else
            return UniverseObject::GetMeter(METER_MAX_SHIELD)->Current();
    }

    return UniverseObject::NextTurnCurrentMeterValue(type);
}

const Meter* Ship::GetPartMeter(MeterType type, const std::string& part_name) const
{ return const_cast<Ship*>(this)->GetPartMeter(type, part_name); }

Meter* Ship::GetPartMeter(MeterType type, const std::string& part_name) {
    Meter* retval = 0;
    PartMeterMap::iterator it = m_part_meters.find(std::make_pair(type, part_name));
    if (it != m_part_meters.end())
        retval = &it->second;
    return retval;
}

float Ship::CurrentPartMeterValue(MeterType type, const std::string& part_name) const {
    if (const Meter* meter = GetPartMeter(type, part_name))
        return meter->Current();
    return 0.0f;
}

float Ship::InitialPartMeterValue(MeterType type, const std::string& part_name) const {
    if (const Meter* meter = GetPartMeter(type, part_name))
        return meter->Initial();
    return 0.0f;
}

float Ship::TotalWeaponsDamage() const {
    // sum up all individual weapons' attack strengths
    float total_attack = 0.0;
    std::vector<float> all_weapons_damage = AllWeaponsDamage();
    for (std::vector<float>::iterator it = all_weapons_damage.begin(); it != all_weapons_damage.end(); ++it)
        total_attack += *it;
    return total_attack;
}

std::vector<float> Ship::AllWeaponsDamage() const {
    std::vector<float> retval;

    const ShipDesign* design = GetShipDesign(m_design_id);
    if (!design)
        return retval;
    const std::vector<std::string>& parts = design->Parts();

    // for each weapon part, get its damage meter value
    for (std::vector<std::string>::const_iterator part_it = parts.begin();
         part_it != parts.end(); ++part_it)
    {
        const std::string& part_name = *part_it;
        const PartType* part = GetPartType(part_name);
        if (!part)
            continue;
        ShipPartClass part_class = part->Class();

        // get the attack power for each weapon part
        float part_attack = 0.0;

        if (part_class == PC_SHORT_RANGE || part_class == PC_POINT_DEFENSE || part_class == PC_MISSILES)
            part_attack = this->CurrentPartMeterValue(METER_DAMAGE, part_name);
        else if (part_class == PC_FIGHTERS)
            part_attack = this->CurrentPartMeterValue(METER_ANTI_SHIP_DAMAGE, part_name);

        if (part_attack > 0.0)
            retval.push_back(part_attack);
    }
    return retval;
}

void Ship::SetFleetID(int fleet_id) {
    if (m_fleet_id != fleet_id) {
        m_fleet_id = fleet_id;
        StateChangedSignal();
    }
}

void Ship::Resupply() {
    Meter* fuel_meter = UniverseObject::GetMeter(METER_FUEL);
    const Meter* max_fuel_meter = UniverseObject::GetMeter(METER_MAX_FUEL);
    if (!fuel_meter || !max_fuel_meter) {
        Logger().errorStream() << "Ship::Resupply couldn't get fuel meters!";
        return;
    }

    fuel_meter->SetCurrent(max_fuel_meter->Current());

    for (ConsumablesMap::iterator it = m_fighters.begin();
         it != m_fighters.end(); ++it)
    {
        const PartType* part_type = GetPartType(it->first);
        if (part_type)
            it->second.second = it->second.first *
                boost::get<FighterStats>(part_type->Stats()).m_capacity;
    }

    for (ConsumablesMap::iterator it = m_missiles.begin();
         it != m_missiles.end(); ++it)
    {
        const PartType* part_type = GetPartType(it->first);
        if (part_type)
            it->second.second = it->second.first *
                boost::get<LRStats>(part_type->Stats()).m_capacity;
    }
}

void Ship::AddFighters(const std::string& part_name, std::size_t n) {
    const PartType* part_type = GetPartType(part_name);
    if (!part_type) return;
    assert(m_fighters[part_name].second + n <=
           m_fighters[part_name].first *
           boost::get<FighterStats>(part_type->Stats()).m_capacity);
    m_fighters[part_name].second += n;
}

void Ship::RemoveFighters(const std::string& part_name, std::size_t n) {
    assert(m_fighters[part_name].second < n);
    m_fighters[part_name].second -= n;
}

void Ship::RemoveMissiles(const std::string& part_name, std::size_t n) {
    assert(m_missiles[part_name].second < n);
    m_missiles[part_name].second -= n;
}

void Ship::SetSpecies(const std::string& species_name) {
    if (!GetSpecies(species_name))
        Logger().errorStream() << "Ship::SetSpecies couldn't get species with name " << species_name;
    m_species_name = species_name;
}

void Ship::SetOrderedScrapped(bool b) {
    if (b == m_ordered_scrapped) return;
    m_ordered_scrapped = b;
    StateChangedSignal();
}

void Ship::SetColonizePlanet(int planet_id) {
    if (planet_id == m_ordered_colonize_planet_id) return;
    m_ordered_colonize_planet_id = planet_id;
    StateChangedSignal();
}

void Ship::ClearColonizePlanet()
{ SetColonizePlanet(INVALID_OBJECT_ID); }

void Ship::SetInvadePlanet(int planet_id) {
    if (planet_id == m_ordered_invade_planet_id) return;
    m_ordered_invade_planet_id = planet_id;
    StateChangedSignal();
}

void Ship::ClearInvadePlanet()
{ SetInvadePlanet(INVALID_OBJECT_ID); }

void Ship::SetBombardPlanet(int planet_id) {
    if (planet_id == m_ordered_bombard_planet_id) return;
    m_ordered_bombard_planet_id = planet_id;
    StateChangedSignal();
}

void Ship::ClearBombardPlanet()
{ SetBombardPlanet(INVALID_OBJECT_ID); }

void Ship::ResetTargetMaxUnpairedMeters() {
    UniverseObject::ResetTargetMaxUnpairedMeters();

    UniverseObject::GetMeter(METER_MAX_FUEL)->ResetCurrent();
    UniverseObject::GetMeter(METER_MAX_SHIELD)->ResetCurrent();
    UniverseObject::GetMeter(METER_MAX_STRUCTURE)->ResetCurrent();

    UniverseObject::GetMeter(METER_DETECTION)->ResetCurrent();
    UniverseObject::GetMeter(METER_BATTLE_SPEED)->ResetCurrent();
    UniverseObject::GetMeter(METER_STARLANE_SPEED)->ResetCurrent();

    for (PartMeterMap::iterator it = m_part_meters.begin(); it != m_part_meters.end(); ++it)
        it->second.ResetCurrent();
}

void Ship::PopGrowthProductionResearchPhase() {
    UniverseObject::PopGrowthProductionResearchPhase();

    UniverseObject::GetMeter(METER_SHIELD)->SetCurrent(Ship::NextTurnCurrentMeterValue(METER_SHIELD));

    StateChangedSignal();
}

void Ship::ClampMeters() {
    UniverseObject::ClampMeters();

    UniverseObject::GetMeter(METER_MAX_FUEL)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_FUEL)->ClampCurrentToRange(Meter::DEFAULT_VALUE, UniverseObject::GetMeter(METER_MAX_FUEL)->Current());
    UniverseObject::GetMeter(METER_MAX_SHIELD)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_SHIELD)->ClampCurrentToRange(Meter::DEFAULT_VALUE, UniverseObject::GetMeter(METER_MAX_SHIELD)->Current());
    UniverseObject::GetMeter(METER_MAX_STRUCTURE)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_STRUCTURE)->ClampCurrentToRange(Meter::DEFAULT_VALUE, UniverseObject::GetMeter(METER_MAX_STRUCTURE)->Current());

    UniverseObject::GetMeter(METER_DETECTION)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_BATTLE_SPEED)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_STARLANE_SPEED)->ClampCurrentToRange();

    for (PartMeterMap::iterator it = m_part_meters.begin(); it != m_part_meters.end(); ++it)
        it->second.ClampCurrentToRange();
}

  ////////////////////
 // Free Functions //
////////////////////
std::string NewMonsterName() {
    static std::vector<std::string> monster_names;
    static std::map<std::string, int> monster_names_used;
    if (monster_names.empty()) {
        // load monster names from stringtable
        std::list<std::string> monster_names_list;
        UserStringList("MONSTER_NAMES", monster_names_list);

        monster_names.reserve(monster_names_list.size());
        std::copy(monster_names_list.begin(), monster_names_list.end(), std::back_inserter(monster_names));
        if (monster_names.empty()) // safety check to ensure not leaving list empty in case of stringtable failure
            monster_names.push_back(UserString("MONSTER"));
    }

    // select name randomly from list
    int monster_name_index = RandSmallInt(0, static_cast<int>(monster_names.size()) - 1);
    std::string result = monster_names[monster_name_index];
    if (monster_names_used[result]++) {
        result += " " + RomanNumber(monster_names_used[result]);
    }
    return result;
}
