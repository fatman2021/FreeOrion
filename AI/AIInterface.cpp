#include "AIInterface.h"

#include "../util/MultiplayerCommon.h"
#include "../network/ClientNetworking.h"
#include "../client/AI/AIClientApp.h"

#include "../universe/Universe.h"
#include "../universe/UniverseObject.h"
#include "../universe/Planet.h"
#include "../universe/Fleet.h"
#include "../universe/Ship.h"
#include "../universe/System.h"
#include "../universe/Tech.h"
#include "../Empire/Empire.h"

#include "../util/Logger.h"
#include "../util/Order.h"
#include "../util/OrderSet.h"

#include <boost/timer.hpp>

#include <stdexcept>
#include <string>
#include <map>

//////////////////////////////////
//          AI Base             //
//////////////////////////////////
AIBase::~AIBase()
{}

void AIBase::GenerateOrders()
{ AIInterface::DoneTurn(); }

void AIBase::GenerateCombatSetupOrders(const CombatData& combat_data)
{ AIInterface::CombatSetup(); }

void AIBase::GenerateCombatOrders(const CombatData& combat_data)
{ AIInterface::DoneCombatTurn(); }

void AIBase::HandleChatMessage(int sender_id, const std::string& msg)
{}

void AIBase::HandleDiplomaticMessage(const DiplomaticMessage& msg)
{}

void AIBase::HandleDiplomaticStatusUpdate(const DiplomaticStatusUpdateInfo& u)
{}

void AIBase::StartNewGame()
{}

void AIBase::ResumeLoadedGame(const std::string& save_state_string)
{}

const std::string& AIBase::GetSaveStateString() {
    static std::string default_state_string("AIBase default save state string");
    Logger().debugStream() << "AIBase::GetSaveStateString() returning: " << default_state_string;
    return default_state_string;
}

void AIBase::SetAggression(int aggr)
{ m_aggression = aggr; }


//////////////////////////////////
//        AI Interface          //
//////////////////////////////////
namespace {
    // stuff used in AIInterface, but not needed to be visible outside this file

    // start of turn initialization for meters
    void InitMeterEstimatesAndDiscrepancies() {
        Universe& universe = AIClientApp::GetApp()->GetUniverse();
        universe.InitMeterEstimatesAndDiscrepancies();
    }
}

namespace AIInterface {
    const std::string& PlayerName()
    { return AIClientApp::GetApp()->PlayerName(); }

    const std::string& PlayerName(int player_id) {
        const std::map<int, PlayerInfo>& players = AIClientApp::GetApp()->Players();
        std::map<int, PlayerInfo>::const_iterator it = players.find(player_id);
        if (it != players.end())
            return it->second.name;
        else {
            Logger().debugStream() << "AIInterface::PlayerName(" << boost::lexical_cast<std::string>(player_id) << ") - passed an invalid player_id";
            throw std::invalid_argument("AIInterface::PlayerName : given invalid player_id");
        }
    }

    int PlayerID()
    { return AIClientApp::GetApp()->PlayerID(); }

    int EmpireID()
    { return AIClientApp::GetApp()->EmpireID(); }

    int PlayerEmpireID(int player_id) {
        const std::map<int, PlayerInfo>& players = AIClientApp::GetApp()->Players();
        for (std::map<int, PlayerInfo>::const_iterator it = players.begin(); it != players.end(); ++it) {
            if (it->first == player_id)
                return it->second.empire_id;
        }
        return ALL_EMPIRES; // default invalid value
    }

    std::vector<int>  AllEmpireIDs() {
        const std::map<int, PlayerInfo>& players = AIClientApp::GetApp()->Players();
        std::vector<int> empire_ids;
        for (std::map<int, PlayerInfo>::const_iterator it = players.begin(); it != players.end(); ++it)
            empire_ids.push_back(it->second.empire_id);
        return empire_ids;
    }

    const Empire* GetEmpire()
    { return AIClientApp::GetApp()->Empires().Lookup(AIClientApp::GetApp()->EmpireID()); }

    const Empire* GetEmpire(int empire_id)
    { return AIClientApp::GetApp()->Empires().Lookup(empire_id); }

    int EmpirePlayerID(int empire_id) {
        int player_id = AIClientApp::GetApp()->EmpirePlayerID(empire_id);
        if (-1 == player_id)
            Logger().debugStream() << "AIInterface::EmpirePlayerID(" << boost::lexical_cast<std::string>(empire_id) << ") - passed an invalid empire_id";
        return player_id;
    }

    std::vector<int> AllPlayerIDs() {
        const std::map<int, PlayerInfo>& players = AIClientApp::GetApp()->Players();
        std::vector<int> player_ids;
        for (std::map<int, PlayerInfo>::const_iterator it = players.begin(); it != players.end(); ++it)
            player_ids.push_back(it->first);
        return player_ids;
    }

    bool PlayerIsAI(int player_id)
    { return AIClientApp::GetApp()->GetPlayerClientType(player_id) == Networking::CLIENT_TYPE_AI_PLAYER; }

    bool PlayerIsHost(int player_id) {
        const std::map<int, PlayerInfo>& players = AIClientApp::GetApp()->Players();
        std::map<int, PlayerInfo>::const_iterator it = players.find(player_id);
        if (it == players.end())
            return false;
        return it->second.host;
    }

    const Universe& GetUniverse()
    { return AIClientApp::GetApp()->GetUniverse(); }

    const Tech* GetTech(const std::string& tech_name)
    { return TechManager::GetTechManager().GetTech(tech_name); }

    int CurrentTurn()
    { return AIClientApp::GetApp()->CurrentTurn(); }

    const GalaxySetupData&  GetGalaxySetupData()
    { return AIClientApp::GetApp()->GetGalaxySetupData(); }

    void InitTurn() {
        boost::timer turn_init_timer;

        InitMeterEstimatesAndDiscrepancies();
        UpdateMeterEstimates();
        UpdateResourcePools();

        Logger().debugStream() << "AIInterface::InitTurn time: " << (turn_init_timer.elapsed() * 1000.0);
    }

    void UpdateMeterEstimates(bool pretend_unowned_planets_owned_by_this_ai_empire) {
        std::vector<TemporaryPtr<Planet> > unowned_planets;
        int player_id = -1;
        Universe& universe = AIClientApp::GetApp()->GetUniverse();
        if (pretend_unowned_planets_owned_by_this_ai_empire) {
            // add this player ownership to all planets that the player can see but which aren't currently colonized.
            // this way, any effects the player knows about that would act on those planets if the player colonized them
            // include those planets in their scope.  This lets effects from techs the player knows alter the max
            // population of planet that is displayed to the player, even if those effects have a condition that causes
            // them to only act on planets the player owns (so as to not improve enemy planets if a player reseraches a
            // tech that should only benefit him/herself)
            player_id = AIInterface::PlayerID();

            // get all planets the player knows about that aren't yet colonized (aren't owned by anyone).  Add this
            // the current player's ownership to all, while remembering which planets this is done to
            std::vector<TemporaryPtr<Planet> > all_planets = universe.Objects().FindObjects<Planet>();
            universe.InhibitUniverseObjectSignals(true);
            for (std::vector<TemporaryPtr<Planet> >::iterator it = all_planets.begin(); it != all_planets.end(); ++it) {
                 TemporaryPtr<Planet> planet = *it;
                 if (planet->Unowned()) {
                     unowned_planets.push_back(planet);
                     planet->SetOwner(player_id);
                 }
            }
        }

        // update meter estimates with temporary ownership
        universe.UpdateMeterEstimates();

        if (pretend_unowned_planets_owned_by_this_ai_empire) {
            // remove temporary ownership added above
            for (std::vector<TemporaryPtr<Planet> >::iterator it = unowned_planets.begin(); it != unowned_planets.end(); ++it)
                (*it)->SetOwner(ALL_EMPIRES);
            universe.InhibitUniverseObjectSignals(false);
        }
    }

    void UpdateResourcePools() {
        EmpireManager& manager = AIClientApp::GetApp()->Empires();
        for (EmpireManager::iterator it = manager.begin(); it != manager.end(); ++it)
            it->second->UpdateResourcePools();
    }

    void UpdateResearchQueue() {
        int empire_id = AIClientApp::GetApp()->EmpireID();
        Empire* empire = Empires().Lookup(empire_id);
        if (!empire) {
            Logger().errorStream() << "AIInterface::UpdateResearchQueue : couldn't get empire with id " << empire_id;
            return;
        }
        empire->UpdateResearchQueue();
    }

    void UpdateProductionQueue() {
        int empire_id = AIClientApp::GetApp()->EmpireID();
        Empire* empire = Empires().Lookup(empire_id);
        if (!empire) {
            Logger().errorStream() << "AIInterface::UpdateProductionQueue : couldn't get empire with id " << empire_id;
            return;
        }
        empire->UpdateProductionQueue();
    }

    int IssueFleetMoveOrder(int fleet_id, int destination_id) {
        TemporaryPtr<const Fleet> fleet = GetFleet(fleet_id);
        if (!fleet) {
            Logger().errorStream() << "AIInterface::IssueFleetMoveOrder : passed an invalid fleet_id";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();
        if (!fleet->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueFleetMoveOrder : passed fleet_id of fleet not owned by player";
            return 0;
        }

        int start_id = fleet->SystemID();
        if (start_id == INVALID_OBJECT_ID)
            start_id = fleet->NextSystemID();

        if (destination_id != INVALID_OBJECT_ID && destination_id == start_id)
            Logger().debugStream() << "AIInterface::IssueFleetMoveOrder : pass destination system id (" << destination_id << ") that fleet is already in";

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new FleetMoveOrder(empire_id, fleet_id, start_id, destination_id)));

        return 1;
    }

    int IssueRenameOrder(int object_id, const std::string& new_name) {
        if (new_name.empty()) {
            Logger().errorStream() << "AIInterface::IssueRenameOrder : passed an empty new name";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();
        TemporaryPtr<const UniverseObject> obj = GetUniverseObject(object_id);

        if (!obj) {
            Logger().errorStream() << "AIInterface::IssueRenameOrder : passed an invalid object_id";
            return 0;
        }
        if (!obj->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueRenameOrder : passed object_id of object not owned by player";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new RenameOrder(empire_id, object_id, new_name)));

        return 1;
    }

    int IssueScrapOrder(const std::vector<int>& object_ids) {
            if (object_ids.empty()) {
                Logger().errorStream() << "AIInterface::IssueScrapOrder : passed empty vector of object_ids";
                return 0;
            }

            int empire_id = AIClientApp::GetApp()->EmpireID();

            // make sure all objects exist and are owned just by this player
            for (std::vector<int>::const_iterator it = object_ids.begin(); it != object_ids.end(); ++it) {
                TemporaryPtr<const UniverseObject> obj = GetUniverseObject(*it);

                if (!obj) {
                    Logger().errorStream() << "AIInterface::IssueScrapOrder : passed an invalid object_id";
                    return 0;
                }

                if (!obj->OwnedBy(empire_id)) {
                    Logger().errorStream() << "AIInterface::IssueScrapOrder : passed object_id of object not owned by player";
                    return 0;
                }

                AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ScrapOrder(empire_id, *it)));
            }

            return 1;
    }

    int IssueScrapOrder(int object_id) {
        std::vector<int> object_ids;
        object_ids.push_back(object_id);
        return IssueScrapOrder(object_ids);
    }

    int IssueNewFleetOrder(const std::string& fleet_name, const std::vector<int>& ship_ids) {
        if (ship_ids.empty()) {
            Logger().errorStream() << "AIInterface::IssueNewFleetOrder : passed empty vector of ship_ids";
            return 0;
        }

        if (fleet_name.empty()) {
            Logger().errorStream() << "AIInterface::IssueNewFleetOrder : tried to create a nameless fleet";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();
        TemporaryPtr<const Ship> ship = TemporaryPtr<Ship>();

        // make sure all ships exist and are owned just by this player
        for (std::vector<int>::const_iterator it = ship_ids.begin(); it != ship_ids.end(); ++it) {
            ship = GetShip(*it);
            if (!ship) {
                Logger().errorStream() << "AIInterface::IssueNewFleetOrder : passed an invalid ship_id";
                return 0;
            }
            if (!ship->OwnedBy(empire_id)) {
                Logger().errorStream() << "AIInterface::IssueNewFleetOrder : passed ship_id of ship not owned by player";
                return 0;
            }
        }

        // make sure all ships are at a system, and that all are at the same system
        int system_id = ship->SystemID();
        if (system_id == INVALID_OBJECT_ID) {
            Logger().errorStream() << "AIInterface::IssueNewFleetOrder : passed ship_ids of ships at different locations";
            return 0;
        }

        std::vector<int>::const_iterator it = ship_ids.begin();
        for (++it; it != ship_ids.end(); ++it) {
            TemporaryPtr<const Ship> ship2 = GetShip(*it);
            if (ship2->SystemID() != system_id) {
                Logger().errorStream() << "AIInterface::IssueNewFleetOrder : passed ship_ids of ships at different locations";
                return 0;
            }
        }

        int new_fleet_id = ClientApp::GetApp()->GetNewObjectID();

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new NewFleetOrder(empire_id, fleet_name, new_fleet_id, system_id, ship_ids)));

        return new_fleet_id;
    }

    int IssueNewFleetOrder(const std::string& fleet_name, int ship_id) {
        std::vector<int> ship_ids;
        ship_ids.push_back(ship_id);
        return IssueNewFleetOrder(fleet_name, ship_ids);
    }

    int IssueFleetTransferOrder(int ship_id, int new_fleet_id) {
        int empire_id = AIClientApp::GetApp()->EmpireID();

        TemporaryPtr<const Ship> ship = GetShip(ship_id);
        if (!ship) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : passed an invalid ship_id " << ship_id;
            return 0;
        }
        int ship_sys_id = ship->SystemID();
        if (ship_sys_id == INVALID_OBJECT_ID) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : ship is not in a system";
            return 0;
        }
        if (!ship->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : passed ship_id of ship not owned by player";
            return 0;
        }

        TemporaryPtr<const Fleet> fleet = GetFleet(new_fleet_id);
        if (!fleet) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : passed an invalid new_fleet_id " << new_fleet_id;
            return 0;
        }
        int fleet_sys_id = fleet->SystemID();
        if (fleet_sys_id == INVALID_OBJECT_ID) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : new fleet is not in a system";
            return 0;
        }
        if (!fleet->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : passed fleet_id "<< new_fleet_id << " of fleet not owned by player";
            return 0;
        }

        if (fleet_sys_id != ship_sys_id) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : new fleet in system " << fleet_sys_id << " and ship in system "<< ship_sys_id <<  " are not in the same system";
            return 0;
        }

        std::vector<int> ship_ids;
        ship_ids.push_back(ship_id);
        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new FleetTransferOrder(empire_id, new_fleet_id, ship_ids)));

        return 1;
    }

    int IssueColonizeOrder(int ship_id, int planet_id) {
        int empire_id = AIClientApp::GetApp()->EmpireID();

        // make sure ship_id is a ship...
        TemporaryPtr<const Ship> ship = GetShip(ship_id);
        if (!ship) {
            Logger().errorStream() << "AIInterface::IssueColonizeOrder : passed an invalid ship_id";
            return 0;
        }

        // get fleet of ship
        TemporaryPtr<const Fleet> fleet = GetFleet(ship->FleetID());
        if (!fleet) {
            Logger().errorStream() << "AIInterface::IssueColonizeOrder : ship with passed ship_id has invalid fleet_id";
            return 0;
        }

        // make sure player owns ship and its fleet
        if (!fleet->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueColonizeOrder : empire does not own fleet of passed ship";
            return 0;
        }
        if (!ship->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueColonizeOrder : empire does not own passed ship";
            return 0;
        }

        // verify that planet exists and is un-occupied.
        TemporaryPtr<const Planet> planet = GetPlanet(planet_id);
        if (!planet) {
            Logger().errorStream() << "AIInterface::IssueColonizeOrder : no planet with passed planet_id";
            return 0;
        }
        if ((!planet->Unowned()) && !( planet->OwnedBy(empire_id) && planet->CurrentMeterValue(METER_POPULATION)==0)) {
            Logger().errorStream() << "AIInterface::IssueColonizeOrder : planet with passed planet_id "<<planet_id<<" is already owned, or colonized by own empire";
            return 0;
        }

        // verify that planet is in same system as the fleet
        if (planet->SystemID() != fleet->SystemID()) {
            Logger().errorStream() << "AIInterface::IssueColonizeOrder : fleet and planet are not in the same system";
            return 0;
        }
        if (ship->SystemID() == INVALID_OBJECT_ID) {
            Logger().errorStream() << "AIInterface::IssueColonizeOrder : ship is not in a system";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ColonizeOrder(empire_id, ship_id, planet_id)));

        return 1;
    }

    int IssueInvadeOrder(int ship_id, int planet_id) {
        int empire_id = AIClientApp::GetApp()->EmpireID();

        // make sure ship_id is a ship...
        TemporaryPtr<const Ship> ship = GetShip(ship_id);
        if (!ship) {
            Logger().errorStream() << "AIInterface::IssueInvadeOrder : passed an invalid ship_id";
            return 0;
        }

        // get fleet of ship
        TemporaryPtr<const Fleet> fleet = GetFleet(ship->FleetID());
        if (!fleet) {
            Logger().errorStream() << "AIInterface::IssueInvadeOrder : ship with passed ship_id has invalid fleet_id";
            return 0;
        }

        // make sure player owns ship and its fleet
        if (!fleet->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueInvadeOrder : empire does not own fleet of passed ship";
            return 0;
        }
        if (!ship->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueInvadeOrder : empire does not own passed ship";
            return 0;
        }

        // verify that planet exists and is occupied by another empire
        TemporaryPtr<const Planet> planet = GetPlanet(planet_id);
        if (!planet) {
            Logger().errorStream() << "AIInterface::IssueInvadeOrder : no planet with passed planet_id";
            return 0;
        }
        bool owned_by_invader = planet->OwnedBy(empire_id);
        bool unowned = planet->Unowned();
        bool populated = planet->CurrentMeterValue(METER_POPULATION) > 0.;
        bool visible = GetUniverse().GetObjectVisibilityByEmpire(planet_id, empire_id) >= VIS_PARTIAL_VISIBILITY;
        bool vulnerable = planet->CurrentMeterValue(METER_SHIELD) <= 0.;
        float shields = planet->CurrentMeterValue(METER_SHIELD);
        std::string thisSpecies = planet->SpeciesName();
        //bool being_invaded = planet->IsAboutToBeInvaded();
        bool invadable = !owned_by_invader && vulnerable && (populated || !unowned) && visible ;// && !being_invaded; a 'being_invaded' check prevents AI from invading with multiple ships at once, which is important
        if (!invadable) {
            Logger().errorStream() << "AIInterface::IssueInvadeOrder : planet with passed planet_id "<< planet_id <<" and species "<<thisSpecies<<" is  "
                                   << "not invadable due to one or more of: owned by invader empire, "
                                   << "not visible to invader empire, has shields above zero, "
                                   << "or is already being invaded.";
            if (!unowned) 
                Logger().errorStream() << "AIInterface::IssueInvadeOrder : planet (id " << planet_id << ") is not unowned";
            if (!visible)
                Logger().errorStream() << "AIInterface::IssueInvadeOrder : planet (id " << planet_id << ") is not visible";
            if (!vulnerable)
                Logger().errorStream() << "AIInterface::IssueInvadeOrder : planet (id " << planet_id << ") is not vulnerable, shields at "<<shields;
            return 0;
        }

        // verify that planet is in same system as the fleet
        if (planet->SystemID() != fleet->SystemID()) {
            Logger().errorStream() << "AIInterface::IssueInvadeOrder : fleet and planet are not in the same system";
            return 0;
        }
        if (ship->SystemID() == INVALID_OBJECT_ID) {
            Logger().errorStream() << "AIInterface::IssueInvadeOrder : ship is not in a system";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new InvadeOrder(empire_id, ship_id, planet_id)));

        return 1;
    }

    int IssueBombardOrder(int ship_id, int planet_id) {
        int empire_id = AIClientApp::GetApp()->EmpireID();

        // make sure ship_id is a ship...
        TemporaryPtr<const Ship> ship = GetShip(ship_id);
        if (!ship) {
            Logger().errorStream() << "AIInterface::IssueBombardOrder : passed an invalid ship_id";
            return 0;
        }
        if (ship->TotalWeaponsDamage() <= 0) {
            Logger().errorStream() << "AIInterface::IssueBombardOrder : ship can't attack / bombard";
            return 0;
        }
        if (!ship->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueBombardOrder : ship isn't owned by the order-issuing empire";
            return 0;
        }


        // verify that planet exists and is occupied by another empire
        TemporaryPtr<const Planet> planet = GetPlanet(planet_id);
        if (!planet) {
            Logger().errorStream() << "AIInterface::IssueBombardOrder : no planet with passed planet_id";
            return 0;
        }
        if (planet->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueBombardOrder : planet is already owned by the order-issuing empire";
            return 0;
        }
        if (!planet->Unowned() && Empires().GetDiplomaticStatus(planet->Owner(), empire_id) != DIPLO_WAR) {
            Logger().errorStream() << "AIInterface::IssueBombardOrder : planet owned by an empire not at war with order-issuing empire";
            return 0;
        }
        if (GetUniverse().GetObjectVisibilityByEmpire(planet_id, empire_id) < VIS_BASIC_VISIBILITY) {
            Logger().errorStream() << "AIInterface::IssueBombardOrder : planet that empire reportedly has insufficient visibility of, but will be allowed to proceed pending investigation";
            //return;
        }


        int ship_system_id = ship->SystemID();
        if (ship_system_id == INVALID_OBJECT_ID) {
            Logger().errorStream() << "AIInterface::IssueBombardOrder : given id of ship not in a system";
            return 0;
        }
        int planet_system_id = planet->SystemID();
        if (ship_system_id != planet_system_id) {
            Logger().errorStream() << "AIInterface::IssueBombardOrder : given ids of ship and planet not in the same system";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new BombardOrder(empire_id, ship_id, planet_id)));

        return 1;
    }

    int IssueDeleteFleetOrder() {
        return 0;
    }

    int IssueAggressionOrder(int object_id, bool aggressive) {
        int empire_id = AIClientApp::GetApp()->EmpireID();

        TemporaryPtr<const Fleet> fleet = GetFleet(object_id);
        if (!fleet) {
            Logger().errorStream() << "AIInterface::IssueAggressionOrder : no fleet with passed id";
            return 0;
        }
        if (!fleet->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueAggressionOrder : passed object_id of object not owned by player";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
            new AggressiveOrder(empire_id, object_id, aggressive)));

        return 1;
    }

    int IssueGiveObjectToEmpireOrder(int object_id, int recipient_id) {
        int empire_id = AIClientApp::GetApp()->EmpireID();

        if (Empires().Lookup(recipient_id) == 0) {
            Logger().errorStream() << "AIInterface::IssueGiveObjectToEmpireOrder : given invalid recipient empire id";
            return 0;
        }

        if (Empires().GetDiplomaticStatus(empire_id, recipient_id) != DIPLO_PEACE) {
            Logger().errorStream() << "AIInterface::IssueGiveObjectToEmpireOrder : attempting to give to empire not at peace";
            return 0;
        }

        TemporaryPtr<UniverseObject> obj = GetUniverseObject(object_id);
        if (!obj) {
            Logger().errorStream() << "AIInterface::IssueGiveObjectToEmpireOrder : passed invalid object id";
            return 0;
        }

        if (!obj->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueGiveObjectToEmpireOrder : passed object not owned by player";
            return 0;
        }

        if (obj->ObjectType() != OBJ_FLEET && obj->ObjectType() != OBJ_PLANET) {
            Logger().errorStream() << "AIInterface::IssueGiveObjectToEmpireOrder : passed object that is not a fleet or planet";
            return 0;
        }

        TemporaryPtr<System> system = GetSystem(obj->SystemID());
        if (!system) {
            Logger().errorStream() << "AIInterface::IssueGiveObjectToEmpireOrder : couldn't get system of object";
            return 0;
        }

        // can only give to empires with something present to receive the gift
        bool recipient_has_something_here = false;
        std::vector<TemporaryPtr<const UniverseObject> > system_objects =
            Objects().FindObjects<const UniverseObject>(system->ObjectIDs());
        for (std::vector<TemporaryPtr<const UniverseObject> >::const_iterator it = system_objects.begin();
                it != system_objects.end(); ++it)
        {
            TemporaryPtr<const UniverseObject> obj = *it;
            if (obj->Owner() == recipient_id) {
                recipient_has_something_here = true;
                break;
            }
        }
        if (!recipient_has_something_here) {
            Logger().errorStream() << "AIInterface::IssueGiveObjectToEmpireOrder : recipient empire has nothing in system";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
            new GiveObjectToEmpireOrder(empire_id, object_id, recipient_id)));

        return 1;
    }

    int IssueChangeFocusOrder(int planet_id, const std::string& focus) {
        int empire_id = AIClientApp::GetApp()->EmpireID();

        TemporaryPtr<const Planet> planet = GetPlanet(planet_id);
        if (!planet) {
            Logger().errorStream() << "AIInterface::IssueChangeFocusOrder : no planet with passed planet_id "<<planet_id;
            return 0;
        }
        if (!planet->OwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueChangeFocusOrder : empire does not own planet with passed planet_id";
            return 0;
        }
        if (false) {    // todo: verify that focus is valid for specified planet
            Logger().errorStream() << "AIInterface::IssueChangeFocusOrder : invalid focus specified";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
            new ChangeFocusOrder(empire_id, planet_id, focus)));

        return 1;
    }

    int IssueEnqueueTechOrder(const std::string& tech_name, int position) {
        const Tech* tech = GetTech(tech_name);
        if (!tech) {
            Logger().errorStream() << "AIInterface::IssueEnqueueTechOrder : passed tech_name that is not the name of a tech.";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
            new ResearchQueueOrder(empire_id, tech_name, position)));

        return 1;
    }

    int IssueDequeueTechOrder(const std::string& tech_name) {
        const Tech* tech = GetTech(tech_name);
        if (!tech) {
            Logger().errorStream() << "AIInterface::IssueDequeueTechOrder : passed tech_name that is not the name of a tech.";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ResearchQueueOrder(empire_id, tech_name)));

        return 1;
    }

    int IssueEnqueueBuildingProductionOrder(const std::string& item_name, int location_id) {
        int empire_id = AIClientApp::GetApp()->EmpireID();
        Empire* empire = AIClientApp::GetApp()->Empires().Lookup(empire_id);

        if (!empire->ProducibleItem(BT_BUILDING, item_name, location_id)) {
            Logger().errorStream() << "AIInterface::IssueEnqueueBuildingProductionOrder : specified item_name and location_id that don't indicate an item that can be built at that location";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
            new ProductionQueueOrder(empire_id, BT_BUILDING, item_name, 1, location_id)));

        return 1;
    }

    int IssueEnqueueShipProductionOrder(int design_id, int location_id) {
        int empire_id = AIClientApp::GetApp()->EmpireID();
        Empire* empire = AIClientApp::GetApp()->Empires().Lookup(empire_id);

        if (!empire->ProducibleItem(BT_SHIP, design_id, location_id)) {
            Logger().errorStream() << "AIInterface::IssueEnqueueShipProductionOrder : specified design_id and location_id that don't indicate a design that can be built at that location";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
            new ProductionQueueOrder(empire_id, BT_SHIP, design_id, 1, location_id)));

        return 1;
    }

    int IssueChangeProductionQuantityOrder(int queue_index, int new_quantity, int new_blocksize) {
        int empire_id = AIClientApp::GetApp()->EmpireID();
        Empire* empire = AIClientApp::GetApp()->Empires().Lookup(empire_id);

        const ProductionQueue& queue = empire->GetProductionQueue();
        if (queue_index < 0 || static_cast<int>(queue.size()) <= queue_index) {
            Logger().errorStream() << "AIInterface::IssueChangeProductionQuantityOrder : passed queue_index outside range of items on queue.";
            return 0;
        }
        if (queue[queue_index].item.build_type != BT_SHIP) {
            Logger().errorStream() << "AIInterface::IssueChangeProductionQuantityOrder : passed queue_index for a non-ship item.";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
            new ProductionQueueOrder(empire_id, queue_index, new_quantity, new_blocksize)));

        return 1;
    }

    int IssueRequeueProductionOrder(int old_queue_index, int new_queue_index) {
        if (old_queue_index == new_queue_index) {
            Logger().errorStream() << "AIInterface::IssueRequeueProductionOrder : passed same old and new indexes... nothing to do.";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();
        Empire* empire = AIClientApp::GetApp()->Empires().Lookup(empire_id);

        const ProductionQueue& queue = empire->GetProductionQueue();
        if (old_queue_index < 0 || static_cast<int>(queue.size()) <= old_queue_index) {
            Logger().errorStream() << "AIInterface::IssueRequeueProductionOrder : passed old_queue_index outside range of items on queue.";
            return 0;
        }

        // After removing an earlier entry in queue, all later entries are shifted down one queue index, so
        // inserting before the specified item index should now insert before the previous item index.  This
        // also allows moving to the end of the queue, rather than only before the last item on the queue.
        int actual_new_index = new_queue_index;
        if (old_queue_index < new_queue_index)
            actual_new_index = new_queue_index - 1;

        if (new_queue_index < 0 || static_cast<int>(queue.size()) <= actual_new_index) {
            Logger().errorStream() << "AIInterface::IssueRequeueProductionOrder : passed new_queue_index outside range of items on queue.";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
            new ProductionQueueOrder(empire_id, old_queue_index, new_queue_index)));

        return 1;
    }

    int IssueDequeueProductionOrder(int queue_index) {
        int empire_id = AIClientApp::GetApp()->EmpireID();
        Empire* empire = AIClientApp::GetApp()->Empires().Lookup(empire_id);

        const ProductionQueue& queue = empire->GetProductionQueue();
        if (queue_index < 0 || static_cast<int>(queue.size()) <= queue_index) {
            Logger().errorStream() << "AIInterface::IssueDequeueProductionOrder : passed queue_index outside range of items on queue.";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(
            new ProductionQueueOrder(empire_id, queue_index)));

        return 1;
    }

    int IssueCreateShipDesignOrder(const std::string& name, const std::string& description,
                                   const std::string& hull, const std::vector<std::string> parts,
                                   const std::string& icon, const std::string& model, bool nameDescInStringTable)
    {
        if (name.empty() || description.empty() || hull.empty()) {
            Logger().errorStream() << "AIInterface::IssueCreateShipDesignOrderOrder : passed an empty name, description, or hull.";
            return 0;
        }
        if (!ShipDesign::ValidDesign(hull, parts)) {
            Logger().errorStream() << "AIInterface::IssueCreateShipDesignOrderOrder : pass a hull and parts that do not make a valid ShipDesign";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();
        int current_turn = CurrentTurn();

        // create design from stuff chosen in UI
        ShipDesign* design = new ShipDesign(name, description, current_turn, ClientApp::GetApp()->EmpireID(),
                                            hull, parts, icon, model, nameDescInStringTable);

        if (!design) {
            Logger().errorStream() << "AIInterface::IssueCreateShipDesignOrderOrder failed to create a new ShipDesign object";
            return 0;
        }

        int new_design_id = AIClientApp::GetApp()->GetNewDesignID();
        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ShipDesignOrder(empire_id, new_design_id, *design)));
        delete design;

        return 1;
    }

    void SendPlayerChatMessage(int recipient_player_id, const std::string& message_text) {
        if (recipient_player_id == -1)
            AIClientApp::GetApp()->Networking().SendMessage(GlobalChatMessage(PlayerID(), message_text));
        else
            AIClientApp::GetApp()->Networking().SendMessage(SingleRecipientChatMessage(PlayerID(), recipient_player_id, message_text));
    }

    void SendDiplomaticMessage(const DiplomaticMessage& diplo_message) {
        AIClientApp* app = AIClientApp::GetApp();
        if (!app) return;
        int sender_player_id = app->PlayerID();
        if (sender_player_id == Networking::INVALID_PLAYER_ID) return;
        int recipient_empire_id = diplo_message.RecipientEmpireID();
        int recipient_player_id = app->EmpirePlayerID(recipient_empire_id);
        if (recipient_player_id == Networking::INVALID_PLAYER_ID) return;
        app->Networking().SendMessage(DiplomacyMessage(sender_player_id, recipient_player_id, diplo_message));
    }

    void DoneTurn() {
        Logger().debugStream() << "AIInterface::DoneTurn()";
        AIClientApp::GetApp()->StartTurn(); // encodes order sets and sends turn orders message.  "done" the turn for the client, but "starts" the turn for the server
    }

    void CombatSetup() {
        Logger().debugStream() << "AIInterface::CombatSetup()";
        AIClientApp::GetApp()->SendCombatSetup();
    }

    void DoneCombatTurn() {
        Logger().debugStream() << "AIInterface::DoneCombatTurn()";
        AIClientApp::GetApp()->StartCombatTurn(); // encodes combat order sets and sends combat turn orders message.  "done" the combat turn for the client, but "starts" the combat turn for the server
    }

    void LogOutput(const std::string& log_text) {
        Logger().debugStream() << log_text;
    }

    void ErrorOutput(const std::string& error_text) {
        Logger().errorStream() << error_text;
    }
} // namespace AIInterface
