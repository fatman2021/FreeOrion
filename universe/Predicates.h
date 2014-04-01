// -*- C++ -*-
#ifndef _Predicates_h_
#define _Predicates_h_

class UniverseObject;
class Building;
class Fleet;
class Planet;
class Ship;
class System;
class Field;

#include <boost/mpl/assert.hpp>
#include <boost/pointer_cast.hpp>
#include <boost/type_traits/add_pointer.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_pointer.hpp>

#include "../util/Export.h"
#include "TemporaryPtr.h"

extern const int ALL_EMPIRES;

struct UniverseObjectVisitorRR;

/** the base class for UniverseObject visitor classes.  These visitors have Visit() overloads for each type in the UniversObject-based
    class herarchy.  Calling Visit() returns the \a obj parameter, if some predicate is true of that object.  Each UniverseObject
    subclass needs to have an Accept(const UniverseObjectVisitor& visitor) method that consists only of "visitor->Visit(this)".  Because
    of the specific-type overloads, passing a UniverseObjectVisitor into the Accept() method of a UniverseObject will cause the
    UniverseObjectVisitor's appropriate Visit() method to be called.  Since the specific type of the \a obj parameter is known within
    each Visit() method, \a obj can be accessed by type, without using a dynamic_cast.  Note that is is therefore safe to static_cast a
    UniversObject pointer that is returned from a UniverseObjectVisitor subclass that only returns a nonzero for one specific
    UniverseObject subclass (e.g. StationaryFleetVisitor<Planet>).  The default behavior of all Visit() methods besides
    Visit(UniverseObject*) is to return the result of a call to Visit(UniverseObject*).  This means that UniverseObjectVisitor
    subclasses can override Visit(UniverseObject*) only, and calls to all Visit() overloads will work.  The default return value for
    Visit(UniverseObject*) is 0, so overridding any \a one Visit() method besides this one will ensure that only UniverseObjects
    of a single subclass are recognized by the visitor. 
    NOTE: the virtual methods for non-const objects default to calling the const methods, then casting the const away from the result,
    so it is enough to override the methods for const objects if you don't modify them or if you need to modify const objects. */
struct FO_COMMON_API UniverseObjectVisitor {
    virtual TemporaryPtr<const UniverseObject> Visit(TemporaryPtr<const UniverseObject> obj);
    virtual TemporaryPtr<const UniverseObject> Visit(TemporaryPtr<const Building> obj);
    virtual TemporaryPtr<const UniverseObject> Visit(TemporaryPtr<const Fleet> obj);
    virtual TemporaryPtr<const UniverseObject> Visit(TemporaryPtr<const Planet> obj);
    virtual TemporaryPtr<const UniverseObject> Visit(TemporaryPtr<const Ship> obj);
    virtual TemporaryPtr<const UniverseObject> Visit(TemporaryPtr<const System> obj);
    virtual TemporaryPtr<const UniverseObject> Visit(TemporaryPtr<const Field> obj);
    
    virtual TemporaryPtr<UniverseObject> Visit(TemporaryPtr<UniverseObject> obj);
    virtual TemporaryPtr<UniverseObject> Visit(TemporaryPtr<Building> obj);
    virtual TemporaryPtr<UniverseObject> Visit(TemporaryPtr<Fleet> obj);
    virtual TemporaryPtr<UniverseObject> Visit(TemporaryPtr<Planet> obj);
    virtual TemporaryPtr<UniverseObject> Visit(TemporaryPtr<Ship> obj);
    virtual TemporaryPtr<UniverseObject> Visit(TemporaryPtr<System> obj);
    virtual TemporaryPtr<UniverseObject> Visit(TemporaryPtr<Field> obj);
    virtual ~UniverseObjectVisitor();
    
    operator UniverseObjectVisitorRR& (); // rvalue reference emulation
};

struct UniverseObjectVisitorRR : public UniverseObjectVisitor {};

/** returns obj iff \a obj is a Fleet belonging to the given empire object that is parked at a System, not under orders to move.  
    If the given empire is -1, all orderd moving fleets will be returned.  Note that it is preferable to use this functor on System
    searches, rather than Universe ones. */
struct FO_COMMON_API StationaryFleetVisitor : UniverseObjectVisitor
{
    StationaryFleetVisitor(int empire = ALL_EMPIRES);
    virtual TemporaryPtr<const UniverseObject> Visit(TemporaryPtr<const Fleet> obj);
    virtual ~StationaryFleetVisitor();
    const int empire_id;
};

/** returns obj iff \a obj is a Fleet belonging to the given empire, and that is under orders to move, but is not yet moving. 
    If the given empire is -1, all stationary fleets will be returned.  Note that it is preferable to use this functor on System
    searches, rather than Universe ones. */
struct FO_COMMON_API OrderedMovingFleetVisitor : UniverseObjectVisitor
{
    OrderedMovingFleetVisitor(int empire = ALL_EMPIRES);
    virtual TemporaryPtr<const UniverseObject> Visit(TemporaryPtr<const Fleet> obj);
    virtual ~OrderedMovingFleetVisitor();
    const int empire_id;
};

/** returns obj iff \a obj is a moving Fleet belonging to the given empire, and that is moving between systems.  
    If the given empire is -1, all moving fleets will be returned. */
struct FO_COMMON_API MovingFleetVisitor : UniverseObjectVisitor
{
    MovingFleetVisitor(int empire = ALL_EMPIRES);
    virtual TemporaryPtr<const UniverseObject> Visit(TemporaryPtr<const Fleet> obj);
    virtual ~MovingFleetVisitor();
    const int empire_id;
};

/** returns obj iff \a obj is owned by the empire with id \a empire, and \a obj is of type T. */
template <class T>
struct OwnedVisitor : UniverseObjectVisitor
{
    OwnedVisitor(int empire = ALL_EMPIRES);
    virtual TemporaryPtr<const UniverseObject> Visit(TemporaryPtr<const T> obj);
    virtual ~OwnedVisitor() {} 
    const int empire_id;
};

template <class T>
OwnedVisitor<T>::OwnedVisitor(int empire) :
    empire_id(empire)
{}

template <class T>
TemporaryPtr<const UniverseObject> OwnedVisitor<T>::Visit(TemporaryPtr<const T> obj)
{
    if (obj->OwnedBy(empire_id))
        return obj;
    return TemporaryPtr<const UniverseObject>();
}

#endif // _Predicates_h_
