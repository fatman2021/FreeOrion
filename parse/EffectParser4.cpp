#include "EffectParserImpl.h"

#include "ConditionParserImpl.h"
#include "EnumParser.h"
#include "Label.h"
#include "ValueRefParser.h"
#include "../universe/Effect.h"
//#include "../universe/ValueRef.h"

#include <boost/spirit/home/phoenix.hpp>

namespace qi = boost::spirit::qi;
namespace phoenix = boost::phoenix;

namespace {
    struct effect_parser_rules_4 {
        effect_parser_rules_4() {
            qi::_1_type _1;
            qi::_a_type _a;
            qi::_b_type _b;
            qi::_c_type _c;
            qi::_val_type _val;
            qi::eps_type eps;
            using phoenix::new_;
            using phoenix::construct;
            using phoenix::push_back;

            const parse::lexer& tok =                                                       parse::lexer::instance();

            const parse::value_ref_parser_rule<int>::type& int_value_ref =                  parse::value_ref_parser<int>();
            const parse::value_ref_parser_rule<double>::type& double_value_ref =            parse::value_ref_parser<double>();
            const parse::value_ref_parser_rule<std::string>::type& string_value_ref =       parse::value_ref_parser<std::string>();
            const parse::value_ref_parser_rule< ::PlanetType>::type& planet_type_value_ref= parse::value_ref_parser< ::PlanetType>();
            const parse::value_ref_parser_rule< ::PlanetSize>::type& planet_size_value_ref= parse::value_ref_parser< ::PlanetSize>();
            const parse::value_ref_parser_rule< ::StarType>::type& star_type_value_ref =    parse::value_ref_parser< ::StarType>();

            create_planet
                =    tok.CreatePlanet_
                >>   parse::label(Type_token)       >> planet_type_value_ref [ _a = _1 ]
                >>   parse::label(PlanetSize_token) >> planet_size_value_ref [ _val = new_<Effect::CreatePlanet>(_a, _1) ]
                ;

            create_building
                =    tok.CreateBuilding_
                >>   parse::label(Name_token)       >> string_value_ref [ _val = new_<Effect::CreateBuilding>(_1) ]
                ;

            create_ship_1
                =    tok.CreateShip_
                >>   parse::label(DesignName_token) >> int_value_ref [ _b = _1 ] // TODO: DesignName -> DesignID.
                >>   parse::label(Empire_token)     >> int_value_ref [ _c = _1 ]
                >>   parse::label(Species_token)    >> string_value_ref [ _val = new_<Effect::CreateShip>(_b, _c, _1) ]
                ;

            create_ship_2
                =    tok.CreateShip_
                >>   parse::label(DesignName_token) >> tok.string [ _a = _1 ]
                >>   parse::label(Empire_token)     >> int_value_ref [ _b = _1 ]
                >>   parse::label(Species_token)    >> string_value_ref [ _val = new_<Effect::CreateShip>(_a, _b, _1) ]
                ;

            create_ship_3
                =    tok.CreateShip_
                >>   parse::label(DesignName_token) >> tok.string [ _a = _1 ]
                >>   parse::label(Empire_token)     >> int_value_ref [ _val = new_<Effect::CreateShip>(_a, _1) ]
                ;

            create_ship_4
                =    tok.CreateShip_
                >>   parse::label(DesignName_token) >> tok.string [ _val = new_<Effect::CreateShip>(_1) ]
                ;

            create_field
                =   tok.CreateField_
                >>  parse::label(Type_token)        >> tok.string [ _a = _1 ]
                >>  (
                        (
                            parse::label(Size_token)    >> double_value_ref [ _val = new_<Effect::CreateField>(_a, _1) ]
                        )
                    |   (
                            parse::label(X_token)       >> double_value_ref [ _b = _1 ]
                        >>  parse::label(Y_token)       >> double_value_ref [ _c = _1 ]
                        >>  parse::label(Size_token)    >> double_value_ref [ _val = new_<Effect::CreateField>(_a, _b, _c, _1) ]
                        )
                    )
                ;

            create_system
                =   tok.CreateSystem_
                >>  (
                        (
                            parse::label(Type_token)     >> star_type_value_ref [ _a = _1 ]
                        >>  parse::label(X_token)        >> double_value_ref [ _b = _1 ]
                        >>  parse::label(Y_token)        >> double_value_ref [ _val = new_<Effect::CreateSystem>(_a, _b, _1) ]
                        )
                    |   (
                            parse::label(X_token)        >> double_value_ref [ _b = _1 ]
                        >>  parse::label(Y_token)        >> double_value_ref [ _val = new_<Effect::CreateSystem>(_b, _1) ]
                        )
                    )
                ;

            start
                =    create_planet
                |    create_building
                |    create_ship_1
                |    create_ship_2
                |    create_ship_3
                |    create_ship_4
                |    create_field
                |    create_system
                ;

            create_planet.name("CreatePlanet");
            create_building.name("CreateBuilding");
            create_ship_1.name("CreateShip (int DesignID)");
            create_ship_2.name("CreateShip (empire and species)");
            create_ship_3.name("CreateShip (string DesignName and empire)");
            create_ship_4.name("CreateShip (string DesignName only)");
            create_field.name("CreateField");
            create_system.name("CreateSystem");

#if DEBUG_EFFECT_PARSERS
            debug(create_planet);
            debug(create_building);
            debug(create_ship_1);
            debug(create_ship_2);
            debug(create_ship_3);
            debug(create_ship_4);
            debug(create_field);
            debug(create_system);
#endif
        }

        typedef boost::spirit::qi::rule<
            parse::token_iterator,
            Effect::EffectBase* (),
            qi::locals<ValueRef::ValueRefBase< ::PlanetType>*>,
            parse::skipper_type
        > create_planet_rule;

        typedef boost::spirit::qi::rule<
            parse::token_iterator,
            Effect::EffectBase* (),
            qi::locals<
                ValueRef::ValueRefBase< ::StarType>*,
                ValueRef::ValueRefBase<double>*
            >,
            parse::skipper_type
        > create_system_rule;

        typedef boost::spirit::qi::rule<
            parse::token_iterator,
            Effect::EffectBase* (),
            qi::locals<
                std::string,
                ValueRef::ValueRefBase<int>*,
                ValueRef::ValueRefBase<int>*
            >,
            parse::skipper_type
        > string_and_intref_and_intref_rule;

        typedef boost::spirit::qi::rule<
            parse::token_iterator,
            Effect::EffectBase* (),
            qi::locals<
                std::string,
                ValueRef::ValueRefBase<double>*,
                ValueRef::ValueRefBase<double>*
            >,
            parse::skipper_type
        > string_and_doubleref_rule;

        typedef boost::spirit::qi::rule<
            parse::token_iterator,
            Effect::EffectBase* (),
            qi::locals<
                std::string,
                std::string,
                std::vector<std::pair<std::string, const ValueRef::ValueRefBase<std::string>*> >,
                EmpireAffiliationType
            >,
            parse::skipper_type
        > generate_sitrep_message_rule;

        create_planet_rule                  create_planet;
        parse::effect_parser_rule           create_building;
        string_and_intref_and_intref_rule   create_ship_1;
        string_and_intref_and_intref_rule   create_ship_2;
        string_and_intref_and_intref_rule   create_ship_3;
        string_and_intref_and_intref_rule   create_ship_4;
        string_and_doubleref_rule           create_field;
        create_system_rule                  create_system;
        parse::effect_parser_rule           start;
    };
}

namespace parse { namespace detail {
    const effect_parser_rule& effect_parser_4() {
        static effect_parser_rules_4 retval;
        return retval.start;
    }
} }
