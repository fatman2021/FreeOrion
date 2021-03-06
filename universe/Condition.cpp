#include "Condition.h"

#include "../util/Logger.h"
#include "../util/Random.h"
#include "UniverseObject.h"
#include "Universe.h"
#include "Building.h"
#include "Fleet.h"
#include "Ship.h"
#include "ObjectMap.h"
#include "ShipDesign.h"
#include "Planet.h"
#include "System.h"
#include "Species.h"
#include "Meter.h"
#include "ValueRef.h"
#include "../Empire/Empire.h"
#include "../Empire/EmpireManager.h"

#include <boost/bind.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/st_connected.hpp>

using boost::io::str;

extern int g_indent;

namespace {
    void AddAllObjectsSet(Condition::ObjectSet& condition_non_targets) {
        condition_non_targets.reserve(condition_non_targets.size() + Objects().NumExistingObjects());
        std::transform( Objects().ExistingObjectsBegin(), Objects().ExistingObjectsEnd(),
                        std::back_inserter(condition_non_targets),
                        boost::bind(&std::map< int, TemporaryPtr< UniverseObject > >::value_type::second,_1) );
    }

    void AddBuildingSet(Condition::ObjectSet& condition_non_targets) {
        condition_non_targets.reserve(condition_non_targets.size() + Objects().NumExistingBuildings());
        std::transform( Objects().ExistingBuildingsBegin(), Objects().ExistingBuildingsEnd(),
                        std::back_inserter(condition_non_targets),
                        boost::bind(&std::map< int, TemporaryPtr< UniverseObject > >::value_type::second,_1) );
    }

    void AddFieldSet(Condition::ObjectSet& condition_non_targets) {
        condition_non_targets.reserve(condition_non_targets.size() + Objects().NumExistingFields());
        std::transform( Objects().ExistingFieldsBegin(), Objects().ExistingFieldsEnd(),
                        std::back_inserter(condition_non_targets),
                        boost::bind(&std::map< int, TemporaryPtr< UniverseObject > >::value_type::second,_1) );
    }

    void AddFleetSet(Condition::ObjectSet& condition_non_targets) {
        condition_non_targets.reserve(condition_non_targets.size() + Objects().NumExistingFleets());
        std::transform( Objects().ExistingFleetsBegin(), Objects().ExistingFleetsEnd(),
                        std::back_inserter(condition_non_targets),
                        boost::bind(&std::map< int, TemporaryPtr< UniverseObject > >::value_type::second,_1) );
    }

    void AddPlanetSet(Condition::ObjectSet& condition_non_targets) {
        condition_non_targets.reserve(condition_non_targets.size() + Objects().NumExistingPlanets());
        std::transform( Objects().ExistingPlanetsBegin(), Objects().ExistingPlanetsEnd(),
                        std::back_inserter(condition_non_targets),
                        boost::bind(&std::map< int, TemporaryPtr< UniverseObject > >::value_type::second,_1) );
    }

    void AddPopCenterSet(Condition::ObjectSet& condition_non_targets) {
        condition_non_targets.reserve(condition_non_targets.size() + Objects().NumExistingPopCenters());
        std::transform( Objects().ExistingPopCentersBegin(), Objects().ExistingPopCentersEnd(),
                        std::back_inserter(condition_non_targets),
                        boost::bind(&std::map< int, TemporaryPtr< UniverseObject > >::value_type::second,_1) );
    }

    void AddResCenterSet(Condition::ObjectSet& condition_non_targets) {
        condition_non_targets.reserve(condition_non_targets.size() + Objects().NumExistingResourceCenters());
        std::transform( Objects().ExistingResourceCentersBegin(), Objects().ExistingResourceCentersEnd(),
                        std::back_inserter(condition_non_targets),
                        boost::bind(&std::map< int, TemporaryPtr< UniverseObject > >::value_type::second,_1) );
    }

    void AddShipSet(Condition::ObjectSet& condition_non_targets) {
        condition_non_targets.reserve(condition_non_targets.size() + Objects().NumExistingShips());
        std::transform( Objects().ExistingShipsBegin(), Objects().ExistingShipsEnd(),
                        std::back_inserter(condition_non_targets),
                        boost::bind(&std::map< int, TemporaryPtr< UniverseObject > >::value_type::second,_1) );
    }

    void AddSystemSet(Condition::ObjectSet& condition_non_targets) {
        condition_non_targets.reserve(condition_non_targets.size() + Objects().NumExistingSystems());
        std::transform( Objects().ExistingSystemsBegin(), Objects().ExistingSystemsEnd(),
                        std::back_inserter(condition_non_targets),
                        boost::bind(&std::map< int, TemporaryPtr< UniverseObject > >::value_type::second,_1) );
    }

    TemporaryPtr<const Fleet> FleetFromObject(TemporaryPtr<const UniverseObject> obj) {
        TemporaryPtr<const Fleet> retval = boost::dynamic_pointer_cast<const Fleet>(obj);
        if (!retval) {
            if (TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(obj))
                retval = GetFleet(ship->FleetID());
        }
        return retval;
    }

    template <class Pred>
    void EvalImpl(Condition::ObjectSet& matches, Condition::ObjectSet& non_matches,
                  Condition::SearchDomain search_domain, const Pred& pred)
    {
        Condition::ObjectSet& from_set = search_domain == Condition::MATCHES ? matches : non_matches;
        Condition::ObjectSet& to_set = search_domain == Condition::MATCHES ? non_matches : matches;
        for (Condition::ObjectSet::iterator it = from_set.begin(); it != from_set.end(); ) {
            bool match = pred(*it);
            if ((search_domain == Condition::MATCHES && !match) || (search_domain == Condition::NON_MATCHES && match)) {
                to_set.push_back(*it);
                *it = from_set.back();
                from_set.pop_back();
            } else {
                ++it;
            }
        }
    }

    std::vector<const Condition::ConditionBase*> FlattenAndNestedConditions(
        const std::vector<const Condition::ConditionBase*>& input_conditions)
    {
        std::vector<const Condition::ConditionBase*> retval;
        for (std::vector<const Condition::ConditionBase*>::const_iterator it = input_conditions.begin();
             it != input_conditions.end(); ++it)
        {
            if (const Condition::And* and_condition = dynamic_cast<const Condition::And*>(*it)) {
                std::vector<const Condition::ConditionBase*> flattened_operands =
                    FlattenAndNestedConditions(and_condition->Operands());
                std::copy(flattened_operands.begin(), flattened_operands.end(), std::back_inserter(retval));
            } else {
                if (*it)
                    retval.push_back(*it);
            }
        }
        return retval;
    }

    std::map<std::string, bool> ConditionDescriptionAndTest(const std::vector<const Condition::ConditionBase*>& conditions,
                                                            const ScriptingContext& parent_context,
                                                            TemporaryPtr<const UniverseObject> candidate_object/* = 0*/)
    {
        std::map<std::string, bool> retval;

        std::vector<const Condition::ConditionBase*> flattened_conditions;
        if (conditions.empty())
            return retval;
        else if (conditions.size() > 1 || dynamic_cast<const Condition::And*>(*conditions.begin()))
            flattened_conditions = FlattenAndNestedConditions(conditions);
        //else if (dynamic_cast<const Condition::Or*>(*conditions.begin()))
        //    flattened_conditions = FlattenOrNestedConditions(conditions);
        else
            flattened_conditions = conditions;

        for (std::vector<const Condition::ConditionBase*>::const_iterator it = flattened_conditions.begin();
             it != flattened_conditions.end(); ++it)
        {
            const Condition::ConditionBase* condition = *it;
            retval[condition->Description()] = condition->Eval(parent_context, candidate_object);
        }
        return retval;
    }
}

std::string ConditionDescription(const std::vector<const Condition::ConditionBase*>& conditions,
                                 TemporaryPtr<const UniverseObject> candidate_object/* = 0*/,
                                 TemporaryPtr<const UniverseObject> source_object/* = 0*/)
{
    if (conditions.empty())
        return UserString("NONE");

    ScriptingContext parent_context(source_object);
    // test candidate against all input conditions, and store descriptions of each
    std::map<std::string, bool> condition_description_and_test_results =
        ConditionDescriptionAndTest(conditions, parent_context, candidate_object);
    bool all_conditions_match_candidate = true, at_least_one_condition_matches_candidate = false;
    for (std::map<std::string, bool>::const_iterator it = condition_description_and_test_results.begin();
         it != condition_description_and_test_results.end(); ++it)
    {
        all_conditions_match_candidate = all_conditions_match_candidate && it->second;
        at_least_one_condition_matches_candidate = at_least_one_condition_matches_candidate || it->second;
    }

    // concatenate (non-duplicated) single-description results
    std::string retval;
    if (conditions.size() > 1 || dynamic_cast<const Condition::And*>(*conditions.begin())) {
        retval += UserString("ALL_OF") + " ";
        retval += (all_conditions_match_candidate ? UserString("PASSED") : UserString("FAILED")) + "\n";
    } else if (dynamic_cast<const Condition::Or*>(*conditions.begin())) {
        retval += UserString("ANY_OF") + " ";
        retval += (at_least_one_condition_matches_candidate ? UserString("PASSED") : UserString("FAILED")) + "\n";
    }
    // else just output single condition description and PASS/FAIL text

    for (std::map<std::string, bool>::const_iterator it = condition_description_and_test_results.begin();
         it != condition_description_and_test_results.end(); ++it)
    {
        retval += (it->second ? UserString("PASSED") : UserString("FAILED"));
        retval += " " + it->first + "\n";
    }
    return retval;
}

#define CHECK_COND_VREF_MEMBER(m_ptr) { if (m_ptr == rhs_.m_ptr) {              \
                                            /* check next member */             \
                                        } else if (!m_ptr || !rhs_.m_ptr) {     \
                                            return false;                       \
                                        } else {                                \
                                            if (*m_ptr != *(rhs_.m_ptr))        \
                                                return false;                   \
                                        }   }

///////////////////////////////////////////////////////////
// Condition::ConditionBase                              //
///////////////////////////////////////////////////////////
struct Condition::ConditionBase::MatchHelper {
    MatchHelper(const Condition::ConditionBase* this_, const ScriptingContext& parent_context) :
        m_this(this_),
        m_parent_context(parent_context)
    {}
    bool operator()(TemporaryPtr<const UniverseObject> candidate) const
    { return m_this->Match(ScriptingContext(m_parent_context, candidate)); }
    const Condition::ConditionBase* m_this;
    const ScriptingContext& m_parent_context;
};

Condition::ConditionBase::~ConditionBase()
{}

bool Condition::ConditionBase::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;

    if (!this)
        return false;

    if (typeid(*this) != typeid(rhs))
        return false;

    return true;
}

void Condition::ConditionBase::Eval(const ScriptingContext& parent_context,
                                    ObjectSet& matches, ObjectSet& non_matches,
                                    SearchDomain search_domain/* = NON_MATCHES*/) const
{ EvalImpl(matches, non_matches, search_domain, MatchHelper(this, parent_context)); }

void Condition::ConditionBase::Eval(ObjectSet& matches, ObjectSet& non_matches,
                                    SearchDomain search_domain/* = NON_MATCHES*/) const
{ Eval(ScriptingContext(), matches, non_matches, search_domain); }

void Condition::ConditionBase::Eval(const ScriptingContext& parent_context,
                                    Condition::ObjectSet& matches) const
{
    matches.clear();
    // evaluate condition on all non-destroyed objects in Universe
    Condition::ObjectSet condition_initial_candidates;
    GetDefaultInitialCandidateObjects(parent_context, condition_initial_candidates);
    matches.reserve(condition_initial_candidates.size());
    Eval(parent_context, matches, condition_initial_candidates);
}

void Condition::ConditionBase::Eval(Condition::ObjectSet& matches) const
{ Eval(ScriptingContext(), matches); }

bool Condition::ConditionBase::Eval(const ScriptingContext& parent_context,
                                    TemporaryPtr<const UniverseObject> candidate) const
{
    if (!candidate)
        return false;
    Condition::ObjectSet non_matches, matches;
    non_matches.push_back(candidate);
    Eval(parent_context, matches, non_matches);
    return non_matches.empty(); // if candidate has been matched, non_matches will now be empty
}

bool Condition::ConditionBase::Eval(TemporaryPtr<const UniverseObject> candidate) const {
    if (!candidate)
        return false;
    Condition::ObjectSet non_matches, matches;
    non_matches.push_back(candidate);
    Eval(ScriptingContext(), matches, non_matches);
    return non_matches.empty(); // if candidate has been matched, non_matches will now be empty
}

void Condition::ConditionBase::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                                 Condition::ObjectSet& condition_non_targets) const
{
    AddAllObjectsSet(condition_non_targets);
}

std::string Condition::ConditionBase::Description(bool negated/* = false*/) const
{ return ""; }

std::string Condition::ConditionBase::Dump() const
{ return ""; }

bool Condition::ConditionBase::Match(const ScriptingContext& local_context) const
{ return false; }

///////////////////////////////////////////////////////////
// Number                                                //
///////////////////////////////////////////////////////////
Condition::Number::~Number()
{
    delete m_low;
    delete m_high;
    delete m_condition;
}

bool Condition::Number::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::Number& rhs_ = static_cast<const Condition::Number&>(rhs);

    CHECK_COND_VREF_MEMBER(m_low)
    CHECK_COND_VREF_MEMBER(m_high)
    CHECK_COND_VREF_MEMBER(m_condition)

    return true;
}

std::string Condition::Number::Description(bool negated/* = false*/) const {
    std::string low_str = (m_low ? (ValueRef::ConstantExpr(m_low) ?
                                    boost::lexical_cast<std::string>(m_low->Eval()) :
                                    m_low->Description())
                                 : "0");
    std::string high_str = (m_high ? (ValueRef::ConstantExpr(m_high) ?
                                      boost::lexical_cast<std::string>(m_high->Eval()) :
                                      m_high->Description())
                                   : boost::lexical_cast<std::string>(INT_MAX));

    const std::string& description_str = (!negated)
        ? UserString("DESC_NUMBER")
        : UserString("DESC_NUMBER_NOT");
    return str(FlexibleFormat(description_str)
               % low_str
               % high_str
               % m_condition->Description());
}

std::string Condition::Number::Dump() const {
    std::string retval = DumpIndent() + "Number";
    if (m_low)
        retval += " low = " + m_low->Dump();
    if (m_high)
        retval += " high = " + m_high->Dump();
    retval += " condition =\n";
    ++g_indent;
        retval += m_condition->Dump();
    --g_indent;
    return retval;
}

void Condition::Number::Eval(const ScriptingContext& parent_context,
                             ObjectSet& matches, ObjectSet& non_matches,
                             SearchDomain search_domain/* = NON_MATCHES*/) const
{
    // Number does not have a single valid local candidate to be matched, as it
    // will match anything if the proper number of objects match the
    // subcondition.  So, the local context that is passed to the subcondition
    // needs to have a null local candidate.
    TemporaryPtr<const UniverseObject> no_object;
    ScriptingContext local_context(parent_context, no_object);

    if (!(
                (!m_low || m_low->LocalCandidateInvariant())
             && (!m_high || m_high->LocalCandidateInvariant())
         )
       )
    {
        Logger().errorStream() << "Condition::Number::Eval has local candidate-dependent ValueRefs, but no valid local candidate!";
    } else if (
                !local_context.condition_root_candidate
                && !(
                        (!m_low || m_low->RootCandidateInvariant())
                     && (!m_high || m_high->RootCandidateInvariant())
                    )
              )
    {
        Logger().errorStream() << "Condition::Number::Eval has root candidate-dependent ValueRefs, but expects local candidate to be the root candidate, and has no valid local candidate!";
    }

    if (!local_context.condition_root_candidate && !this->RootCandidateInvariant()) {
        // no externally-defined root candidate, so each object matched must
        // separately act as a root candidate, and sub-condition must be re-
        // evaluated for each tested object and the number of objects matched
        // checked for each object being tested
        Condition::ConditionBase::Eval(local_context, matches, non_matches, search_domain);

    } else {
        // parameters for number of subcondition objects that needs to be matched
        int low = (m_low ? m_low->Eval(local_context) : 0);
        int high = (m_high ? m_high->Eval(local_context) : INT_MAX);

        // get set of all UniverseObjects that satisfy m_condition
        ObjectSet condition_matches;
        // can evaluate subcondition once for all objects being tested by this condition
        m_condition->Eval(local_context, condition_matches);
        // compare number of objects that satisfy m_condition to the acceptable range of such objects
        int matched = condition_matches.size();
        bool in_range = (low <= matched && matched <= high);

        // transfer objects to or from candidate set, according to whether number of matches was within
        // the requested range.
        if (search_domain == MATCHES && !in_range) {
            non_matches.insert(non_matches.end(), matches.begin(), matches.end());
            matches.clear();
        }
        if (search_domain == NON_MATCHES && in_range) {
            matches.insert(matches.end(), non_matches.begin(), non_matches.end());
            non_matches.clear();
        }
    }
}

bool Condition::Number::RootCandidateInvariant() const {
    return (!m_low || m_low->RootCandidateInvariant()) &&
           (!m_high || m_high->RootCandidateInvariant()) &&
           m_condition->RootCandidateInvariant();
}

bool Condition::Number::TargetInvariant() const {
    return (!m_low || m_low->TargetInvariant()) &&
           (!m_high || m_high->TargetInvariant()) &&
           m_condition->TargetInvariant();
}

bool Condition::Number::SourceInvariant() const {
    return (!m_low || m_low->SourceInvariant()) &&
           (!m_high || m_high->SourceInvariant()) &&
           m_condition->SourceInvariant();
}

bool Condition::Number::Match(const ScriptingContext& local_context) const {
    // get acceptable range of subcondition matches for candidate
    int low = (m_low ? std::max(0, m_low->Eval(local_context)) : 0);
    int high = (m_high ? std::min(m_high->Eval(local_context), INT_MAX) : INT_MAX);

    // get set of all UniverseObjects that satisfy m_condition
    ObjectSet condition_matches;
    m_condition->Eval(local_context, condition_matches);

    // compare number of objects that satisfy m_condition to the acceptable range of such objects
    int matched = condition_matches.size();
    bool in_range = (low <= matched && matched <= high);
    return in_range;
}

///////////////////////////////////////////////////////////
// Turn                                                  //
///////////////////////////////////////////////////////////
Condition::Turn::~Turn() {
    delete m_low;
    delete m_high;
}

bool Condition::Turn::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::Turn& rhs_ = static_cast<const Condition::Turn&>(rhs);

    CHECK_COND_VREF_MEMBER(m_low)
    CHECK_COND_VREF_MEMBER(m_high)

    return true;
}

void Condition::Turn::Eval(const ScriptingContext& parent_context,
                           ObjectSet& matches, ObjectSet& non_matches,
                           SearchDomain search_domain/* = NON_MATCHES*/) const
{
    // if ValueRef for low or high range limits depend on local candidate, then
    // they must be evaluated per-candidate.
    // if there already is a root candidate, then this condition's parameters
    // can be evaluated assuming it will not change.
    // if there is no root candidate in the parent context, then this
    // condition's candidates will be the root candidates, and this condition's
    // parameters must be root candidate invariant or else must be evaluated
    // per-candidate
    bool simple_eval_safe = ((!m_low || m_low->LocalCandidateInvariant()) &&
                             (!m_high || m_high->LocalCandidateInvariant()) &&
                             (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate turn limits once, check range, and use result to match or
        // reject all the search domain, since the current turn doesn't change
        // from object to object, and neither do the range limits.
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);
        int low =  (m_low ? std::max(BEFORE_FIRST_TURN, m_low->Eval(local_context)) : BEFORE_FIRST_TURN);
        int high = (m_high ? std::min(m_high->Eval(local_context), IMPOSSIBLY_LARGE_TURN) : IMPOSSIBLY_LARGE_TURN);
        int turn = CurrentTurn();
        bool match = (low <= turn && turn <= high);

        if (match && search_domain == NON_MATCHES) {
            // move all objects from non_matches to matches
            matches.insert(matches.end(), non_matches.begin(), non_matches.end());
            non_matches.clear();
        } else if (!match && search_domain == MATCHES) {
            // move all objects from matches to non_matches
            non_matches.insert(non_matches.end(), matches.begin(), matches.end());
            matches.clear();
        }
    } else {
        // re-evaluate allowed turn range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::Turn::RootCandidateInvariant() const
{ return (!m_low || m_low->RootCandidateInvariant()) && (!m_high || m_high->RootCandidateInvariant()); }

bool Condition::Turn::TargetInvariant() const
{ return (!m_low || m_low->TargetInvariant()) && (!m_high || m_high->TargetInvariant()); }

bool Condition::Turn::SourceInvariant() const
{ return (!m_low || m_low->SourceInvariant()) && (!m_high || m_high->SourceInvariant()); }

std::string Condition::Turn::Description(bool negated/* = false*/) const {
    std::string low_str;
    if (m_low)
        low_str = (ValueRef::ConstantExpr(m_low) ?
                   boost::lexical_cast<std::string>(m_low->Eval()) :
                   m_low->Description());
    std::string high_str;
    if (m_high)
        high_str = (ValueRef::ConstantExpr(m_high) ?
                    boost::lexical_cast<std::string>(m_high->Eval()) :
                    m_high->Description());
    std::string description_str;
    if (m_low && m_high)
    {
        description_str = (!negated)
            ? UserString("DESC_TURN")
            : UserString("DESC_TURN_NOT");
        return str(FlexibleFormat(description_str)
                   % low_str
                   % high_str);
    }
    else if (m_low)
    {
        description_str = (!negated)
            ? UserString("DESC_TURN_MIN_ONLY")
            : UserString("DESC_TURN_MIN_ONLY_NOT");
        return str(FlexibleFormat(description_str)
                   % low_str);
    }
    else if (m_high)
    {
        description_str = (!negated)
            ? UserString("DESC_TURN_MAX_ONLY")
            : UserString("DESC_TURN_MAX_ONLY_NOT");
        return str(FlexibleFormat(description_str)
                   % high_str);
    }
    else
    {
        return (!negated)
            ? UserString("DESC_TURN_ANY")
            : UserString("DESC_TURN_ANY_NOT");
    }
}

std::string Condition::Turn::Dump() const {
    std::string retval = DumpIndent() + "Turn";
    if (m_low)
        retval += " low = " + m_low->Dump();
    if (m_high)
        retval += " high = " + m_high->Dump();
    retval += "\n";
    return retval;
}

bool Condition::Turn::Match(const ScriptingContext& local_context) const {
    int low = (m_low ? std::max(0, m_low->Eval(local_context)) : 0);
    int high = (m_high ? std::min(m_high->Eval(local_context), IMPOSSIBLY_LARGE_TURN) : IMPOSSIBLY_LARGE_TURN);
    int turn = CurrentTurn();
    return (low <= turn && turn <= high);
}

///////////////////////////////////////////////////////////
// SortedNumberOf                                        //
///////////////////////////////////////////////////////////
Condition::SortedNumberOf::~SortedNumberOf() {
    delete m_number;
    delete m_sort_key;
    delete m_condition;
}

bool Condition::SortedNumberOf::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::SortedNumberOf& rhs_ = static_cast<const Condition::SortedNumberOf&>(rhs);

    if (m_sorting_method != rhs_.m_sorting_method)
        return false;

    CHECK_COND_VREF_MEMBER(m_number)
    CHECK_COND_VREF_MEMBER(m_sort_key)
    CHECK_COND_VREF_MEMBER(m_condition)

    return true;
}

namespace {
    /** Random number genrator function to use with random_shuffle */
    int CustomRandInt(int max_plus_one) {
        return RandSmallInt(0, max_plus_one - 1);
    }
    int (*CRI)(int) = CustomRandInt;

    /** Transfers the indicated \a number of objects, randomly selected from from_set to to_set */
    void TransferRandomObjects(unsigned int number, Condition::ObjectSet& from_set, Condition::ObjectSet& to_set) {
        // ensure number of objects to be moved is within reasonable range
        number = std::min<unsigned int>(number, from_set.size());
        if (number == 0)
            return;

        // create list of bool flags to indicate whether each item in from_set
        // with corresponding place in iteration order should be transfered
        std::vector<bool> transfer_flags(from_set.size(), false);   // initialized to all false

        // set first  number  flags to true
        std::fill_n(transfer_flags.begin(), number, true);

        // shuffle flags to randomize which flags are set
        std::random_shuffle(transfer_flags.begin(), transfer_flags.end(), CRI);

        // transfer objects that have been flagged
        int i = 0;
        for (Condition::ObjectSet::iterator it = from_set.begin(); it != from_set.end(); ++i) {
            if (transfer_flags[i]) {
                to_set.push_back(*it);
                *it = from_set.back();
                from_set.pop_back();
            } else {
                ++it;
            }
        }
    }

    /** Transfers the indicated \a number of objects, selected from \a from_set
      * into \a to_set.  The objects transferred are selected based on the value
      * of \a sort_key evaluated on them, with the largest / smallest / most
      * common sort keys chosen, or a random selection chosen, depending on the
      * specified \a sorting_method */
    void TransferSortedObjects(unsigned int number, const ValueRef::ValueRefBase<double>* sort_key,
                               const ScriptingContext& context, Condition::SortingMethod sorting_method,
                               Condition::ObjectSet& from_set, Condition::ObjectSet& to_set)
    {
        // handle random case, which doesn't need sorting key
        if (sorting_method == Condition::SORT_RANDOM) {
            TransferRandomObjects(number, from_set, to_set);
            return;
        }

        // for other SoringMethods, need sort key values
        if (!sort_key) {
            Logger().errorStream() << "TransferSortedObjects given null sort_key";
            return;
        }

        // get sort key values for all objects in from_set, and sort by inserting into map
        std::multimap<float, TemporaryPtr<const UniverseObject> > sort_key_objects;
        for (Condition::ObjectSet::const_iterator it = from_set.begin(); it != from_set.end(); ++it) {
            float sort_value = sort_key->Eval(ScriptingContext(context, *it));
            sort_key_objects.insert(std::make_pair(sort_value, *it));
        }

        // how many objects to select?
        number = std::min<unsigned int>(number, sort_key_objects.size());
        if (number == 0)
            return;
        unsigned int number_transferred(0);

        // pick max / min / most common values
        if (sorting_method == Condition::SORT_MIN) {
            // move (number) objects with smallest sort key (at start of map)
            // from the from_set into the to_set.
            for (std::multimap<float, TemporaryPtr<const UniverseObject> >::const_iterator sorted_it = sort_key_objects.begin();
                 sorted_it != sort_key_objects.end(); ++sorted_it)
            {
                TemporaryPtr<const UniverseObject> object_to_transfer = sorted_it->second;
                Condition::ObjectSet::iterator from_it = std::find(from_set.begin(), from_set.end(), object_to_transfer);
                if (from_it != from_set.end()) {
                    *from_it = from_set.back();
                    from_set.pop_back();
                    to_set.push_back(object_to_transfer);
                    number_transferred++;
                    if (number_transferred >= number)
                        return;
                }
            }
        } else if (sorting_method == Condition::SORT_MAX) {
            // move (number) objects with largest sort key (at end of map)
            // from the from_set into the to_set.
            for (std::multimap<float, TemporaryPtr<const UniverseObject> >::reverse_iterator sorted_it = sort_key_objects.rbegin();  // would use const_reverse_iterator but this causes a compile error in some compilers
                 sorted_it != sort_key_objects.rend(); ++sorted_it)
            {
                TemporaryPtr<const UniverseObject> object_to_transfer = sorted_it->second;
                Condition::ObjectSet::iterator from_it = std::find(from_set.begin(), from_set.end(), object_to_transfer);
                if (from_it != from_set.end()) {
                    *from_it = from_set.back();
                    from_set.pop_back();
                    to_set.push_back(object_to_transfer);
                    number_transferred++;
                    if (number_transferred >= number)
                        return;                }
            }
        } else if (sorting_method == Condition::SORT_MODE) {
            // compile histogram of of number of times each sort key occurs
            std::map<float, unsigned int> histogram;
            for (std::multimap<float, TemporaryPtr<const UniverseObject> >::const_iterator sorted_it = sort_key_objects.begin();
                 sorted_it != sort_key_objects.end(); ++sorted_it)
            {
                histogram[sorted_it->first]++;
            }
            // invert histogram to index by number of occurances
            std::multimap<unsigned int, float> inv_histogram;
            for (std::map<float, unsigned int>::const_iterator hist_it = histogram.begin();
                 hist_it != histogram.end(); ++hist_it)
            {
                inv_histogram.insert(std::make_pair(hist_it->second, hist_it->first));
            }
            // reverse-loop through inverted histogram to find which sort keys
            // occurred most frequently, and transfer objects with those sort
            // keys from from_set to to_set.
            for (std::multimap<unsigned int, float>::reverse_iterator inv_hist_it = inv_histogram.rbegin();  // would use const_reverse_iterator but this causes a compile error in some compilers
                 inv_hist_it != inv_histogram.rend(); ++inv_hist_it)
            {
                float cur_sort_key = inv_hist_it->second;

                // get range of objects with the current sort key
                std::pair<std::multimap<float, TemporaryPtr<const UniverseObject> >::const_iterator,
                          std::multimap<float, TemporaryPtr<const UniverseObject> >::const_iterator> key_range =
                    sort_key_objects.equal_range(cur_sort_key);

                // loop over range, selecting objects to transfer from from_set to to_set
                for (std::multimap<float, TemporaryPtr<const UniverseObject> >::const_iterator sorted_it = key_range.first;
                     sorted_it != key_range.second; ++sorted_it)
                {
                    TemporaryPtr<const UniverseObject> object_to_transfer = sorted_it->second;
                    Condition::ObjectSet::iterator from_it = std::find(from_set.begin(), from_set.end(), object_to_transfer);
                    if (from_it != from_set.end()) {
                        *from_it = from_set.back();
                        from_set.pop_back();
                        to_set.push_back(object_to_transfer);
                        number_transferred++;
                        if (number_transferred >= number)
                            return;
                    }
                }
            }
        } else {
            Logger().debugStream() << "TransferSortedObjects given unknown sort method";
        }
    }
}

void Condition::SortedNumberOf::Eval(const ScriptingContext& parent_context,
                                     Condition::ObjectSet& matches, Condition::ObjectSet& non_matches,
                                     SearchDomain search_domain/* = NON_MATCHES*/) const
{
    // Most conditions match objects independently of the other objects being
    // tested, but the number parameter for NumberOf conditions makes things
    // more complicated.  In order to match some number of the potential
    // matchs property, both the matches and non_matches need to be checked
    // against the subcondition, and the total number of subcondition matches
    // counted.
    // Then, when searching NON_MATCHES, non_matches may be moved into matches
    // so that the number of subcondition matches in matches is equal to the
    // requested number.  There may also be subcondition non-matches in
    // matches, but these are not counted or affected by this condition.
    // Or, when searching MATCHES, matches may be moved into non_matches so
    // that the number of subcondition matches in matches is equal to the
    // requested number.  There again may be subcondition non-matches in
    // matches, but these are also not counted or affected by this condition.

    // SortedNumberOf does not have a valid local candidate to be matched
    // before the subcondition is evaluated, so the local context that is
    // passed to the subcondition needs to have a null local candidate.
    TemporaryPtr<const UniverseObject> no_object;
    ScriptingContext local_context(parent_context, no_object);

    // which input matches match the subcondition?
    ObjectSet subcondition_matching_matches;
    subcondition_matching_matches.reserve(matches.size());
    m_condition->Eval(local_context, subcondition_matching_matches, matches, NON_MATCHES);

    // remaining input matches don't match the subcondition...
    ObjectSet subcondition_non_matching_matches = matches;
    matches.clear();    // to be refilled later

    // which input non_matches match the subcondition?
    ObjectSet subcondition_matching_non_matches;
    subcondition_matching_non_matches.reserve(non_matches.size());
    m_condition->Eval(local_context, subcondition_matching_non_matches, non_matches, NON_MATCHES);

    // remaining input non_matches don't match the subcondition...
    ObjectSet subcondition_non_matching_non_matches = non_matches;
    non_matches.clear();    // to be refilled later

    // assemble single set of subcondition matching objects
    ObjectSet all_subcondition_matches;
    all_subcondition_matches.reserve(subcondition_matching_matches.size() + subcondition_matching_non_matches.size());
    all_subcondition_matches.insert(all_subcondition_matches.end(), subcondition_matching_matches.begin(), subcondition_matching_matches.end());
    all_subcondition_matches.insert(all_subcondition_matches.end(), subcondition_matching_non_matches.begin(), subcondition_matching_non_matches.end());

    // how many subcondition matches to select as matches to this condition
    int number = m_number->Eval(local_context);

    // compile single set of all objects that are matched by this condition.
    // these are the objects that should be transferred from non_matches into
    // matches, or those left in matches while the rest are moved into non_matches
    ObjectSet matched_objects;
    matched_objects.reserve(number);
    TransferSortedObjects(number, m_sort_key, local_context, m_sorting_method, all_subcondition_matches, matched_objects);

    // put objects back into matches and non_target sets as output...

    if (search_domain == NON_MATCHES) {
        // put matched objects that are in subcondition_matching_non_matches into matches
        for (ObjectSet::const_iterator match_it = matched_objects.begin(); match_it != matched_objects.end(); ++match_it) {
            TemporaryPtr<const UniverseObject> matched_object = *match_it;

            // is this matched object in subcondition_matching_non_matches?
            ObjectSet::iterator smnt_it = std::find(subcondition_matching_non_matches.begin(), subcondition_matching_non_matches.end(), matched_object);
            if (smnt_it != subcondition_matching_non_matches.end()) {
                // yes; move object to matches
                *smnt_it = subcondition_matching_non_matches.back();
                subcondition_matching_non_matches.pop_back();
                matches.push_back(matched_object);
            }
        }

        // put remaining (non-matched) objects in subcondition_matching_non_matches back into non_matches
        non_matches.insert( non_matches.end(), subcondition_matching_non_matches.begin(),      subcondition_matching_non_matches.end());
        // put objects in subcondition_non_matching_non_matches back into non_matches
        non_matches.insert( non_matches.end(), subcondition_non_matching_non_matches.begin(),  subcondition_non_matching_non_matches.end());
        // put objects in subcondition_matching_matches and subcondition_non_matching_matches back into matches
        matches.insert(     matches.end(),     subcondition_matching_matches.begin(),          subcondition_matching_matches.end());
        matches.insert(     matches.end(),     subcondition_non_matching_matches.begin(),      subcondition_non_matching_matches.end());
        // this leaves the original contents of matches unchanged, other than
        // possibly having transferred some objects into matches from non_matches

    } else { /*(search_domain == MATCHES)*/
        // put matched objecs that are in subcondition_matching_matches back into matches
        for (ObjectSet::const_iterator match_it = matched_objects.begin(); match_it != matched_objects.end(); ++match_it) {
            TemporaryPtr<const UniverseObject> matched_object = *match_it;

            // is this matched object in subcondition_matching_matches?
            ObjectSet::iterator smt_it = std::find(subcondition_matching_matches.begin(), subcondition_matching_matches.end(), matched_object);
            if (smt_it != subcondition_matching_matches.end()) {
                // yes; move back into matches
                *smt_it = subcondition_matching_matches.back();
                subcondition_matching_matches.pop_back();
                matches.push_back(matched_object);
            }
        }

        // put remaining (non-matched) objects in subcondition_matching_matches) into non_matches
        non_matches.insert( non_matches.end(), subcondition_matching_matches.begin(),          subcondition_matching_matches.end());
        // put objects in subcondition_non_matching_matches into non_matches
        non_matches.insert( non_matches.end(), subcondition_non_matching_matches.begin(),      subcondition_non_matching_matches.end());
        // put objects in subcondition_matching_non_matches and subcondition_non_matching_non_matches back into non_matches
        non_matches.insert( non_matches.end(), subcondition_matching_non_matches.begin(),      subcondition_matching_non_matches.end());
        non_matches.insert( non_matches.end(), subcondition_non_matching_non_matches.begin(),  subcondition_non_matching_non_matches.end());
        // this leaves the original contents of non_matches unchanged, other than
        // possibly having transferred some objects into non_matches from matches
    }
}

bool Condition::SortedNumberOf::RootCandidateInvariant() const
{ return ((!m_number || m_number->SourceInvariant()) &&
          (!m_sort_key || m_sort_key->SourceInvariant()) &&
          (!m_condition || m_condition->SourceInvariant())); }

bool Condition::SortedNumberOf::TargetInvariant() const
{ return ((!m_number || m_number->SourceInvariant()) &&
          (!m_sort_key || m_sort_key->SourceInvariant()) &&
          (!m_condition || m_condition->SourceInvariant())); }

bool Condition::SortedNumberOf::SourceInvariant() const
{ return ((!m_number || m_number->SourceInvariant()) &&
          (!m_sort_key || m_sort_key->SourceInvariant()) &&
          (!m_condition || m_condition->SourceInvariant())); }

std::string Condition::SortedNumberOf::Description(bool negated/* = false*/) const {
    std::string number_str = ValueRef::ConstantExpr(m_number) ? boost::lexical_cast<std::string>(m_number->Dump()) : m_number->Description();

    if (m_sorting_method == SORT_RANDOM) {
        return str(FlexibleFormat((!negated)
                                  ? UserString("DESC_NUMBER_OF")
                                  : UserString("DESC_NUMBER_OF_NOT")
                                 )
                   % number_str
                   % m_condition->Description());
    } else {
        std::string sort_key_str = ValueRef::ConstantExpr(m_sort_key) ? boost::lexical_cast<std::string>(m_sort_key->Dump()) : m_sort_key->Description();

        std::string description_str, temp;
        switch (m_sorting_method) {
        case SORT_MAX:
            description_str = (!negated)
                ? UserString("DESC_MAX_NUMBER_OF")
                : UserString("DESC_MAX_NUMBER_OF_NOT");
            break;

        case SORT_MIN:
            description_str = (!negated)
                ? UserString("DESC_MIN_NUMBER_OF")
                : UserString("DESC_MIN_NUMBER_OF_NOT");
            break;

        case SORT_MODE:
            description_str = (!negated)
                ? UserString("DESC_MODE_NUMBER_OF")
                : UserString("DESC_MODE_NUMBER_OF_NOT");
            break;
        default:
            break;
        }

        return str(FlexibleFormat(description_str)
                   % number_str
                   % sort_key_str
                   % m_condition->Description());
    }
}

std::string Condition::SortedNumberOf::Dump() const {
    std::string retval = DumpIndent();
    switch (m_sorting_method) {
    case SORT_RANDOM:
        retval += "NumberOf";   break;
    case SORT_MAX:
        retval += "MaximumNumberOf";  break;
    case SORT_MIN:
        retval += "MinimumNumberOf"; break;
    case SORT_MODE:
        retval += "ModeNumberOf"; break;
    default:
        retval += "??NumberOf??"; break;
    }

    retval += " number = " + m_number->Dump();

    if (m_sort_key)
         retval += " sortby = " + m_sort_key->Dump();

    retval += " condition =\n";
    ++g_indent;
        retval += m_condition->Dump();
    --g_indent;

    return retval;
}

void Condition::SortedNumberOf::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                                  Condition::ObjectSet& condition_non_targets) const
{
    if (m_condition) {
        m_condition->GetDefaultInitialCandidateObjects(parent_context, condition_non_targets);
        return;
    } else {
        ConditionBase::GetDefaultInitialCandidateObjects(parent_context, condition_non_targets);
    }
}

///////////////////////////////////////////////////////////
// All                                                   //
///////////////////////////////////////////////////////////
void Condition::All::Eval(const ScriptingContext& parent_context,
                          ObjectSet& matches, ObjectSet& non_matches,
                          SearchDomain search_domain/* = NON_MATCHES*/) const
{
    if (search_domain == NON_MATCHES) {
        // move all objects from non_matches to matches
        matches.insert(matches.end(), non_matches.begin(), non_matches.end());
        non_matches.clear();
    }
    // if search_comain is MATCHES, do nothing: all objects in matches set
    // match this condition, so should remain in matches set
}

bool Condition::All::operator==(const Condition::ConditionBase& rhs) const
{ return Condition::ConditionBase::operator==(rhs); }

std::string Condition::All::Description(bool negated/* = false*/) const {
    return (!negated)
        ? UserString("DESC_ALL")
        : UserString("DESC_ALL_NOT");
}

std::string Condition::All::Dump() const
{ return DumpIndent() + "All\n"; }

///////////////////////////////////////////////////////////
// EmpireAffiliation                                     //
///////////////////////////////////////////////////////////
Condition::EmpireAffiliation::~EmpireAffiliation() {
    if (m_empire_id)
        delete m_empire_id;
}

bool Condition::EmpireAffiliation::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::EmpireAffiliation& rhs_ = static_cast<const Condition::EmpireAffiliation&>(rhs);

    if (m_affiliation != rhs_.m_affiliation)
        return false;

    CHECK_COND_VREF_MEMBER(m_empire_id)

    return true;
}

namespace {
    struct EmpireAffiliationSimpleMatch {
        EmpireAffiliationSimpleMatch(int empire_id, EmpireAffiliationType affiliation) :
            m_empire_id(empire_id),
            m_affiliation(affiliation)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate || candidate->Unowned())
                return false;

            switch (m_affiliation) {
            case AFFIL_SELF:
                return m_empire_id != ALL_EMPIRES && candidate->OwnedBy(m_empire_id);
                break;

            case AFFIL_ENEMY: {
                if (m_empire_id == ALL_EMPIRES)
                    return true;
                if (m_empire_id == candidate->Owner())
                    return false;
                DiplomaticStatus status = Empires().GetDiplomaticStatus(m_empire_id, candidate->Owner());
                return (status == DIPLO_WAR);
                break;
            }

            case AFFIL_ALLY: {
                if (m_empire_id == ALL_EMPIRES)
                    return false;
                if (m_empire_id == candidate->Owner())
                    return false;
                DiplomaticStatus status = Empires().GetDiplomaticStatus(m_empire_id, candidate->Owner());
                return (status == DIPLO_PEACE);
                break;
            }

            case AFFIL_ANY:
                return true;
                //return !candidate->Unowned();
                break;

            default:
                return false;
                break;
            }
        }

        int m_empire_id;
        EmpireAffiliationType m_affiliation;
    };
}

void Condition::EmpireAffiliation::Eval(const ScriptingContext& parent_context,
                                        ObjectSet& matches, ObjectSet& non_matches,
                                        SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = (!m_empire_id || ValueRef::ConstantExpr(m_empire_id)) ||
                            ((!m_empire_id || m_empire_id->LocalCandidateInvariant()) &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate empire id once, and use to check all candidate objects
        TemporaryPtr<const UniverseObject> no_object;
        int empire_id = m_empire_id ? m_empire_id->Eval(ScriptingContext(parent_context, no_object)) : ALL_EMPIRES;
        EvalImpl(matches, non_matches, search_domain, EmpireAffiliationSimpleMatch(empire_id, m_affiliation));
    } else {
        // re-evaluate empire id for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::EmpireAffiliation::RootCandidateInvariant() const
{ return m_empire_id ? m_empire_id->RootCandidateInvariant() : true; }

bool Condition::EmpireAffiliation::TargetInvariant() const
{ return m_empire_id ? m_empire_id->TargetInvariant() : true; }

bool Condition::EmpireAffiliation::SourceInvariant() const
{ return m_empire_id ? m_empire_id->SourceInvariant() : true; }

std::string Condition::EmpireAffiliation::Description(bool negated/* = false*/) const {
    std::string empire_str;
    if (m_empire_id) {
        int empire_id = ALL_EMPIRES;
        if (ValueRef::ConstantExpr(m_empire_id))
            empire_id = m_empire_id->Eval();
        if (const Empire* empire = Empires().Lookup(empire_id))
            empire_str = empire->Name();
        else
            empire_str = m_empire_id->Description();
    }

    if (m_affiliation == AFFIL_SELF) {
        return str(FlexibleFormat((!negated)
            ? UserString("DESC_EMPIRE_AFFILIATION_SELF")
            : UserString("DESC_EMPIRE_AFFILIATION_SELF_NOT")) % empire_str);
    } else if (m_affiliation == AFFIL_ANY) {
        return (!negated)
            ? UserString("DESC_EMPIRE_AFFILIATION_ANY")
            : UserString("DESC_EMPIRE_AFFILIATION_ANY_NOT");
    } else {
        return str(FlexibleFormat((!negated)
            ? UserString("DESC_EMPIRE_AFFILIATION")
            : UserString("DESC_EMPIRE_AFFILIATION_NOT"))
                   % UserString(boost::lexical_cast<std::string>(m_affiliation))
                   % empire_str);
    }
}

std::string Condition::EmpireAffiliation::Dump() const {
    std::string retval = DumpIndent() + "OwnedBy";
    retval += " affiliation = ";
    switch (m_affiliation) {
    case AFFIL_SELF:    retval += "TheEmpire";  break;
    case AFFIL_ENEMY:   retval += "EnemyOf";    break;
    case AFFIL_ALLY:    retval += "AllyOf";     break;
    case AFFIL_ANY:     retval += "AnyEmpire";  break;
    default:            retval += "?";          break;
    }
    if (m_empire_id)
        retval += " empire = " + m_empire_id->Dump();
    retval += "\n";
    return retval;
}

bool Condition::EmpireAffiliation::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "EmpireAffiliation::Match passed no candidate object";
        return false;
    }

    int empire_id = m_empire_id ? m_empire_id->Eval(local_context) : ALL_EMPIRES;

    return EmpireAffiliationSimpleMatch(empire_id, m_affiliation)(candidate);
}

///////////////////////////////////////////////////////////
// Source                                                //
///////////////////////////////////////////////////////////
bool Condition::Source::operator==(const Condition::ConditionBase& rhs) const
{ return Condition::ConditionBase::operator==(rhs); }

std::string Condition::Source::Description(bool negated/* = false*/) const {
    return (!negated)
        ? UserString("DESC_SOURCE")
        : UserString("DESC_SOURCE_NOT");
}

std::string Condition::Source::Dump() const
{ return DumpIndent() + "Source\n"; }

bool Condition::Source::Match(const ScriptingContext& local_context) const {
    if (!local_context.source)
        return false;
    return local_context.source == local_context.condition_local_candidate;
}

void Condition::Source::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                          Condition::ObjectSet& condition_non_targets) const
{
    if (parent_context.source)
        condition_non_targets.push_back(parent_context.source);
    //Logger().debugStream() << "Condition::ConditionBase::Eval will check at most one source object rather than " << Objects().NumObjects() << " total objects";
}

///////////////////////////////////////////////////////////
// RootCandidate                                         //
///////////////////////////////////////////////////////////
bool Condition::RootCandidate::operator==(const Condition::ConditionBase& rhs) const
{ return Condition::ConditionBase::operator==(rhs); }

std::string Condition::RootCandidate::Description(bool negated/* = false*/) const {
    return (!negated)
        ? UserString("DESC_ROOT_CANDIDATE")
        : UserString("DESC_ROOT_CANDIDATE_NOT");
}

std::string Condition::RootCandidate::Dump() const
{ return DumpIndent() + "RootCandidate\n"; }

bool Condition::RootCandidate::Match(const ScriptingContext& local_context) const {
    if (!local_context.condition_root_candidate)
        return false;
    return local_context.condition_root_candidate == local_context.condition_local_candidate;
}

///////////////////////////////////////////////////////////
// Target                                                //
///////////////////////////////////////////////////////////
bool Condition::Target::operator==(const Condition::ConditionBase& rhs) const
{ return Condition::ConditionBase::operator==(rhs); }

std::string Condition::Target::Description(bool negated/* = false*/) const {
    return (!negated)
        ? UserString("DESC_TARGET")
        : UserString("DESC_TARGET_NOT");
}

std::string Condition::Target::Dump() const
{ return DumpIndent() + "Target\n"; }

bool Condition::Target::Match(const ScriptingContext& local_context) const {
    if (!local_context.effect_target)
        return false;
    return local_context.effect_target == local_context.condition_local_candidate;
}

void Condition::Target::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                          Condition::ObjectSet& condition_non_targets) const
{
    if (parent_context.effect_target)
        condition_non_targets.push_back(parent_context.effect_target);
}

///////////////////////////////////////////////////////////
// Homeworld                                             //
///////////////////////////////////////////////////////////
Condition::Homeworld::~Homeworld() {
    for (unsigned int i = 0; i < m_names.size(); ++i)
        delete m_names[i];
}

bool Condition::Homeworld::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::Homeworld& rhs_ = static_cast<const Condition::Homeworld&>(rhs);

    if (m_names.size() != rhs_.m_names.size())
        return false;
    for (unsigned int i = 0; i < m_names.size(); ++i) {
        CHECK_COND_VREF_MEMBER(m_names.at(i))
    }

    return true;
}

namespace {
    struct HomeworldSimpleMatch {
        HomeworldSimpleMatch(const std::vector<std::string>& names) :
            m_names(names)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            // is it a planet or a building on a planet?
            TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
            TemporaryPtr<const ::Building> building;
            if (!planet && (building = boost::dynamic_pointer_cast<const ::Building>(candidate))) {
                planet = GetPlanet(building->PlanetID());
            }
            if (!planet)
                return false;

            int planet_id = planet->ID();
            const SpeciesManager& manager = GetSpeciesManager();

            if (m_names.empty()) {
                // match homeworlds for any species
                for (SpeciesManager::iterator species_it = manager.begin(); species_it != manager.end(); ++species_it) {
                    if (const ::Species* species = species_it->second) {
                        const std::set<int>& homeworld_ids = species->Homeworlds();
                        if (homeworld_ids.find(planet_id) != homeworld_ids.end())
                            return true;
                    }
                }

            } else {
                // match any of the species specified
                for (std::vector<std::string>::const_iterator it = m_names.begin(); it != m_names.end(); ++it) {
                    if (const ::Species* species = GetSpecies(*it)) {
                        const std::set<int>& homeworld_ids = species->Homeworlds();
                        if (homeworld_ids.find(planet_id) != homeworld_ids.end())
                            return true;
                    }
                }
            }

            return false;
        }

        const std::vector<std::string>& m_names;
    };
}

void Condition::Homeworld::Eval(const ScriptingContext& parent_context,
                                ObjectSet& matches, ObjectSet& non_matches,
                                SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // check each valueref for invariance to local candidate
        for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
             it != m_names.end(); ++it)
        {
            if (!(*it)->LocalCandidateInvariant()) {
                simple_eval_safe = false;
                break;
            }
        }
    }
    if (simple_eval_safe) {
        // evaluate names once, and use to check all candidate objects
        std::vector<std::string> names;
        // get all names from valuerefs
        for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
             it != m_names.end(); ++it)
        {
            names.push_back((*it)->Eval(parent_context));
        }
        EvalImpl(matches, non_matches, search_domain, HomeworldSimpleMatch(names));
    } else {
        // re-evaluate allowed names for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::Homeworld::RootCandidateInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->RootCandidateInvariant())
            return false;
    }
    return true;
}

bool Condition::Homeworld::TargetInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->TargetInvariant())
            return false;
    }
    return true;
}

bool Condition::Homeworld::SourceInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->SourceInvariant())
            return false;
    }
    return true;
}

std::string Condition::Homeworld::Description(bool negated/* = false*/) const {
    std::string values_str;
    for (unsigned int i = 0; i < m_names.size(); ++i) {
        values_str += ValueRef::ConstantExpr(m_names[i]) ?
                        UserString(boost::lexical_cast<std::string>(m_names[i]->Eval())) :
                        m_names[i]->Description();
        if (2 <= m_names.size() && i < m_names.size() - 2) {
            values_str += ", ";
        } else if (i == m_names.size() - 2) {
            values_str += m_names.size() < 3 ? " " : ", ";
            values_str += UserString("OR");
            values_str += " ";
        }
    }
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_HOMEWORLD")
        : UserString("DESC_HOMEWORLD_NOT"))
        % values_str);
}

std::string Condition::Homeworld::Dump() const {
    std::string retval = DumpIndent() + "HomeWorld";
    if (m_names.size() == 1) {
        retval += " name = " + m_names[0]->Dump();
    } else if (!m_names.empty()) {
        retval += " name = [ ";
        for (unsigned int i = 0; i < m_names.size(); ++i) {
            retval += m_names[i]->Dump() + " ";
        }
        retval += "]";
    }
    return retval;
}

bool Condition::Homeworld::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "Homeworld::Match passed no candidate object";
        return false;
    }

    // is it a planet or a building on a planet?
    TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
    TemporaryPtr<const ::Building> building;
    if (!planet && (building = boost::dynamic_pointer_cast<const ::Building>(candidate))) {
        planet = GetPlanet(building->PlanetID());
    }
    if (!planet)
        return false;

    int planet_id = planet->ID();
    const SpeciesManager& manager = GetSpeciesManager();

    if (m_names.empty()) {
        // match homeworlds for any species
        for (SpeciesManager::iterator species_it = manager.begin(); species_it != manager.end(); ++species_it) {
            if (const ::Species* species = species_it->second) {
                const std::set<int>& homeworld_ids = species->Homeworlds();
                if (homeworld_ids.find(planet_id) != homeworld_ids.end())   // is this planet the homeworld for this species?
                    return true;
            }
        }

    } else {
        // match any of the species specified
        for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
             it != m_names.end(); ++it)
        {
            const std::string& species_name = (*it)->Eval(local_context);
            if (const ::Species* species = manager.GetSpecies(species_name)) {
                const std::set<int>& homeworld_ids = species->Homeworlds();
                if (homeworld_ids.find(planet_id) != homeworld_ids.end())   // is this planet the homeworld for this species?
                    return true;
            }
        }
    }

    return false;
}

void Condition::Homeworld::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                             Condition::ObjectSet& condition_non_targets) const
{ AddPlanetSet(condition_non_targets); }

///////////////////////////////////////////////////////////
// Capital                                               //
///////////////////////////////////////////////////////////
bool Condition::Capital::operator==(const Condition::ConditionBase& rhs) const
{ return Condition::ConditionBase::operator==(rhs); }

std::string Condition::Capital::Description(bool negated/* = false*/) const {
    return (!negated)
        ? UserString("DESC_CAPITAL")
        : UserString("DESC_CAPITAL_NOT");
}

std::string Condition::Capital::Dump() const
{ return DumpIndent() + "Capital\n"; }

bool Condition::Capital::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "Capital::Match passed no candidate object";
        return false;
    }
    int candidate_id = candidate->ID();

    // check if any empire's capital's ID is that candidate object's id.
    // if it is, the candidate object is a capital.
    const EmpireManager& empires = Empires();
    for (EmpireManager::const_iterator it = empires.begin(); it != empires.end(); ++it)
        if (it->second->CapitalID() == candidate_id)
            return true;
    return false;
}

void Condition::Capital::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                           Condition::ObjectSet& condition_non_targets) const
{ AddPlanetSet(condition_non_targets); }

///////////////////////////////////////////////////////////
// Monster                                               //
///////////////////////////////////////////////////////////
bool Condition::Monster::operator==(const Condition::ConditionBase& rhs) const
{ return Condition::ConditionBase::operator==(rhs); }

std::string Condition::Monster::Description(bool negated/* = false*/) const {
    return (!negated)
        ? UserString("DESC_MONSTER")
        : UserString("DESC_MONSTER_NOT");
}

std::string Condition::Monster::Dump() const
{ return DumpIndent() + "Monster\n"; }

bool Condition::Monster::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "Monster::Match passed no candidate object";
        return false;
    }

    if (TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(candidate))
        if (ship->IsMonster())
            return true;

    return false;
}

void Condition::Monster::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                           Condition::ObjectSet& condition_non_targets) const
{ AddShipSet(condition_non_targets); }

///////////////////////////////////////////////////////////
// Armed                                                 //
///////////////////////////////////////////////////////////
bool Condition::Armed::operator==(const Condition::ConditionBase& rhs) const
{ return Condition::ConditionBase::operator==(rhs); }

std::string Condition::Armed::Description(bool negated/* = false*/) const {
    return (!negated)
        ? UserString("DESC_ARMED")
        : UserString("DESC_ARMED_NOT");
}

std::string Condition::Armed::Dump() const
{ return DumpIndent() + "Armed\n"; }

bool Condition::Armed::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "Armed::Match passed no candidate object";
        return false;
    }

    if (TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(candidate))
        if (ship->IsArmed())
            return true;

    return false;
}

///////////////////////////////////////////////////////////
// Type                                                  //
///////////////////////////////////////////////////////////
Condition::Type::~Type()
{ delete m_type; }

bool Condition::Type::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::Type& rhs_ = static_cast<const Condition::Type&>(rhs);

    CHECK_COND_VREF_MEMBER(m_type)

    return true;
}

namespace {
    struct TypeSimpleMatch {
        TypeSimpleMatch(UniverseObjectType type) :
            m_type(type)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            switch (m_type) {
            case OBJ_BUILDING:
            case OBJ_SHIP:
            case OBJ_FLEET:
            case OBJ_PLANET:
            case OBJ_SYSTEM:
                return candidate->ObjectType() == m_type;
                break;
            case OBJ_POP_CENTER:
                return (bool)boost::dynamic_pointer_cast<const PopCenter>(candidate);
                break;
            case OBJ_PROD_CENTER:
                return (bool)boost::dynamic_pointer_cast<const ResourceCenter>(candidate);
                break;
            default:
                break;
            }
            return false;
        }

        UniverseObjectType m_type;
    };
}

void Condition::Type::Eval(const ScriptingContext& parent_context,
                           ObjectSet& matches, ObjectSet& non_matches,
                           SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ValueRef::ConstantExpr(m_type) ||
                            (m_type->LocalCandidateInvariant() &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        UniverseObjectType type = m_type->Eval(parent_context);
        EvalImpl(matches, non_matches, search_domain, TypeSimpleMatch(type));
    } else {
        // re-evaluate allowed turn range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::Type::RootCandidateInvariant() const
{ return m_type->RootCandidateInvariant(); }

bool Condition::Type::TargetInvariant() const
{ return m_type->TargetInvariant(); }

bool Condition::Type::SourceInvariant() const
{ return m_type->SourceInvariant(); }

std::string Condition::Type::Description(bool negated/* = false*/) const {
    std::string value_str = ValueRef::ConstantExpr(m_type) ?
                                UserString(boost::lexical_cast<std::string>(m_type->Eval())) :
                                m_type->Description();
    return str(FlexibleFormat((!negated)
           ? UserString("DESC_TYPE")
           : UserString("DESC_TYPE_NOT"))
           % value_str);
}

std::string Condition::Type::Dump() const {
    std::string retval = DumpIndent();
    if (dynamic_cast<const ValueRef::Constant<UniverseObjectType>*>(m_type)) {
        switch (m_type->Eval()) {
        case OBJ_BUILDING:    retval += "Building\n"; break;
        case OBJ_SHIP:        retval += "Ship\n"; break;
        case OBJ_FLEET:       retval += "Fleet\n"; break;
        case OBJ_PLANET:      retval += "Planet\n"; break;
        case OBJ_POP_CENTER:  retval += "PopulationCenter\n"; break;
        case OBJ_PROD_CENTER: retval += "ProductionCenter\n"; break;
        case OBJ_SYSTEM:      retval += "System\n"; break;
        case OBJ_FIELD:       retval += "Field\n"; break;
        default: retval += "?\n"; break;
        }
    } else {
        retval += "ObjectType type = " + m_type->Dump() + "\n";
    }
    return retval;
}

bool Condition::Type::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "Type::Match passed no candidate object";
        return false;
    }

    return TypeSimpleMatch(m_type->Eval(local_context))(candidate);
}

void Condition::Type::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                        Condition::ObjectSet& condition_non_targets) const
{
    // Ships, Fleets and default checks for current objects only
    bool found_type = false;
    if (m_type) {
        found_type = true;
        //std::vector<TemporaryPtr<const T> > this_base;
        switch (m_type->Eval()) {
            case OBJ_BUILDING:
                AddBuildingSet(condition_non_targets);
                break;
            case OBJ_FIELD:
                AddFieldSet(condition_non_targets);
                break;
            case OBJ_FLEET:
                AddFleetSet(condition_non_targets);
                break;
            case OBJ_PLANET:
                AddPlanetSet(condition_non_targets);
                break;
            case OBJ_POP_CENTER:
                AddPopCenterSet(condition_non_targets);
                break;
            case OBJ_PROD_CENTER:
                AddResCenterSet(condition_non_targets);
                break;
            case OBJ_SHIP:
                AddShipSet(condition_non_targets);
                break;
            case OBJ_SYSTEM:
                AddSystemSet(condition_non_targets);
                break;
            default: 
                found_type = false;
                break;
        }
    }
    if (found_type) {
        //if (int(condition_non_targets.size()) < Objects().NumObjects()) {
        //    Logger().debugStream() << "Condition::Type::GetBaseNonMatches will provide " << condition_non_targets.size() 
        //                            << " objects of type " << GetType()->Eval() << " rather than " << Objects().NumObjects() << " total objects";
        //}
    } else {
        ConditionBase::GetDefaultInitialCandidateObjects(parent_context, condition_non_targets);
    }
}

///////////////////////////////////////////////////////////
// Building                                              //
///////////////////////////////////////////////////////////
Condition::Building::~Building() {
    for (unsigned int i = 0; i < m_names.size(); ++i) {
        delete m_names[i];
    }
}

bool Condition::Building::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::Building& rhs_ = static_cast<const Condition::Building&>(rhs);

    if (m_names.size() != rhs_.m_names.size())
        return false;
    for (unsigned int i = 0; i < m_names.size(); ++i) {
        CHECK_COND_VREF_MEMBER(m_names.at(i))
    }

    return true;
}

namespace {
    struct BuildingSimpleMatch {
        BuildingSimpleMatch(const std::vector<std::string>& names) :
            m_names(names)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            // is it a building?
            TemporaryPtr<const ::Building> building = boost::dynamic_pointer_cast<const ::Building>(candidate);
            if (!building)
                return false;

            // if no name supplied, match any building
            if (m_names.empty())
                return true;

            // is it one of the specified building types?
            return std::find(m_names.begin(), m_names.end(), building->BuildingTypeName()) != m_names.end();
        }

        const std::vector<std::string>& m_names;
    };
}

void Condition::Building::Eval(const ScriptingContext& parent_context,
                               ObjectSet& matches, ObjectSet& non_matches,
                               SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // check each valueref for invariance to local candidate
        for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
             it != m_names.end(); ++it)
        {
            if (!(*it)->LocalCandidateInvariant()) {
                simple_eval_safe = false;
                break;
            }
        }
    }
    if (simple_eval_safe) {
        // evaluate names once, and use to check all candidate objects
        std::vector<std::string> names;
        // get all names from valuerefs
        for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
             it != m_names.end(); ++it)
        {
            names.push_back((*it)->Eval(parent_context));
        }
        EvalImpl(matches, non_matches, search_domain, BuildingSimpleMatch(names));
    } else {
        // re-evaluate allowed building types range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::Building::RootCandidateInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->RootCandidateInvariant())
            return false;
    }
    return true;
}

bool Condition::Building::TargetInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->TargetInvariant())
            return false;
    }
    return true;
}

bool Condition::Building::SourceInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->SourceInvariant())
            return false;
    }
    return true;
}

std::string Condition::Building::Description(bool negated/* = false*/) const {
    std::string values_str;
    for (unsigned int i = 0; i < m_names.size(); ++i) {
        values_str += ValueRef::ConstantExpr(m_names[i]) ?
                        UserString(boost::lexical_cast<std::string>(m_names[i]->Eval())) :
                        m_names[i]->Description();
        if (2 <= m_names.size() && i < m_names.size() - 2) {
            values_str += ", ";
        } else if (i == m_names.size() - 2) {
            values_str += m_names.size() < 3 ? " " : ", ";
            values_str += UserString("OR");
            values_str += " ";
        }
    }
    return str(FlexibleFormat((!negated)
           ? UserString("DESC_BUILDING")
           : UserString("DESC_BUILDING_NOT"))
           % values_str);
}

std::string Condition::Building::Dump() const {
    std::string retval = DumpIndent() + "Building name = ";
    if (m_names.size() == 1) {
        retval += m_names[0]->Dump() + "\n";
    } else {
        retval += "[ ";
        for (unsigned int i = 0; i < m_names.size(); ++i) {
            retval += m_names[i]->Dump() + " ";
        }
        retval += "]\n";
    }
    return retval;
}

void Condition::Building::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                            Condition::ObjectSet& condition_non_targets) const
{ AddBuildingSet(condition_non_targets); }

bool Condition::Building::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "Building::Match passed no candidate object";
        return false;
    }

    // is it a building?
    TemporaryPtr<const ::Building> building = boost::dynamic_pointer_cast<const ::Building>(candidate);
    if (building) {
        // match any building type?
        if (m_names.empty())
            return true;

        // match one of the specified building types
        for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
             it != m_names.end(); ++it)
        {
            if ((*it)->Eval(local_context) == building->BuildingTypeName())
                return true;
        }
    }

    return false;
}

///////////////////////////////////////////////////////////
// HasSpecial                                            //
///////////////////////////////////////////////////////////
Condition::HasSpecial::~HasSpecial() {
    delete m_since_turn_low;
    delete m_since_turn_high;
}

bool Condition::HasSpecial::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::HasSpecial& rhs_ = static_cast<const Condition::HasSpecial&>(rhs);

    if (m_name != rhs_.m_name)
        return false;

    CHECK_COND_VREF_MEMBER(m_since_turn_low)
    CHECK_COND_VREF_MEMBER(m_since_turn_high)

    return true;
}

namespace {
    struct HasSpecialSimpleMatch {
        HasSpecialSimpleMatch(const std::string& name, int low_turn, int high_turn) :
            m_name(name),
            m_low_turn(low_turn),
            m_high_turn(high_turn)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            if (m_name.empty())
                return !candidate->Specials().empty();

            std::map<std::string, int>::const_iterator it = candidate->Specials().find(m_name);
            if (it == candidate->Specials().end())
                return false;

            int special_since_turn = it->second;

            return m_low_turn <= special_since_turn && special_since_turn <= m_high_turn;
        }

        const std::string& m_name;
        int m_low_turn;
        int m_high_turn;
    };
}

void Condition::HasSpecial::Eval(const ScriptingContext& parent_context,
                                 ObjectSet& matches, ObjectSet& non_matches,
                                 SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ((!m_since_turn_low || m_since_turn_low->LocalCandidateInvariant()) &&
                             (!m_since_turn_high || m_since_turn_high->LocalCandidateInvariant()) &&
                             (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate turn limits once, pass to simple match for all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);
        int low = (m_since_turn_low ? m_since_turn_low->Eval(local_context) : BEFORE_FIRST_TURN);
        int high = (m_since_turn_high ? m_since_turn_high->Eval(local_context) : IMPOSSIBLY_LARGE_TURN);
        EvalImpl(matches, non_matches, search_domain, HasSpecialSimpleMatch(m_name, low, high));
    } else {
        // re-evaluate allowed turn range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::HasSpecial::RootCandidateInvariant() const
{ return ((!m_since_turn_low || m_since_turn_low->RootCandidateInvariant()) &&
          (!m_since_turn_high || m_since_turn_high->RootCandidateInvariant())); }

bool Condition::HasSpecial::TargetInvariant() const
{ return ((!m_since_turn_low || m_since_turn_low->TargetInvariant()) &&
          (!m_since_turn_high || m_since_turn_high->TargetInvariant())); }

bool Condition::HasSpecial::SourceInvariant() const
{ return ((!m_since_turn_low || m_since_turn_low->SourceInvariant()) &&
          (!m_since_turn_high || m_since_turn_high->SourceInvariant())); }

std::string Condition::HasSpecial::Description(bool negated/* = false*/) const {
    if (!m_since_turn_low && !m_since_turn_high) {
        return str(FlexibleFormat((!negated)
            ? UserString("DESC_SPECIAL")
            : UserString("DESC_SPECIAL_NOT"))
                   % UserString(m_name));
    }

    // turn ranges have been specified; must indicate in description
    std::string low_str = boost::lexical_cast<std::string>(BEFORE_FIRST_TURN);
    if (m_since_turn_low) {
        low_str = ValueRef::ConstantExpr(m_since_turn_low) ?
                  boost::lexical_cast<std::string>(m_since_turn_low->Eval()) :
                  m_since_turn_low->Description();
    }
    std::string high_str = boost::lexical_cast<std::string>(IMPOSSIBLY_LARGE_TURN);
    if (m_since_turn_high) {
        high_str = ValueRef::ConstantExpr(m_since_turn_high) ?
                   boost::lexical_cast<std::string>(m_since_turn_high->Eval()) :
                   m_since_turn_high->Description();
    }

    return str(FlexibleFormat((!negated)
            ? UserString("DESC_SPECIAL_TURN_RANGE")
            : UserString("DESC_SPECIAL_TURN_RANGE_NOT"))
               % UserString(m_name)
               % low_str
               % high_str);
}

std::string Condition::HasSpecial::Dump() const {
    if (!m_since_turn_low && !m_since_turn_high)
        return DumpIndent() + "HasSpecial name = \"" + m_name + "\"\n";

    std::string low_dump = (m_since_turn_low ? m_since_turn_low->Dump() : boost::lexical_cast<std::string>(BEFORE_FIRST_TURN));
    std::string high_dump = (m_since_turn_high ? m_since_turn_high->Dump() : boost::lexical_cast<std::string>(IMPOSSIBLY_LARGE_TURN));
    return DumpIndent() + "HasSpecialSinceTurn name = \"" + m_name + "\" low = " + low_dump + " high = " + high_dump;
}

bool Condition::HasSpecial::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "HasSpecial::Match passed no candidate object";
        return false;
    }
    int low = (m_since_turn_low ? m_since_turn_low->Eval(local_context) : BEFORE_FIRST_TURN);
    int high = (m_since_turn_high ? m_since_turn_high->Eval(local_context) : IMPOSSIBLY_LARGE_TURN);
    return HasSpecialSimpleMatch(m_name, low, high)(candidate);
}

///////////////////////////////////////////////////////////
// HasTag                                                //
///////////////////////////////////////////////////////////
bool Condition::HasTag::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::HasTag& rhs_ = static_cast<const Condition::HasTag&>(rhs);

    if (m_name != rhs_.m_name)
        return false;

    return true;
}

std::string Condition::HasTag::Description(bool negated/* = false*/) const {
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_HAS_TAG")
        : UserString("DESC_HAS_TAG_NOT"))
        % UserString(m_name));
}

std::string Condition::HasTag::Dump() const
{ return DumpIndent() + "HasTag name = \"" + m_name + "\"\n"; }

bool Condition::HasTag::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "HasTag::Match passed no candidate object";
        return false;
    }

    return candidate->HasTag(m_name);
}

///////////////////////////////////////////////////////////
// CreatedOnTurn                                         //
///////////////////////////////////////////////////////////
Condition::CreatedOnTurn::~CreatedOnTurn() {
    delete m_low;
    delete m_high;
}

bool Condition::CreatedOnTurn::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::CreatedOnTurn& rhs_ = static_cast<const Condition::CreatedOnTurn&>(rhs);

    CHECK_COND_VREF_MEMBER(m_low)
    CHECK_COND_VREF_MEMBER(m_high)

    return true;
}

namespace {
    struct CreatedOnTurnSimpleMatch {
        CreatedOnTurnSimpleMatch(int low, int high) :
            m_low(low),
            m_high(high)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;
            int turn = candidate->CreationTurn();
            return m_low <= turn && turn <= m_high;
        }

        int m_low;
        int m_high;
    };
}

void Condition::CreatedOnTurn::Eval(const ScriptingContext& parent_context,
                                    ObjectSet& matches, ObjectSet& non_matches,
                                    SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ((!m_low || m_low->LocalCandidateInvariant()) &&
                             (!m_high || m_high->LocalCandidateInvariant()) &&
                             (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);
        int low = (m_low ? m_low->Eval(local_context) : BEFORE_FIRST_TURN);
        int high = (m_high ? m_high->Eval(local_context) : IMPOSSIBLY_LARGE_TURN);
        EvalImpl(matches, non_matches, search_domain, CreatedOnTurnSimpleMatch(low, high));
    } else {
        // re-evaluate allowed turn range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::CreatedOnTurn::RootCandidateInvariant() const
{ return ((!m_low || m_low->RootCandidateInvariant()) && (!m_high || m_high->RootCandidateInvariant())); }

bool Condition::CreatedOnTurn::TargetInvariant() const
{ return ((!m_low || m_low->TargetInvariant()) && (!m_high || m_high->TargetInvariant())); }

bool Condition::CreatedOnTurn::SourceInvariant() const
{ return ((!m_low || m_low->SourceInvariant()) && (!m_high || m_high->SourceInvariant())); }

std::string Condition::CreatedOnTurn::Description(bool negated/* = false*/) const {
    std::string low_str = (m_low ? (ValueRef::ConstantExpr(m_low) ?
                                    boost::lexical_cast<std::string>(m_low->Eval()) :
                                    m_low->Description())
                                 : boost::lexical_cast<std::string>(BEFORE_FIRST_TURN));
    std::string high_str = (m_high ? (ValueRef::ConstantExpr(m_high) ?
                                      boost::lexical_cast<std::string>(m_high->Eval()) :
                                      m_high->Description())
                                   : boost::lexical_cast<std::string>(IMPOSSIBLY_LARGE_TURN));
    return str(FlexibleFormat((!negated)
            ? UserString("DESC_CREATED_ON_TURN")
            : UserString("DESC_CREATED_ON_TURN_NOT"))
               % low_str
               % high_str);
}

std::string Condition::CreatedOnTurn::Dump() const {
    std::string retval = DumpIndent() + "CreatedOnTurn";
    if (m_low)
        retval += " low = " + m_low->Dump();
    if (m_high)
        retval += " high = " + m_high->Dump();
    retval += "\n";
    return retval;
}

bool Condition::CreatedOnTurn::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "CreatedOnTurn::Match passed no candidate object";
        return false;
    }
    int low = (m_low ? std::max(0, m_low->Eval(local_context)) : BEFORE_FIRST_TURN);
    int high = (m_high ? std::min(m_high->Eval(local_context), IMPOSSIBLY_LARGE_TURN) : IMPOSSIBLY_LARGE_TURN);
    return CreatedOnTurnSimpleMatch(low, high)(candidate);
}

///////////////////////////////////////////////////////////
// Contains                                              //
///////////////////////////////////////////////////////////
Condition::Contains::~Contains()
{ delete m_condition; }

bool Condition::Contains::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::Contains& rhs_ = static_cast<const Condition::Contains&>(rhs);

    CHECK_COND_VREF_MEMBER(m_condition)

    return true;
}

namespace {
    struct ContainsSimpleMatch {
        ContainsSimpleMatch(const Condition::ObjectSet& subcondition_matches) :
            m_subcondition_matches_ids()
        {
            // We need a sorted container for efficiently intersecting 
            // subcondition_matches with the set of objects contained in some
            // candidate object.
            // We only need ids, not objects, so we can do that conversion
            // here as well, simplifying later code.
            // Note that this constructor is called only once per 
            // Contains::Eval(), its work cannot help performance when executed 
            // for each candidate.
            m_subcondition_matches_ids.reserve(subcondition_matches.size());
            // gather the ids
            for (Condition::ObjectSet::const_iterator it = subcondition_matches.begin(); it != subcondition_matches.end(); ++it) {
                TemporaryPtr<const UniverseObject> obj = *it;
                if (obj)
                { m_subcondition_matches_ids.push_back(obj->ID()); }
            }
            // sort them
            std::sort(m_subcondition_matches_ids.begin(), m_subcondition_matches_ids.end());
        }

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            bool match = false;
            const std::set<int>& candidate_elements = candidate->ContainedObjectIDs(); // guaranteed O(1)

            // We need to test whether candidate_elements and m_subcondition_matches_ids have a common element.
            // We choose the strategy that is more efficient by comparing the sizes of both sets.
            if (candidate_elements.size() < m_subcondition_matches_ids.size()) {
                // candidate_elements is smaller, so we iterate it and look up each candidate element in m_subcondition_matches_ids
                for (std::set<int>::const_iterator it = candidate_elements.begin(), end_it = candidate_elements.end(); it != end_it; ++it) {
                    // std::lower_bound requires m_subcondition_matches_ids to be sorted
                    std::vector<int>::const_iterator matching_it = std::lower_bound(m_subcondition_matches_ids.begin(), m_subcondition_matches_ids.end(), *it);
                    
                    if (matching_it != m_subcondition_matches_ids.end() && *matching_it == *it) {
                        match = true;
                        break;
                    }
                }
            } else {
                // m_subcondition_matches_ids is smaller, so we iterate it and look up each subcondition match in the set of candidate's elements
                for (std::vector<int>::const_iterator it = m_subcondition_matches_ids.begin(); it != m_subcondition_matches_ids.end(); ++it) {
                    // candidate->Contains() may have a faster implementation than candidate_elements->find()
                    if (candidate->Contains(*it)) {
                        match = true;
                        break;
                    }
                }
            }

            return match;
        }

        std::vector<int> m_subcondition_matches_ids;
    };
}

void Condition::Contains::Eval(const ScriptingContext& parent_context,
                               ObjectSet& matches, ObjectSet& non_matches,
                               SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // evaluate contained objects once and check for all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);

        // get objects to be considering for matching against subcondition
        ObjectSet subcondition_matches;
        m_condition->Eval(local_context, subcondition_matches);

        EvalImpl(matches, non_matches, search_domain, ContainsSimpleMatch(subcondition_matches));
    } else {
        // re-evaluate contained objects for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::Contains::RootCandidateInvariant() const
{ return m_condition->RootCandidateInvariant(); }

bool Condition::Contains::TargetInvariant() const
{ return m_condition->TargetInvariant(); }

bool Condition::Contains::SourceInvariant() const
{ return m_condition->SourceInvariant(); }

std::string Condition::Contains::Description(bool negated/* = false*/) const {
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_CONTAINS")
        : UserString("DESC_CONTAINS_NOT"))
        % m_condition->Description());
}

std::string Condition::Contains::Dump() const {
    std::string retval = DumpIndent() + "Contains condition =\n";
    ++g_indent;
    retval += m_condition->Dump();
    --g_indent;
    return retval;
}

void Condition::Contains::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                            Condition::ObjectSet& condition_non_targets) const
{
    // objects that can contain other objects: fleets, planets, and systems
    AddFleetSet(condition_non_targets);
    AddPlanetSet(condition_non_targets);
    AddSystemSet(condition_non_targets);
}

bool Condition::Contains::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "Contains::Match passed no candidate object";
        return false;
    }

    // get subcondition matches
    ObjectSet subcondition_matches;
    m_condition->Eval(local_context, subcondition_matches);

    // does candidate object contain any subcondition matches?
    for (ObjectSet::iterator subcon_it = subcondition_matches.begin(); subcon_it != subcondition_matches.end(); ++subcon_it)
        if (candidate->Contains((*subcon_it)->ID()))
            return true;

    return false;
}

///////////////////////////////////////////////////////////
// ContainedBy                                           //
///////////////////////////////////////////////////////////
Condition::ContainedBy::~ContainedBy()
{ delete m_condition; }

bool Condition::ContainedBy::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::ContainedBy& rhs_ = static_cast<const Condition::ContainedBy&>(rhs);

    CHECK_COND_VREF_MEMBER(m_condition)

    return true;
}

namespace {
    struct ContainedBySimpleMatch {
        ContainedBySimpleMatch(const Condition::ObjectSet& subcondition_matches) :
            m_subcondition_matches_ids()
        {
            // We need a sorted container for efficiently intersecting 
            // subcondition_matches with the set of objects containing some
            // candidate object.
            // We only need ids, not objects, so we can do that conversion
            // here as well, simplifying later code.
            // Note that this constructor is called only once per 
            // ContainedBy::Eval(), its work cannot help performance when
            // executed for each candidate.
            m_subcondition_matches_ids.reserve(subcondition_matches.size());
            // gather the ids
            for (Condition::ObjectSet::const_iterator it = subcondition_matches.begin(); it != subcondition_matches.end(); ++it) {
                TemporaryPtr<const UniverseObject> obj = *it;
                if (obj)
                { m_subcondition_matches_ids.push_back(obj->ID()); }
            }
            // sort them
            std::sort(m_subcondition_matches_ids.begin(), m_subcondition_matches_ids.end());
        }

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            bool match = false;
            // gather the objects containing candidate
            std::vector<int> candidate_containers;
            const int candidate_id = candidate->ID();
            const int    system_id = candidate->SystemID();
            const int container_id = candidate->ContainerObjectID();
            if (   system_id != INVALID_OBJECT_ID &&    system_id != candidate_id) candidate_containers.push_back(   system_id);
            if (container_id != INVALID_OBJECT_ID && container_id !=    system_id) candidate_containers.push_back(container_id);
            // FIXME: currently, direct container and system will do. In the future, we might need a way to retrieve containers of containers

            // We need to test whether candidate_containers and m_subcondition_matches_ids have a common element.
            // We choose the strategy that is more efficient by comparing the sizes of both sets.
            if (candidate_containers.size() < m_subcondition_matches_ids.size()) {
                // candidate_containers is smaller, so we iterate it and look up each candidate container in m_subcondition_matches_ids
                for (std::vector<int>::const_iterator it = candidate_containers.begin(), end_it = candidate_containers.end(); it != end_it; ++it) {
                    // std::lower_bound requires m_subcondition_matches_ids to be sorted
                    std::vector<int>::const_iterator matching_it = std::lower_bound(m_subcondition_matches_ids.begin(), m_subcondition_matches_ids.end(), *it);
                    
                    if (matching_it != m_subcondition_matches_ids.end() && *matching_it == *it) {
                        match = true;
                        break;
                    }
                }
            } else {
                // m_subcondition_matches_ids is smaller, so we iterate it and look up each subcondition match in the set of candidate's containers
                for (std::vector<int>::const_iterator it = m_subcondition_matches_ids.begin(); it != m_subcondition_matches_ids.end(); ++it) {
                    // candidate->ContainedBy() may have a faster implementation than candidate_containers->find()
                    if (candidate->ContainedBy(*it)) {
                        match = true;
                        break;
                    }
                }
            }

            return match;
        }

        std::vector<int> m_subcondition_matches_ids;
    };
}

void Condition::ContainedBy::Eval(const ScriptingContext& parent_context,
                                  ObjectSet& matches, ObjectSet& non_matches,
                                  SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // evaluate contained objects once and check for all candidates

        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);

        // get subcondition matches
        ObjectSet subcondition_matches;
        m_condition->Eval(local_context, subcondition_matches);

        EvalImpl(matches, non_matches, search_domain, ContainedBySimpleMatch(subcondition_matches));
    } else {
        // re-evaluate contained objects for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::ContainedBy::RootCandidateInvariant() const
{ return m_condition->RootCandidateInvariant(); }

bool Condition::ContainedBy::TargetInvariant() const
{ return m_condition->TargetInvariant(); }

bool Condition::ContainedBy::SourceInvariant() const
{ return m_condition->SourceInvariant(); }

std::string Condition::ContainedBy::Description(bool negated/* = false*/) const {
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_CONTAINED_BY")
        : UserString("DESC_CONTAINED_BY_NOT"))
        % m_condition->Description());
}

std::string Condition::ContainedBy::Dump() const {
    std::string retval = DumpIndent() + "ContainedBy condition =\n";
    ++g_indent;
        retval += m_condition->Dump();
    --g_indent;
    return retval;
}

bool Condition::ContainedBy::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "ContainedBy::Match passed no candidate object";
        return false;
    }

    // get containing objects
    std::set<int> containers;
    if (candidate->SystemID() != INVALID_OBJECT_ID)
        containers.insert(candidate->SystemID());
    if (candidate->ContainerObjectID() != INVALID_OBJECT_ID && candidate->ContainerObjectID() != candidate->SystemID())
        containers.insert(candidate->ContainerObjectID());

    ObjectSet container_objects = Objects().FindObjects<const UniverseObject>(containers);
    if (container_objects.empty())
        return false;

    m_condition->Eval(local_context, container_objects);

    return container_objects.empty();
}

///////////////////////////////////////////////////////////
// InSystem                                              //
///////////////////////////////////////////////////////////
Condition::InSystem::~InSystem()
{ delete m_system_id; }

bool Condition::InSystem::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::InSystem& rhs_ = static_cast<const Condition::InSystem&>(rhs);

    CHECK_COND_VREF_MEMBER(m_system_id)

    return true;
}

namespace {
    struct InSystemSimpleMatch {
        InSystemSimpleMatch(int system_id) :
            m_system_id(system_id)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;
            if (m_system_id == INVALID_OBJECT_ID)
                return candidate->SystemID() != INVALID_OBJECT_ID;  // match objects in any system
            else
                return candidate->SystemID() == m_system_id;        // match objects in specified system
        }

        int m_system_id;
    };
}

void Condition::InSystem::Eval(const ScriptingContext& parent_context,
                               ObjectSet& matches, ObjectSet& non_matches,
                               SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = !m_system_id || ValueRef::ConstantExpr(m_system_id) ||
                            (m_system_id->LocalCandidateInvariant() &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate empire id once, and use to check all candidate objects
        TemporaryPtr<const UniverseObject> no_object;
        int system_id = (m_system_id ? m_system_id->Eval(ScriptingContext(parent_context, no_object)) : INVALID_OBJECT_ID);
        EvalImpl(matches, non_matches, search_domain, InSystemSimpleMatch(system_id));
    } else {
        // re-evaluate empire id for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::InSystem::RootCandidateInvariant() const
{ return !m_system_id || m_system_id->RootCandidateInvariant(); }

bool Condition::InSystem::TargetInvariant() const
{ return !m_system_id || m_system_id->TargetInvariant(); }

bool Condition::InSystem::SourceInvariant() const
{ return !m_system_id || m_system_id->SourceInvariant(); }

std::string Condition::InSystem::Description(bool negated/* = false*/) const {
    std::string system_str;
    int system_id = INVALID_OBJECT_ID;
    if (m_system_id && ValueRef::ConstantExpr(m_system_id))
        system_id = m_system_id->Eval();
    if (TemporaryPtr<const System> system = GetSystem(system_id))
        system_str = system->Name();
    else if (m_system_id)
        system_str = m_system_id->Description();

    std::string description_str;
    if (!system_str.empty())
        description_str = (!negated)
            ? UserString("DESC_IN_SYSTEM")
            : UserString("DESC_IN_SYSTEM_NOT");
    else
        description_str = (!negated)
            ? UserString("DESC_IN_SYSTEM_SIMPLE")
            : UserString("DESC_IN_SYSTEM_SIMPLE_NOT");

    return str(FlexibleFormat(description_str) % system_str);
}

std::string Condition::InSystem::Dump() const {
    std::string retval = DumpIndent() + "InSystem";
    if (m_system_id)
        retval += " id = " + m_system_id->Dump();
    retval += "\n";
    return retval;
}

bool Condition::InSystem::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "InSystem::Match passed no candidate object";
        return false;
    }
    int system_id = (m_system_id ? m_system_id->Eval(local_context) : INVALID_OBJECT_ID);
    return InSystemSimpleMatch(system_id)(candidate);
}

///////////////////////////////////////////////////////////
// ObjectID                                              //
///////////////////////////////////////////////////////////
Condition::ObjectID::~ObjectID()
{ delete m_object_id; }

bool Condition::ObjectID::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::ObjectID& rhs_ = static_cast<const Condition::ObjectID&>(rhs);

    CHECK_COND_VREF_MEMBER(m_object_id)

    return true;
}

namespace {
    struct ObjectIDSimpleMatch {
        ObjectIDSimpleMatch(int object_id) :
            m_object_id(object_id)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            return candidate &&
                m_object_id != INVALID_OBJECT_ID &&
                candidate->ID() == m_object_id;
        }

        int m_object_id;
    };
}

void Condition::ObjectID::Eval(const ScriptingContext& parent_context,
                               ObjectSet& matches, ObjectSet& non_matches,
                               SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = !m_object_id || ValueRef::ConstantExpr(m_object_id) ||
                            (m_object_id->LocalCandidateInvariant() &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate empire id once, and use to check all candidate objects
        TemporaryPtr<const UniverseObject> no_object;
        int object_id = (m_object_id ? m_object_id->Eval(ScriptingContext(parent_context, no_object)) : INVALID_OBJECT_ID);
        EvalImpl(matches, non_matches, search_domain, ObjectIDSimpleMatch(object_id));
    } else {
        // re-evaluate empire id for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::ObjectID::RootCandidateInvariant() const
{ return !m_object_id || m_object_id->RootCandidateInvariant(); }

bool Condition::ObjectID::TargetInvariant() const
{ return !m_object_id || m_object_id->TargetInvariant(); }

bool Condition::ObjectID::SourceInvariant() const
{ return !m_object_id || m_object_id->SourceInvariant(); }

std::string Condition::ObjectID::Description(bool negated/* = false*/) const {
    std::string object_str;
    int object_id = INVALID_OBJECT_ID;
    if (m_object_id && ValueRef::ConstantExpr(m_object_id))
        object_id = m_object_id->Eval();
    if (TemporaryPtr<const System> system = GetSystem(object_id))
        object_str = system->Name();
    else if (m_object_id)
        object_str = m_object_id->Description();
    else
        object_str = UserString("ERROR");   // should always have a valid ID for this condition

    return str(FlexibleFormat((!negated)
        ? UserString("DESC_OBJECT_ID")
        : UserString("DESC_OBJECT_ID_NOT"))
               % object_str);
}

std::string Condition::ObjectID::Dump() const
{ return DumpIndent() + "Object id = " + m_object_id->Dump(); }

void Condition::ObjectID::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                            Condition::ObjectSet& condition_non_targets) const
{
    if (!m_object_id)
        return;

    bool simple_eval_safe = ValueRef::ConstantExpr(m_object_id) ||
                            (m_object_id->LocalCandidateInvariant() &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));

    if (!simple_eval_safe) {
        AddAllObjectsSet(condition_non_targets);
        return;
    }

    // simple case of a single specified id; can add just that object
    TemporaryPtr<const UniverseObject> no_object;
    int object_id = m_object_id->Eval(ScriptingContext(parent_context, no_object));
    if (object_id == INVALID_OBJECT_ID)
        return;

    TemporaryPtr<UniverseObject> obj = Objects().ExistingObject(object_id);
    if (obj)
        condition_non_targets.push_back(obj);
}

bool Condition::ObjectID::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "ObjectID::Match passed no candidate object";
        return false;
    }

    return ObjectIDSimpleMatch(m_object_id->Eval(local_context))(candidate);
}

///////////////////////////////////////////////////////////
// PlanetType                                            //
///////////////////////////////////////////////////////////
Condition::PlanetType::~PlanetType() {
    for (unsigned int i = 0; i < m_types.size(); ++i) {
        delete m_types[i];
    }
}

bool Condition::PlanetType::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::PlanetType& rhs_ = static_cast<const Condition::PlanetType&>(rhs);

    if (m_types.size() != rhs_.m_types.size())
        return false;
    for (unsigned int i = 0; i < m_types.size(); ++i) {
        CHECK_COND_VREF_MEMBER(m_types.at(i))
    }

    return true;
}

namespace {
    struct PlanetTypeSimpleMatch {
        PlanetTypeSimpleMatch(const std::vector< ::PlanetType>& types) :
            m_types(types)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            // is it a planet or on a planet?
            TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
            TemporaryPtr<const ::Building> building;
            if (!planet && (building = boost::dynamic_pointer_cast<const ::Building>(candidate))) {
                planet = GetPlanet(building->PlanetID());
            }
            if (planet) {
                // is it one of the specified building types?
                return std::find(m_types.begin(), m_types.end(), planet->Type()) != m_types.end();
            }

            return false;
        }

        const std::vector< ::PlanetType>& m_types;
    };
}

void Condition::PlanetType::Eval(const ScriptingContext& parent_context,
                                 ObjectSet& matches, ObjectSet& non_matches,
                                 SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // check each valueref for invariance to local candidate
        for (std::vector<const ValueRef::ValueRefBase< ::PlanetType>*>::const_iterator it = m_types.begin();
            it != m_types.end(); ++it)
        {
            if (!(*it)->LocalCandidateInvariant()) {
                simple_eval_safe = false;
                break;
            }
        }
    }
    if (simple_eval_safe) {
        // evaluate types once, and use to check all candidate objects
        std::vector< ::PlanetType> types;
        // get all types from valuerefs
        for (std::vector<const ValueRef::ValueRefBase< ::PlanetType>*>::const_iterator it = m_types.begin();
             it != m_types.end(); ++it)
        {
            types.push_back((*it)->Eval(parent_context));
        }
        EvalImpl(matches, non_matches, search_domain, PlanetTypeSimpleMatch(types));
    } else {
        // re-evaluate contained objects for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::PlanetType::RootCandidateInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::PlanetType>*>::const_iterator it = m_types.begin();
         it != m_types.end(); ++it)
    {
        if (!(*it)->RootCandidateInvariant())
            return false;
    }
    return true;
}

bool Condition::PlanetType::TargetInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::PlanetType>*>::const_iterator it = m_types.begin();
         it != m_types.end(); ++it)
    {
        if (!(*it)->TargetInvariant())
            return false;
    }
    return true;
}

bool Condition::PlanetType::SourceInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::PlanetType>*>::const_iterator it = m_types.begin();
         it != m_types.end(); ++it)
    {
        if (!(*it)->SourceInvariant())
            return false;
    }
    return true;
}

std::string Condition::PlanetType::Description(bool negated/* = false*/) const {
    std::string values_str;
    for (unsigned int i = 0; i < m_types.size(); ++i) {
        values_str += ValueRef::ConstantExpr(m_types[i]) ?
                        UserString(boost::lexical_cast<std::string>(m_types[i]->Eval())) :
                        m_types[i]->Description();
        if (2 <= m_types.size() && i < m_types.size() - 2) {
            values_str += ", ";
        } else if (i == m_types.size() - 2) {
            values_str += m_types.size() < 3 ? " " : ", ";
            values_str += UserString("OR");
            values_str += " ";
        }
    }
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_PLANET_TYPE")
        : UserString("DESC_PLANET_TYPE_NOT"))
        % values_str);
}

std::string Condition::PlanetType::Dump() const {
    std::string retval = DumpIndent() + "Planet type = ";
    if (m_types.size() == 1) {
        retval += m_types[0]->Dump() + "\n";
    } else {
        retval += "[ ";
        for (unsigned int i = 0; i < m_types.size(); ++i) {
            retval += m_types[i]->Dump() + " ";
        }
        retval += "]\n";
    }
    return retval;
}

void Condition::PlanetType::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                              Condition::ObjectSet& condition_non_targets) const
{
    AddPlanetSet(condition_non_targets);
    AddBuildingSet(condition_non_targets);
}

bool Condition::PlanetType::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "PlanetType::Match passed no candidate object";
        return false;
    }

    TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
    TemporaryPtr<const ::Building> building;
    if (!planet && (building = boost::dynamic_pointer_cast<const ::Building>(candidate))) {
        planet = GetPlanet(building->PlanetID());
    }
    if (planet) {
        for (std::vector<const ValueRef::ValueRefBase< ::PlanetType>*>::const_iterator it = m_types.begin();
             it != m_types.end(); ++it)
        {
            if ((*it)->Eval(ScriptingContext(local_context)) == planet->Type())
                return true;
        }
    }
    return false;
}

///////////////////////////////////////////////////////////
// PlanetSize                                            //
///////////////////////////////////////////////////////////
Condition::PlanetSize::~PlanetSize() {
    for (unsigned int i = 0; i < m_sizes.size(); ++i) {
        delete m_sizes[i];
    }
}

bool Condition::PlanetSize::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::PlanetSize& rhs_ = static_cast<const Condition::PlanetSize&>(rhs);

    if (m_sizes.size() != rhs_.m_sizes.size())
        return false;
    for (unsigned int i = 0; i < m_sizes.size(); ++i) {
        CHECK_COND_VREF_MEMBER(m_sizes.at(i))
    }

    return true;
}

namespace {
    struct PlanetSizeSimpleMatch {
        PlanetSizeSimpleMatch(const std::vector< ::PlanetSize>& sizes) :
            m_sizes(sizes)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            // is it a planet or on a planet? TODO: This concept should be generalized and factored out.
            TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
            TemporaryPtr<const ::Building> building;
            if (!planet && (building = boost::dynamic_pointer_cast<const ::Building>(candidate))) {
                planet = GetPlanet(building->PlanetID());
            }
            if (planet) {
                // is it one of the specified building types?
                for (std::vector< ::PlanetSize>::const_iterator it = m_sizes.begin();
                        it != m_sizes.end(); ++it)
                {
                    if (planet->Size() == *it)
                        return true;
                }
            }

            return false;
        }

        const std::vector< ::PlanetSize>& m_sizes;
    };
}

void Condition::PlanetSize::Eval(const ScriptingContext& parent_context,
                                 ObjectSet& matches, ObjectSet& non_matches,
                                 SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // check each valueref for invariance to local candidate
        for (std::vector<const ValueRef::ValueRefBase< ::PlanetSize>*>::const_iterator it = m_sizes.begin();
            it != m_sizes.end(); ++it)
        {
            if (!(*it)->LocalCandidateInvariant()) {
                simple_eval_safe = false;
                break;
            }
        }
    }
    if (simple_eval_safe) {
        // evaluate types once, and use to check all candidate objects
        std::vector< ::PlanetSize> sizes;
        // get all types from valuerefs
        for (std::vector<const ValueRef::ValueRefBase< ::PlanetSize>*>::const_iterator it = m_sizes.begin();
             it != m_sizes.end(); ++it)
        {
            sizes.push_back((*it)->Eval(parent_context));
        }
        EvalImpl(matches, non_matches, search_domain, PlanetSizeSimpleMatch(sizes));
    } else {
        // re-evaluate contained objects for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::PlanetSize::RootCandidateInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::PlanetSize>*>::const_iterator it = m_sizes.begin();
         it != m_sizes.end(); ++it)
    {
        if (!(*it)->RootCandidateInvariant())
            return false;
    }
    return true;
}

bool Condition::PlanetSize::TargetInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::PlanetSize>*>::const_iterator it = m_sizes.begin();
         it != m_sizes.end(); ++it)
    {
        if (!(*it)->TargetInvariant())
            return false;
    }
    return true;
}

bool Condition::PlanetSize::SourceInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::PlanetSize>*>::const_iterator it = m_sizes.begin();
         it != m_sizes.end(); ++it)
    {
        if (!(*it)->SourceInvariant())
            return false;
    }
    return true;
}

std::string Condition::PlanetSize::Description(bool negated/* = false*/) const {
    std::string values_str;
    for (unsigned int i = 0; i < m_sizes.size(); ++i) {
        values_str += ValueRef::ConstantExpr(m_sizes[i]) ?
                        UserString(boost::lexical_cast<std::string>(m_sizes[i]->Eval())) :
                        m_sizes[i]->Description();
        if (2 <= m_sizes.size() && i < m_sizes.size() - 2) {
            values_str += ", ";
        } else if (i == m_sizes.size() - 2) {
            values_str += m_sizes.size() < 3 ? " " : ", ";
            values_str += UserString("OR");
            values_str += " ";
        }
    }
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_PLANET_SIZE")
        : UserString("DESC_PLANET_SIZE_NOT"))
        % values_str);
}

std::string Condition::PlanetSize::Dump() const {
    std::string retval = DumpIndent() + "Planet size = ";
    if (m_sizes.size() == 1) {
        retval += m_sizes[0]->Dump() + "\n";
    } else {
        retval += "[ ";
        for (unsigned int i = 0; i < m_sizes.size(); ++i) {
            retval += m_sizes[i]->Dump() + " ";
        }
        retval += "]\n";
    }
    return retval;
}

void Condition::PlanetSize::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                              Condition::ObjectSet& condition_non_targets) const
{
    AddPlanetSet(condition_non_targets);
    AddBuildingSet(condition_non_targets);
}

bool Condition::PlanetSize::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "PlanetSize::Match passed no candidate object";
        return false;
    }

    TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
    TemporaryPtr<const ::Building> building;
    if (!planet && (building = boost::dynamic_pointer_cast<const ::Building>(candidate))) {
        planet = GetPlanet(building->PlanetID());
    }
    if (planet) {
        for (unsigned int i = 0; i < m_sizes.size(); ++i) {
            if (m_sizes[i]->Eval(local_context) == planet->Size())
                return true;
        }
    }
    return false;
}

///////////////////////////////////////////////////////////
// PlanetEnvironment                                     //
///////////////////////////////////////////////////////////
Condition::PlanetEnvironment::~PlanetEnvironment() {
    for (unsigned int i = 0; i < m_environments.size(); ++i) {
        delete m_environments[i];
    }
}

bool Condition::PlanetEnvironment::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::PlanetEnvironment& rhs_ = static_cast<const Condition::PlanetEnvironment&>(rhs);

    if (m_environments.size() != rhs_.m_environments.size())
        return false;
    for (unsigned int i = 0; i < m_environments.size(); ++i) {
        CHECK_COND_VREF_MEMBER(m_environments.at(i))
    }

    return true;
}

namespace {
    struct PlanetEnvironmentSimpleMatch {
        PlanetEnvironmentSimpleMatch(const std::vector< ::PlanetEnvironment>& environments) :
            m_environments(environments)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            // is it a planet or on a planet? TODO: factor out
            TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
            TemporaryPtr<const ::Building> building;
            if (!planet && (building = boost::dynamic_pointer_cast<const ::Building>(candidate))) {
                planet = GetPlanet(building->PlanetID());
            }
            if (planet) {
                // is it one of the specified building types?
                for (std::vector< ::PlanetEnvironment>::const_iterator it = m_environments.begin();
                        it != m_environments.end(); ++it)
                {
                    if (planet->EnvironmentForSpecies() == *it)
                        return true;
                }
            }

            return false;
        }

        const std::vector< ::PlanetEnvironment>& m_environments;
    };
}

void Condition::PlanetEnvironment::Eval(const ScriptingContext& parent_context,
                                        ObjectSet& matches, ObjectSet& non_matches,
                                        SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // check each valueref for invariance to local candidate
        for (std::vector<const ValueRef::ValueRefBase< ::PlanetEnvironment>*>::const_iterator it = m_environments.begin();
             it != m_environments.end(); ++it)
        {
            if (!(*it)->LocalCandidateInvariant()) {
                simple_eval_safe = false;
                break;
            }
        }
    }
    if (simple_eval_safe) {
        // evaluate types once, and use to check all candidate objects
        std::vector< ::PlanetEnvironment> environments;
        // get all types from valuerefs
        for (std::vector<const ValueRef::ValueRefBase< ::PlanetEnvironment>*>::const_iterator it = m_environments.begin();
             it != m_environments.end(); ++it)
        {
            environments.push_back((*it)->Eval(parent_context));
        }
        EvalImpl(matches, non_matches, search_domain, PlanetEnvironmentSimpleMatch(environments));
    } else {
        // re-evaluate contained objects for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::PlanetEnvironment::RootCandidateInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::PlanetEnvironment>*>::const_iterator it = m_environments.begin();
         it != m_environments.end(); ++it)
    {
        if (!(*it)->RootCandidateInvariant())
            return false;
    }
    return true;
}

bool Condition::PlanetEnvironment::TargetInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::PlanetEnvironment>*>::const_iterator it = m_environments.begin();
         it != m_environments.end(); ++it)
    {
        if (!(*it)->TargetInvariant())
            return false;
    }
    return true;
}

bool Condition::PlanetEnvironment::SourceInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::PlanetEnvironment>*>::const_iterator it = m_environments.begin();
         it != m_environments.end(); ++it)
    {
        if (!(*it)->SourceInvariant())
            return false;
    }
    return true;
}

std::string Condition::PlanetEnvironment::Description(bool negated/* = false*/) const {
    std::string values_str;
    for (unsigned int i = 0; i < m_environments.size(); ++i) {
        values_str += ValueRef::ConstantExpr(m_environments[i]) ?
                        UserString(boost::lexical_cast<std::string>(m_environments[i]->Eval())) :
                        m_environments[i]->Description();
        if (2 <= m_environments.size() && i < m_environments.size() - 2) {
            values_str += ", ";
        } else if (i == m_environments.size() - 2) {
            values_str += m_environments.size() < 3 ? " " : ", ";
            values_str += UserString("OR");
            values_str += " ";
        }
    }
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_PLANET_ENVIRONMENT")
        : UserString("DESC_PLANET_ENVIRONMENT_NOT"))
        % values_str);
}

std::string Condition::PlanetEnvironment::Dump() const {
    std::string retval = DumpIndent() + "Planet environment = ";
    if (m_environments.size() == 1) {
        retval += m_environments[0]->Dump() + "\n";
    } else {
        retval += "[ ";
        for (unsigned int i = 0; i < m_environments.size(); ++i) {
            retval += m_environments[i]->Dump() + " ";
        }
        retval += "]\n";
    }
    return retval;
}

void Condition::PlanetEnvironment::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context,
                                                                     Condition::ObjectSet& condition_non_targets) const
{
    AddPlanetSet(condition_non_targets);
    AddBuildingSet(condition_non_targets);
}

bool Condition::PlanetEnvironment::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "PlanetEnvironment::Match passed no candidate object";
        return false;
    }
    
    // is it a planet or on a planet? TODO: factor out
    TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
    TemporaryPtr<const ::Building> building;
    if (!planet && (building = boost::dynamic_pointer_cast<const ::Building>(candidate))) {
        planet = GetPlanet(building->PlanetID());
    }
    if (planet) {
        ::PlanetEnvironment env_for_planets_species = planet->EnvironmentForSpecies();
        for (unsigned int i = 0; i < m_environments.size(); ++i) {
            if (m_environments[i]->Eval(local_context) == env_for_planets_species)
                return true;
        }
    }
    return false;
}

///////////////////////////////////////////////////////////
// Species                                              //
///////////////////////////////////////////////////////////
Condition::Species::~Species() {
    for (unsigned int i = 0; i < m_names.size(); ++i) {
        delete m_names[i];
    }
}

bool Condition::Species::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::Species& rhs_ = static_cast<const Condition::Species&>(rhs);

    if (m_names.size() != rhs_.m_names.size())
        return false;
    for (unsigned int i = 0; i < m_names.size(); ++i) {
        CHECK_COND_VREF_MEMBER(m_names.at(i))
    }

    return true;
}

namespace {
    struct SpeciesSimpleMatch {
        SpeciesSimpleMatch(const std::vector<std::string>& names) :
            m_names(names)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            // is it a population centre?
            if (TemporaryPtr<const ::PopCenter> pop = boost::dynamic_pointer_cast<const ::PopCenter>(candidate)) {
                const std::string& species_name = pop->SpeciesName();
                // if the popcenter has a species and that species is one of those specified...
                return !species_name.empty() && (m_names.empty() || (std::find(m_names.begin(), m_names.end(), species_name) != m_names.end()));
            }
            // is it a ship?
            if (TemporaryPtr<const ::Ship> ship = boost::dynamic_pointer_cast<const Ship>(candidate)) {
                // if the ship has a species and that species is one of those specified...
                const std::string& species_name = ship->SpeciesName();
                return !species_name.empty() && (m_names.empty() || (std::find(m_names.begin(), m_names.end(), species_name) != m_names.end()));
            }
            // is it a building on a planet?
            if (TemporaryPtr<const ::Building> building = boost::dynamic_pointer_cast<const Building>(candidate)) {
                TemporaryPtr<const ::Planet> planet = GetPlanet(building->PlanetID());
                const std::string& species_name = planet->SpeciesName();
                // if the planet (which IS a popcenter) has a species and that species is one of those specified...
                return !species_name.empty() && (m_names.empty() || (std::find(m_names.begin(), m_names.end(), species_name) != m_names.end()));
            }

            return false;
        }

        const std::vector<std::string>& m_names;
    };
}

void Condition::Species::Eval(const ScriptingContext& parent_context,
                              ObjectSet& matches, ObjectSet& non_matches,
                              SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // check each valueref for invariance to local candidate
        for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
             it != m_names.end(); ++it)
        {
            if (!(*it)->LocalCandidateInvariant()) {
                simple_eval_safe = false;
                break;
            }
        }
    }
    if (simple_eval_safe) {
        // evaluate names once, and use to check all candidate objects
        std::vector<std::string> names;
        // get all names from valuerefs
        for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
             it != m_names.end(); ++it)
        { names.push_back((*it)->Eval(parent_context)); }
        EvalImpl(matches, non_matches, search_domain, SpeciesSimpleMatch(names));
    } else {
        // re-evaluate allowed building types range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::Species::RootCandidateInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->RootCandidateInvariant())
            return false;
    }
    return true;
}

bool Condition::Species::TargetInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->TargetInvariant())
            return false;
    }
    return true;
}

bool Condition::Species::SourceInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->SourceInvariant())
            return false;
    }
    return true;
}

std::string Condition::Species::Description(bool negated/* = false*/) const {
    std::string values_str;
    for (unsigned int i = 0; i < m_names.size(); ++i) {
        values_str += ValueRef::ConstantExpr(m_names[i]) ?
                        UserString(boost::lexical_cast<std::string>(m_names[i]->Eval())) :
                        m_names[i]->Description();
        if (2 <= m_names.size() && i < m_names.size() - 2) {
            values_str += ", ";
        } else if (i == m_names.size() - 2) {
            values_str += m_names.size() < 3 ? " " : ", ";
            values_str += UserString("OR");
            values_str += " ";
        }
    }
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_SPECIES")
        : UserString("DESC_SPECIES_NOT"))
        % values_str);
}

std::string Condition::Species::Dump() const {
    std::string retval = DumpIndent() + "Species";
    if (m_names.empty()) {
        // do nothing else
    } else if (m_names.size() == 1) {
        retval += " name = " + m_names[0]->Dump() + "\n";
    } else {
        retval += " name = [ ";
        for (unsigned int i = 0; i < m_names.size(); ++i) {
            retval += m_names[i]->Dump() + " ";
        }
        retval += "]\n";
    }
    return retval;
}

bool Condition::Species::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "Species::Match passed no candidate object";
        return false;
    }

    // is it a planet or a building on a planet? TODO: factor out
    TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
    TemporaryPtr<const ::Building> building;
    if (!planet && (building = boost::dynamic_pointer_cast<const ::Building>(candidate))) {
        planet = GetPlanet(building->PlanetID());
    }
    if (planet) {
        if (m_names.empty()) {
            return !planet->SpeciesName().empty();  // match any species name
        } else {
            // match only specified species names
            for (unsigned int i = 0; i < m_names.size(); ++i)
                if (m_names[i]->Eval(local_context) == planet->SpeciesName())
                    return true;
        }
    }
    // is it a ship?
    TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(candidate);
    if (ship) {
        if (m_names.empty()) {
            return !ship->SpeciesName().empty();    // match any species name
        } else {
            // match only specified species names
            for (unsigned int i = 0; i < m_names.size(); ++i)
                if (m_names[i]->Eval(local_context) == ship->SpeciesName())
                    return true;
        }
    }
    return false;
}

///////////////////////////////////////////////////////////
// Enqueued                                              //
///////////////////////////////////////////////////////////
Condition::Enqueued::~Enqueued() {
    delete m_design_id;
    delete m_low;
    delete m_high;
}

bool Condition::Enqueued::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::Enqueued& rhs_ = static_cast<const Condition::Enqueued&>(rhs);

    if (m_build_type != rhs_.m_build_type)
        return false;

    if (m_name != rhs_.m_name)
        return false;

    CHECK_COND_VREF_MEMBER(m_design_id)
    CHECK_COND_VREF_MEMBER(m_empire_id)
    CHECK_COND_VREF_MEMBER(m_low)
    CHECK_COND_VREF_MEMBER(m_high)

    return true;
}

namespace {
    int NumberOnQueue(const ProductionQueue& queue, BuildType build_type, const int location_id,
                      const std::string& name = "", int design_id = ShipDesign::INVALID_DESIGN_ID)
    {
        int retval = 0;
        for (ProductionQueue::const_iterator it = queue.begin(); it != queue.end(); ++it) {
            if (!(build_type == INVALID_BUILD_TYPE || build_type == it->item.build_type))
                continue;
            if (location_id != it->location)
                continue;
            if (build_type == BT_BUILDING) {
                // if looking for buildings, accept specifically named building
                // or any building if no name specified
                if (!name.empty() && it->item.name != name)
                    continue;
            } else if (build_type == BT_SHIP) {
                if (design_id != ShipDesign::INVALID_DESIGN_ID) {
                    // if looking for ships, accept design by id number...
                    if (design_id != it->item.design_id)
                        continue;
                } else if (!name.empty()) {
                    // ... or accept design by predefined name
                    const ShipDesign* design = GetShipDesign(it->item.design_id);
                    if (!design || name != design->Name(false))
                        continue;
                }
            } // else: looking for any production item

            retval += it->blocksize;
        }
        return retval;
    }

    struct EnqueuedSimpleMatch {
        EnqueuedSimpleMatch(BuildType build_type, const std::string& name, int design_id,
                            int empire_id, int low, int high) :
            m_build_type(build_type),
            m_name(name),
            m_design_id(design_id),
            m_empire_id(empire_id),
            m_low(low),
            m_high(high)
        {}
        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            int count = 0;

            if (m_empire_id == ALL_EMPIRES) {
                for (EmpireManager::const_iterator empire_it = Empires().begin();
                     empire_it != Empires().end(); ++empire_it)
                {
                    const Empire* empire = empire_it->second;
                    count += NumberOnQueue(empire->GetProductionQueue(), m_build_type,
                                           candidate->ID(), m_name, m_design_id);
                }

            } else {
                const Empire* empire = Empires().Lookup(m_empire_id);
                if (!empire) return false;
                count = NumberOnQueue(empire->GetProductionQueue(), m_build_type,
                                      candidate->ID(), m_name, m_design_id);
            }

            return (m_low <= count && count <= m_high);
        }

        BuildType   m_build_type;
        std::string m_name;
        int         m_design_id;
        int         m_empire_id;
        int         m_low;
        int         m_high;
    };
}

void Condition::Enqueued::Eval(const ScriptingContext& parent_context,
                               ObjectSet& matches, ObjectSet& non_matches,
                               SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // check each valueref for invariance to local candidate
        if ((m_design_id && !m_design_id->LocalCandidateInvariant()) ||
            (m_empire_id && !m_empire_id->LocalCandidateInvariant()) ||
            (m_low &&       !m_low->LocalCandidateInvariant()) ||
            (m_high &&      !m_high->LocalCandidateInvariant()))
        { simple_eval_safe = false; }
    }
    if (simple_eval_safe) {
        // evaluate valuerefs once, and use to check all candidate objects
        int design_id = (m_design_id ?  m_design_id->Eval(parent_context) : ShipDesign::INVALID_DESIGN_ID);
        int empire_id = (m_empire_id ?  m_empire_id->Eval(parent_context) : ALL_EMPIRES);
        int low =       (m_low ?        m_low->Eval(parent_context) :       0);
        int high =      (m_high ?       m_high->Eval(parent_context) :      INT_MAX);

        EvalImpl(matches, non_matches, search_domain, EnqueuedSimpleMatch(m_build_type, m_name, design_id, 
                                                                          empire_id, low, high));
    } else {
        // re-evaluate allowed building types range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::Enqueued::RootCandidateInvariant() const {
    if ((m_design_id && !m_design_id->RootCandidateInvariant()) ||
        (m_empire_id && !m_empire_id->RootCandidateInvariant()) ||
        (m_low &&       !m_low->RootCandidateInvariant()) ||
        (m_high &&      !m_high->RootCandidateInvariant()))
    { return false; }
    return true;
}

bool Condition::Enqueued::TargetInvariant() const {
    if ((m_design_id && !m_design_id->TargetInvariant()) ||
        (m_empire_id && !m_empire_id->TargetInvariant()) ||
        (m_low &&       !m_low->TargetInvariant()) ||
        (m_high &&      !m_high->TargetInvariant()))
    { return false; }
    return true;    return true;
}

bool Condition::Enqueued::SourceInvariant() const {
    if ((m_design_id && !m_design_id->SourceInvariant()) ||
        (m_empire_id && !m_empire_id->SourceInvariant()) ||
        (m_low &&       !m_low->SourceInvariant()) ||
        (m_high &&      !m_high->SourceInvariant()))
    { return false; }
    return true;    return true;
}

std::string Condition::Enqueued::Description(bool negated/* = false*/) const {
    std::string empire_str;
    if (m_empire_id) {
        int empire_id = ALL_EMPIRES;
        if (ValueRef::ConstantExpr(m_empire_id))
            empire_id = m_empire_id->Eval();
        if (const Empire* empire = Empires().Lookup(empire_id))
            empire_str = empire->Name();
        else
            empire_str = m_empire_id->Description();
    }
    std::string low_str = "0";
    if (m_low) {
        low_str = ValueRef::ConstantExpr(m_low) ?
                    boost::lexical_cast<std::string>(m_low->Eval()) :
                    m_low->Description();
    }
    std::string high_str = boost::lexical_cast<std::string>(INT_MAX);
    if (m_high) {
        high_str = ValueRef::ConstantExpr(m_high) ?
                    boost::lexical_cast<std::string>(m_high->Eval()) :
                    m_high->Description();
    }
    std::string what_str;
    if (!m_name.empty()) {
        what_str = UserString(m_name);
    } else if (m_design_id) {
        what_str = ValueRef::ConstantExpr(m_design_id) ?
                    boost::lexical_cast<std::string>(m_design_id->Eval()) :
                    m_design_id->Description();
    }
    std::string description_str;
    switch (m_build_type) {
    case BT_BUILDING:   description_str = (!negated)
                            ? UserString("DESC_ENQUEUED_BUILDING")
                            : UserString("DESC_ENQUEUED_BUILDING_NOT");
    break;
    case BT_SHIP:       description_str = (!negated)
                            ? UserString("DESC_ENQUEUED_DESIGN")
                            : UserString("DESC_ENQUEUED_DESIGN_NOT");
    break;
    default:            description_str = (!negated)
                            ? UserString("DESC_ENQUEUED")
                            : UserString("DESC_ENQUEUED_NOT");
    break;
    }
    return str(FlexibleFormat(description_str)
               % empire_str
               % low_str
               % high_str
               % what_str);
}

std::string Condition::Enqueued::Dump() const {
    std::string retval = DumpIndent() + "Enqueued";

    if (m_build_type == BT_BUILDING) {
        retval += " type = Building";
        if (!m_name.empty())
            retval += " name = " + m_name;
    } else if (m_build_type == BT_SHIP) {
        retval += " type = Ship";
        if (!m_name.empty())
            retval += " design = " + m_name;
        else if (m_design_id)
            retval += " design = " + m_design_id->Dump();
    }
    if (m_empire_id)
        retval += " empire = " + m_empire_id->Dump();
    if (m_low)
        retval += " low = " + m_low->Dump();
    if (m_high)
        retval += " high = " + m_high->Dump();
    retval += "\n";
    return retval;
}

bool Condition::Enqueued::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "Enqueued::Match passed no candidate object";
        return false;
    }
    int empire_id = (m_empire_id ?  m_empire_id->Eval(local_context) :  ALL_EMPIRES);
    int design_id = (m_design_id ?  m_design_id->Eval(local_context) :  ShipDesign::INVALID_DESIGN_ID);
    int low =       (m_low ?        m_low->Eval(local_context) :        0);
    int high =      (m_high ?       m_high->Eval(local_context) :       INT_MAX);
    return EnqueuedSimpleMatch(m_build_type, m_name, design_id, empire_id, low, high)(candidate);
}

///////////////////////////////////////////////////////////
// FocusType                                             //
///////////////////////////////////////////////////////////
Condition::FocusType::~FocusType() {
    for (unsigned int i = 0; i < m_names.size(); ++i) {
        delete m_names[i];
    }
}

bool Condition::FocusType::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::FocusType& rhs_ = static_cast<const Condition::FocusType&>(rhs);

    if (m_names.size() != rhs_.m_names.size())
        return false;
    for (unsigned int i = 0; i < m_names.size(); ++i) {
        CHECK_COND_VREF_MEMBER(m_names.at(i))
    }

    return true;
}

namespace {
    struct FocusTypeSimpleMatch {
        FocusTypeSimpleMatch(const std::vector<std::string>& names) :
            m_names(names)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            // is it a ResourceCenter or a Building on a Planet (that is a ResourceCenter)
            TemporaryPtr<const ResourceCenter> res_center = boost::dynamic_pointer_cast<const ResourceCenter>(candidate);
            TemporaryPtr<const ::Building> building;
            if (!res_center && (building = boost::dynamic_pointer_cast<const ::Building>(candidate))) {
                if (TemporaryPtr<const Planet> planet = GetPlanet(building->PlanetID()))
                    res_center = boost::dynamic_pointer_cast<const ResourceCenter>(planet);
            }
            if (res_center) {
                return !res_center->Focus().empty() && (std::find(m_names.begin(), m_names.end(), res_center->Focus()) != m_names.end());

            }

            return false;
        }

        const std::vector<std::string>& m_names;
    };
}

void Condition::FocusType::Eval(const ScriptingContext& parent_context,
                                ObjectSet& matches, ObjectSet& non_matches,
                                SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // check each valueref for invariance to local candidate
        for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
             it != m_names.end(); ++it)
        {
            if (!(*it)->LocalCandidateInvariant()) {
                simple_eval_safe = false;
                break;
            }
        }
    }
    if (simple_eval_safe) {
        // evaluate names once, and use to check all candidate objects
        std::vector<std::string> names;
        // get all names from valuerefs
        for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
             it != m_names.end(); ++it)
        {
            names.push_back((*it)->Eval(parent_context));
        }
        EvalImpl(matches, non_matches, search_domain, FocusTypeSimpleMatch(names));
    } else {
        // re-evaluate allowed building types range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::FocusType::RootCandidateInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->RootCandidateInvariant())
            return false;
    }
    return true;
}

bool Condition::FocusType::TargetInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->TargetInvariant())
            return false;
    }
    return true;
}

bool Condition::FocusType::SourceInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase<std::string>*>::const_iterator it = m_names.begin();
         it != m_names.end(); ++it)
    {
        if (!(*it)->SourceInvariant())
            return false;
    }
    return true;
}

std::string Condition::FocusType::Description(bool negated/* = false*/) const {
    std::string values_str;
    for (unsigned int i = 0; i < m_names.size(); ++i) {
        values_str += ValueRef::ConstantExpr(m_names[i]) ?
            UserString(boost::lexical_cast<std::string>(m_names[i]->Eval())) :
            m_names[i]->Description();
        if (2 <= m_names.size() && i < m_names.size() - 2) {
            values_str += ", ";
        } else if (i == m_names.size() - 2) {
            values_str += m_names.size() < 3 ? " " : ", ";
            values_str += UserString("OR");
            values_str += " ";
        }
    }
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_FOCUS_TYPE")
        : UserString("DESC_FOCUS_TYPE_NOT"))
        % values_str);
}

std::string Condition::FocusType::Dump() const {
    std::string retval = DumpIndent() + "Focus name = ";
    if (m_names.size() == 1) {
        retval += m_names[0]->Dump() + "\n";
    } else {
        retval += "[ ";
        for (unsigned int i = 0; i < m_names.size(); ++i) {
            retval += m_names[i]->Dump() + " ";
        }
        retval += "]\n";
    }
    return retval;
}

bool Condition::FocusType::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "FocusType::Match passed no candidate object";
        return false;
    }

    // is it a ResourceCenter or a Building on a Planet (that is a ResourceCenter)
    TemporaryPtr<const ResourceCenter> res_center = boost::dynamic_pointer_cast<const ResourceCenter>(candidate);
    TemporaryPtr<const ::Building> building;
    if (!res_center && (building = boost::dynamic_pointer_cast<const ::Building>(candidate))) {
        if (TemporaryPtr<const Planet> planet = GetPlanet(building->PlanetID()))
            res_center = boost::dynamic_pointer_cast<const ResourceCenter>(planet);
    }
    if (res_center) {
        for (unsigned int i = 0; i < m_names.size(); ++i) {
            if (m_names[i]->Eval(local_context) == res_center->Focus())
                return true;
        }
    }
    return false;
}

///////////////////////////////////////////////////////////
// StarType                                              //
///////////////////////////////////////////////////////////
Condition::StarType::~StarType() {
    for (unsigned int i = 0; i < m_types.size(); ++i) {
        delete m_types[i];
    }
}

bool Condition::StarType::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::StarType& rhs_ = static_cast<const Condition::StarType&>(rhs);

    if (m_types.size() != rhs_.m_types.size())
        return false;
    for (unsigned int i = 0; i < m_types.size(); ++i) {
        CHECK_COND_VREF_MEMBER(m_types.at(i))
    }

    return true;
}

namespace {
    struct StarTypeSimpleMatch {
        StarTypeSimpleMatch(const std::vector< ::StarType>& types) :
            m_types(types)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            TemporaryPtr<const System> system = GetSystem(candidate->SystemID());
            if (system || (system = boost::dynamic_pointer_cast<const System>(candidate)))
                return !m_types.empty() && (std::find(m_types.begin(), m_types.end(), system->GetStarType()) != m_types.end());

            return false;
        }

        const std::vector< ::StarType>& m_types;
    };
}

void Condition::StarType::Eval(const ScriptingContext& parent_context,
                               ObjectSet& matches, ObjectSet& non_matches,
                               SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // check each valueref for invariance to local candidate
        for (std::vector<const ValueRef::ValueRefBase< ::StarType>*>::const_iterator it = m_types.begin();
            it != m_types.end(); ++it)
        {
            if (!(*it)->LocalCandidateInvariant()) {
                simple_eval_safe = false;
                break;
            }
        }
    }
    if (simple_eval_safe) {
        // evaluate types once, and use to check all candidate objects
        std::vector< ::StarType> types;
        // get all types from valuerefs
        for (std::vector<const ValueRef::ValueRefBase< ::StarType>*>::const_iterator it = m_types.begin();
             it != m_types.end(); ++it)
        { types.push_back((*it)->Eval(parent_context)); }
        EvalImpl(matches, non_matches, search_domain, StarTypeSimpleMatch(types));
    } else {
        // re-evaluate contained objects for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::StarType::RootCandidateInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::StarType>*>::const_iterator it = m_types.begin();
         it != m_types.end(); ++it)
    {
        if (!(*it)->RootCandidateInvariant())
            return false;
    }
    return true;
}

bool Condition::StarType::TargetInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::StarType>*>::const_iterator it = m_types.begin();
         it != m_types.end(); ++it)
    {
        if (!(*it)->TargetInvariant())
            return false;
    }
    return true;
}

bool Condition::StarType::SourceInvariant() const {
    for (std::vector<const ValueRef::ValueRefBase< ::StarType>*>::const_iterator it = m_types.begin();
         it != m_types.end(); ++it)
    {
        if (!(*it)->SourceInvariant())
            return false;
    }
    return true;
}

std::string Condition::StarType::Description(bool negated/* = false*/) const {
    std::string values_str;
    for (unsigned int i = 0; i < m_types.size(); ++i) {
        values_str += ValueRef::ConstantExpr(m_types[i]) ?
                        UserString(boost::lexical_cast<std::string>(m_types[i]->Eval())) :
                        m_types[i]->Description();
        if (2 <= m_types.size() && i < m_types.size() - 2) {
            values_str += ", ";
        } else if (i == m_types.size() - 2) {
            values_str += m_types.size() < 3 ? " " : ", ";
            values_str += UserString("OR");
            values_str += " ";
        }
    }
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_STAR_TYPE")
        : UserString("DESC_STAR_TYPE_NOT"))
        % values_str);
}

std::string Condition::StarType::Dump() const {
    std::string retval = DumpIndent() + "Star type = ";
    if (m_types.size() == 1) {
        retval += m_types[0]->Dump() + "\n";
    } else {
        retval += "[ ";
        for (unsigned int i = 0; i < m_types.size(); ++i) {
            retval += m_types[i]->Dump() + " ";
        }
        retval += "]\n";
    }
    return retval;
}

bool Condition::StarType::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "StarType::Match passed no candidate object";
        return false;
    }

    TemporaryPtr<const System> system = GetSystem(candidate->SystemID());
    if (system || (system = boost::dynamic_pointer_cast<const System>(candidate))) {
        for (unsigned int i = 0; i < m_types.size(); ++i) {
            if (m_types[i]->Eval(local_context) == system->GetStarType())
                return true;
        }
    }
    return false;
}

///////////////////////////////////////////////////////////
// DesignHasHull                                         //
///////////////////////////////////////////////////////////
bool Condition::DesignHasHull::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::DesignHasHull& rhs_ = static_cast<const Condition::DesignHasHull&>(rhs);

    if (m_name != rhs_.m_name)
        return false;

    return true;
}

std::string Condition::DesignHasHull::Description(bool negated/* = false*/) const {
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_DESIGN_HAS_HULL")
        : UserString("DESC_DESIGN_HAS_HULL"))
        % UserString(m_name));
}

std::string Condition::DesignHasHull::Dump() const
{ return DumpIndent() + "DesignHasHull name = \"" + m_name + "\"\n"; }

bool Condition::DesignHasHull::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "DesignHasHull::Match passed no candidate object";
        return false;
    }

    if (TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(candidate))
        if (const ShipDesign* design = ship->Design())
            return (design->Hull() == m_name);
    return false;
}

///////////////////////////////////////////////////////////
// DesignHasPart                                         //
///////////////////////////////////////////////////////////
Condition::DesignHasPart::~DesignHasPart() {
    delete m_low;
    delete m_high;
}

bool Condition::DesignHasPart::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::DesignHasPart& rhs_ = static_cast<const Condition::DesignHasPart&>(rhs);

    if (m_name != rhs_.m_name)
        return false;

    CHECK_COND_VREF_MEMBER(m_low)
    CHECK_COND_VREF_MEMBER(m_high)

    return true;
}

namespace {
    struct DesignHasPartSimpleMatch {
        DesignHasPartSimpleMatch(int low, int high, const std::string& name) :
            m_low(low),
            m_high(high),
            m_name(name)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            // is it a ship?
            TemporaryPtr<const ::Ship> ship = boost::dynamic_pointer_cast<const ::Ship>(candidate);
            if (!ship)
                return false;
            // with a valid design?
            const ShipDesign* design = ship->Design();
            if (!design)
                return false;

            const std::vector<std::string>& parts = design->Parts();
            int count = 0;
            for (std::vector<std::string>::const_iterator it = parts.begin(); it != parts.end(); ++it)
                if (*it == m_name || (m_name.empty() && !(*it).empty()))    // # of copies of specified part, or total number of parts if no part name specified
                    ++count;
            return (m_low <= count && count <= m_high);
        }

        int m_low;
        int m_high;
        const std::string& m_name;
    };
}

void Condition::DesignHasPart::Eval(const ScriptingContext& parent_context,
                                    ObjectSet& matches, ObjectSet& non_matches,
                                    SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = (m_low->LocalCandidateInvariant() && m_high->LocalCandidateInvariant() &&
                             (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate number limits once, use to match all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);
        int low =  std::max(0, m_low->Eval(local_context));
        int high = std::min(m_high->Eval(local_context), INT_MAX);
        EvalImpl(matches, non_matches, search_domain, DesignHasPartSimpleMatch(low, high, m_name));
    } else {
        // re-evaluate allowed turn range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::DesignHasPart::RootCandidateInvariant() const
{ return (m_low->RootCandidateInvariant() && m_high->RootCandidateInvariant()); }

bool Condition::DesignHasPart::TargetInvariant() const
{ return (m_low->TargetInvariant() && m_high->TargetInvariant()); }

bool Condition::DesignHasPart::SourceInvariant() const
{ return (m_low->SourceInvariant() && m_high->SourceInvariant()); }

std::string Condition::DesignHasPart::Description(bool negated/* = false*/) const {
    std::string low_str = "0";
    if (m_low) {
        low_str = ValueRef::ConstantExpr(m_low) ?
                    boost::lexical_cast<std::string>(m_low->Eval()) :
                    m_low->Description();
    }
    std::string high_str = boost::lexical_cast<std::string>(INT_MAX);
    if (m_high) {
        high_str = ValueRef::ConstantExpr(m_high) ?
                    boost::lexical_cast<std::string>(m_high->Eval()) :
                    m_high->Description();
    };
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_DESIGN_HAS_PART")
        : UserString("DESC_DESIGN_HAS_PART_NOT"))
        % low_str
        % high_str
        % m_name);
}

std::string Condition::DesignHasPart::Dump() const
{ return DumpIndent() + "DesignHasPart low = " + m_low->Dump() + " high = " + m_high->Dump() + " name = " + m_name; }

bool Condition::DesignHasPart::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "DesignHasPart::Match passed no candidate object";
        return false;
    }

    int low =  std::max(0, m_low->Eval(local_context));
    int high = std::min(m_high->Eval(local_context), IMPOSSIBLY_LARGE_TURN);

    return DesignHasPartSimpleMatch(low, high, m_name)(candidate);
}

///////////////////////////////////////////////////////////
// DesignHasPartClass                                    //
///////////////////////////////////////////////////////////
Condition::DesignHasPartClass::~DesignHasPartClass() {
    delete m_low;
    delete m_high;
}

bool Condition::DesignHasPartClass::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::DesignHasPartClass& rhs_ = static_cast<const Condition::DesignHasPartClass&>(rhs);

    if (m_class != rhs_.m_class)
        return false;

    CHECK_COND_VREF_MEMBER(m_low)
    CHECK_COND_VREF_MEMBER(m_high)

    return true;
}

namespace {
    struct DesignHasPartClassSimpleMatch {
        DesignHasPartClassSimpleMatch(int low, int high, ShipPartClass part_class) :
            m_low(low),
            m_high(high),
            m_part_class(part_class)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            // is it a ship?
            TemporaryPtr<const ::Ship> ship = boost::dynamic_pointer_cast<const ::Ship>(candidate);
            if (!ship)
                return false;
            // with a valid design?
            const ShipDesign* design = ship->Design();
            if (!design)
                return false;


            const std::vector<std::string>& parts = design->Parts();
            int count = 0;
            for (std::vector<std::string>::const_iterator it = parts.begin(); it != parts.end(); ++it) {
                if (const PartType* part_type = GetPartType(*it)) {
                    if (part_type->Class() == m_part_class)
                        ++count;
                }
            }
            return (m_low <= count && count <= m_high);
        }

        int m_low;
        int m_high;
        ShipPartClass m_part_class;
    };
}

void Condition::DesignHasPartClass::Eval(const ScriptingContext& parent_context,
                                         ObjectSet& matches, ObjectSet& non_matches,
                                         SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = (m_low->LocalCandidateInvariant() && m_high->LocalCandidateInvariant() &&
                             (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate number limits once, use to match all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);
        int low =  std::max(0, m_low->Eval(local_context));
        int high = std::min(m_high->Eval(local_context), IMPOSSIBLY_LARGE_TURN);
        EvalImpl(matches, non_matches, search_domain, DesignHasPartClassSimpleMatch(low, high, m_class));
    } else {
        // re-evaluate allowed turn range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::DesignHasPartClass::RootCandidateInvariant() const
{ return (m_low->RootCandidateInvariant() && m_high->RootCandidateInvariant()); }

bool Condition::DesignHasPartClass::TargetInvariant() const
{ return (m_low->TargetInvariant() && m_high->TargetInvariant()); }

bool Condition::DesignHasPartClass::SourceInvariant() const
{ return (m_low->SourceInvariant() && m_high->SourceInvariant()); }

std::string Condition::DesignHasPartClass::Description(bool negated/* = false*/) const {
    std::string low_str = "0";
    if (m_low) {
        low_str = ValueRef::ConstantExpr(m_low) ?
                    boost::lexical_cast<std::string>(m_low->Eval()) :
                    m_low->Description();
    }
    std::string high_str = boost::lexical_cast<std::string>(INT_MAX);
    if (m_high) {
        high_str = ValueRef::ConstantExpr(m_high) ?
                    boost::lexical_cast<std::string>(m_high->Eval()) :
                    m_high->Description();
    }
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_DESIGN_HAS_PART_CLASS")
        : UserString("DESC_DESIGN_HAS_PART_CLASS_NOT"))
               % low_str
               % high_str
               % UserString(boost::lexical_cast<std::string>(m_class)));
}

std::string Condition::DesignHasPartClass::Dump() const
{ return DumpIndent() + "DesignHasPartClass low = " + m_low->Dump() + " high = " + m_high->Dump() + " class = " + UserString(boost::lexical_cast<std::string>(m_class)); }

bool Condition::DesignHasPartClass::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "DesignHasPartClass::Match passed no candidate object";
        return false;
    }

    int low =  (m_low ? m_low->Eval(local_context) : 0);
    int high = (m_high ? m_high->Eval(local_context) : INT_MAX);

    return DesignHasPartClassSimpleMatch(low, high, m_class)(candidate);
}

///////////////////////////////////////////////////////////
// PredefinedShipDesign                                  //
///////////////////////////////////////////////////////////
bool Condition::PredefinedShipDesign::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::PredefinedShipDesign& rhs_ = static_cast<const Condition::PredefinedShipDesign&>(rhs);

    if (m_name != rhs_.m_name)
        return false;

    return true;
}

std::string Condition::PredefinedShipDesign::Description(bool negated/* = false*/) const {
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_PREDEFINED_SHIP_DESIGN")
        : UserString("DESC_PREDEFINED_SHIP_DESIGN_NOT"))
        % UserString(m_name));
}

std::string Condition::PredefinedShipDesign::Dump() const
{ return DumpIndent() + "PredefinedShipDesign name = \"" + m_name + "\"\n"; }

bool Condition::PredefinedShipDesign::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "PredefinedShipDesign::Match passed no candidate object";
        return false;
    }

    TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(candidate);
    if (!ship)
        return false;
    const ShipDesign* candidate_design = ship->Design();
    if (!candidate_design)
        return false;

    // ship has a valid design.  see if it is / could be a predefined ship design...

    // all predefined named designs are hard-coded in parsing to have a designed on turn 0 (before first turn)
    if (candidate_design->DesignedOnTurn() != 0)
        return false;

    // all predefined designs have a name, and that name should match the condition m_name
    return (m_name == candidate_design->Name(false));   // don't look up in stringtable; predefined designs are stored by stringtable entry key
}

///////////////////////////////////////////////////////////
// NumberedShipDesign                                    //
///////////////////////////////////////////////////////////
Condition::NumberedShipDesign::~NumberedShipDesign()
{ delete m_design_id; }

bool Condition::NumberedShipDesign::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::NumberedShipDesign& rhs_ = static_cast<const Condition::NumberedShipDesign&>(rhs);

    CHECK_COND_VREF_MEMBER(m_design_id)

    return true;
}

namespace {
    struct NumberedShipDesignSimpleMatch {
        NumberedShipDesignSimpleMatch(int design_id) :
            m_design_id(design_id)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;
            if (m_design_id == ShipDesign::INVALID_DESIGN_ID)
                return false;
            if (TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(candidate))
                if (ship->DesignID() == m_design_id)
                    return true;
            return false;
        }

        int m_design_id;
    };
}

void Condition::NumberedShipDesign::Eval(const ScriptingContext& parent_context,
                                         ObjectSet& matches, ObjectSet& non_matches,
                                         SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ValueRef::ConstantExpr(m_design_id) ||
                            (m_design_id->LocalCandidateInvariant() &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate empire id once, and use to check all candidate objects
        TemporaryPtr<const UniverseObject> no_object;
        int design_id = m_design_id->Eval(ScriptingContext(parent_context, no_object));
        EvalImpl(matches, non_matches, search_domain, NumberedShipDesignSimpleMatch(design_id));
    } else {
        // re-evaluate design id for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::NumberedShipDesign::RootCandidateInvariant() const
{ return m_design_id->RootCandidateInvariant(); }

bool Condition::NumberedShipDesign::TargetInvariant() const
{ return m_design_id->TargetInvariant(); }

bool Condition::NumberedShipDesign::SourceInvariant() const
{ return m_design_id->SourceInvariant(); }

std::string Condition::NumberedShipDesign::Description(bool negated/* = false*/) const {
    std::string id_str = ValueRef::ConstantExpr(m_design_id) ?
                            boost::lexical_cast<std::string>(m_design_id->Eval()) :
                            m_design_id->Description();

    return str(FlexibleFormat((!negated)
        ? UserString("DESC_NUMBERED_SHIP_DESIGN")
        : UserString("DESC_NUMBERED_SHIP_DESIGN_NOT"))
               % id_str);
}

std::string Condition::NumberedShipDesign::Dump() const
{ return DumpIndent() + "NumberedShipDesign design_id = " + m_design_id->Dump(); }

bool Condition::NumberedShipDesign::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "NumberedShipDesign::Match passed no candidate object";
        return false;
    }
    return NumberedShipDesignSimpleMatch(m_design_id->Eval(local_context))(candidate);
}

///////////////////////////////////////////////////////////
// ProducedByEmpire                                      //
///////////////////////////////////////////////////////////
Condition::ProducedByEmpire::~ProducedByEmpire()
{ delete m_empire_id; }

bool Condition::ProducedByEmpire::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::ProducedByEmpire& rhs_ = static_cast<const Condition::ProducedByEmpire&>(rhs);

    CHECK_COND_VREF_MEMBER(m_empire_id)

    return true;
}

namespace {
    struct ProducedByEmpireSimpleMatch {
        ProducedByEmpireSimpleMatch(int empire_id) :
            m_empire_id(empire_id)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            if (TemporaryPtr<const ::Ship> ship = boost::dynamic_pointer_cast<const ::Ship>(candidate))
                return ship->ProducedByEmpireID() == m_empire_id;
            else if (TemporaryPtr<const ::Building> building = boost::dynamic_pointer_cast<const ::Building>(candidate))
                return building->ProducedByEmpireID() == m_empire_id;
            return false;
        }

        int m_empire_id;
    };
}

void Condition::ProducedByEmpire::Eval(const ScriptingContext& parent_context,
                                       ObjectSet& matches, ObjectSet& non_matches,
                                       SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ValueRef::ConstantExpr(m_empire_id) ||
                            (m_empire_id->LocalCandidateInvariant() &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate empire id once, and use to check all candidate objects
        TemporaryPtr<const UniverseObject> no_object;
        int empire_id = m_empire_id->Eval(ScriptingContext(parent_context, no_object));
        EvalImpl(matches, non_matches, search_domain, ProducedByEmpireSimpleMatch(empire_id));
    } else {
        // re-evaluate empire id for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::ProducedByEmpire::RootCandidateInvariant() const
{ return m_empire_id->RootCandidateInvariant(); }

bool Condition::ProducedByEmpire::TargetInvariant() const
{ return m_empire_id->TargetInvariant(); }

bool Condition::ProducedByEmpire::SourceInvariant() const
{ return m_empire_id->SourceInvariant(); }

std::string Condition::ProducedByEmpire::Description(bool negated/* = false*/) const {
    std::string empire_str;
    if (m_empire_id) {
        int empire_id = ALL_EMPIRES;
        if (ValueRef::ConstantExpr(m_empire_id))
            empire_id = m_empire_id->Eval();
        if (const Empire* empire = Empires().Lookup(empire_id))
            empire_str = empire->Name();
        else
            empire_str = m_empire_id->Description();
    }

    return str(FlexibleFormat((!negated)
        ? UserString("DESC_PRODUCED_BY_EMPIRE")
        : UserString("DESC_PRODUCED_BY_EMPIRE_NOT"))
               % empire_str);
}

std::string Condition::ProducedByEmpire::Dump() const
{ return DumpIndent() + "ProducedByEmpire empire_id = " + m_empire_id->Dump(); }

bool Condition::ProducedByEmpire::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "ProducedByEmpire::Match passed no candidate object";
        return false;
    }

    return ProducedByEmpireSimpleMatch(m_empire_id->Eval(local_context))(candidate);
}

///////////////////////////////////////////////////////////
// Chance                                                //
///////////////////////////////////////////////////////////
Condition::Chance::~Chance()
{ delete m_chance; }

bool Condition::Chance::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::Chance& rhs_ = static_cast<const Condition::Chance&>(rhs);

    CHECK_COND_VREF_MEMBER(m_chance)

    return true;
}

namespace {
    struct ChanceSimpleMatch {
        ChanceSimpleMatch(float chance) :
            m_chance(chance)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const
        { return RandZeroToOne() <= m_chance; }

        float m_chance;
    };
}

void Condition::Chance::Eval(const ScriptingContext& parent_context,
                             ObjectSet& matches, ObjectSet& non_matches,
                             SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ValueRef::ConstantExpr(m_chance) ||
                            (m_chance->LocalCandidateInvariant() &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate empire id once, and use to check all candidate objects
        TemporaryPtr<const UniverseObject> no_object;
        float chance = std::max(0.0, std::min(1.0, m_chance->Eval(ScriptingContext(parent_context, no_object))));
        EvalImpl(matches, non_matches, search_domain, ChanceSimpleMatch(chance));
    } else {
        // re-evaluate empire id for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::Chance::RootCandidateInvariant() const
{ return m_chance->RootCandidateInvariant(); }

bool Condition::Chance::TargetInvariant() const
{ return m_chance->TargetInvariant(); }

bool Condition::Chance::SourceInvariant() const
{ return m_chance->SourceInvariant(); }

std::string Condition::Chance::Description(bool negated/* = false*/) const {
    std::string value_str;
    if (ValueRef::ConstantExpr(m_chance)) {
        return str(FlexibleFormat((!negated)
            ? UserString("DESC_CHANCE_PERCENTAGE")
            : UserString("DESC_CHANCE_PERCENTAGE_NOT"))
                % boost::lexical_cast<std::string>(std::max(0.0, std::min(m_chance->Eval(), 1.0)) * 100));
    } else {
        return str(FlexibleFormat((!negated)
            ? UserString("DESC_CHANCE")
            : UserString("DESC_CHANCE_NOT"))
            % m_chance->Description());
    }
}

std::string Condition::Chance::Dump() const
{ return DumpIndent() + "Random probability = " + m_chance->Dump() + "\n"; }

bool Condition::Chance::Match(const ScriptingContext& local_context) const {
    float chance = std::max(0.0, std::min(m_chance->Eval(local_context), 1.0));
    return RandZeroToOne() <= chance;
}

///////////////////////////////////////////////////////////
// MeterValue                                            //
///////////////////////////////////////////////////////////
Condition::MeterValue::~MeterValue() {
    delete m_low;
    delete m_high;
}

bool Condition::MeterValue::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::MeterValue& rhs_ = static_cast<const Condition::MeterValue&>(rhs);

    if (m_meter != rhs_.m_meter)
        return false;

    CHECK_COND_VREF_MEMBER(m_low)
    CHECK_COND_VREF_MEMBER(m_high)

    return true;
}

namespace {
    struct MeterValueSimpleMatch {
        MeterValueSimpleMatch(float low, float high, MeterType meter_type) :
            m_low(low),
            m_high(high),
            m_meter_type(meter_type)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            if (const Meter* meter = candidate->GetMeter(m_meter_type)) {
                float value = meter->Initial();    // match Initial rather than Current to make results reproducible in a given turn, until back propegation happens
                return m_low <= value && value <= m_high;
            }

            return false;
        }

        float m_low;
        float m_high;
        MeterType m_meter_type;
    };

    std::string MeterTypeDumpString(MeterType meter) {
        switch (meter) {
        case INVALID_METER_TYPE:        return "INVALID_METER_TYPE"; break;
        case METER_TARGET_POPULATION:   return "TargetPopulation";   break;
        case METER_TARGET_INDUSTRY:     return "TargetIndustry";     break;
        case METER_TARGET_RESEARCH:     return "TargetResearch";     break;
        case METER_TARGET_TRADE:        return "TargetTrade";        break;
        case METER_TARGET_CONSTRUCTION: return "TargetConstruction"; break;
        case METER_MAX_FUEL:            return "MaxFuel";            break;
        case METER_MAX_SHIELD:          return "MaxShield";          break;
        case METER_MAX_STRUCTURE:       return "MaxStructure";       break;
        case METER_MAX_DEFENSE:         return "MaxDefense";         break;
        case METER_POPULATION:          return "Population";         break;
        case METER_INDUSTRY:            return "Industry";           break;
        case METER_RESEARCH:            return "Research";           break;
        case METER_TRADE:               return "Trade";              break;
        case METER_CONSTRUCTION:        return "Construction";       break;
        case METER_FUEL:                return "Fuel";               break;
        case METER_SHIELD:              return "Shield";             break;
        case METER_STRUCTURE:           return "Structure";          break;
        case METER_DEFENSE:             return "Defense";            break;
        case METER_SUPPLY:              return "Supply";             break;
        case METER_STEALTH:             return "Stealth";            break;
        case METER_DETECTION:           return "Detection";          break;
        case METER_BATTLE_SPEED:        return "BattleSpeed";        break;
        case METER_STARLANE_SPEED:      return "StarlaneSpeed";      break;
        case METER_DAMAGE:              return "Damage";             break;
        case METER_ROF:                 return "ROF";                break;
        case METER_RANGE:               return "Range";              break;
        case METER_SPEED:               return "Speed";              break;
        case METER_CAPACITY:            return "Capacity";           break;
        case METER_ANTI_SHIP_DAMAGE:    return "AntiShipDamage";     break;
        case METER_ANTI_FIGHTER_DAMAGE: return "AntiFighterDamage";  break;
        case METER_LAUNCH_RATE:         return "LaunchRate";         break;
        case METER_FIGHTER_WEAPON_RANGE:return "FighterWeaponRange"; break;
        default:                        return "?Meter?";            break;
        }
    }
}

void Condition::MeterValue::Eval(const ScriptingContext& parent_context,
                                 ObjectSet& matches, ObjectSet& non_matches,
                                 SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ((!m_low || m_low->LocalCandidateInvariant()) &&
                             (!m_high || m_high->LocalCandidateInvariant()) &&
                             (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate number limits once, use to match all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);
        float low = (m_low ? m_low->Eval(local_context) : -Meter::LARGE_VALUE);
        float high = (m_high ? m_high->Eval(local_context) : Meter::LARGE_VALUE);
        EvalImpl(matches, non_matches, search_domain, MeterValueSimpleMatch(low, high, m_meter));
    } else {
        // re-evaluate allowed turn range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::MeterValue::RootCandidateInvariant() const
{ return (!m_low || m_low->RootCandidateInvariant()) && (!m_high || m_high->RootCandidateInvariant()); }

bool Condition::MeterValue::TargetInvariant() const
{ return (!m_low || m_low->TargetInvariant()) && (!m_high || m_high->TargetInvariant()); }

bool Condition::MeterValue::SourceInvariant() const
{ return (!m_low || m_low->SourceInvariant()) && (!m_high || m_high->SourceInvariant()); }

std::string Condition::MeterValue::Description(bool negated/* = false*/) const {
    std::string low_str = (m_low ? (ValueRef::ConstantExpr(m_low) ?
                                    boost::lexical_cast<std::string>(m_low->Eval()) :
                                    m_low->Description())
                                 : boost::lexical_cast<std::string>(-Meter::LARGE_VALUE));
    std::string high_str = (m_high ? (ValueRef::ConstantExpr(m_high) ?
                                      boost::lexical_cast<std::string>(m_high->Eval()) :
                                      m_high->Description())
                                   : boost::lexical_cast<std::string>(Meter::LARGE_VALUE));
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_METER_VALUE_CURRENT")
        : UserString("DESC_METER_VALUE_CURRENT_NOT"))
        % UserString(boost::lexical_cast<std::string>(m_meter))
        % low_str
        % high_str);
}

std::string Condition::MeterValue::Dump() const {
    std::string retval = DumpIndent();
    retval += MeterTypeDumpString(m_meter);
    if (m_low)
        retval += " low = " + m_low->Dump();
    if (m_high)
        retval += " high = " + m_high->Dump();
    retval += "\n";
    return retval;
}

bool Condition::MeterValue::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "MeterValue::Match passed no candidate object";
        return false;
    }
    float low = (m_low ? m_low->Eval(local_context) : -Meter::LARGE_VALUE);
    float high = (m_high ? m_high->Eval(local_context) : Meter::LARGE_VALUE);
    return MeterValueSimpleMatch(low, high, m_meter)(candidate);
}

///////////////////////////////////////////////////////////
// ShipPartMeterValue                                    //
///////////////////////////////////////////////////////////
Condition::ShipPartMeterValue::~ShipPartMeterValue() {
    delete m_low;
    delete m_high;
}

bool Condition::ShipPartMeterValue::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::ShipPartMeterValue& rhs_ = static_cast<const Condition::ShipPartMeterValue&>(rhs);

    if (m_meter != rhs_.m_meter)
        return false;

    if (m_part_name != rhs_.m_part_name)
        return false;

    CHECK_COND_VREF_MEMBER(m_low)
    CHECK_COND_VREF_MEMBER(m_high)

    return true;
}

namespace {
    struct ShipPartMeterValueSimpleMatch {
        ShipPartMeterValueSimpleMatch(const std::string& ship_part_name,
                                      MeterType meter, float low, float high) :
            m_part_name(ship_part_name),
            m_low(low),
            m_high(high),
            m_meter(meter)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;
            TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(candidate);
            if (!ship)
                return false;
            const Meter* meter = ship->GetPartMeter(m_meter, m_part_name);
            if (!meter)
                return false;
            float meter_current = meter->Current();
            return (m_low <= meter_current && meter_current <= m_high);
        }

        std::string m_part_name;
        float      m_low;
        float      m_high;
        MeterType   m_meter;
    };
}

void Condition::ShipPartMeterValue::Eval(const ScriptingContext& parent_context,
                                         ObjectSet& matches, ObjectSet& non_matches,
                                         SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ((!m_low || m_low->LocalCandidateInvariant()) &&
                             (!m_high || m_high->LocalCandidateInvariant()) &&
                             (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate number limits once, use to match all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);
        float low = (m_low ? m_low->Eval(local_context) : -Meter::LARGE_VALUE);
        float high = (m_high ? m_high->Eval(local_context) : Meter::LARGE_VALUE);
        EvalImpl(matches, non_matches, search_domain, ShipPartMeterValueSimpleMatch(m_part_name, m_meter, low, high));
    } else {
        // re-evaluate allowed turn range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::ShipPartMeterValue::RootCandidateInvariant() const {
    return ((!m_low || m_low->RootCandidateInvariant()) &&
            (!m_high || m_high->RootCandidateInvariant()));
}

bool Condition::ShipPartMeterValue::TargetInvariant() const {
    return ((!m_low || m_low->TargetInvariant()) &&
            (!m_high || m_high->TargetInvariant()));
}

bool Condition::ShipPartMeterValue::SourceInvariant() const {
    return ((!m_low || m_low->SourceInvariant()) &&
            (!m_high || m_high->SourceInvariant()));
}

std::string Condition::ShipPartMeterValue::Description(bool negated/* = false*/) const {
    std::string low_str = (m_low ? (ValueRef::ConstantExpr(m_low) ?
                                    boost::lexical_cast<std::string>(m_low->Eval()) :
                                    m_low->Description())
                                 : boost::lexical_cast<std::string>(-Meter::LARGE_VALUE));
    std::string high_str = (m_high ? (ValueRef::ConstantExpr(m_high) ?
                                      boost::lexical_cast<std::string>(m_high->Eval()) :
                                      m_high->Description())
                                   : boost::lexical_cast<std::string>(Meter::LARGE_VALUE));
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_SHIP_PART_METER_VALUE_CURRENT")
        : UserString("DESC_SHIP_PART_METER_VALUE_CURRENT_NOT"))
               % UserString(boost::lexical_cast<std::string>(m_meter))
               % UserString(m_part_name)
               % low_str
               % high_str);
}

std::string Condition::ShipPartMeterValue::Dump() const {
    std::string retval = DumpIndent();
    retval += MeterTypeDumpString(m_meter);
    retval += " partname = " + m_part_name;
    if (m_low)
        retval += " low = " + m_low->Dump();
    if (m_high)
        retval += " high = " + m_high->Dump();
    retval += "\n";
    return retval;
}

bool Condition::ShipPartMeterValue::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "ShipPartMeterValue::Match passed no candidate object";
        return false;
    }
    float low = (m_low ? m_low->Eval(local_context) : -Meter::LARGE_VALUE);
    float high = (m_high ? m_high->Eval(local_context) : Meter::LARGE_VALUE);
    return ShipPartMeterValueSimpleMatch(m_part_name, m_meter, low, high)(candidate);
}

///////////////////////////////////////////////////////////
// EmpireMeterValue                                      //
///////////////////////////////////////////////////////////
Condition::EmpireMeterValue::~EmpireMeterValue() {
    delete m_empire_id;
    delete m_low;
    delete m_high;
}

bool Condition::EmpireMeterValue::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::EmpireMeterValue& rhs_ = static_cast<const Condition::EmpireMeterValue&>(rhs);

    if (m_empire_id != rhs_.m_empire_id)
        return false;

    if (m_meter != rhs_.m_meter)
        return false;

    CHECK_COND_VREF_MEMBER(m_low)
    CHECK_COND_VREF_MEMBER(m_high)

    return true;
}

namespace {
    struct EmpireMeterValueSimpleMatch {
        EmpireMeterValueSimpleMatch(int empire_id, float low, float high, const std::string& meter) :
            m_empire_id(empire_id),
            m_low(low),
            m_high(high),
            m_meter(meter)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;
            const Empire* empire = Empires().Lookup(m_empire_id);
            if (!empire)
                return false;
            const Meter* meter = empire->GetMeter(m_meter);
            if (!meter)
                return false;
            float meter_current = meter->Current();
            return (m_low <= meter_current && meter_current <= m_high);
        }

        int         m_empire_id;
        float      m_low;
        float      m_high;
        std::string m_meter;
    };
}

void Condition::EmpireMeterValue::Eval(const ScriptingContext& parent_context,
                                       ObjectSet& matches, ObjectSet& non_matches,
                                       SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ((m_empire_id && m_empire_id->LocalCandidateInvariant()) &&
                             (!m_low || m_low->LocalCandidateInvariant()) &&
                             (!m_high || m_high->LocalCandidateInvariant()) &&
                             (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate number limits once, use to match all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);
        int empire_id = m_empire_id->Eval(local_context);   // if m_empire_id not set, default to local candidate's owner, which is not target invariant
        float low = (m_low ? m_low->Eval(local_context) : -Meter::LARGE_VALUE);
        float high = (m_high ? m_high->Eval(local_context) : Meter::LARGE_VALUE);
        EvalImpl(matches, non_matches, search_domain, EmpireMeterValueSimpleMatch(empire_id, low, high, m_meter));
    } else {
        // re-evaluate allowed turn range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::EmpireMeterValue::RootCandidateInvariant() const {
    return (!m_empire_id || m_empire_id->RootCandidateInvariant()) &&
           (!m_low || m_low->RootCandidateInvariant()) &&
           (!m_high || m_high->RootCandidateInvariant());
}

bool Condition::EmpireMeterValue::TargetInvariant() const {
    return (!m_empire_id || m_empire_id->TargetInvariant()) &&
           (!m_low || m_low->TargetInvariant()) &&
           (!m_high || m_high->TargetInvariant());
}

bool Condition::EmpireMeterValue::SourceInvariant() const {
    return (!m_empire_id || m_empire_id->SourceInvariant()) &&
           (!m_low || m_low->SourceInvariant()) &&
           (!m_high || m_high->SourceInvariant());
}

std::string Condition::EmpireMeterValue::Description(bool negated/* = false*/) const {
    std::string empire_str;
    if (m_empire_id) {
        int empire_id = ALL_EMPIRES;
        if (ValueRef::ConstantExpr(m_empire_id))
            empire_id = m_empire_id->Eval();
        if (const Empire* empire = Empires().Lookup(empire_id))
            empire_str = empire->Name();
        else
            empire_str = m_empire_id->Description();
    }
    std::string low_str = (m_low ? (ValueRef::ConstantExpr(m_low) ?
                                    boost::lexical_cast<std::string>(m_low->Eval()) :
                                    m_low->Description())
                                 : boost::lexical_cast<std::string>(-Meter::LARGE_VALUE));
    std::string high_str = (m_high ? (ValueRef::ConstantExpr(m_high) ?
                                      boost::lexical_cast<std::string>(m_high->Eval()) :
                                      m_high->Description())
                                   : boost::lexical_cast<std::string>(Meter::LARGE_VALUE));
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_EMPIRE_METER_VALUE_CURRENT")
        : UserString("DESC_EMPIRE_METER_VALUE_CURRENT_NOT"))
               % UserString(m_meter)
               % low_str
               % high_str
               % empire_str);
}

std::string Condition::EmpireMeterValue::Dump() const {
    std::string retval = DumpIndent() + "EmpireMeterValue";
    if (m_empire_id)
        retval += " empire = " + m_empire_id->Dump();
    retval += " meter = " + m_meter;
    if (m_low)
        retval += " low = " + m_low->Dump();
    if (m_high)
        retval += " high = " + m_high->Dump();
    retval += "\n";
    return retval;
}

bool Condition::EmpireMeterValue::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "EmpireMeterValue::Match passed no candidate object";
        return false;
    }
    int empire_id = (m_empire_id ? m_empire_id->Eval(local_context) : candidate->Owner());
    if (empire_id == ALL_EMPIRES)
        return false;
    float low = (m_low ? m_low->Eval(local_context) : -Meter::LARGE_VALUE);
    float high = (m_high ? m_high->Eval(local_context) : Meter::LARGE_VALUE);
    return EmpireMeterValueSimpleMatch(empire_id, low, high, m_meter)(candidate);
}

///////////////////////////////////////////////////////////
// EmpireStockpileValue                                  //
///////////////////////////////////////////////////////////
Condition::EmpireStockpileValue::~EmpireStockpileValue() {
    delete m_low;
    delete m_high;
}

bool Condition::EmpireStockpileValue::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::EmpireStockpileValue& rhs_ = static_cast<const Condition::EmpireStockpileValue&>(rhs);

    if (m_stockpile != rhs_.m_stockpile)
        return false;

    CHECK_COND_VREF_MEMBER(m_low)
    CHECK_COND_VREF_MEMBER(m_high)

    return true;
}

namespace {
    struct EmpireStockpileValueSimpleMatch {
        EmpireStockpileValueSimpleMatch(float low, float high, ResourceType stockpile) :
            m_low(low),
            m_high(high),
            m_stockpile(stockpile)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            if (candidate->Unowned())
                return false;

            const Empire* empire = Empires().Lookup(candidate->Owner());
            if (!empire)
                return false;

            if (m_stockpile == RE_TRADE) {
                float amount = empire->ResourceStockpile(m_stockpile);
                return (m_low <= amount && amount <= m_high);
            }

            return false;
        }

        float m_low;
        float m_high;
        ResourceType m_stockpile;
    };
}

void Condition::EmpireStockpileValue::Eval(const ScriptingContext& parent_context,
                                           ObjectSet& matches, ObjectSet& non_matches,
                                           SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = (m_low->LocalCandidateInvariant() && m_high->LocalCandidateInvariant() &&
                             (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate number limits once, use to match all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);
        float low = m_low->Eval(local_context);
        float high = m_high->Eval(local_context);
        EvalImpl(matches, non_matches, search_domain, EmpireStockpileValueSimpleMatch(low, high, m_stockpile));
    } else {
        // re-evaluate allowed turn range for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::EmpireStockpileValue::RootCandidateInvariant() const
{ return (m_low->RootCandidateInvariant() && m_high->RootCandidateInvariant()); }

bool Condition::EmpireStockpileValue::TargetInvariant() const
{ return (m_low->TargetInvariant() && m_high->TargetInvariant()); }

bool Condition::EmpireStockpileValue::SourceInvariant() const
{ return (m_low->SourceInvariant() && m_high->SourceInvariant()); }

std::string Condition::EmpireStockpileValue::Description(bool negated/* = false*/) const {
    std::string low_str = ValueRef::ConstantExpr(m_low) ?
                            boost::lexical_cast<std::string>(m_low->Eval()) :
                            m_low->Description();
    std::string high_str = ValueRef::ConstantExpr(m_high) ?
                            boost::lexical_cast<std::string>(m_high->Eval()) :
                            m_high->Description();
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_EMPIRE_STOCKPILE_VALUE")
        : UserString("DESC_EMPIRE_STOCKPILE_VALUE_NOT"))
               % UserString(boost::lexical_cast<std::string>(m_stockpile))
               % low_str
               % high_str);
}

std::string Condition::EmpireStockpileValue::Dump() const {
    std::string retval = DumpIndent();
    switch (m_stockpile) {
    case RE_TRADE:      retval += "OwnerTradeStockpile";    break;
    case RE_RESEARCH:   retval += "OwnerResearchStockpile"; break;
    case RE_INDUSTRY:   retval += "OwnerIndustryStockpile"; break;
    default: retval += "?"; break;
    }
    retval += " low = " + m_low->Dump() + " high = " + m_high->Dump() + "\n";
    return retval;
}

bool Condition::EmpireStockpileValue::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "EmpireStockpileValue::Match passed no candidate object";
        return false;
    }

    float low = m_low->Eval(local_context);
    float high = m_high->Eval(local_context);
    return EmpireStockpileValueSimpleMatch(low, high, m_stockpile)(candidate);
}

///////////////////////////////////////////////////////////
// OwnerHasTech                                          //
///////////////////////////////////////////////////////////
bool Condition::OwnerHasTech::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::OwnerHasTech& rhs_ = static_cast<const Condition::OwnerHasTech&>(rhs);

    if (m_name != rhs_.m_name)
        return false;

    return true;
}

std::string Condition::OwnerHasTech::Description(bool negated/* = false*/) const {
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_OWNER_HAS_TECH")
        : UserString("DESC_OWNER_HAS_TECH_NOT"))
        % UserString(m_name));
}

std::string Condition::OwnerHasTech::Dump() const
{ return DumpIndent() + "OwnerHasTech name = \"" + m_name + "\"\n"; }

bool Condition::OwnerHasTech::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "OwnerHasTech::Match passed no candidate object";
        return false;
    }

    if (candidate->Unowned())
        return false;

    if (const Empire* empire = Empires().Lookup(candidate->Owner()))
        return empire->TechResearched(m_name);
    else
        return false;
}

///////////////////////////////////////////////////////////
// OwnerHasBuildingTypeAvailable                         //
///////////////////////////////////////////////////////////
bool Condition::OwnerHasBuildingTypeAvailable::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::OwnerHasBuildingTypeAvailable& rhs_ = static_cast<const Condition::OwnerHasBuildingTypeAvailable&>(rhs);

    if (m_name != rhs_.m_name)
        return false;

    return true;
}

std::string Condition::OwnerHasBuildingTypeAvailable::Description(bool negated/* = false*/) const {
    // used internally for a tooltip where context is apparent, so don't need
    // to name builing type here
    return (!negated)
        ? UserString("DESC_OWNER_HAS_BUILDING_TYPE")
        : UserString("DESC_OWNER_HAS_BUILDING_TYPE_NOT");
}

std::string Condition::OwnerHasBuildingTypeAvailable::Dump() const
{ return DumpIndent() + "OwnerHasBuildingTypeAvailable name = \"" + m_name + "\"\n"; }

bool Condition::OwnerHasBuildingTypeAvailable::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "OwnerHasBuildingTypeAvailable::Match passed no candidate object";
        return false;
    }

    if (candidate->Unowned())
        return false;

    if (const Empire* empire = Empires().Lookup(candidate->Owner()))
        return empire->BuildingTypeAvailable(m_name);
    else
        return false;
}

///////////////////////////////////////////////////////////
// OwnerHasShipDesignAvailable                           //
///////////////////////////////////////////////////////////
bool Condition::OwnerHasShipDesignAvailable::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::OwnerHasShipDesignAvailable& rhs_ = static_cast<const Condition::OwnerHasShipDesignAvailable&>(rhs);

    if (m_id != rhs_.m_id)
        return false;

    return true;
}

std::string Condition::OwnerHasShipDesignAvailable::Description(bool negated/* = false*/) const {
    // used internally for a tooltip where context is apparent, so don't need
    // to name design here
    return (!negated)
        ? UserString("DESC_OWNER_HAS_SHIP_DESIGN")
        : UserString("DESC_OWNER_HAS_SHIP_DESIGN_NOT");
}

std::string Condition::OwnerHasShipDesignAvailable::Dump() const
{ return DumpIndent() + "OwnerHasShipDesignAvailable id = \"" + boost::lexical_cast<std::string>(m_id) + "\"\n"; }

bool Condition::OwnerHasShipDesignAvailable::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "OwnerHasShipDesignAvailable::Match passed no candidate object";
        return false;
    }

    if (candidate->Unowned())
        return false;

    if (const Empire* empire = Empires().Lookup(candidate->Owner()))
        return empire->ShipDesignAvailable(m_id);
    else
        return false;
}

///////////////////////////////////////////////////////////
// VisibleToEmpire                                       //
///////////////////////////////////////////////////////////
Condition::VisibleToEmpire::~VisibleToEmpire()
{ delete m_empire_id; }

bool Condition::VisibleToEmpire::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::VisibleToEmpire& rhs_ = static_cast<const Condition::VisibleToEmpire&>(rhs);

    CHECK_COND_VREF_MEMBER(m_empire_id)

    return true;
}

namespace {
    struct VisibleToEmpireSimpleMatch {
        VisibleToEmpireSimpleMatch(int empire_id) :
            m_empire_id(empire_id)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;
            return candidate->GetVisibility(m_empire_id) != VIS_NO_VISIBILITY;
        }

        int m_empire_id;
    };
}

void Condition::VisibleToEmpire::Eval(const ScriptingContext& parent_context,
                                      ObjectSet& matches, ObjectSet& non_matches,
                                      SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ValueRef::ConstantExpr(m_empire_id) ||
                            (m_empire_id->LocalCandidateInvariant() &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate empire id once, and use to check all candidate objects
        TemporaryPtr<const UniverseObject> no_object;
        int empire_id = m_empire_id->Eval(ScriptingContext(parent_context, no_object));
        EvalImpl(matches, non_matches, search_domain, VisibleToEmpireSimpleMatch(empire_id));
    } else {
        // re-evaluate empire id for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::VisibleToEmpire::RootCandidateInvariant() const
{ return m_empire_id->RootCandidateInvariant(); }

bool Condition::VisibleToEmpire::TargetInvariant() const
{ return m_empire_id->TargetInvariant(); }

bool Condition::VisibleToEmpire::SourceInvariant() const
{ return m_empire_id->SourceInvariant(); }

std::string Condition::VisibleToEmpire::Description(bool negated/* = false*/) const {
    std::string empire_str;
    if (m_empire_id) {
        int empire_id = ALL_EMPIRES;
        if (ValueRef::ConstantExpr(m_empire_id))
            empire_id = m_empire_id->Eval();
        if (const Empire* empire = Empires().Lookup(empire_id))
            empire_str = empire->Name();
        else
            empire_str = m_empire_id->Description();
    }

    return str(FlexibleFormat((!negated)
        ? UserString("DESC_VISIBLE_TO_EMPIRE")
        : UserString("DESC_VISIBLE_TO_EMPIRE_NOT"))
               % empire_str);
}

std::string Condition::VisibleToEmpire::Dump() const
{ return DumpIndent() + "VisibleToEmpire empire_id = " + m_empire_id->Dump(); }

bool Condition::VisibleToEmpire::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "VisibleToEmpire::Match passed no candidate object";
        return false;
    }

    return candidate->GetVisibility(m_empire_id->Eval(local_context)) != VIS_NO_VISIBILITY;
}

///////////////////////////////////////////////////////////
// WithinDistance                                        //
///////////////////////////////////////////////////////////
Condition::WithinDistance::~WithinDistance() {
    delete m_distance;
    delete m_condition;
}

bool Condition::WithinDistance::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::WithinDistance& rhs_ = static_cast<const Condition::WithinDistance&>(rhs);

    CHECK_COND_VREF_MEMBER(m_distance)
    CHECK_COND_VREF_MEMBER(m_condition)

    return true;
}

namespace {
    struct WithinDistanceSimpleMatch {
        WithinDistanceSimpleMatch(const Condition::ObjectSet& from_objects, double distance) :
            m_from_objects(from_objects),
            m_distance2(distance*distance)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            // is candidate object close enough to any of the passed-in objects?
            for (Condition::ObjectSet::const_iterator it = m_from_objects.begin();
                 it != m_from_objects.end(); ++it)
            {
                double delta_x = candidate->X() - (*it)->X();
                double delta_y = candidate->Y() - (*it)->Y();
                if (delta_x*delta_x + delta_y*delta_y <= m_distance2)
                    return true;
            }

            return false;
        }

        const Condition::ObjectSet& m_from_objects;
        double m_distance2;
    };
}

void Condition::WithinDistance::Eval(const ScriptingContext& parent_context,
                                     ObjectSet& matches, ObjectSet& non_matches,
                                     SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = m_distance->LocalCandidateInvariant() &&
                            parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // evaluate contained objects once and check for all candidates

        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);

        // get subcondition matches
        ObjectSet subcondition_matches;
        m_condition->Eval(local_context, subcondition_matches);

        double distance = m_distance->Eval(local_context);

        EvalImpl(matches, non_matches, search_domain, WithinDistanceSimpleMatch(subcondition_matches, distance));
    } else {
        // re-evaluate contained objects for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::WithinDistance::RootCandidateInvariant() const
{ return m_distance->RootCandidateInvariant() && m_condition->RootCandidateInvariant(); }

bool Condition::WithinDistance::TargetInvariant() const
{ return m_distance->TargetInvariant() && m_condition->TargetInvariant(); }

bool Condition::WithinDistance::SourceInvariant() const
{ return m_distance->SourceInvariant() && m_condition->SourceInvariant(); }

std::string Condition::WithinDistance::Description(bool negated/* = false*/) const {
    std::string value_str = ValueRef::ConstantExpr(m_distance) ?
                                boost::lexical_cast<std::string>(m_distance->Eval()) :
                                m_distance->Description();
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_WITHIN_DISTANCE")
        : UserString("DESC_WITHIN_DISTANCE_NOT"))
               % value_str
               % m_condition->Description());
}

std::string Condition::WithinDistance::Dump() const {
    std::string retval = DumpIndent() + "WithinDistance distance = " + m_distance->Dump() + " condition =\n";
    ++g_indent;
    retval += m_condition->Dump();
    --g_indent;
    return retval;
}

bool Condition::WithinDistance::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "WithinDistance::Match passed no candidate object";
        return false;
    }

    // get subcondition matches
    ObjectSet subcondition_matches;
    m_condition->Eval(local_context, subcondition_matches);
    if (subcondition_matches.empty())
        return false;

    return WithinDistanceSimpleMatch(subcondition_matches, m_distance->Eval(local_context))(candidate);
}

///////////////////////////////////////////////////////////
// WithinStarlaneJumps                                   //
///////////////////////////////////////////////////////////
Condition::WithinStarlaneJumps::~WithinStarlaneJumps() {
    delete m_jumps;
    delete m_condition;
}

bool Condition::WithinStarlaneJumps::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::WithinStarlaneJumps& rhs_ = static_cast<const Condition::WithinStarlaneJumps&>(rhs);

    CHECK_COND_VREF_MEMBER(m_jumps)
    CHECK_COND_VREF_MEMBER(m_condition)

    return true;
}

namespace {
    const int MANY_JUMPS(999999);

    int JumpsBetweenObjects(TemporaryPtr<const UniverseObject> one, TemporaryPtr<const UniverseObject> two) {
        if (!one || !two)
            return MANY_JUMPS;

        TemporaryPtr<const System> system_one = GetSystem(one->SystemID());
        TemporaryPtr<const System> system_two = GetSystem(two->SystemID());

        if (system_one && system_two) {
            // both condition-matching object and candidate are / in systems.
            // can just find the shortest path between the two systems
            short jumps = -1;
            try {
                jumps = GetUniverse().JumpDistance(system_one->ID(), system_two->ID());
            } catch (...) {
                Logger().errorStream() << "JumpsBetweenObjects caught exception when calling JumpDistance";
            }
            if (jumps != -1)    // if jumps is -1, no path exists between the systems
                return static_cast<int>(jumps);
            else
                return MANY_JUMPS;

        } else if (system_one) {
            // just object one is / in a system.
            if (TemporaryPtr<const Fleet> fleet = FleetFromObject(two)) {
                // other object is a fleet that is between systems
                // need to check shortest path from systems on either side of starlane fleet is on
                short jumps1 = -1, jumps2 = -1;
                try {
                    if (fleet->PreviousSystemID() != -1)
                        jumps1 = GetUniverse().JumpDistance(system_one->ID(), fleet->PreviousSystemID());
                    if (fleet->NextSystemID() != -1)
                        jumps2 = GetUniverse().JumpDistance(system_one->ID(), fleet->NextSystemID());
                } catch (...) {
                    Logger().errorStream() << "JumpsBetweenObjects caught exception when calling JumpDistance";
                }
                int jumps = static_cast<int>(std::max(jumps1, jumps2));
                if (jumps != -1) {
                    return jumps - 1;
                } else {
                    // no path
                    return MANY_JUMPS;
                }
            }

        } else if (system_two) {
            // just object two is a system.
            if (TemporaryPtr<const Fleet> fleet = FleetFromObject(one)) {
                // other object is a fleet that is between systems
                // need to check shortest path from systems on either side of starlane fleet is on
                short jumps1 = -1, jumps2 = -1;
                try {
                    if (fleet->PreviousSystemID() != -1)
                        jumps1 = GetUniverse().JumpDistance(system_two->ID(), fleet->PreviousSystemID());
                    if (fleet->NextSystemID() != -1)
                        jumps2 = GetUniverse().JumpDistance(system_two->ID(), fleet->NextSystemID());
                } catch (...) {
                    Logger().errorStream() << "JumpsBetweenObjects caught exception when calling JumpDistance";
                }
                int jumps = static_cast<int>(std::max(jumps1, jumps2));
                if (jumps != -1) {
                    return jumps - 1;
                } else {
                    // no path
                    return MANY_JUMPS;
                }
            }
        } else {
            // neither object is / in a system

            TemporaryPtr<const Fleet> fleet_one = FleetFromObject(one);
            TemporaryPtr<const Fleet> fleet_two = FleetFromObject(two);

            if (fleet_one && fleet_two) {
                // both objects are / in a fleet.
                // need to check all combinations of systems on either sides of
                // starlanes condition-matching object and candidate are on
                int fleet_one_prev_system_id = fleet_one->PreviousSystemID();
                int fleet_one_next_system_id = fleet_one->NextSystemID();
                int fleet_two_prev_system_id = fleet_two->PreviousSystemID();
                int fleet_two_next_system_id = fleet_two->NextSystemID();
                short jumps1 = -1, jumps2 = -1, jumps3 = -1, jumps4 = -1;
                try {
                    if (fleet_one_prev_system_id != -1 && fleet_two_prev_system_id != -1)
                        jumps1 = GetUniverse().JumpDistance(fleet_one_prev_system_id, fleet_two_prev_system_id);
                    if (fleet_one_prev_system_id != -1 && fleet_two_next_system_id != -1)
                        jumps2 = GetUniverse().JumpDistance(fleet_one_prev_system_id, fleet_two_next_system_id);
                    if (fleet_one_next_system_id != -1 && fleet_two_prev_system_id != -1)
                        jumps3 = GetUniverse().JumpDistance(fleet_one_next_system_id, fleet_two_prev_system_id);
                    if (fleet_one_next_system_id != -1 && fleet_two_next_system_id != -1)
                        jumps4 = GetUniverse().JumpDistance(fleet_one_next_system_id, fleet_two_next_system_id);
                } catch (...) {
                    Logger().errorStream() << "JumpsBetweenObjects caught exception when calling JumpDistance";
                }
                int jumps = static_cast<int>(std::max(jumps1, std::max(jumps2, std::max(jumps3, jumps4))));
                if (jumps != -1) {
                    return jumps - 1;
                } else {
                    // no path
                    return MANY_JUMPS;
                }
            }
        }
        return MANY_JUMPS;
    }

    struct WithinStarlaneJumpsSimpleMatch {
        WithinStarlaneJumpsSimpleMatch(const Condition::ObjectSet& from_objects, int jump_limit) :
            m_from_objects(from_objects),
            m_jump_limit(jump_limit)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;
            if (m_from_objects.empty())
                return false;
            if (m_jump_limit < 0)
                return false;

            // is candidate object close enough to any subcondition matches?
            for (Condition::ObjectSet::const_iterator it = m_from_objects.begin(); it != m_from_objects.end(); ++it) {
                if (m_jump_limit == 0) {
                    // special case, since LeastJumpsPath() doesn't expect the start point to be the end point
                    double delta_x = (*it)->X() - candidate->X();
                    double delta_y = (*it)->Y() - candidate->Y();
                    if (delta_x*delta_x + delta_y*delta_y == 0)
                        return true;
                } else {
                    int jumps = JumpsBetweenObjects(*it, candidate);
                    if (jumps != -1 && jumps <= m_jump_limit)
                        return true;
                }
            }

            return false;
        }

        const Condition::ObjectSet& m_from_objects;
        int m_jump_limit;
    };
}

void Condition::WithinStarlaneJumps::Eval(const ScriptingContext& parent_context,
                                          ObjectSet& matches, ObjectSet& non_matches,
                                          SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = m_jumps->LocalCandidateInvariant() &&
                            parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // evaluate contained objects once and check for all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);

        // get subcondition matches
        ObjectSet subcondition_matches;
        m_condition->Eval(local_context, subcondition_matches);
        int jump_limit = m_jumps->Eval(local_context);

        EvalImpl(matches, non_matches, search_domain, WithinStarlaneJumpsSimpleMatch(subcondition_matches, jump_limit));
    } else {
        // re-evaluate contained objects for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::WithinStarlaneJumps::RootCandidateInvariant() const
{ return m_jumps->RootCandidateInvariant() && m_condition->RootCandidateInvariant(); }

bool Condition::WithinStarlaneJumps::TargetInvariant() const
{ return m_jumps->TargetInvariant() && m_condition->TargetInvariant(); }

bool Condition::WithinStarlaneJumps::SourceInvariant() const
{ return m_jumps->SourceInvariant() && m_condition->SourceInvariant(); }

std::string Condition::WithinStarlaneJumps::Description(bool negated/* = false*/) const {
    std::string value_str = ValueRef::ConstantExpr(m_jumps) ? boost::lexical_cast<std::string>(m_jumps->Eval()) : m_jumps->Description();
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_WITHIN_STARLANE_JUMPS")
        : UserString("DESC_WITHIN_STARLANE_JUMPS_NOT"))
               % value_str
               % m_condition->Description());
}

std::string Condition::WithinStarlaneJumps::Dump() const {
    std::string retval = DumpIndent() + "WithinStarlaneJumps jumps = " + m_jumps->Dump() + " condition =\n";
    ++g_indent;
    retval += m_condition->Dump();
    --g_indent;
    return retval;
}

bool Condition::WithinStarlaneJumps::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "WithinStarlaneJumps::Match passed no candidate object";
        return false;
    }

    // get subcondition matches
    ObjectSet subcondition_matches;
    m_condition->Eval(local_context, subcondition_matches);
    int jump_limit = m_jumps->Eval(local_context);

    return WithinStarlaneJumpsSimpleMatch(subcondition_matches, jump_limit)(candidate);
}

///////////////////////////////////////////////////////////
// CanAddStarlaneConnection                              //
///////////////////////////////////////////////////////////
Condition::CanAddStarlaneConnection::~CanAddStarlaneConnection()
{ delete m_condition; }

bool Condition::CanAddStarlaneConnection::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::CanAddStarlaneConnection& rhs_ = static_cast<const Condition::CanAddStarlaneConnection&>(rhs);

    CHECK_COND_VREF_MEMBER(m_condition)

    return true;
}

namespace {
    struct CanAddStarlaneConnectionSimpleMatch {
        CanAddStarlaneConnectionSimpleMatch(const Condition::ObjectSet& destination_objects) :
            m_destination_objects(destination_objects)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;
            // TODO: implement this test
            return true;
        }

        const Condition::ObjectSet& m_destination_objects;
    };
}

void Condition::CanAddStarlaneConnection::Eval(const ScriptingContext& parent_context,
                                               ObjectSet& matches, ObjectSet& non_matches,
                                               SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // evaluate contained objects once and check for all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);

        // get subcondition matches
        ObjectSet subcondition_matches;
        m_condition->Eval(local_context, subcondition_matches);

        EvalImpl(matches, non_matches, search_domain, CanAddStarlaneConnectionSimpleMatch(subcondition_matches));
    } else {
        // re-evaluate contained objects for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::CanAddStarlaneConnection::RootCandidateInvariant() const
{ return m_condition->RootCandidateInvariant(); }

bool Condition::CanAddStarlaneConnection::TargetInvariant() const
{ return m_condition->TargetInvariant(); }

bool Condition::CanAddStarlaneConnection::SourceInvariant() const
{ return m_condition->SourceInvariant(); }

std::string Condition::CanAddStarlaneConnection::Description(bool negated/* = false*/) const {
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_CAN_ADD_STARLANE_CONNECTION")
        : UserString("DESC_CAN_ADD_STARLANE_CONNECTION_NOT"))
        % m_condition->Description());
}

std::string Condition::CanAddStarlaneConnection::Dump() const {
    std::string retval = DumpIndent() + "CanAddStarlaneConnection condition =\n";
    ++g_indent;
        retval += m_condition->Dump();
    --g_indent;
    return retval;
}

bool Condition::CanAddStarlaneConnection::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "CanAddStarlaneConnection::Match passed no candidate object";
        return false;
    }

    // get subcondition matches
    ObjectSet subcondition_matches;
    m_condition->Eval(local_context, subcondition_matches);

    return CanAddStarlaneConnectionSimpleMatch(subcondition_matches)(candidate);
}

///////////////////////////////////////////////////////////
// ExploredByEmpire                                      //
///////////////////////////////////////////////////////////
Condition::ExploredByEmpire::~ExploredByEmpire()
{ delete m_empire_id; }

bool Condition::ExploredByEmpire::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::ExploredByEmpire& rhs_ = static_cast<const Condition::ExploredByEmpire&>(rhs);

    CHECK_COND_VREF_MEMBER(m_empire_id)

    return true;
}

namespace {
    struct ExploredByEmpireSimpleMatch {
        ExploredByEmpireSimpleMatch(int empire_id) :
            m_empire_id(empire_id)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            const Empire* empire = Empires().Lookup(m_empire_id);
            if (!empire)
                return false;
            return empire->HasExploredSystem(candidate->ID());
        }

        int m_empire_id;
    };
}

void Condition::ExploredByEmpire::Eval(const ScriptingContext& parent_context,
                                       ObjectSet& matches, ObjectSet& non_matches,
                                       SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ValueRef::ConstantExpr(m_empire_id) ||
                            (m_empire_id->LocalCandidateInvariant() &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate empire id once, and use to check all candidate objects
        TemporaryPtr<const UniverseObject> no_object;
        int empire_id = m_empire_id->Eval(ScriptingContext(parent_context, no_object));
        EvalImpl(matches, non_matches, search_domain, ExploredByEmpireSimpleMatch(empire_id));
    } else {
        // re-evaluate empire id for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::ExploredByEmpire::RootCandidateInvariant() const
{ return m_empire_id->RootCandidateInvariant(); }

bool Condition::ExploredByEmpire::TargetInvariant() const
{ return m_empire_id->TargetInvariant(); }

bool Condition::ExploredByEmpire::SourceInvariant() const
{ return m_empire_id->SourceInvariant(); }

std::string Condition::ExploredByEmpire::Description(bool negated/* = false*/) const {
    std::string empire_str;
    if (m_empire_id) {
        int empire_id = ALL_EMPIRES;
        if (ValueRef::ConstantExpr(m_empire_id))
            empire_id = m_empire_id->Eval();
        if (const Empire* empire = Empires().Lookup(empire_id))
            empire_str = empire->Name();
        else
            empire_str = m_empire_id->Description();
    }

    return str(FlexibleFormat((!negated)
               ? UserString("DESC_EXPLORED_BY_EMPIRE")
               : UserString("DESC_EXPLORED_BY_EMPIRE_NOT"))
               % empire_str);
}

std::string Condition::ExploredByEmpire::Dump() const
{ return DumpIndent() + "ExploredByEmpire empire_id = " + m_empire_id->Dump(); }

bool Condition::ExploredByEmpire::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "ExploredByEmpire::Match passed no candidate object";
        return false;
    }

    return ExploredByEmpireSimpleMatch(m_empire_id->Eval(local_context))(candidate);
}

///////////////////////////////////////////////////////////
// Stationary                                            //
///////////////////////////////////////////////////////////
bool Condition::Stationary::operator==(const Condition::ConditionBase& rhs) const
{ return Condition::ConditionBase::operator==(rhs); }

std::string Condition::Stationary::Description(bool negated/* = false*/) const {
    return (!negated)
        ? UserString("DESC_STATIONARY")
        : UserString("DESC_STATIONARY_NOT");
}

std::string Condition::Stationary::Dump() const
{ return DumpIndent() + "Stationary\n"; }

bool Condition::Stationary::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "Stationary::Match passed no candidate object";
        return false;
    }

    // the only objects that can move are fleets and the ships in them.  so,
    // attempt to cast the candidate object to a fleet or ship, and if it's a ship
    // get the fleet of that ship
    TemporaryPtr<const Fleet> fleet = boost::dynamic_pointer_cast<const Fleet>(candidate);
    if (!fleet)
        if (TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(candidate))
            fleet = GetFleet(ship->FleetID());

    if (fleet) {
        // if a fleet is available, it is "moving", or not stationary, if it's
        // next system is a system and isn't the current system.  This will
        // mean fleets that have arrived at a system on the current turn will
        // be stationary, but fleets departing won't be stationary.
        int next_id = fleet->NextSystemID();
        int cur_id = fleet->SystemID();
        if (next_id != INVALID_OBJECT_ID && next_id != cur_id)
            return false;
    }

    return true;
}

///////////////////////////////////////////////////////////
// FleetSupplyableByEmpire                               //
///////////////////////////////////////////////////////////
Condition::FleetSupplyableByEmpire::~FleetSupplyableByEmpire()
{ delete m_empire_id; }

bool Condition::FleetSupplyableByEmpire::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::FleetSupplyableByEmpire& rhs_ = static_cast<const Condition::FleetSupplyableByEmpire&>(rhs);

    CHECK_COND_VREF_MEMBER(m_empire_id)

    return true;
}

namespace {
    struct FleetSupplyableSimpleMatch {
        FleetSupplyableSimpleMatch(int empire_id) :
            m_empire_id(empire_id)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;

            const EmpireManager& empires = Empires();
            const Empire* empire = empires.Lookup(m_empire_id);
            if (!empire)
                return false;

            const std::set<int>& supplyable_systems = empire->FleetSupplyableSystemIDs();
            return supplyable_systems.find(candidate->SystemID()) != supplyable_systems.end();
        }

        int m_empire_id;
    };
}

void Condition::FleetSupplyableByEmpire::Eval(const ScriptingContext& parent_context,
                                              ObjectSet& matches, ObjectSet& non_matches,
                                              SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ValueRef::ConstantExpr(m_empire_id) ||
                            (m_empire_id->LocalCandidateInvariant() &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate empire id once, and use to check all candidate objects
        TemporaryPtr<const UniverseObject> no_object;
        int empire_id = m_empire_id->Eval(ScriptingContext(parent_context, no_object));
        EvalImpl(matches, non_matches, search_domain, FleetSupplyableSimpleMatch(empire_id));
    } else {
        // re-evaluate empire id for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::FleetSupplyableByEmpire::RootCandidateInvariant() const
{ return m_empire_id->RootCandidateInvariant(); }

bool Condition::FleetSupplyableByEmpire::TargetInvariant() const
{ return m_empire_id->TargetInvariant(); }

bool Condition::FleetSupplyableByEmpire::SourceInvariant() const
{ return m_empire_id->SourceInvariant(); }

std::string Condition::FleetSupplyableByEmpire::Description(bool negated/* = false*/) const {
    std::string empire_str;
    if (m_empire_id) {
        int empire_id = ALL_EMPIRES;
        if (ValueRef::ConstantExpr(m_empire_id))
            empire_id = m_empire_id->Eval();
        if (const Empire* empire = Empires().Lookup(empire_id))
            empire_str = empire->Name();
        else
            empire_str = m_empire_id->Description();
    }

    return str(FlexibleFormat((!negated)
               ? UserString("DESC_SUPPLY_CONNECTED_FLEET")
               : UserString("DESC_SUPPLY_CONNECTED_FLEET_NOT"))
               % empire_str);
}

std::string Condition::FleetSupplyableByEmpire::Dump() const
{ return DumpIndent() + "ResupplyableBy empire_id = " + m_empire_id->Dump(); }

bool Condition::FleetSupplyableByEmpire::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "FleetSupplyableByEmpire::Match passed no candidate object";
        return false;
    }

    int empire_id = m_empire_id->Eval(local_context);

    return FleetSupplyableSimpleMatch(empire_id)(candidate);
}

///////////////////////////////////////////////////////////
// ResourceSupplyConnectedByEmpire                       //
///////////////////////////////////////////////////////////
Condition::ResourceSupplyConnectedByEmpire::~ResourceSupplyConnectedByEmpire() {
    delete m_empire_id;
    delete m_condition;
}

bool Condition::ResourceSupplyConnectedByEmpire::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::ResourceSupplyConnectedByEmpire& rhs_ = static_cast<const Condition::ResourceSupplyConnectedByEmpire&>(rhs);

    CHECK_COND_VREF_MEMBER(m_empire_id)

    return true;
}

namespace {
    struct ResourceSupplySimpleMatch {
        ResourceSupplySimpleMatch(int empire_id, const Condition::ObjectSet& from_objects) :
            m_empire_id(empire_id),
            m_from_objects(from_objects)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;
            if (m_from_objects.empty())
                return false;
            const Empire* empire = Empires().Lookup(m_empire_id);
            if (!empire)
                return false;
            const std::set<std::set<int> >& groups = empire->ResourceSupplyGroups();

            // is candidate object connected to a subcondition matching object by resource supply?
            for (Condition::ObjectSet::const_iterator it = m_from_objects.begin(); it != m_from_objects.end(); ++it) {
                TemporaryPtr<const UniverseObject> from_object(*it);

                for (std::set<std::set<int> >::const_iterator groups_it = groups.begin(); groups_it != groups.end(); ++groups_it) {
                    const std::set<int>& group = *groups_it;
                    if (group.find(from_object->SystemID()) != group.end()) {
                        // found resource sharing group containing test object.  Does it also contain candidate?
                        if (group.find(candidate->SystemID()) != group.end())
                            return true;    // test object and candidate object are in same resourse sharing group
                        else
                            return false;   // test object is not in resource sharing group with candidate (each object can be in only one group)
                    }
                    // current subcondition-matching object is not in this resource sharing group
                }
                // current subcondition-matching object is not in any resource sharing group for this empire
            }

            return false;
        }

        int m_empire_id;
        const Condition::ObjectSet& m_from_objects;
    };
}

void Condition::ResourceSupplyConnectedByEmpire::Eval(const ScriptingContext& parent_context,
                                                      ObjectSet& matches, ObjectSet& non_matches,
                                                      SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ValueRef::ConstantExpr(m_empire_id) ||
                            (m_empire_id->LocalCandidateInvariant() &&
                            (parent_context.condition_root_candidate || RootCandidateInvariant()));
    if (simple_eval_safe) {
        // evaluate contained objects once and check for all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);

        // get objects to be considering for matching against subcondition
        ObjectSet subcondition_matches;
        m_condition->Eval(local_context, subcondition_matches);

        int empire_id = m_empire_id->Eval(local_context);

        EvalImpl(matches, non_matches, search_domain, ResourceSupplySimpleMatch(empire_id, subcondition_matches));
    } else {
        // re-evaluate empire id for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::ResourceSupplyConnectedByEmpire::RootCandidateInvariant() const
{ return m_empire_id->RootCandidateInvariant() && m_condition->RootCandidateInvariant(); }

bool Condition::ResourceSupplyConnectedByEmpire::TargetInvariant() const
{ return m_empire_id->TargetInvariant() && m_condition->TargetInvariant(); }

bool Condition::ResourceSupplyConnectedByEmpire::SourceInvariant() const
{ return m_empire_id->SourceInvariant() && m_condition->SourceInvariant(); }

bool Condition::ResourceSupplyConnectedByEmpire::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "ResourceSupplyConnectedByEmpire::Match passed no candidate object";
        return false;
    }

    // get subcondition matches
    ObjectSet subcondition_matches;
    m_condition->Eval(local_context, subcondition_matches);
    int empire_id = m_empire_id->Eval(local_context);

    return ResourceSupplySimpleMatch(empire_id, subcondition_matches)(candidate);
}

std::string Condition::ResourceSupplyConnectedByEmpire::Description(bool negated/* = false*/) const {
    std::string empire_str;
    if (m_empire_id) {
        int empire_id = ALL_EMPIRES;
        if (ValueRef::ConstantExpr(m_empire_id))
            empire_id = m_empire_id->Eval();
        if (const Empire* empire = Empires().Lookup(empire_id))
            empire_str = empire->Name();
        else
            empire_str = m_empire_id->Description();
    }

    return str(FlexibleFormat((!negated)
               ? UserString("DESC_SUPPLY_CONNECTED_RESOURCE")
               : UserString("DESC_SUPPLY_CONNECTED_RESOURCE_NOT"))
               % empire_str
               % m_condition->Description());
}

std::string Condition::ResourceSupplyConnectedByEmpire::Dump() const {
    std::string retval = DumpIndent() + "ResourceSupplyConnectedBy empire_id = " + m_empire_id->Dump() +
                                        " condition = \n";
    ++g_indent;
    retval += m_condition->Dump();
    --g_indent;
    return retval;
}

///////////////////////////////////////////////////////////
// CanColonize                                           //
///////////////////////////////////////////////////////////
bool Condition::CanColonize::operator==(const Condition::ConditionBase& rhs) const
{ return Condition::ConditionBase::operator==(rhs); }

std::string Condition::CanColonize::Description(bool negated/* = false*/) const {
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_CAN_COLONIZE")
        : UserString("DESC_CAN_COLONIZE_NOT")));
}

std::string Condition::CanColonize::Dump() const
{ return DumpIndent() + "CanColonize\n"; }

bool Condition::CanColonize::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "CanColonize::Match passed no candidate object";
        return false;
    }

    // is it a ship, a planet, or a building on a planet?
    std::string species_name;
    if (candidate->ObjectType() == OBJ_PLANET) {
        TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
        if (!planet) {
            Logger().errorStream() << "CanColonize couldn't cast supposedly planet candidate";
            return false;
        }
        species_name = planet->SpeciesName();

    } else if (candidate->ObjectType() == OBJ_BUILDING) {
        TemporaryPtr<const ::Building> building = boost::dynamic_pointer_cast<const ::Building>(candidate);
        if (!building) {
            Logger().errorStream() << "CanColonize couldn't cast supposedly building candidate";
            return false;
        }
        TemporaryPtr<const Planet> planet = GetPlanet(building->PlanetID());
        if (!planet) {
            Logger().errorStream() << "CanColonize couldn't get building's planet";
            return false;
        }
        species_name = planet->SpeciesName();

    } else if (candidate->ObjectType() == OBJ_SHIP) {
        TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(candidate);
        if (!ship) {
            Logger().errorStream() << "CanColonize couldn't cast supposedly ship candidate";
            return false;
        }
        species_name = ship->SpeciesName();
    }

    if (species_name.empty())
        return false;
    const ::Species* species = GetSpecies(species_name);
    if (!species) {
        Logger().errorStream() << "CanColonize couldn't get species: " << species_name;
        return false;
    }
    return species->CanColonize();
}

///////////////////////////////////////////////////////////
// CanProduceShips                                       //
///////////////////////////////////////////////////////////
bool Condition::CanProduceShips::operator==(const Condition::ConditionBase& rhs) const
{ return Condition::ConditionBase::operator==(rhs); }

std::string Condition::CanProduceShips::Description(bool negated/* = false*/) const {
    return str(FlexibleFormat((!negated)
        ? UserString("DESC_CAN_PRODUCE_SHIPS")
        : UserString("DESC_CAN_PRODUCE_SHIPS_NOT")));
}

std::string Condition::CanProduceShips::Dump() const
{ return DumpIndent() + "CanColonize\n"; }

bool Condition::CanProduceShips::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "CanProduceShips::Match passed no candidate object";
        return false;
    }

    // is it a ship, a planet, or a building on a planet?
    std::string species_name;
    if (candidate->ObjectType() == OBJ_PLANET) {
        TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
        if (!planet) {
            Logger().errorStream() << "CanProduceShips couldn't cast supposedly planet candidate";
            return false;
        }
        species_name = planet->SpeciesName();

    } else if (candidate->ObjectType() == OBJ_BUILDING) {
        TemporaryPtr<const ::Building> building = boost::dynamic_pointer_cast<const ::Building>(candidate);
        if (!building) {
            Logger().errorStream() << "CanProduceShips couldn't cast supposedly building candidate";
            return false;
        }
        TemporaryPtr<const Planet> planet = GetPlanet(building->PlanetID());
        if (!planet) {
            Logger().errorStream() << "CanProduceShips couldn't get building's planet";
            return false;
        }
        species_name = planet->SpeciesName();

    } else if (candidate->ObjectType() == OBJ_SHIP) {
        TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(candidate);
        if (!ship) {
            Logger().errorStream() << "CanProduceShips couldn't cast supposedly ship candidate";
            return false;
        }
        species_name = ship->SpeciesName();
    }

    if (species_name.empty())
        return false;
    const ::Species* species = GetSpecies(species_name);
    if (!species) {
        Logger().errorStream() << "CanProduceShips couldn't get species: " << species_name;
        return false;
    }
    return species->CanProduceShips();
}

///////////////////////////////////////////////////////////
// OrderedBombarded                               //
///////////////////////////////////////////////////////////
Condition::OrderedBombarded::~OrderedBombarded()
{ delete m_by_object_condition; }

bool Condition::OrderedBombarded::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::OrderedBombarded& rhs_ = static_cast<const Condition::OrderedBombarded&>(rhs);

    CHECK_COND_VREF_MEMBER(m_by_object_condition)

    return true;
}

namespace {
    struct OrderedBombardedSimpleMatch {
        OrderedBombardedSimpleMatch(const Condition::ObjectSet& by_objects) :
            m_by_objects(by_objects)
        {}

        bool operator()(TemporaryPtr<const UniverseObject> candidate) const {
            if (!candidate)
                return false;
            if (m_by_objects.empty())
                return false;
            TemporaryPtr<const Planet> planet = boost::dynamic_pointer_cast<const Planet>(candidate);
            if (!planet)
                return false;
            int planet_id = planet->ID();
            if (planet_id == INVALID_OBJECT_ID)
                return false;

            // check if any of the by_objects is ordered to bombard the candidate planet
            for (Condition::ObjectSet::const_iterator it = m_by_objects.begin();
                 it != m_by_objects.end(); ++it)
            {
                TemporaryPtr<const Ship> ship = boost::dynamic_pointer_cast<const Ship>(*it);
                if (!ship)
                    continue;
                if (ship->OrderedBombardPlanet() == planet_id)
                    return true;
            }
            return false;
        }

        const Condition::ObjectSet& m_by_objects;
    };
}

void Condition::OrderedBombarded::Eval(const ScriptingContext& parent_context,
                                       ObjectSet& matches, ObjectSet& non_matches,
                                       SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = parent_context.condition_root_candidate || RootCandidateInvariant();
    if (simple_eval_safe) {
        // evaluate contained objects once and check for all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);

        // get subcondition matches
        ObjectSet subcondition_matches;
        m_by_object_condition->Eval(local_context, subcondition_matches);

        EvalImpl(matches, non_matches, search_domain, OrderedBombardedSimpleMatch(subcondition_matches));
    } else {
        // re-evaluate contained objects for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::OrderedBombarded::RootCandidateInvariant() const
{ return m_by_object_condition->RootCandidateInvariant(); }

bool Condition::OrderedBombarded::TargetInvariant() const
{ return m_by_object_condition->TargetInvariant(); }

bool Condition::OrderedBombarded::SourceInvariant() const
{ return m_by_object_condition->SourceInvariant(); }

std::string Condition::OrderedBombarded::Description(bool negated/* = false*/) const {
    std::string by_str;
    if (m_by_object_condition)
        by_str = m_by_object_condition->Description();

    return str(FlexibleFormat((!negated)
               ? UserString("DESC_ORDERED_BOMBARDED")
               : UserString("DESC_ORDERED_BOMBARDED_NOT"))
               % by_str);
}

std::string Condition::OrderedBombarded::Dump() const
{ return DumpIndent() + "OrderedBombarded by_object = " + m_by_object_condition->Dump(); }

bool Condition::OrderedBombarded::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "OrderedBombarded::Match passed no candidate object";
        return false;
    }

    // get subcondition matches
    ObjectSet subcondition_matches;
    m_by_object_condition->Eval(local_context, subcondition_matches);

    return OrderedBombardedSimpleMatch(subcondition_matches)(candidate);
}

///////////////////////////////////////////////////////////
// ValueTest                                             //
///////////////////////////////////////////////////////////
Condition::ValueTest::~ValueTest() {
    delete m_value_ref;
    delete m_low;
    delete m_high;
}

bool Condition::ValueTest::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::ValueTest& rhs_ = static_cast<const Condition::ValueTest&>(rhs);

    CHECK_COND_VREF_MEMBER(m_value_ref)
    CHECK_COND_VREF_MEMBER(m_low)
    CHECK_COND_VREF_MEMBER(m_high)

    return true;
}

void Condition::ValueTest::Eval(const ScriptingContext& parent_context,
                                       ObjectSet& matches, ObjectSet& non_matches,
                                       SearchDomain search_domain/* = NON_MATCHES*/) const
{
    bool simple_eval_safe = ((!m_low || m_low->LocalCandidateInvariant()) &&
                             (!m_high || m_high->LocalCandidateInvariant()) &&
                             (!m_value_ref || m_value_ref->LocalCandidateInvariant()) &&
                             (parent_context.condition_root_candidate || RootCandidateInvariant()));

    if (simple_eval_safe) {
        // evaluate value and range limits once, use to match all candidates
        TemporaryPtr<const UniverseObject> no_object;
        ScriptingContext local_context(parent_context, no_object);

        float low = (m_low ? m_low->Eval(local_context) : -Meter::LARGE_VALUE);
        float high = (m_high ? m_high->Eval(local_context) : Meter::LARGE_VALUE);
        float value = (m_value_ref ? m_value_ref->Eval(local_context) : 0.0);

        bool in_range = (low <= value && value <= high);

        // transfer objects to or from candidate set, according to whether number of matches was within
        // the requested range.
        if (search_domain == MATCHES && !in_range) {
            non_matches.insert(non_matches.end(), matches.begin(), matches.end());
            matches.clear();
        }
        if (search_domain == NON_MATCHES && in_range) {
            matches.insert(matches.end(), non_matches.begin(), non_matches.end());
            non_matches.clear();
        }

    } else {
        // re-evaluate value and ranges for each candidate object
        Condition::ConditionBase::Eval(parent_context, matches, non_matches, search_domain);
    }
}

bool Condition::ValueTest::RootCandidateInvariant() const {
    return (!m_value_ref  || m_value_ref->RootCandidateInvariant()) &&
           (!m_low        || m_low->RootCandidateInvariant()) &&
           (!m_high       || m_high->RootCandidateInvariant());
}

bool Condition::ValueTest::TargetInvariant() const {
    return (!m_value_ref  || m_value_ref->TargetInvariant()) &&
           (!m_low        || m_low->TargetInvariant()) &&
           (!m_high       || m_high->TargetInvariant());
}

bool Condition::ValueTest::SourceInvariant() const {
    return (!m_value_ref  || m_value_ref->SourceInvariant()) &&
           (!m_low        || m_low->SourceInvariant()) &&
           (!m_high       || m_high->SourceInvariant());
}

std::string Condition::ValueTest::Description(bool negated/* = false*/) const {
    std::string value_str;
    if (m_value_ref)
        value_str = m_value_ref->Description();

    std::string low_str = (m_low ? (ValueRef::ConstantExpr(m_low) ?
                                    boost::lexical_cast<std::string>(m_low->Eval()) :
                                    m_low->Description())
                                 : boost::lexical_cast<std::string>(-Meter::LARGE_VALUE));
    std::string high_str = (m_high ? (ValueRef::ConstantExpr(m_high) ?
                                      boost::lexical_cast<std::string>(m_high->Eval()) :
                                      m_high->Description())
                                   : boost::lexical_cast<std::string>(Meter::LARGE_VALUE));

    return str(FlexibleFormat((!negated)
               ? UserString("DESC_VALUE_TEST")
               : UserString("DESC_VALUE_TEST_NOT"))
               % value_str
               % low_str
               % high_str);
}

std::string Condition::ValueTest::Dump() const {
    std::string retval = DumpIndent() + "ValueTest";
    if (m_low)
        retval += " low = " + m_low->Dump();
    if (m_high)
        retval += " high = " + m_high->Dump();
    if (m_value_ref) {
        retval += " value_ref =\n";
        ++g_indent;
            retval += m_value_ref->Dump();
        --g_indent;
    }
    return retval;
}

bool Condition::ValueTest::Match(const ScriptingContext& local_context) const {
    TemporaryPtr<const UniverseObject> candidate = local_context.condition_local_candidate;
    if (!candidate) {
        Logger().errorStream() << "ValueTest::Match passed no candidate object";
        return false;
    }
    if (!m_value_ref)
        return false;

    float low = (m_low ? m_low->Eval(local_context) : -Meter::LARGE_VALUE);
    float high = (m_high ? m_high->Eval(local_context) : Meter::LARGE_VALUE);
    float value = (m_value_ref ? m_value_ref->Eval(local_context) : 0);

    return low <= value && value <= high;
}


///////////////////////////////////////////////////////////
// And                                                   //
///////////////////////////////////////////////////////////
Condition::And::~And() {
    for (unsigned int i = 0; i < m_operands.size(); ++i)
        delete m_operands[i];
}

bool Condition::And::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::And& rhs_ = static_cast<const Condition::And&>(rhs);

    if (m_operands.size() != rhs_.m_operands.size())
        return false;
    for (unsigned int i = 0; i < m_operands.size(); ++i) {
        CHECK_COND_VREF_MEMBER(m_operands.at(i))
    }

    return true;
}

void Condition::And::Eval(const ScriptingContext& parent_context, ObjectSet& matches,
                          ObjectSet& non_matches, SearchDomain search_domain/* = NON_MATCHES*/) const
{
    TemporaryPtr<const UniverseObject> no_object;
    ScriptingContext local_context(parent_context, no_object);

    if (search_domain == NON_MATCHES) {
        ObjectSet partly_checked_non_matches;
        partly_checked_non_matches.reserve(non_matches.size());

        // move items in non_matches set that pass first operand condition into
        // partly_checked_non_matches set
        m_operands[0]->Eval(local_context, partly_checked_non_matches, non_matches, NON_MATCHES);

        // move items that don't pass one of the other conditions back to non_matches
        for (unsigned int i = 1; i < m_operands.size(); ++i) {
            if (partly_checked_non_matches.empty()) break;
            m_operands[i]->Eval(local_context, partly_checked_non_matches, non_matches, MATCHES);
        }

        // merge items that passed all operand conditions into matches
        matches.insert(matches.end(), partly_checked_non_matches.begin(), partly_checked_non_matches.end());

        // items already in matches set are not checked, and remain in matches set even if
        // they don't match one of the operand conditions

    } else /*(search_domain == MATCHES)*/ {
        // check all operand conditions on all objects in the matches set, moving those
        // that don't pass a condition to the non-matches set

        for (unsigned int i = 0; i < m_operands.size(); ++i) {
            if (matches.empty()) break;
            m_operands[i]->Eval(local_context, matches, non_matches, MATCHES);
        }

        // items already in non_matches set are not checked, and remain in non_matches set
        // even if they pass all operand conditions
    }
}

bool Condition::And::RootCandidateInvariant() const {
    if (m_root_candidate_invariant != UNKNOWN_INVARIANCE)
        return m_root_candidate_invariant == INVARIANT;

    for (std::vector<const ConditionBase*>::const_iterator it = m_operands.begin();
         it != m_operands.end(); ++it)
    {
        if (!(*it)->RootCandidateInvariant()) {
            m_root_candidate_invariant = VARIANT;
            return false;
        }
    }
    m_root_candidate_invariant = INVARIANT;
    return true;
}

bool Condition::And::TargetInvariant() const {
    if (m_target_invariant != UNKNOWN_INVARIANCE)
        return m_target_invariant == INVARIANT;

    for (std::vector<const ConditionBase*>::const_iterator it = m_operands.begin();
         it != m_operands.end(); ++it)
    {
        if (!(*it)->TargetInvariant()) {
            m_target_invariant = VARIANT;
            return false;
        }
    }
    m_target_invariant = INVARIANT;
    return true;
}

bool Condition::And::SourceInvariant() const {
    if (m_source_invariant != UNKNOWN_INVARIANCE)
        return m_source_invariant == INVARIANT;

    for (std::vector<const ConditionBase*>::const_iterator it = m_operands.begin();
         it != m_operands.end(); ++it)
    {
        if (!(*it)->SourceInvariant()) {
            m_source_invariant = VARIANT;
            return false;
        }
    }
    m_source_invariant = INVARIANT;
    return true;
}

std::string Condition::And::Description(bool negated/* = false*/) const {
    if (m_operands.size() == 1) {
        return m_operands[0]->Description();
    } else {
        // TODO: use per-operand-type connecting language
        std::string values_str;
        for (unsigned int i = 0; i < m_operands.size(); ++i) {
            values_str += m_operands[i]->Description();
            if (i != m_operands.size() - 1) {
                values_str += UserString("DESC_AND_BETWEEN_OPERANDS");
            }
        }
        return values_str;
    }
}

std::string Condition::And::Dump() const {
    std::string retval = DumpIndent() + "And [\n";
    ++g_indent;
    for (unsigned int i = 0; i < m_operands.size(); ++i) {
        retval += m_operands[i]->Dump();
    }
    --g_indent;
    retval += DumpIndent() + "]\n";
    return retval;
}

void Condition::And::GetDefaultInitialCandidateObjects(const ScriptingContext& parent_context, Condition::ObjectSet& condition_non_targets) const {
    if (!Operands().empty()) {
        Operands()[0]->GetDefaultInitialCandidateObjects(parent_context, condition_non_targets); //gets condition_non_targets from first operand condition
    } else {
        ConditionBase::GetDefaultInitialCandidateObjects(parent_context, condition_non_targets);
    }
}

///////////////////////////////////////////////////////////
// Or                                                    //
///////////////////////////////////////////////////////////
Condition::Or::~Or() {
    for (unsigned int i = 0; i < m_operands.size(); ++i)
        delete m_operands[i];
}

bool Condition::Or::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::Or& rhs_ = static_cast<const Condition::Or&>(rhs);

    if (m_operands.size() != rhs_.m_operands.size())
        return false;
    for (unsigned int i = 0; i < m_operands.size(); ++i) {
        CHECK_COND_VREF_MEMBER(m_operands.at(i))
    }

    return true;
}

void Condition::Or::Eval(const ScriptingContext& parent_context, ObjectSet& matches,
                         ObjectSet& non_matches, SearchDomain search_domain/* = NON_MATCHES*/) const
{
    TemporaryPtr<const UniverseObject> no_object;
    ScriptingContext local_context(parent_context, no_object);

    if (search_domain == NON_MATCHES) {
        // check each item in the non-matches set against each of the operand conditions
        // if a non-candidate item matches an operand condition, move the item to the
        // matches set.

        for (unsigned int i = 0; i < m_operands.size(); ++i) {
            if (non_matches.empty()) break;
            m_operands[i]->Eval(local_context, matches, non_matches, NON_MATCHES);
        }

        // items already in matches set are not checked and remain in the
        // matches set even if they fail all the operand conditions

    } else {
        ObjectSet partly_checked_matches;
        partly_checked_matches.reserve(matches.size());

        // move items in matches set the fail the first operand condition into 
        // partly_checked_matches set
        m_operands[0]->Eval(local_context, matches, partly_checked_matches, MATCHES);

        // move items that pass any of the other conditions back into matches
        for (unsigned int i = 1; i < m_operands.size(); ++i) {
            if (partly_checked_matches.empty()) break;
            m_operands[i]->Eval(local_context, matches, partly_checked_matches, NON_MATCHES);
        }

        // merge items that failed all operand conditions into non_matches
        non_matches.insert(non_matches.end(), partly_checked_matches.begin(), partly_checked_matches.end());

        // items already in non_matches set are not checked and remain in
        // non_matches set even if they pass one or more of the operand 
        // conditions
    }
}

bool Condition::Or::RootCandidateInvariant() const {
    if (m_root_candidate_invariant != UNKNOWN_INVARIANCE)
        return m_root_candidate_invariant == INVARIANT;

    for (std::vector<const ConditionBase*>::const_iterator it = m_operands.begin();
         it != m_operands.end(); ++it)
    {
        if (!(*it)->RootCandidateInvariant()) {
            m_root_candidate_invariant = VARIANT;
            return false;
        }
    }
    m_root_candidate_invariant = INVARIANT;
    return true;
}

bool Condition::Or::TargetInvariant() const {
    if (m_target_invariant != UNKNOWN_INVARIANCE)
        return m_target_invariant == INVARIANT;

    for (std::vector<const ConditionBase*>::const_iterator it = m_operands.begin();
         it != m_operands.end(); ++it)
    {
        if (!(*it)->TargetInvariant()) {
            m_target_invariant = VARIANT;
            return false;
        }
    }
    m_target_invariant = INVARIANT;
    return true;
}

bool Condition::Or::SourceInvariant() const {
    if (m_source_invariant != UNKNOWN_INVARIANCE)
        return m_source_invariant == INVARIANT;

    for (std::vector<const ConditionBase*>::const_iterator it = m_operands.begin();
         it != m_operands.end(); ++it)
    {
        if (!(*it)->SourceInvariant()) {
            m_source_invariant = VARIANT;
            return false;
        }
    }
    m_source_invariant = INVARIANT;
    return true;
}

std::string Condition::Or::Description(bool negated/* = false*/) const {
    if (m_operands.size() == 1) {
        return m_operands[0]->Description();
    } else {
        // TODO: use per-operand-type connecting language
        std::string values_str;
        for (unsigned int i = 0; i < m_operands.size(); ++i) {
            values_str += m_operands[i]->Description();
            if (i != m_operands.size() - 1) {
                values_str += UserString("DESC_OR_BETWEEN_OPERANDS");
            }
        }
        return values_str;
    }
}

std::string Condition::Or::Dump() const {
    std::string retval = DumpIndent() + "Or [\n";
    ++g_indent;
    for (unsigned int i = 0; i < m_operands.size(); ++i) {
        retval += m_operands[i]->Dump();
    }
    --g_indent;
    retval += "\n" + DumpIndent() + "]\n";
    return retval;
}

///////////////////////////////////////////////////////////
// Not                                                   //
///////////////////////////////////////////////////////////
Condition::Not::~Not()
{ delete m_operand; }

bool Condition::Not::operator==(const Condition::ConditionBase& rhs) const {
    if (this == &rhs)
        return true;
    if (typeid(*this) != typeid(rhs))
        return false;

    const Condition::Not& rhs_ = static_cast<const Condition::Not&>(rhs);

    CHECK_COND_VREF_MEMBER(m_operand)

    return true;
}

void Condition::Not::Eval(const ScriptingContext& parent_context, ObjectSet& matches, ObjectSet& non_matches,
                          SearchDomain search_domain/* = NON_MATCHES*/) const
{
    TemporaryPtr<const UniverseObject> no_object;
    ScriptingContext local_context(parent_context, no_object);

    if (search_domain == NON_MATCHES) {
        // search non_matches set for items that don't meet the operand
        // condition, and move those to the matches set
        m_operand->Eval(local_context, non_matches, matches, MATCHES); // swapping order of matches and non_matches set parameters and MATCHES / NON_MATCHES search domain effects NOT on requested search domain
    } else {
        // search matches set for items that meet the operand condition
        // condition, and move those to the non_matches set
        m_operand->Eval(local_context, non_matches, matches, NON_MATCHES);
    }
}

bool Condition::Not::RootCandidateInvariant() const {
    if (m_root_candidate_invariant != UNKNOWN_INVARIANCE)
        return m_root_candidate_invariant == INVARIANT;
    m_root_candidate_invariant = m_operand->RootCandidateInvariant() ? INVARIANT: VARIANT;
    return m_root_candidate_invariant == INVARIANT;
}

bool Condition::Not::TargetInvariant() const {
    if (m_target_invariant != UNKNOWN_INVARIANCE)
        return m_target_invariant == INVARIANT;
    m_target_invariant = m_operand->TargetInvariant() ? INVARIANT: VARIANT;
    return m_target_invariant == INVARIANT;
}

bool Condition::Not::SourceInvariant() const {
    if (m_source_invariant != UNKNOWN_INVARIANCE)
        return m_source_invariant == INVARIANT;
    m_source_invariant = m_operand->SourceInvariant() ? INVARIANT: VARIANT;
    return m_source_invariant == INVARIANT;
}

std::string Condition::Not::Description(bool negated/* = false*/) const
{ return m_operand->Description(true); }

std::string Condition::Not::Dump() const {
    std::string retval = DumpIndent() + "Not\n";
    ++g_indent;
    retval += m_operand->Dump();
    --g_indent;
    return retval;
}
