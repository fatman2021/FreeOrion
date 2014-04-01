// -*- C++ -*-
#ifndef _Meter_h_
#define _Meter_h_

#include <boost/serialization/access.hpp>
#include <boost/serialization/nvp.hpp>
#include <string>

#include "../util/Export.h"

/** A Meter is a value with an associated maximum value.  A typical example is
  * the population meter.  The max represents the max pop for a planet, and the
  * current represents the current pop there.  The max may be adjusted upwards
  * or downwards, and the current may be as well. */
class FO_COMMON_API Meter {
public:
    /** \name Structors */ //@{
    Meter();                                        ///< default ctor.  values all set to DEFAULT_VALUE
    explicit Meter(float current_value);            ///< basic ctor.  current value set to \a current_value and initial value set to DEFAULT_VALUE
    Meter(float current_value, float initial_value);///< full ctor
    //@}

    /** \name Accessors */ //@{
    float      Current() const;                     ///< returns the current value of the meter
    float      Initial() const;                     ///< returns the value of the meter as it was at the beginning of the turn

    std::string Dump() const;                       ///< returns text of meter values
    //@}

    /** \name Mutators */ //@{
    void        SetCurrent(float current_value);    ///< sets current value, leaving initial value unchanged
    void        Set(float current_value, float initial_value);  ///< sets current and initial values
    void        ResetCurrent();                     ///< sets current value to DEFAULT_VALUE
    void        Reset();                            ///< sets current and initial values to DEFAULT_VALUE

    void        AddToCurrent(float adjustment);     ///< adds \a current to the current value of the Meter
    void        ClampCurrentToRange(float min = DEFAULT_VALUE, float max = LARGE_VALUE);    ///< ensures the current value falls in the range [\a min, \a max]

    void        BackPropegate();                    ///< sets previous equal to initial, then sets initial equal to current
    //@}

    static const float DEFAULT_VALUE;               ///< value assigned to current or initial when resetting or when no value is specified in a constructor
    static const float LARGE_VALUE;                 ///< a very large number, which is useful to set current to when it will be later clamped, to ensure that the result is the max value in the clamp range
    static const float INVALID_VALUE;               ///< sentinel value to indicate no valid value for this meter

private:
    float  m_current_value;
    float  m_initial_value;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

// template implementations
template <class Archive>
void Meter::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_NVP(m_current_value)
        & BOOST_SERIALIZATION_NVP(m_initial_value);
}

#endif // _Meter_h_
