// -*- C++ -*-
#ifndef PATHING_ENGINE_FWD_H
#define PATHING_ENGINE_FWD_H

#include "ProximityDatabase.h"

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>


namespace OpenSteer {
    class AbstractVehicle;
    class AbstractObstacle;
}

class CombatFighterFormation;
class CombatFighter;
class CombatShip;
class Missile;
class PathingEngine;

class CombatObject;

typedef boost::shared_ptr<CombatObject> CombatObjectPtr;
typedef boost::weak_ptr<CombatObject> CombatObjectWeakPtr;
typedef boost::shared_ptr<CombatFighterFormation> CombatFighterFormationPtr;
typedef boost::shared_ptr<CombatFighter> CombatFighterPtr;
typedef boost::shared_ptr<CombatShip> CombatShipPtr;
typedef boost::shared_ptr<Missile> MissilePtr;

typedef ProximityDatabase<OpenSteer::AbstractVehicle*> ProximityDB;
typedef ProximityDB::TokenType ProximityDBToken;

extern const unsigned int ENTER_STARLANE_DELAY_TURNS;

#endif
