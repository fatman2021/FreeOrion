#include "VarText.h"

#include "../universe/Universe.h"
#include "../universe/ShipDesign.h"
#include "../universe/System.h"
#include "../Empire/EmpireManager.h"
#include "../Empire/Empire.h"
#include "i18n.h"
#include "Logger.h"

#include <boost/spirit/include/classic.hpp>

// Forward declarations
class Tech;
class BuildingType;
class Special;
class Species;
class FieldType;
const Tech*         GetTech(const std::string& name);
const BuildingType* GetBuildingType(const std::string& name);
const Special*      GetSpecial(const std::string& name);
const Species*      GetSpecies(const std::string& name);
const FieldType*    GetFieldType(const std::string& name);

namespace {
    const std::string START_VAR("%");
    const std::string END_VAR("%");

    /** Converts (first, last) to a string, looks up its value in the Universe,
      * then appends this to the end of a std::string. */
    struct SubstituteAndAppend {
        SubstituteAndAppend(const XMLElement& variables, std::string& str, bool& valid) :
            m_variables(variables),
            m_str(str),
            m_valid(valid)
        {}

        void operator()(const char* first, const char* last) const {
            std::string token(first, last);

            // special case: "%%" is interpreted to be a '%' character
            if (token.empty()) {
                m_str += "%";
                return;
            }

            // look up child
            if (!m_variables.ContainsChild(token)) {
                m_str += UserString("ERROR");
                m_valid = false;
                return;
            }

            const XMLElement& token_elem = m_variables.Child(token);

            // stringtable text substitution token
            if (token == VarText::TEXT_TAG) {
                const std::string& text = token_elem.Attribute("value");
                m_str += UserString(text);
                return;
            } else if (token == VarText::RAW_TEXT_TAG) {
                const std::string& text = token_elem.Attribute("value");
                m_str += text;
                return;
            }

            std::string open_tag = "<" + token_elem.Tag() + " " + token_elem.Attribute("value") + ">";
            std::string close_tag = "</" + token_elem.Tag() + ">";

            // universe object token types
            if (token == VarText::PLANET_ID_TAG ||
                token == VarText::SYSTEM_ID_TAG ||
                token == VarText::SHIP_ID_TAG ||
                token == VarText::FLEET_ID_TAG ||
                token == VarText::BUILDING_ID_TAG ||
                token == VarText::FIELD_ID_TAG)
            {
                int object_id = INVALID_OBJECT_ID;
                try {
                    object_id = boost::lexical_cast<int>(token_elem.Attribute("value"));
                } catch (const std::exception&) {
                    Logger().errorStream() << "SubstituteAndAppend couldn't cast \"" << token_elem.Attribute("value") << "\" to int for object ID.";
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                TemporaryPtr<const UniverseObject> obj = GetUniverseObject(object_id);
                if (!obj) {
                    //Logger().errorStream() << "SubstituteAndAppend couldn't get object with ID " << object_id;
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }

                std::string name_text = GetVisibleObjectName(obj);
                m_str += open_tag + name_text + close_tag;

            // combat log
            } else if (token == VarText::COMBAT_ID_TAG) {
                // combat links always just labelled "Combat"; don't need to look up details
                m_str += open_tag + UserString("COMBAT") + close_tag;

            // technology token
            } else if (token == VarText::TECH_TAG) {
                std::string name = token_elem.Attribute("value");
                if (!GetTech(name)) {
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                m_str += open_tag + UserString(name) + close_tag;

            // building type token
            } else if (token == VarText::BUILDING_TYPE_TAG) {
                std::string name = token_elem.Attribute("value");
                if (!GetBuildingType(name)) {
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                m_str += open_tag + UserString(name) + close_tag;

            // ship hull token
            } else if (token == VarText::SHIP_HULL_TAG) {
                std::string name = token_elem.Attribute("value");
                if (!GetHullType(name)) {
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                m_str += open_tag + UserString(name) + close_tag;

            // ship part token
            } else if (token == VarText::SHIP_PART_TAG) {
                std::string name = token_elem.Attribute("value");
                if (!GetPartType(name)) {
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                m_str += open_tag + UserString(name) + close_tag;

            // special token
            } else if (token == VarText::SPECIAL_TAG) {
                std::string name = token_elem.Attribute("value");
                if (!GetSpecial(name)) {
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                m_str += open_tag + UserString(name) + close_tag;

            // species token
            } else if (token == VarText::SPECIES_TAG) {
                std::string name = token_elem.Attribute("value");
                if (!GetSpecies(name)) {
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                m_str += open_tag + UserString(name) + close_tag;

            // field type token
            } else if (token == VarText::FIELD_TYPE_TAG) {
                std::string name = token_elem.Attribute("value");
                if (!GetFieldType(name)) {
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                m_str += open_tag + UserString(name) + close_tag;

                // ship design token
            } else if (token == VarText::DESIGN_ID_TAG) {
                int design_id = ShipDesign::INVALID_DESIGN_ID;
                try {
                    design_id = boost::lexical_cast<int>(token_elem.Attribute("value"));
                } catch (const std::exception&) {
                    Logger().errorStream() << "SubstituteAndAppend couldn't cast \"" << token_elem.Attribute("value") << "\" to int for ship design ID.";
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                const ShipDesign* design = GetShipDesign(design_id);
                if (!design) {
                    Logger().errorStream() << "SubstituteAndAppend couldn't get ship design with ID " << design_id;
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                m_str += open_tag + design->Name() + close_tag;

            // predefined ship design token
            } else if (token == VarText::PREDEFINED_DESIGN_TAG) {
                const std::string& design_name = token_elem.Attribute("value");
                const ShipDesign* design = GetPredefinedShipDesign(design_name);
                if (!design) {
                    Logger().errorStream() << "SubstituteAndAppend couldn't get predefined ship design with name " << design_name;
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                m_str += open_tag + design->Name() + close_tag;

            // empire token
            } else if (token == VarText::EMPIRE_ID_TAG) {
                int empire_id = ALL_EMPIRES;
                try {
                    empire_id = boost::lexical_cast<int>(token_elem.Attribute("value"));
                } catch (const std::exception&) {
                    Logger().errorStream() << "SubstituteAndAppend couldn't cast \"" << token_elem.Attribute("value") << "\" to int for empire ID.";
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                const Empire* empire = Empires().Lookup(empire_id);
                if (!empire) {
                    Logger().errorStream() << "SubstituteAndAppend couldn't get empire with ID " << empire_id;
                    m_str += UserString("ERROR");
                    m_valid = false;
                    return;
                }
                m_str += open_tag + empire->Name() + close_tag;
            }
        }

        const XMLElement&   m_variables;
        std::string&        m_str;
        bool&               m_valid;
    };

    // sticks a sequence of characters onto the end of a std::string
    struct StringAppend {
        StringAppend(std::string& str) :
            m_str(str)
        {}

        void operator()(const char* first, const char* last) const
        { m_str += std::string(first, last); }
        std::string& m_str;
    };
}

// static(s)
const std::string VarText::TEXT_TAG = "text";
const std::string VarText::RAW_TEXT_TAG = "rawtext";

const std::string VarText::PLANET_ID_TAG = "planet";
const std::string VarText::SYSTEM_ID_TAG = "system";
const std::string VarText::SHIP_ID_TAG = "ship";
const std::string VarText::FLEET_ID_TAG = "fleet";
const std::string VarText::BUILDING_ID_TAG = "building";
const std::string VarText::FIELD_ID_TAG = "field";

const std::string VarText::COMBAT_ID_TAG = "combat";

const std::string VarText::EMPIRE_ID_TAG = "empire";
const std::string VarText::DESIGN_ID_TAG = "shipdesign";
const std::string VarText::PREDEFINED_DESIGN_TAG = "predefinedshipdesign";

const std::string VarText::TECH_TAG = "tech";
const std::string VarText::BUILDING_TYPE_TAG = "buildingtype";
const std::string VarText::SPECIAL_TAG = "special";
const std::string VarText::SHIP_HULL_TAG = "shiphull";
const std::string VarText::SHIP_PART_TAG = "shippart";
const std::string VarText::SPECIES_TAG = "species";
const std::string VarText::FIELD_TYPE_TAG = "fieldtype";


VarText::VarText() :
    m_template_string(""),
    m_stringtable_lookup_flag(false),
    m_variables(),
    m_text(),
    m_validated(false)
{}

VarText::VarText(const std::string& template_string, bool stringtable_lookup_template/* = true*/) :
    m_template_string(template_string),
    m_stringtable_lookup_flag(stringtable_lookup_template),
    m_variables(),
    m_text(),
    m_validated(false)
{}

const std::string& VarText::GetText() const {
    if (m_text.empty())
        GenerateVarText();
    return m_text;
}

bool VarText::Validate() const {
    if (m_text.empty())
        GenerateVarText();
    return m_validated;
}

void VarText::SetTemplateString(const std::string& text, bool stringtable_lookup_template/* = true*/) {
    m_text = text;
    m_stringtable_lookup_flag = stringtable_lookup_template;
}

std::vector<std::string> VarText::GetVariableTags() const {
    std::vector<std::string> retval;
    for (XMLElement::const_child_iterator it = m_variables.child_begin(); it != m_variables.child_end(); ++it)
        retval.push_back(it->Tag());
    return retval;
}

void VarText::AddVariable(const std::string& tag, const std::string& data) {
    XMLElement elem(tag);
    elem.SetAttribute("value", data);
    m_variables.AppendChild(elem);
}

void VarText::GenerateVarText() const {
    // generate a string complete with substituted variables and hyperlinks
    // the procedure here is to replace any tokens within %% with variables of
    // the same name in the SitRep XML data
    m_text.clear();
    m_validated = true;
    if (m_template_string.empty())
        return;

    // get string into which to substitute variables
    std::string template_str = m_stringtable_lookup_flag ? UserString(m_template_string) : m_template_string;

    // set up parser
    using namespace boost::spirit::classic;
    rule<> token = *(anychar_p - space_p - END_VAR.c_str());
    rule<> var = START_VAR.c_str() >> token[SubstituteAndAppend(m_variables, m_text, m_validated)] >> END_VAR.c_str();
    rule<> non_var = anychar_p - START_VAR.c_str();

    // parse and substitute variables
    try {
        parse(template_str.c_str(), *(non_var[StringAppend(m_text)] | var));
    } catch (const std::exception&) {
        Logger().errorStream() << "VarText::GenerateVartText caught exception when parsing template string: " << m_template_string;
    }
}
