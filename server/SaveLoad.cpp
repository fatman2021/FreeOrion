#include "SaveLoad.h"

#include "ServerApp.h"
#include "../Empire/Empire.h"
#include "../Empire/EmpireManager.h"
#include "../universe/Building.h"
#include "../universe/Fleet.h"
#include "../universe/Ship.h"
#include "../universe/Planet.h"
#include "../universe/ShipDesign.h"
#include "../universe/System.h"
#include "../universe/Species.h"
#include "../util/i18n.h"
#include "../util/Logger.h"
#include "../util/MultiplayerCommon.h"
#include "../util/Order.h"
#include "../util/OrderSet.h"
#include "../util/Serialize.h"
#include "../combat/CombatLogManager.h"

#include <GG/utf8/checked.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/serialization/deque.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/vector.hpp>

#include <fstream>


namespace fs = boost::filesystem;

namespace {
    std::map<int, SaveGameEmpireData> CompileSaveGameEmpireData(const EmpireManager& empire_manager) {
        std::map<int, SaveGameEmpireData> retval;
        const EmpireManager& empires = Empires();
        for (EmpireManager::const_iterator it = empires.begin(); it != empires.end(); ++it)
            retval[it->first] = SaveGameEmpireData(it->first, it->second->Name(), it->second->PlayerName(), it->second->Color());
        return retval;
    }
    const std::string UNABLE_TO_OPEN_FILE("Unable to open file");
}

void SaveGame(const std::string& filename, const ServerSaveGameData& server_save_game_data,
              const std::vector<PlayerSaveGameData>& player_save_game_data,
              const Universe& universe, const EmpireManager& empire_manager,
              const SpeciesManager& species_manager, const CombatLogManager& combat_log_manager,
              const GalaxySetupData& galaxy_setup_data)
{
    GetUniverse().EncodingEmpire() = ALL_EMPIRES;

    std::map<int, SaveGameEmpireData> empire_save_game_data = CompileSaveGameEmpireData(empire_manager);

    try {
#ifdef FREEORION_WIN32
        // convert UTF-8 file name to UTF-16
        fs::path::string_type file_name_native;
        utf8::utf8to16(filename.begin(), filename.end(), std::back_inserter(file_name_native));
        fs::path path = fs::path(file_name_native);
#else
        fs::path path = fs::path(filename);
#endif
        fs::ofstream ofs(path, std::ios_base::binary);

        if (!ofs)
            throw std::runtime_error(UNABLE_TO_OPEN_FILE);

        freeorion_oarchive oa(ofs);
        oa << BOOST_SERIALIZATION_NVP(galaxy_setup_data);
        oa << BOOST_SERIALIZATION_NVP(server_save_game_data);
        oa << BOOST_SERIALIZATION_NVP(player_save_game_data);
        oa << BOOST_SERIALIZATION_NVP(empire_save_game_data);
        oa << BOOST_SERIALIZATION_NVP(empire_manager);
        oa << BOOST_SERIALIZATION_NVP(species_manager);
        oa << BOOST_SERIALIZATION_NVP(combat_log_manager);
        Serialize(oa, universe);
    } catch (const std::exception& e) {
        Logger().errorStream() << UserString("UNABLE_TO_WRITE_SAVE_FILE") << " SaveGame exception: " << ": " << e.what();
        throw e;
    }
}

void LoadGame(const std::string& filename, ServerSaveGameData& server_save_game_data,
              std::vector<PlayerSaveGameData>& player_save_game_data,
              Universe& universe, EmpireManager& empire_manager,
              SpeciesManager& species_manager, CombatLogManager& combat_log_manager,
              GalaxySetupData& galaxy_setup_data)
{
    //boost::this_thread::sleep(boost::posix_time::seconds(5));

    // player notifications
    if (ServerApp* server = ServerApp::GetApp())
        server->Networking().SendMessage(TurnProgressMessage(Message::LOADING_GAME));

    GetUniverse().EncodingEmpire() = ALL_EMPIRES;

    std::map<int, SaveGameEmpireData> ignored_save_game_empire_data;

    empire_manager.Clear();
    universe.Clear();

    try {
#ifdef FREEORION_WIN32
        // convert UTF-8 file name to UTF-16
        fs::path::string_type file_name_native;
        utf8::utf8to16(filename.begin(), filename.end(), std::back_inserter(file_name_native));
        fs::path path = fs::path(file_name_native);
#else
        fs::path path = fs::path(filename);
#endif
        fs::ifstream ifs(path, std::ios_base::binary);

        if (!ifs)
            throw std::runtime_error(UNABLE_TO_OPEN_FILE);
        freeorion_iarchive ia(ifs);

        Logger().debugStream() << "LoadGame : Reading Galaxy Setup Data";
        ia >> BOOST_SERIALIZATION_NVP(galaxy_setup_data);

        Logger().debugStream() << "LoadGame : Reading Server Save Game Data";
        ia >> BOOST_SERIALIZATION_NVP(server_save_game_data);
        Logger().debugStream() << "LoadGame : Reading Player Save Game Data";
        ia >> BOOST_SERIALIZATION_NVP(player_save_game_data);

        Logger().debugStream() << "LoadGame : Reading Empire Save Game Data (Ignored)";
        ia >> BOOST_SERIALIZATION_NVP(ignored_save_game_empire_data);
        Logger().debugStream() << "LoadGame : Reading Empires Data";
        ia >> BOOST_SERIALIZATION_NVP(empire_manager);
        Logger().debugStream() << "LoadGame : Reading Species Data";
        ia >> BOOST_SERIALIZATION_NVP(species_manager);
        Logger().debugStream() << "LoadGame : Reading Combat Logs";
        ia >> BOOST_SERIALIZATION_NVP(combat_log_manager);
        Logger().debugStream() << "LoadGame : Reading Universe Data";
        Deserialize(ia, universe);
    } catch (const std::exception& e) {
        Logger().errorStream() << UserString("UNABLE_TO_READ_SAVE_FILE") << " LoadGame exception: " << ": " << e.what();
        throw e;
    }
    Logger().debugStream() << "LoadGame : Done loading save file";
}

void LoadGalaxySetupData(const std::string& filename, GalaxySetupData& galaxy_setup_data) {

    try {
#ifdef FREEORION_WIN32
        // convert UTF-8 file name to UTF-16
        fs::path::string_type file_name_native;
        utf8::utf8to16(filename.begin(), filename.end(), std::back_inserter(file_name_native));
        fs::path path = fs::path(file_name_native);
#else
        fs::path path = fs::path(filename);
#endif
        fs::ifstream ifs(path, std::ios_base::binary);

        if (!ifs)
            throw std::runtime_error(UNABLE_TO_OPEN_FILE);
        freeorion_iarchive ia(ifs);

        ia >> BOOST_SERIALIZATION_NVP(galaxy_setup_data);
        // skipping additional deserialization which is not needed for this function
    } catch (const std::exception& e) {
        Logger().errorStream() << UserString("UNABLE_TO_READ_SAVE_FILE") << " LoadPlayerSaveGameData exception: " << ": " << e.what();
        throw e;
    }
}


void LoadPlayerSaveGameData(const std::string& filename, std::vector<PlayerSaveGameData>& player_save_game_data) {
    ServerSaveGameData  ignored_server_save_game_data;
    GalaxySetupData     ignored_galaxy_setup_data;

    try {
#ifdef FREEORION_WIN32
        // convert UTF-8 file name to UTF-16
        fs::path::string_type file_name_native;
        utf8::utf8to16(filename.begin(), filename.end(), std::back_inserter(file_name_native));
        fs::path path = fs::path(file_name_native);
#else
        fs::path path = fs::path(filename);
#endif
        fs::ifstream ifs(path, std::ios_base::binary);

        if (!ifs)
            throw std::runtime_error(UNABLE_TO_OPEN_FILE);
        freeorion_iarchive ia(ifs);

        ia >> BOOST_SERIALIZATION_NVP(ignored_galaxy_setup_data);
        ia >> BOOST_SERIALIZATION_NVP(ignored_server_save_game_data);
        ia >> BOOST_SERIALIZATION_NVP(player_save_game_data);
        // skipping additional deserialization which is not needed for this function
    } catch (const std::exception& e) {
        Logger().errorStream() << UserString("UNABLE_TO_READ_SAVE_FILE") << " LoadPlayerSaveGameData exception: " << ": " << e.what();
        throw e;
    }
}

void LoadEmpireSaveGameData(const std::string& filename, std::map<int, SaveGameEmpireData>& empire_save_game_data) {
    ServerSaveGameData              ignored_server_save_game_data;
    std::vector<PlayerSaveGameData> ignored_player_save_game_data;
    GalaxySetupData                 ignored_galaxy_setup_data;

    try {
#ifdef FREEORION_WIN32
        // convert UTF-8 file name to UTF-16
        fs::path::string_type file_name_native;
        utf8::utf8to16(filename.begin(), filename.end(), std::back_inserter(file_name_native));
        fs::path path = fs::path(file_name_native);
#else
        fs::path path = fs::path(filename);
#endif
        fs::ifstream ifs(path, std::ios_base::binary);

        if (!ifs)
            throw std::runtime_error(UNABLE_TO_OPEN_FILE);
        freeorion_iarchive ia(ifs);
        ia >> BOOST_SERIALIZATION_NVP(ignored_galaxy_setup_data);
        ia >> BOOST_SERIALIZATION_NVP(ignored_server_save_game_data);
        ia >> BOOST_SERIALIZATION_NVP(ignored_player_save_game_data);
        ia >> BOOST_SERIALIZATION_NVP(empire_save_game_data);
        // skipping additional deserialization which is not needed for this function
    } catch (const std::exception& e) {
        Logger().errorStream() << UserString("UNABLE_TO_READ_SAVE_FILE") << " LoadEmpireSaveGameData exception: " << ": " << e.what();
        throw e;
    }
}

