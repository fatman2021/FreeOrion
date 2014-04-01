// -*- C++ -*-
#ifndef _System_h_
#define _System_h_

#include "UniverseObject.h"

#include "../util/AppInterface.h"
#include "../util/Export.h"

#include <map>

struct UniverseObjectVisitor;

/** contains UniverseObjects and connections to other systems (starlanes and
   wormholes).  All systems are UniversObjects contained within the universe,
   and (nearly) all systems also contain UniverseObjects.  Systems are
   searchable using arbirary predicates, much in the same way that Universes
   are.  Each System consists of a star and 0 or more orbits.  UniversObjects
   can be placed in the system "at large" or in a particular orbit.  No
   checking is done to determine whether an object is orbit-bound (like a
   Planet) or not (like a Fleet) when placing them with the Insert()
   functions.  Iteration is available over all starlanes and wormholes
   (together), all system objects, all free system objects (those not in an
   orbit), and all objects in a paricular orbit.*/
class FO_COMMON_API System : public UniverseObject {
public:
    /** \name Accessors */ //@{
    virtual UniverseObjectType  ObjectType() const;
    virtual std::string         Dump() const;

    /** returns the name to display for players for this system.  While all
      * systems may have a proper name assigned, if they contain no planets or
      * star, then "Deep Space" (or its translation)  Or, a system that is not
      * yet explored might be "Unexplored Region", rather than an empty string
      * for the name.  This is distinct from PublicName functions, which filter
      * the name based on ownership. */
    const std::string&      ApparentName(int empire_id, bool blank_unexplored_and_none = false) const;

    StarType                GetStarType() const             { return m_star; }  ///< returns the type of star for this system
    StarType                NextOlderStarType() const;
    StarType                NextYoungerStarType() const;

    int                     Orbits() const                  { return m_orbits.size(); } ///< returns the number of orbits in this system

    int                     NumStarlanes() const;                       ///< returns the number of starlanes from this system to other systems
    int                     NumWormholes() const;                       ///< returns the number of wormholes from this system to other systems
    bool                    HasStarlaneTo(int id) const;                ///< returns true if there is a starlane from this system to the system with ID number \a id
    bool                    HasWormholeTo(int id) const;                ///< returns true if there is a wormhole from this system to the system with ID number \a id

    virtual int             SystemID() const                { return this->ID(); }

    virtual const std::set<int>&    ContainedObjectIDs() const;                     ///< returns ids of objects in this system

    const std::set<int>&    ObjectIDs() const               { return m_objects; }
    const std::set<int>&    PlanetIDs() const               { return m_planets; }
    const std::set<int>&    BuildingIDs() const             { return m_buildings; }
    const std::set<int>&    FleetIDs() const                { return m_fleets; }
    const std::set<int>&    ShipIDs() const                 { return m_ships; }
    const std::set<int>&    FieldIDs() const                { return m_fields; }

    virtual bool            Contains(int object_id) const;                                  ///< returns true if object with id \a object_id is in this System
    virtual bool            ContainedBy(int object_id) const{ return false; }               ///< returns true if there is an object with id \a object_id that contains this UniverseObject

    int                     PlanetInOrbit(int orbit) const;             ///< returns the ID of the planet in the specified \a orbit, or INVALID_OBJECT_ID if there is no planet in that orbit or it is an invalid orbit
    int                     OrbitOfPlanet(int object_id) const;         ///< returns the orbit ID in which the planet with \a object_id is located, or -1 the specified ID is not a planet in an orbit of this system
    bool                    OrbitOccupied(int orbit) const;             ///< returns true if there is an object in \a orbit
    std::set<int>           FreeOrbits() const;                         ///< returns the set of orbit numbers that are unoccupied

    const std::map<int, bool>&  StarlanesWormholes() const;             ///< returns map of all starlanes and wormholes; map contains keys that are IDs of connected systems, and bool values indicating whether each is a starlane (false) or a wormhole (true)

    /** returns a map of the starlanes and wormholes visible to empire
      * \a empire_id; the map contains keys that are IDs of connected systems,
      * and bool values indicating whether each is a starlane (false) or a
      * wormhole (true)*/
    std::map<int, bool>     VisibleStarlanesWormholes(int empire_id) const;

    int                     LastTurnBattleHere() const  { return m_last_turn_battle_here; }

    virtual TemporaryPtr<UniverseObject>
                                Accept(UniverseObjectVisitor* visitor);
    virtual TemporaryPtr<const UniverseObject>
                                Accept(UniverseObjectVisitor* visitor) const;

    const std::string&      OverlayTexture() const      { return m_overlay_texture; }
    double                  OverlaySize() const         { return m_overlay_size; }  ///< size in universe units

    /** fleets are inserted into system */
    mutable boost::signals2::signal<void (const std::vector<TemporaryPtr<Fleet> >& fleets)> FleetsInsertedSignal;
    /** fleets are removed from system */
    mutable boost::signals2::signal<void (const std::vector<TemporaryPtr<Fleet> >& fleets)> FleetsRemovedSignal;
    //@}

    /** \name Mutators */ //@{
    virtual void            Copy(TemporaryPtr<const UniverseObject> copied_object, int empire_id = ALL_EMPIRES);

    /** adds an object to this system. */
    void                    Insert(TemporaryPtr<UniverseObject> obj, int orbit = -1);

    /** removes the object with ID number \a id from this system. */
    void                    Remove(int id);

    void                    SetStarType(StarType type);     ///< sets the type of the star in this Systems to \a StarType
    void                    AddStarlane(int id);            ///< adds a starlane between this system and the system with ID number \a id.  \note Adding a starlane to a system to which there is already a wormhole erases the wormhole; you may want to check for a wormhole before calling this function.
    void                    AddWormhole(int id);            ///< adds a wormhole between this system and the system with ID number \a id  \note Adding a wormhole to a system to which there is already a starlane erases the starlane; you may want to check for a starlane before calling this function.
    bool                    RemoveStarlane(int id);         ///< removes a starlane between this system and the system with ID number \a id.  Returns false if there was no starlane from this system to system \a id.
    bool                    RemoveWormhole(int id);         ///< removes a wormhole between this system and the system with ID number \a id.  Returns false if there was no wormhole from this system to system \a id.

    virtual void            SetOwner(int id) {};            ///< adding owner to system objects is a no-op
    void                    SetLastTurnBattleHere(int turn);///< Sets the last turn there was a battle at this system

    void                    SetOverlayTexture(const std::string& texture, double size);

    virtual void            ResetTargetMaxUnpairedMeters();
    //@}

protected:
    friend class Universe;
    friend class ObjectMap;

    /** \name Structors */ //@{
    System();   ///< default ctor

    /** general ctor.  \throw std::invalid_arugment May throw
      * std::invalid_arugment if \a star is out of the range of StarType,
      * \a orbits is negative, or either x or y coordinate is outside the map
      * area.*/
    System(StarType star, const std::string& name, double x, double y);

    /** general ctor.  \throw std::invalid_arugment May throw
      * std::invalid_arugment if \a star is out of the range of StarType,
      * \a orbits is negative, or either x or y coordinate is outside the map
      * area.*/
    System(StarType star, const std::map<int, bool>& lanes_and_holes,
           const std::string& name, double x, double y);

    template <class T> friend void boost::python::detail::value_destroyer<false>::execute(T const volatile* p);
    template <class T> friend void boost::checked_delete(T* x);
    ~System() {}

    virtual System*         Clone(int empire_id = ALL_EMPIRES) const;   ///< returns new copy of this System
    //@}

private:
    StarType            m_star;
    std::vector<int>    m_orbits;                   ///< indexed by orbit number, indicates the id of the planet in that orbit
    std::set<int>       m_objects;
    std::set<int>       m_planets;
    std::set<int>       m_buildings;
    std::set<int>       m_fleets;
    std::set<int>       m_ships;
    std::set<int>       m_fields;
    std::map<int, bool> m_starlanes_wormholes;      ///< the ints represent the IDs of other connected systems; the bools indicate whether the connection is a wormhole (true) or a starlane (false)
    int                 m_last_turn_battle_here;    ///< the turn on which there was last a battle in this system

    std::string         m_overlay_texture;          // intentionally not serialized; set by local effects
    double              m_overlay_size;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};


// Tactical combat system geometry free functions:

/** Returns the radius, in tactical combat units, of a system.  Note that the
    tactical combat map is square. */
FO_COMMON_API double SystemRadius();

/** Returns the radius, in tactical combat units, of the star at the center of
    a system. */
FO_COMMON_API double StarRadius();

/** Returns the radius, in tactical combat units, of orbit \a orbit of a
    system.  \a orbit must be < 10. */
FO_COMMON_API double OrbitalRadius(unsigned int orbit);

/** Returns the orbital radius, in tactical combat units, of starlane entrance
    ellipses out of a system. */
double StarlaneEntranceOrbitalRadius();

/** Returns the angular position, in radians, of a starlane entrance ellipses
    out of the system with id \a from_system. */
FO_COMMON_API double StarlaneEntranceOrbitalPosition(int from_system, int to_system);

/** Returns the radius, in tactical combat units, of the star-aligned axes of
    starlane entrance ellipses out of a system. */
double StarlaneEntranceRadialAxis();

/** Returns the radius, in tactical combat units, of the non-star-aligned axes
    starlane entrance ellipses out of a system. */
double StarlaneEntranceTangentAxis();

/** Returns true iff tactical combat point <i>(x, y)</i> falls inside the
    indicated starlane ellipse. */
bool PointInStarlaneEllipse(double x, double y, int from_system, int to_system);

#endif // _System_h_
