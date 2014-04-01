#include "ConditionParserImpl.h"
#include "Double.h"
#include "EnumParser.h"
#include "Int.h"
#include "Label.h"
#include "Parse.h"
#include "ParseImpl.h"
#include "ValueRefParser.h"

#include "../universe/Building.h"

#include <boost/spirit/home/phoenix.hpp>

#define DEBUG_PARSERS 0

#if DEBUG_PARSERS
namespace std {
    inline ostream& operator<<(ostream& os, const std::vector<boost::shared_ptr<const Effect::EffectsGroup> >&) { return os; }
    inline ostream& operator<<(ostream& os, const std::map<std::string, BuildingType*>&) { return os; }
    inline ostream& operator<<(ostream& os, const std::pair<const std::string, BuildingType*>&) { return os; }
}
#endif

namespace {
    struct insert_ {
        template <typename Arg1, typename Arg2>
        struct result
        { typedef void type; };

        void operator()(std::map<std::string, BuildingType*>& building_types, BuildingType* building_type) const {
            if (!building_types.insert(std::make_pair(building_type->Name(), building_type)).second) {
                std::string error_str = "ERROR: More than one building type in buildings.txt has the name " + building_type->Name();
                throw std::runtime_error(error_str.c_str());
            }
        }
    };
    const boost::phoenix::function<insert_> insert;

    struct rules {
        rules() {
            const parse::lexer& tok = parse::lexer::instance();

            const parse::value_ref_parser_rule<int>::type& int_value_ref =          parse::value_ref_parser<int>();
            const parse::value_ref_parser_rule<double>::type& double_value_ref =    parse::value_ref_parser<double>();

            qi::_1_type _1;
            qi::_2_type _2;
            qi::_3_type _3;
            qi::_4_type _4;
            qi::_a_type _a;
            qi::_b_type _b;
            qi::_c_type _c;
            qi::_d_type _d;
            qi::_e_type _e;
            qi::_f_type _f;
            qi::_g_type _g;
            qi::_h_type _h;
            qi::_i_type _i;
            qi::_r1_type _r1;
            qi::_r2_type _r2;
            qi::_val_type _val;
            qi::eps_type eps;
            using phoenix::new_;

            building_prefix
                =    tok.BuildingType_
                >    parse::label(Name_token)        > tok.string [ _r1 = _1 ]
                >    parse::label(Description_token) > tok.string [ _r2 = _1 ]
                ;

            producible
                =    tok.Unproducible_ [ _val = false ]
                |    tok.Producible_ [ _val = true ]
                |    eps [ _val = true ]
                ;

            building_type
                =    building_prefix(_a, _b)
                >    parse::label(BuildCost_token)               > double_value_ref [ _c = _1 ]
                >    parse::label(BuildTime_token)               > int_value_ref [ _d = _1 ]
                >    producible [ _e = _1 ]
                >    (
                            parse::label(CaptureResult_token)   >> parse::enum_parser<CaptureResult>() [ _f = _1 ]
                        |   eps [ _f = CR_CAPTURE ]
                     )
                >    parse::detail::tags_parser()(_g)
                >    parse::label(Location_token)                > parse::detail::condition_parser [ _h = _1 ]
                >    parse::label(EffectsGroups_token)           > -parse::detail::effects_group_parser() [ _i = _1 ]
                >    parse::label(Icon_token)                    > tok.string
                    [ insert(_r1, new_<BuildingType>(_a, _b, _c, _d, _e, _f, _g, _h, _i, _1)) ]
                ;

            start
                =   +building_type(_r1)
                ;

            building_prefix.name("BuildingType");
            producible.name("Producible or Unproducible");
            building_type.name("BuildingType");

#if DEBUG_PARSERS
            debug(building_prefix);
            debug(producible);
            debug(building_type);
#endif

            qi::on_error<qi::fail>(start, parse::report_error(_1, _2, _3, _4));
        }

        typedef boost::spirit::qi::rule<
            parse::token_iterator,
            void (std::string&, std::string&),
            parse::skipper_type
        > building_prefix_rule;

        typedef boost::spirit::qi::rule<
            parse::token_iterator,
            bool (),
            parse::skipper_type
        > producible_rule;

        typedef boost::spirit::qi::rule<
            parse::token_iterator,
            void (std::map<std::string, BuildingType*>&),
            qi::locals<
                std::string,
                std::string,
                ValueRef::ValueRefBase<double>*,
                ValueRef::ValueRefBase<int>*,
                bool,
                CaptureResult,
                std::set<std::string>,
                Condition::ConditionBase*,
                std::vector<boost::shared_ptr<const Effect::EffectsGroup> >
            >,
            parse::skipper_type
        > building_type_rule;

        typedef boost::spirit::qi::rule<
            parse::token_iterator,
            void (std::map<std::string, BuildingType*>&),
            parse::skipper_type
        > start_rule;

        building_prefix_rule    building_prefix;
        producible_rule         producible;
        building_type_rule      building_type;
        start_rule              start;
    };
}

namespace parse {
    bool buildings(const boost::filesystem::path& path, std::map<std::string, BuildingType*>& building_types)
    { return detail::parse_file<rules, std::map<std::string, BuildingType*> >(path, building_types); }
}
