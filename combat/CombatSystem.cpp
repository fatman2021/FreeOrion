#include "CombatSystem.h"

#include "../universe/Universe.h"
#include "../util/MultiplayerCommon.h"
#include "../util/OptionsDB.h"
#include "../universe/Predicates.h"

#include "../universe/Planet.h"
#include "../universe/Fleet.h"
#include "../universe/Ship.h"
#include "../universe/ShipDesign.h"
#include "../universe/System.h"
#include "../Empire/Empire.h"

#include "../util/Logger.h"
#include "../util/Random.h"

#include "../server/ServerApp.h"
#include "../network/Message.h"

////////////////////////////////////////////////
// CombatInfo
////////////////////////////////////////////////
CombatInfo::CombatInfo() :
    turn(INVALID_GAME_TURN),
    system_id(INVALID_OBJECT_ID)
{}

CombatInfo::CombatInfo(int system_id_, int turn_) :
    turn(turn_),
    system_id(system_id_)
{
    TemporaryPtr<System> system = ::GetSystem(system_id);
    if (!system) {
        Logger().errorStream() << "CombatInfo constructed with invalid system id: " << system_id;
        return;
    }

    // add system to full / complete objects in combat - NOTE: changed from copy of system
    objects.Insert(system);


    // find ships and their owners in system
    std::vector<TemporaryPtr<Ship> > ships =
        Objects().FindObjects<Ship>(system->ShipIDs());

    for (std::vector<TemporaryPtr<Ship> >::const_iterator ship_it = ships.begin();
         ship_it != ships.end(); ++ship_it)
    {
        TemporaryPtr<Ship> ship = *ship_it;
        // add owner to empires that have assets in this battle
        empire_ids.insert(ship->Owner());

        objects.Insert(ship);
    }

    // find planets and their owners in system
    std::vector<TemporaryPtr<Planet> > planets =
        Objects().FindObjects<Planet>(system->PlanetIDs());

    for (std::vector<TemporaryPtr<Planet> >::const_iterator planet_it = planets.begin();
         planet_it != planets.end(); ++planet_it)
    {
        TemporaryPtr<Planet> planet = *planet_it;
        // if planet is populated, add owner to empires that have assets in this battle
        if (planet->CurrentMeterValue(METER_POPULATION) > 0.0)
            empire_ids.insert(planet->Owner());

        objects.Insert(planet);
    }

    // TODO: should buildings be considered separately?

    // now that all participants in the battle have been found, loop through
    // objects again to assemble each participant empire's latest
    // known information about all objects in this battle

    // system
    for (std::set<int>::const_iterator empire_it = empire_ids.begin();
         empire_it != empire_ids.end(); ++empire_it)
    {
        int empire_id = *empire_it;
        if (empire_id == ALL_EMPIRES)
            continue;
        empire_known_objects[empire_id].Insert(GetEmpireKnownSystem(system->ID(), empire_id));
    }

    // ships
    for (std::vector<TemporaryPtr<Ship> >::const_iterator it = ships.begin();
         it != ships.end(); ++it)
    {
        TemporaryPtr<Ship> ship = *it;
        int ship_id = ship->ID();
        TemporaryPtr<const Fleet> fleet = GetFleet(ship->FleetID());
        if (!fleet) {
            Logger().errorStream() << "CombatInfo::CombatInfo couldn't get fleet with id "
                                   << ship->FleetID() << " in system " << system->Name() << " (" << system_id << ")";
            continue;
        }

        for (std::set<int>::const_iterator empire_it = empire_ids.begin();
             empire_it != empire_ids.end(); ++empire_it)
        {
            int empire_id = *empire_it;
            if (empire_id == ALL_EMPIRES)
                continue;
            if (GetUniverse().GetObjectVisibilityByEmpire(ship_id, empire_id) >= VIS_BASIC_VISIBILITY ||
                   (fleet->Aggressive() &&
                       (empire_id == ALL_EMPIRES ||
                        fleet->Unowned() ||
                        Empires().GetDiplomaticStatus(empire_id, fleet->Owner()) == DIPLO_WAR)))
            { empire_known_objects[empire_id].Insert(GetEmpireKnownShip(ship->ID(), empire_id));}
        }
    }

    // planets
    for (std::vector<TemporaryPtr<Planet> >::const_iterator it = planets.begin();
         it != planets.end(); ++it)
    {
        TemporaryPtr<Planet> planet = *it;
        int planet_id = planet->ID();

        for (std::set<int>::const_iterator empire_it = empire_ids.begin(); empire_it != empire_ids.end(); ++empire_it) {
            int empire_id = *empire_it;
            if (empire_id == ALL_EMPIRES)
                continue;
            if (GetUniverse().GetObjectVisibilityByEmpire(planet_id, empire_id) >= VIS_BASIC_VISIBILITY) {
                empire_known_objects[empire_id].Insert(GetEmpireKnownPlanet(planet->ID(), empire_id));
            }
        }
    }

    // after battle is simulated, any changes to latest known or actual objects
    // will be copied back to the main Universe's ObjectMap and the Universe's
    // empire latest known objects ObjectMap - NOTE: Using the real thing now
}

TemporaryPtr<const System> CombatInfo::GetSystem() const
{ return this->objects.Object<System>(this->system_id); }

TemporaryPtr<System> CombatInfo::GetSystem()
{ return this->objects.Object<System>(this->system_id); }

void CombatInfo::GetEmpireIdsToSerialize(std::set<int>& filtered_empire_ids, int encoding_empire) const {
    if (encoding_empire == ALL_EMPIRES) {
        filtered_empire_ids = this->empire_ids;
        return;
    }
    // TODO: include only empires that the encoding empire knows are present in the system / battle
    filtered_empire_ids = this->empire_ids; // for now, include all empires involved in battle
}

void CombatInfo::GetObjectsToSerialize(ObjectMap& filtered_objects, int encoding_empire) const {
    if (&filtered_objects == &this->objects)
        return;

    filtered_objects.Clear();

    if (encoding_empire == ALL_EMPIRES) {
        filtered_objects = this->objects;
        return;
    }
    // TODO: include only objects that the encoding empire has visibility of
    //       using the combat visibility system.
    filtered_objects = this->objects;       // for now, include all objects in battle / system
}

void CombatInfo::GetEmpireKnownObjectsToSerialize(std::map<int, ObjectMap>& filtered_empire_known_objects, int encoding_empire) const {
    if (&filtered_empire_known_objects == &this->empire_known_objects)
        return;

    for (std::map<int, ObjectMap>::iterator it = filtered_empire_known_objects.begin(); it != filtered_empire_known_objects.end(); ++it)
        it->second.Clear();
    filtered_empire_known_objects.clear();

    if (encoding_empire == ALL_EMPIRES) {
        filtered_empire_known_objects = this->empire_known_objects;
        return;
    }

    // include only latest known objects for the encoding empire
    std::map<int, ObjectMap>::const_iterator it = this->empire_known_objects.find(encoding_empire);
    if (it != this->empire_known_objects.end()) {
        const ObjectMap& map = it->second;
        filtered_empire_known_objects[encoding_empire].Copy(map, ALL_EMPIRES);
    }
}

void CombatInfo::GetDamagedObjectsToSerialize(std::set<int>& filtered_damaged_objects, int encoding_empire) const {
    if (encoding_empire == ALL_EMPIRES) {
        filtered_damaged_objects = this->damaged_object_ids;
        return;
    }
    // TODO: decide if some filtering is needed for damaged objects... it may not be.
    filtered_damaged_objects = this->damaged_object_ids;
}

void CombatInfo::GetDestroyedObjectsToSerialize(std::set<int>& filtered_destroyed_objects, int encoding_empire) const {
    if (encoding_empire == ALL_EMPIRES) {
        filtered_destroyed_objects = this->destroyed_object_ids;
        return;
    }
    // TODO: decide if some filtering is needed for destroyed objects... it may not be.
    filtered_destroyed_objects = this->destroyed_object_ids;
}

void CombatInfo::GetDestroyedObjectKnowersToSerialize(std::map<int, std::set<int> >&
    filtered_destroyed_object_knowers, int encoding_empire) const
{
    if (encoding_empire == ALL_EMPIRES) {
        filtered_destroyed_object_knowers = this->destroyed_object_knowers;
        return;
    }
    // TODO: decide if some filtering is needed for which empires know about which
    // other empires know which objects have been destroyed during the battle.
    filtered_destroyed_object_knowers = this->destroyed_object_knowers;
}

void CombatInfo::GetCombatEventsToSerialize(std::vector<AttackEvent>& filtered_combat_events,
                                            int encoding_empire) const
{
    filtered_combat_events = this->combat_events;
}

void CombatInfo::GetEmpireObjectVisibilityToSerialize(Universe::EmpireObjectVisibilityMap&
                                                        filtered_empire_object_visibility,
                                                      int encoding_empire) const
{
    filtered_empire_object_visibility = this->empire_object_visibility;
}

////////////////////////////////////////////////
// AutoResolveCombat
////////////////////////////////////////////////
namespace {
    void AttackShipShip(TemporaryPtr<Ship> attacker, float damage, TemporaryPtr<Ship> target, CombatInfo& combat_info, int round) {
        if (!attacker || ! target) return;

        std::set<int>& damaged_object_ids = combat_info.damaged_object_ids;

        Meter* target_structure = target->UniverseObject::GetMeter(METER_STRUCTURE);
        if (!target_structure) {
            Logger().errorStream() << "couldn't get target structure or shield meter";
            return;
        }

        Meter* target_shield = target->UniverseObject::GetMeter(METER_SHIELD);
        float shield = (target_shield ? target_shield->Current() : 0.0f);

        Logger().debugStream() << "AttackShipShip: attacker: " << attacker->Name() << " damage: " << damage
                               << "  target: " << target->Name() << " shield: " << target_shield->Current()
                                                                 << " structure: " << target_structure->Current();

        damage = std::max(0.0f, damage - shield);

        if (damage > 0.0f) {
            target_structure->AddToCurrent(-damage);
            damaged_object_ids.insert(target->ID());
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << "COMBAT: Ship " << attacker->Name() << " (" << attacker->ID() << ") does " << damage << " damage to Ship " << target->Name() << " (" << target->ID() << ")";
        }

        AttackEvent attack(round, attacker->ID(), target->ID(), damage,
                           (target_structure->Current() <= 0.0f));
        combat_info.combat_events.push_back(attack);

        attacker->SetLastTurnActiveInCombat(CurrentTurn());
        target->SetLastTurnActiveInCombat(CurrentTurn());
    }

    void AttackShipPlanet(TemporaryPtr<Ship> attacker, float damage, TemporaryPtr<Planet> target, CombatInfo& combat_info, int round) {
        if (!attacker || ! target) return;
        if (damage <= 0.0f)
            return;

        std::set<int>& damaged_object_ids = combat_info.damaged_object_ids;

        const ShipDesign* attacker_design = attacker->Design();
        if (!attacker_design)
            return;

        Meter* target_shield = target->GetMeter(METER_SHIELD);
        Meter* target_defense = target->UniverseObject::GetMeter(METER_DEFENSE);
        Meter* target_construction = target->UniverseObject::GetMeter(METER_CONSTRUCTION);
        if (!target_shield) {
            Logger().errorStream() << "couldn't get target shield meter";
            return;
        }
        if (!target_defense) {
            Logger().errorStream() << "couldn't get target defense meter";
            return;
        }
        if (!target_construction) {
            Logger().errorStream() << "couldn't get target construction meter";
            return;
        }

        if (GetOptionsDB().Get<bool>("verbose-logging")) {
            Logger().debugStream() << "AttackShipPlanet: attacker: " << attacker->Name() << " damage: " << damage
                               << "\ntarget: " << target->Name() << " shield: " << target_shield->Current()
                                                                 << " defense: " << target_defense->Current()
                                                                 << " infra: " << target_construction->Current();
        }

        // damage shields, limited by shield current value and damage amount.
        // remaining damage, if any, above shield current value goes to defense.
        // remaining damage, if any, above defense current value goes to construction
        float shield_damage = std::min(target_shield->Current(), damage);
        float defense_damage = 0.0f;
        float construction_damage = 0.0f;
        if (shield_damage >= target_shield->Current())
            defense_damage = std::min(target_defense->Current(), damage - shield_damage);

        if (shield_damage > 0 || defense_damage > 0 || construction_damage > 0)
            damaged_object_ids.insert(target->ID());

        if (defense_damage >= target_defense->Current())
            construction_damage = std::min(target_construction->Current(), damage - shield_damage - defense_damage);

        if (shield_damage >= 0) {
            target_shield->AddToCurrent(-shield_damage);
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << "COMBAT: Ship " << attacker->Name() << " (" << attacker->ID() << ") does " << shield_damage << " shield damage to Planet " << target->Name() << " (" << target->ID() << ")";
        }
        if (defense_damage >= 0) {
            target_defense->AddToCurrent(-defense_damage);
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << "COMBAT: Ship " << attacker->Name() << " (" << attacker->ID() << ") does " << defense_damage << " defense damage to Planet " << target->Name() << " (" << target->ID() << ")";
        }
        if (construction_damage >= 0) {
            target_construction->AddToCurrent(-construction_damage);
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << "COMBAT: Ship " << attacker->Name() << " (" << attacker->ID() << ") does " << construction_damage << " instrastructure damage to Planet " << target->Name() << " (" << target->ID() << ")";
        }

        AttackEvent attack(round, attacker->ID(), target->ID(), damage, false);
        combat_info.combat_events.push_back(attack);

        attacker->SetLastTurnActiveInCombat(CurrentTurn());
        target->SetLastTurnAttackedByShip(CurrentTurn());
    }

    void AttackPlanetShip(TemporaryPtr<Planet> attacker, TemporaryPtr<Ship> target, CombatInfo& combat_info, int round) {
        if (!attacker || ! target) return;

        float damage = 0.0f;
        const Meter* attacker_damage = attacker->UniverseObject::GetMeter(METER_DEFENSE);
        if (attacker_damage)
            damage = attacker_damage->Current();   // planet "Defense" meter is actually its attack power

        std::set<int>& damaged_object_ids = combat_info.damaged_object_ids;

        Meter* target_structure = target->UniverseObject::GetMeter(METER_STRUCTURE);
        if (!target_structure) {
            Logger().errorStream() << "couldn't get target structure or shield meter";
            return;
        }

        Meter* target_shield = target->UniverseObject::GetMeter(METER_SHIELD);
        float shield = (target_shield ? target_shield->Current() : 0.0f);

        if (GetOptionsDB().Get<bool>("verbose-logging")) {
            Logger().debugStream() << "AttackPlanetShip: attacker: " << attacker->Name() << " damage: " << damage
                               << "  target: " << target->Name() << " shield: " << target_shield->Current()
                                                                 << " structure: " << target_structure->Current();
        }

        damage = std::max(0.0f, damage - shield);

        if (damage > 0.0f) {
            target_structure->AddToCurrent(-damage);
            damaged_object_ids.insert(target->ID());
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << "COMBAT: Planet " << attacker->Name() << " (" << attacker->ID() << ") does " << damage << " damage to Ship " << target->Name() << " (" << target->ID() << ")";
        }

        AttackEvent attack(round, attacker->ID(), target->ID(), damage,
                           (target_structure->Current() <= 0.0f));
        combat_info.combat_events.push_back(attack);

        target->SetLastTurnActiveInCombat(CurrentTurn());
    }

    void AttackPlanetPlanet(TemporaryPtr<Planet> attacker, TemporaryPtr<Planet> target, CombatInfo& combat_info, int round) {
        if (GetOptionsDB().Get<bool>("verbose-logging"))
            Logger().debugStream() << "AttackPlanetPlanet does nothing!";
        // intentionally left empty
    }

    bool ObjectCanBeAttacked(TemporaryPtr<const UniverseObject> obj) {
        if (!obj)
            return false;
        UniverseObjectType obj_type = obj->ObjectType();
        if (obj_type == OBJ_SHIP) {
            return true;
        } else if (obj_type == OBJ_PLANET) {
            if (!obj->Unowned() || obj->CurrentMeterValue(METER_POPULATION) > 0.0f)
                return true;
            else
                return false;
        } else {
            return false;
        }
    }

    bool ObjectAttackableByEmpire(TemporaryPtr<const UniverseObject> obj, int empire_id) {
        if (obj->OwnedBy(empire_id))
            return false;
        if (obj->Unowned() && empire_id == ALL_EMPIRES)
            return false;

        if (empire_id != ALL_EMPIRES && !obj->Unowned() &&
            Empires().GetDiplomaticStatus(empire_id, obj->Owner()) != DIPLO_WAR)
        { return false; }

        if (GetUniverse().GetObjectVisibilityByEmpire(obj->ID(), empire_id) < VIS_BASIC_VISIBILITY)
            return false;

        return ObjectCanBeAttacked(obj);
    }

    // monsters / natives can attack any planet, but can only attack
    // visible ships or ships that are in aggressive fleets
    bool ObjectAttackableByMonsters(TemporaryPtr<const UniverseObject> obj, float monster_detection = 0.0) {
        if (obj->Unowned())
            return false;

        //Logger().debugStream() << "Testing if object " << obj->Name() << " is attackable by monsters";

        UniverseObjectType obj_type = obj->ObjectType();
        if (obj_type == OBJ_PLANET) {
            return true;
        } else if (obj_type == OBJ_SHIP) {
            float stealth = obj->CurrentMeterValue(METER_STEALTH);
            if (monster_detection >= stealth)
                return true;
        }
        //Logger().debugStream() << "... ... is NOT attackable by monsters";
        return false;
    }

    bool ObjectCanAttack(TemporaryPtr<const UniverseObject> obj) {
        if (TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(obj)) {
            return ship->IsArmed();
        } else if (TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(obj)) {
            return planet->CurrentMeterValue(METER_DEFENSE) > 0.0;
        } else {
            return false;
        }
    }

    struct PartAttackInfo {
        PartAttackInfo(ShipPartClass part_class_, const std::string& part_name_, float part_attack_) :
            part_class(part_class_),
            part_type_name(part_name_),
            part_attack(part_attack_)
        {}
        ShipPartClass   part_class;
        std::string     part_type_name;
        float           part_attack;
    };

    std::vector<PartAttackInfo> ShipWeaponsStrengths(TemporaryPtr<const Ship> ship) {
        std::vector<PartAttackInfo> retval;
        if (!ship)
            return retval;
        const ShipDesign* design = GetShipDesign(ship->DesignID());
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
                part_attack = ship->CurrentPartMeterValue(METER_DAMAGE, part_name);
            else if (part_class == PC_FIGHTERS)
                part_attack = ship->CurrentPartMeterValue(METER_ANTI_SHIP_DAMAGE, part_name);

            if (part_attack > 0.0)
                retval.push_back(PartAttackInfo(part_class, part_name, part_attack));
        }
        return retval;
    }
}

void AutoResolveCombat(CombatInfo& combat_info) {
    if (combat_info.objects.Empty())
        return;

    TemporaryPtr<const System> system = combat_info.objects.Object<System>(combat_info.system_id);
    if (!system)
        Logger().errorStream() << "AutoResolveCombat couldn't get system with id " << combat_info.system_id;
    else
        Logger().debugStream() << "AutoResolveCombat at " << system->Name();

    if (GetOptionsDB().Get<bool>("verbose-logging")) {
        Logger().debugStream() << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%";
        Logger().debugStream() << "AutoResolveCombat objects before resolution: " << combat_info.objects.Dump();
    }

    // reasonably unpredictable but reproducible random seeding
    const int base_seed = combat_info.objects.begin()->ID() + CurrentTurn();


    // compile list of valid objects to attack or be attacked in this combat
    std::set<int> valid_target_object_ids;                          // all objects that can be attacked
    std::set<int> valid_attacker_object_ids;                        // all objects that can attack
    std::map<int, std::set<int> > empire_valid_attacker_object_ids; // objects that can attack that each empire owns
    float monster_detection = 0.0;

    for (ObjectMap::iterator<> it = combat_info.objects.begin(); it != combat_info.objects.end(); ++it) {
        TemporaryPtr<const UniverseObject> obj = *it;
        //Logger().debugStream() << "Considerting object " << obj->Name() << " owned by " << obj->Owner();
        if (ObjectCanAttack(obj)) {
            //Logger().debugStream() << "... can attack";
            valid_attacker_object_ids.insert(it->ID());
            empire_valid_attacker_object_ids[obj->Owner()].insert(it->ID());
        }
        if (ObjectCanBeAttacked(obj)) {
            //Logger().debugStream() << "... can be attacked";
            valid_target_object_ids.insert(it->ID());
        }
        if (obj->Unowned() && obj->ObjectType() == OBJ_SHIP)
            monster_detection = std::max(monster_detection, obj->CurrentMeterValue(METER_DETECTION));
    }


    // map from empire to set of IDs of objects that empire's objects
    // could potentially target.
    std::map<int, std::set<int> > empire_valid_target_object_ids;   // objects that each empire can attack
    for (std::set<int>::const_iterator target_it = valid_target_object_ids.begin();
         target_it != valid_target_object_ids.end(); ++target_it)
    {
        int object_id = *target_it;
        TemporaryPtr<const UniverseObject> obj = combat_info.objects.Object(object_id);
        //Logger().debugStream() << "Considering attackability of object " << obj->Name() << " owned by " << obj->Owner();

        // for all empires, can they attack this object?
        for (std::set<int>::const_iterator empire_it = combat_info.empire_ids.begin();
             empire_it != combat_info.empire_ids.end(); ++empire_it)
        {
            int attacking_empire_id = *empire_it;
            if (attacking_empire_id == ALL_EMPIRES) {
                if (ObjectAttackableByMonsters(obj, monster_detection)) {
                    //Logger().debugStream() << "object: " << obj->Name() << " attackable by monsters";
                    empire_valid_target_object_ids[ALL_EMPIRES].insert(object_id);
                }

            } else {
                // call function to find if empires can attack objects...
                if (ObjectAttackableByEmpire(obj, attacking_empire_id)) {
                    //Logger().debugStream() << "object: " << obj->Name() << " attackable by empire " << attacking_empire_id;
                    empire_valid_target_object_ids[attacking_empire_id].insert(object_id);
                }
            }
        }
    }


    // Each combat "round" a randomly-selected object in the battle attacks
    // something, if it is able to do so.  The number of rounds scales with the
    // number of objects, so the total actions per object is independent of
    // number of objects in the battle
    const int NUM_COMBAT_ROUNDS = 3*valid_attacker_object_ids.size();

    for (int round = 1; round <= NUM_COMBAT_ROUNDS; ++round) {
        Seed(base_seed + round);    // ensure each combat round produces different results

        // ensure something can attack and something can be attacked
        if (valid_attacker_object_ids.empty()) {
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << "Nothing left can attack; combat over";
            break;
        }
        if (empire_valid_target_object_ids.empty()) {
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << "Nothing left can be attacked; combat over";
            break;
        }
        // empires may have valid targets, but nothing to attack with.  If all
        // empires have no attackers or no valid targers, combat is over
        bool someone_can_attack_something = false;
        for (std::map<int, std::set<int> >::const_iterator attacker_it = empire_valid_attacker_object_ids.begin();
             attacker_it != empire_valid_attacker_object_ids.end(); ++attacker_it)
        {
            if (empire_valid_target_object_ids.find(attacker_it->first) != empire_valid_target_object_ids.end()) {
                someone_can_attack_something = true;
                break;
            }
        }
        if (!someone_can_attack_something) {
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << "No empire has valid targets and something to attack with; combat over.";
            break;
        }

        if (GetOptionsDB().Get<bool>("verbose-logging"))
            Logger().debugStream() << "Combat at " << system->Name() << " (" << combat_info.system_id << ") Round " << round;

        // select attacking object in battle
        int attacker_idx = RandInt(0, valid_attacker_object_ids.size() - 1);
        if (GetOptionsDB().Get<bool>("verbose-logging"))
            Logger().debugStream() << "Battle round " << round << " attacker index: " << attacker_idx << " of " << valid_attacker_object_ids.size() - 1;
        std::set<int>::const_iterator attacker_it = valid_attacker_object_ids.begin();
        std::advance(attacker_it, attacker_idx);
        assert(attacker_it != valid_attacker_object_ids.end());
        int attacker_id = *attacker_it;

        TemporaryPtr<UniverseObject> attacker = combat_info.objects.Object(attacker_id);
        if (!attacker) {
            Logger().errorStream() << "AutoResolveCombat couldn't get object with id " << attacker_id;
            continue;
        }
        if (GetOptionsDB().Get<bool>("verbose-logging"))
            Logger().debugStream() << "Attacker: " << attacker->Name();


        TemporaryPtr<Ship> attack_ship = boost::dynamic_pointer_cast<Ship>(attacker);
        TemporaryPtr<Planet> attack_planet = boost::dynamic_pointer_cast<Planet>(attacker);

        // loop over weapons of attacking object.  each gets a shot at a
        // randomly selected target object
        std::vector<PartAttackInfo> weapons;

        if (attack_ship) {
            weapons = ShipWeaponsStrengths(attack_ship);
            for (std::vector<PartAttackInfo>::const_iterator part_it = weapons.begin();
                 part_it != weapons.end(); ++part_it)
            {
                if (GetOptionsDB().Get<bool>("verbose-logging")) {
                    Logger().debugStream() << "weapon: " << part_it->part_type_name
                                           << " attack: " << part_it->part_attack;
                }
            }
        } else if (attack_planet) { // treat planet defenses as short range
            weapons.push_back(PartAttackInfo(PC_SHORT_RANGE, "", attack_planet->CurrentMeterValue(METER_DEFENSE)));
        }

        if (weapons.empty()) {
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << "no weapons' can't attack";
            continue;   // no ability to attack!
        }

        for (std::vector<PartAttackInfo>::const_iterator weapon_it = weapons.begin();
             weapon_it != weapons.end(); ++weapon_it)
        {
            // select object from valid targets for this object's owner   TODO: with this weapon...
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << "Attacking with weapon " << weapon_it->part_type_name << " with power " << weapon_it->part_attack;

            // get valid targets set for attacker owner.  need to do this for
            // each weapon that is attacking, as the previous shot might have
            // destroyed something
            int attacker_owner_id = attacker->Owner();

            std::map<int, std::set<int> >::iterator target_vec_it = empire_valid_target_object_ids.find(attacker_owner_id);
            if (target_vec_it == empire_valid_target_object_ids.end() || target_vec_it->second.empty()) {
                if (GetOptionsDB().Get<bool>("verbose-logging"))
                    Logger().debugStream() << "No targets for attacker with id: " << attacker_owner_id;
                break;
            }

            const std::set<int>& valid_target_ids = target_vec_it->second;

            // DEBUG
            std::string id_list;
            for (std::set<int>::const_iterator target_it = valid_target_ids.begin();
                    target_it != valid_target_ids.end(); ++target_it)
            { id_list += boost::lexical_cast<std::string>(*target_it) + " "; }

            if (GetOptionsDB().Get<bool>("verbose-logging")) { 
                Logger().debugStream() << "Valid targets for attacker with id: " << attacker_owner_id
                                    << " owned by empire: " << attacker_owner_id
                                    << " :  " << id_list;
            }
            // END DEBUG


            // select target object
            int target_idx = RandInt(0, valid_target_ids.size() - 1);
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << " ... target index: " << target_idx << " of " << valid_target_ids.size() - 1;
            std::set<int>::const_iterator target_it = valid_target_ids.begin();
            std::advance(target_it, target_idx);
            assert(target_it != valid_target_ids.end());
            int target_id = *target_it;

            TemporaryPtr<UniverseObject> target = combat_info.objects.Object(target_id);
            if (!target) {
                Logger().errorStream() << "AutoResolveCombat couldn't get target object with id " << target_id;
                continue;
            }
            if (GetOptionsDB().Get<bool>("verbose-logging"))
                Logger().debugStream() << "Target: " << target->Name();


            // do actual attacks, and mark attackers as valid targets for attacked object's owners
            if (attack_ship) {
                if (TemporaryPtr<Ship> target_ship = boost::dynamic_pointer_cast<Ship>(target)) {
                    AttackShipShip(attack_ship, weapon_it->part_attack, target_ship, combat_info, round);
                    empire_valid_target_object_ids[target_ship->Owner()].insert(attacker_id);
                } else if (TemporaryPtr<Planet> target_planet = boost::dynamic_pointer_cast<Planet>(target)) {
                    AttackShipPlanet(attack_ship, weapon_it->part_attack,  target_planet, combat_info, round);
                    empire_valid_target_object_ids[target_planet->Owner()].insert(attacker_id);
                }
            } else if (attack_planet) {
                if (TemporaryPtr<Ship> target_ship = boost::dynamic_pointer_cast<Ship>(target)) {
                    AttackPlanetShip(attack_planet, target_ship, combat_info, round);
                    empire_valid_target_object_ids[target_ship->Owner()].insert(attacker_id);
                } else if (TemporaryPtr<Planet> target_planet = boost::dynamic_pointer_cast<Planet>(target)) {
                    AttackPlanetPlanet(attack_planet, target_planet, combat_info, round);
                    empire_valid_target_object_ids[target_planet->Owner()].insert(attacker_id);
                }
            }


            // check for destruction of target object
            if (target->ObjectType() == OBJ_SHIP) {
                if (target->CurrentMeterValue(METER_STRUCTURE) <= 0.0) {
                    if (GetOptionsDB().Get<bool>("verbose-logging"))
                        Logger().debugStream() << "!! Target Ship is destroyed!";
                    // object id destroyed
                    combat_info.destroyed_object_ids.insert(target_id);
                    // all empires in battle know object was destroyed
                    for (std::set<int>::const_iterator it = combat_info.empire_ids.begin();
                         it != combat_info.empire_ids.end(); ++it)
                    {
                        int empire_id = *it;
                        if (empire_id != ALL_EMPIRES) {
                            if (GetOptionsDB().Get<bool>("verbose-logging"))
                                Logger().debugStream() << "Giving knowledge of destroyed object " << target_id << " to empire " << empire_id;
                            combat_info.destroyed_object_knowers[empire_id].insert(target_id);
                        }
                    }

                    // remove destroyed ship's ID from lists of valid attackers and targets
                    valid_attacker_object_ids.erase(target_id);
                    valid_target_object_ids.erase(target_id);   // probably not necessary as this set isn't used in this loop

                    for (target_vec_it = empire_valid_target_object_ids.begin();
                         target_vec_it != empire_valid_target_object_ids.end(); ++target_vec_it)
                    { target_vec_it->second.erase(target_id); }

                    for (target_vec_it = empire_valid_attacker_object_ids.begin();
                         target_vec_it != empire_valid_attacker_object_ids.end(); ++target_vec_it)
                    { target_vec_it->second.erase(target_id); } // TODO: only erase from owner's entry in this list
                }

            } else if (target->ObjectType() == OBJ_PLANET) {
                if (!ObjectCanAttack(target) && valid_attacker_object_ids.find(target_id)!=valid_attacker_object_ids.end()) {
                    if (GetOptionsDB().Get<bool>("verbose-logging"))
                        Logger().debugStream() << "!! Target Planet defenses knocked out, can no longer attack";
                    // remove disabled planet's ID from lists of valid attackers
                    valid_attacker_object_ids.erase(target_id);
                }
                if (target->CurrentMeterValue(METER_SHIELD) <= 0.0 &&
                    target->CurrentMeterValue(METER_DEFENSE) <= 0.0 &&
                    target->CurrentMeterValue(METER_CONSTRUCTION) <= 0.0)
                {
                    if (GetOptionsDB().Get<bool>("verbose-logging"))
                        Logger().debugStream() << "!! Target Planet is entirely knocked out of battle";

                    // remove disabled planet's ID from lists of valid targets
                    valid_target_object_ids.erase(target_id);   // probably not necessary as this set isn't used in this loop

                    for (target_vec_it = empire_valid_target_object_ids.begin();
                         target_vec_it != empire_valid_target_object_ids.end(); ++target_vec_it)
                    { target_vec_it->second.erase(target_id); }

                    for (target_vec_it = empire_valid_attacker_object_ids.begin();
                         target_vec_it != empire_valid_attacker_object_ids.end(); ++target_vec_it)
                    { target_vec_it->second.erase(target_id); } // TODO: only erase from owner's entry in this list
                }
            }

            // check if any empire has no remaining target or attacker objects.
            // If so, remove that empire's entry
            std::map<int, std::set<int> > temp = empire_valid_target_object_ids;
            for (target_vec_it = empire_valid_target_object_ids.begin();
                 target_vec_it != empire_valid_target_object_ids.end(); ++target_vec_it)
            {
                if (target_vec_it->second.empty()) {
                    temp.erase(target_vec_it->first);
                    if (GetOptionsDB().Get<bool>("verbose-logging"))
                        Logger().debugStream() << "No valid targets left for empire with id: " << target_vec_it->first;
                }
            }
            empire_valid_target_object_ids = temp;

            temp = empire_valid_attacker_object_ids;
            for (target_vec_it = empire_valid_attacker_object_ids.begin();
                 target_vec_it != empire_valid_attacker_object_ids.end(); ++target_vec_it)
            {
                if (target_vec_it->second.empty()) {
                    temp.erase(target_vec_it->first);
                    if (GetOptionsDB().Get<bool>("verbose-logging"))
                        Logger().debugStream() << "No valid attacking objects left for empire with id: " << target_vec_it->first;
                }
            }
            empire_valid_attacker_object_ids = temp;
        } // end for over weapons
    } // end for over combat arounds

    // ensure every participant knows what happened.
    // TODO: assemble list of objects to copy for each empire.  this should
    //       include objects the empire already knows about with standard
    //       visibility system, and also any objects the empire knows are
    //       destroyed during this combat...
    for (std::map<int, ObjectMap>::iterator it = combat_info.empire_known_objects.begin();
         it != combat_info.empire_known_objects.end(); ++it)
    { it->second.Copy(combat_info.objects); }

    if (GetOptionsDB().Get<bool>("verbose-logging")) {
        Logger().debugStream() << "AutoResolveCombat objects after resolution: " << combat_info.objects.Dump();

        Logger().debugStream() << "combat event log:";
        for (std::vector<AttackEvent>::const_iterator it = combat_info.combat_events.begin();
            it != combat_info.combat_events.end(); ++it)
        {
            Logger().debugStream() << "rnd: " << it->round << " : "
                                << it->attacker_id << " -> " << it->target_id << " : "
                                << it->damage
                                << (it->target_destroyed ? " (destroyed)" : "");
        }
    }
}
