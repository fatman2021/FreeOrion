// -*- C++ -*-
#ifndef _Enums_h_
#define _Enums_h_

#include <GG/Enum.h>

#include <iostream>

#include "../util/Export.h"

/* the various major subclasses of UniverseObject */
GG_ENUM(UniverseObjectType,
    INVALID_UNIVERSE_OBJECT_TYPE = -1,
    OBJ_BUILDING,
    OBJ_SHIP,
    OBJ_FLEET,
    OBJ_PLANET,
    OBJ_POP_CENTER,
    OBJ_PROD_CENTER,
    OBJ_SYSTEM,
    OBJ_FIELD,
    NUM_OBJ_TYPES
)

/** types of stars */
GG_ENUM(StarType,
    INVALID_STAR_TYPE = -1,
    STAR_BLUE,
    STAR_WHITE,
    STAR_YELLOW,
    STAR_ORANGE,
    STAR_RED,
    STAR_NEUTRON,
    STAR_BLACK,
    STAR_NONE,
    NUM_STAR_TYPES
)

/** types of planets */
GG_ENUM(PlanetType,
    INVALID_PLANET_TYPE = -1,
    PT_SWAMP,
    PT_TOXIC,
    PT_INFERNO,
    PT_RADIATED,
    PT_BARREN,
    PT_TUNDRA,
    PT_DESERT,
    PT_TERRAN,
    PT_OCEAN,
    PT_ASTEROIDS,
    PT_GASGIANT,
    NUM_PLANET_TYPES
)

/** sizes of planets */
GG_ENUM(PlanetSize,
    INVALID_PLANET_SIZE = -1,
    SZ_NOWORLD,         ///< used to designate an empty planet slot
    SZ_TINY,
    SZ_SMALL,
    SZ_MEDIUM,
    SZ_LARGE,
    SZ_HUGE,
    SZ_ASTEROIDS,
    SZ_GASGIANT,
    NUM_PLANET_SIZES
)


/** environmental suitability of planets for a particular race */
GG_ENUM(PlanetEnvironment,
    INVALID_PLANET_ENVIRONMENT = -1,
    PE_UNINHABITABLE,
    PE_HOSTILE,
    PE_POOR,
    PE_ADEQUATE,
    PE_GOOD,
    NUM_PLANET_ENVIRONMENTS
)

/** Types for Meters 
* Only active paired meters should lie between METER_POPULATION and METER_TROOPS
* (See: UniverseObject::ResetPairedActiveMeters())
*/
GG_ENUM(MeterType,
    INVALID_METER_TYPE = -1,
    METER_TARGET_POPULATION,
    METER_TARGET_INDUSTRY,
    METER_TARGET_RESEARCH,
    METER_TARGET_TRADE,
    METER_TARGET_CONSTRUCTION,
    METER_TARGET_HAPPINESS,

    METER_MAX_FUEL,
    METER_MAX_SHIELD,
    METER_MAX_STRUCTURE,
    METER_MAX_DEFENSE,
    METER_MAX_TROOPS,
    METER_MAX_SUPPLY,

    METER_POPULATION,
    METER_INDUSTRY,
    METER_RESEARCH,
    METER_TRADE,
    METER_CONSTRUCTION,
    METER_HAPPINESS,

    METER_FUEL,
    METER_SHIELD,
    METER_STRUCTURE,
    METER_DEFENSE,
    METER_TROOPS,
    METER_SUPPLY,

    METER_REBEL_TROOPS,
    METER_SIZE,
    METER_STEALTH,
    METER_DETECTION,
    METER_BATTLE_SPEED,
    METER_STARLANE_SPEED,

    // These meter enumerators only apply to ship part meters.
    METER_DAMAGE,
    METER_ROF,
    METER_RANGE,
    METER_SPEED,
    METER_CAPACITY,
    METER_ANTI_SHIP_DAMAGE,
    METER_ANTI_FIGHTER_DAMAGE,
    METER_LAUNCH_RATE,
    METER_FIGHTER_WEAPON_RANGE,
    // End only ship part meters

    NUM_METER_TYPES
)

/** types of universe shapes during galaxy generation */
GG_ENUM(Shape,
    INVALID_SHAPE = -1,
    SPIRAL_2,       ///< a two-armed spiral galaxy
    SPIRAL_3,       ///< a three-armed spiral galaxy
    SPIRAL_4,       ///< a four-armed spiral galaxy
    CLUSTER,        ///< a cluster galaxy
    ELLIPTICAL,     ///< an elliptical galaxy
    IRREGULAR,      ///< an irregular galaxy
    RING,           ///< a ring galaxy
    PYTHON_TEST,    ///< "test" galaxy shape for use in Python universe generator script
    RANDOM,         ///< a random one of the above
    GALAXY_SHAPES   ///< the number of shapes in this enum (leave this last)
)

/** levels of AI Aggression during galaxy generation */
GG_ENUM(Aggression,
    INVALID_AGGRESSION = -1,
    BEGINNER,
    TURTLE,        ///< very defensive
    CAUTIOUS,      ///< Somewhat Defensive
    TYPICAL,       ///< Typical
    AGGRESSIVE,    ///< Aggressive
    MANIACAL,     ///< Very Aggressive
    NUM_AI_AGGRESSION_LEVELS
)

/** General-use option for galaxy setup picks with "more" or "less" options. */
GG_ENUM(GalaxySetupOption,
    INVALID_GALAXY_SETUP_OPTION = -1,
    GALAXY_SETUP_NONE,
    GALAXY_SETUP_LOW,
    GALAXY_SETUP_MEDIUM,
    GALAXY_SETUP_HIGH,
    NUM_GALAXY_SETUP_OPTIONS
)

/** types of diplomatic empire affiliations to another empire*/
GG_ENUM(EmpireAffiliationType,
    INVALID_EMPIRE_AFFIL_TYPE = -1,
    AFFIL_SELF,     ///< not an affiliation as such; this indicates that the given empire, rather than its affiliates
    AFFIL_ENEMY,    ///< enemies of the given empire
    AFFIL_ALLY,     ///< allies of the given empire
    AFFIL_ANY,      ///< any empire
    NUM_AFFIL_TYPES ///< keep last, the number of affiliation types
)

/** diplomatic statuses */
GG_ENUM(DiplomaticStatus,
    INVALID_DIPLOMATIC_STATUS = -1,
    DIPLO_WAR,
    DIPLO_PEACE,
    NUM_DIPLO_STATUSES
)

/** types of items that can be unlocked for empires */
GG_ENUM(UnlockableItemType,
    INVALID_UNLOCKABLE_ITEM_TYPE = -1,
    UIT_BUILDING,               ///< a kind of Building
    UIT_SHIP_PART,              ///< a kind of ship part (which are placed into hulls to make designs)
    UIT_SHIP_HULL,              ///< a ship hull (into which parts are placed)
    UIT_SHIP_DESIGN,            ///< a complete ship design
    UIT_TECH,                   ///< a technology
    NUM_UNLOCKABLE_ITEM_TYPES   ///< keep last, the number of types of unlockable item
)

/** General classification for purpose and function of techs, and allowed place in tech prerequisite tree */
GG_ENUM(TechType,
    INVALID_TECH_TYPE = -1,
    TT_THEORY,      ///< Theory: does nothing itself, but is prerequisite for applications and refinements
    TT_APPLICATION, ///< Application: has effects that do things, or may unlock something such as a building that does things
    TT_REFINEMENT,  ///< Refinement: does nothing itself, but if researched, may alter the effects of something else
    NUM_TECH_TYPES  ///< keep last, the number of types of tech
)

/** Research status of techs, relating to whether they have been or can be researched */
GG_ENUM(TechStatus,
    INVALID_TECH_STATUS = -1,
    TS_UNRESEARCHABLE,
    TS_RESEARCHABLE,
    TS_COMPLETE,
    NUM_TECH_STATUSES
)

/** The general type of construction being done at a ProdCenter.  Within each valid type, a specific kind 
    of item is being built, e.g. under BUILDING a kind of building called "SuperFarm" might be built. */
GG_ENUM(BuildType,
    INVALID_BUILD_TYPE = -1,
    BT_NOT_BUILDING,         ///< no building is taking place
    BT_BUILDING,             ///< a Building object is being built
    BT_SHIP,                 ///< a Ship object is being built
    NUM_BUILD_TYPES
)

/** Types of resources that planets can produce */
GG_ENUM(ResourceType,
    INVALID_RESOURCE_TYPE = -1,
    RE_INDUSTRY,
    RE_TRADE,
    RE_RESEARCH,
    NUM_RESOURCE_TYPES
)

/** Types of fighters. */
GG_ENUM(CombatFighterType,
    INVALID_COMBAT_FIGHTER_TYPE,

    /** A fighter that is better at attacking other fighters than at
        attacking ships. */
    INTERCEPTOR,

    /** A fighter that is better at attacking ships than at attacking
        other fighters. */
    BOMBER
)

/** Types "classes" of ship parts */
GG_ENUM(ShipPartClass,
    INVALID_SHIP_PART_CLASS = -1,
    PC_SHORT_RANGE,         ///< short range direct weapons, good against ships, bad against fighters
    PC_MISSILES,            ///< long range indirect weapons, good against ships, bad against fighters
    PC_FIGHTERS,            ///< self-propelled long-range weapon platforms, good against fighters and/or ships
    PC_POINT_DEFENSE,       ///< short range direct weapons, good against fighters or incoming missiles, bad against ships
    PC_SHIELD,              ///< energy-based defense
    PC_ARMOUR,              ///< defensive material on hull of ship
    PC_TROOPS,              ///< ground troops, used to conquer planets
    PC_DETECTION,           ///< range of vision and seeing through stealth
    PC_STEALTH,             ///< hiding from enemies
    PC_FUEL,                ///< distance that can be traveled away from resupply
    PC_COLONY,              ///< transports colonists and allows ships to make new colonies
    PC_BATTLE_SPEED,        ///< affects ship speed in battle
    PC_STARLANE_SPEED,      ///< affects ship speed on starlanes
    PC_GENERAL,             ///< special purpose parts that don't fall into another class
    PC_BOMBARD,             ///< permit orbital bombardment by ships against planets
    NUM_SHIP_PART_CLASSES
)

/* Types of slots in hulls.  Parts may be restricted to only certain slot types */
GG_ENUM(ShipSlotType,
    INVALID_SHIP_SLOT_TYPE = -1,
    SL_EXTERNAL,            ///< external slots.  more easily damaged
    SL_INTERNAL,            ///< internal slots.  more protected, fewer in number
    SL_CORE,
    NUM_SHIP_SLOT_TYPES
)


/** Returns the equivalent meter type for the given resource type; if no such
  * meter type exists, returns INVALID_METER_TYPE. */
FO_COMMON_API MeterType ResourceToMeter(ResourceType type);

/** Returns the equivalent resource type for the given meter type; if no such
  * resource type exists, returns INVALID_RESOURCE_TYPE. */
FO_COMMON_API ResourceType MeterToResource(MeterType type);

/** Returns the target or max meter type that is associated with the given
  * active meter type.  If no associated meter type exists, INVALID_METER_TYPE
  * is returned. */
FO_COMMON_API MeterType AssociatedMeterType(MeterType meter_type);


extern const int ALL_EMPIRES;


/** degrees of visibility an Empire or UniverseObject can have for an
  * UniverseObject.  determines how much information the empire
  * gets about the (non)visible object. */
GG_ENUM(Visibility,
    INVALID_VISIBILITY = -1,
    VIS_NO_VISIBILITY,
    VIS_BASIC_VISIBILITY,
    VIS_PARTIAL_VISIBILITY,
    VIS_FULL_VISIBILITY,
    NUM_VISIBILITIES
)


/** Possible results of an UniverseObject being captured by other empires, or an
  * object's containing UniverseObject being captured, or the location of a
  * Production Queue Build Item being conquered, or the result of other future
  * events, such as spy activity... */
GG_ENUM(CaptureResult,
    INVALID_CAPTURE_RESULT = -1,
    CR_CAPTURE,    // object has ownership by original empire(s) removed, and conquering empire added
    CR_DESTROY,    // object is destroyed
    CR_RETAIN      // object ownership unchanged: original empire(s) still own object
)

/** Types of in-game things that might contain an EffectsGroup, or "cause" effects to occur */
GG_ENUM(EffectsCauseType,
    INVALID_EFFECTS_GROUP_CAUSE_TYPE = -1,
    ECT_UNKNOWN_CAUSE,
    ECT_INHERENT,
    ECT_TECH,
    ECT_BUILDING,
    ECT_FIELD,
    ECT_SPECIAL,
    ECT_SPECIES,
    ECT_SHIP_PART,
    ECT_SHIP_HULL
)

/** Used for tracking what moderator action is set */
GG_ENUM(ModeratorActionSetting,
    MAS_NoAction,
    MAS_Destroy,
    MAS_SetOwner,
    MAS_AddStarlane,
    MAS_RemoveStarlane,
    MAS_CreateSystem,
    MAS_CreatePlanet
)


#endif // _Enums_h_