#include "StringTable.h"

#include "Logger.h"

#include <GG/utf8/checked.h>

#include <boost/filesystem/fstream.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/algorithm/string/replace.hpp>


// static(s)
const std::string StringTable_::S_DEFAULT_FILENAME = "en.txt";
const std::string StringTable_::S_ERROR_STRING = "ERROR: ";

// StringTable
StringTable_::StringTable_():
    m_filename(S_DEFAULT_FILENAME)
{ Load(); }

StringTable_::StringTable_(const std::string& filename):
    m_filename(filename)
{ Load(); }

StringTable_::~StringTable_()
{}

bool StringTable_::StringExists(const std::string& index) const
{ return m_strings.find(index) != m_strings.end(); }

bool StringTable_::Empty() const
{ return m_strings.empty(); }

const std::string& StringTable_::operator[] (const std::string& index) const {
    static std::string error_retval;
    std::map<std::string, std::string>::const_iterator it = m_strings.find(index);
    return it == m_strings.end() ? error_retval = S_ERROR_STRING + index : it->second;
}

namespace {
    bool read_file(const boost::filesystem::path& path, std::string& file_contents) {
        boost::filesystem::ifstream ifs(path);
        if (!ifs)
            return false;

        // skip byte order mark (BOM)
        static const int UTF8_BOM[3] = {0x00EF, 0x00BB, 0x00BF};
        for (int i = 0; i < 3; i++) {
            if (UTF8_BOM[i] != ifs.get()) {
                // no header set stream back to start of file
                ifs.seekg(0, std::ios::beg);
                // and continue
                break;
            }
        }

        std::getline(ifs, file_contents, '\0');

        // no problems?
        return true;
    }
}


void StringTable_::Load() {
#ifndef FREEORION_WIN32
    boost::filesystem::path path(m_filename);
#else
    boost::filesystem::path::string_type native_filename;
    utf8::utf8to16(m_filename.begin(), m_filename.end(), std::back_inserter(native_filename));
    boost::filesystem::path path(native_filename);
#endif
    std::string file_contents;

    bool read_success = read_file(path, file_contents);
    if (!read_success) {
        Logger().errorStream() << "StringTable_::Load failed to read file at path: " << path.string();
        return;
    }

    using namespace boost::xpressive;

    const sregex IDENTIFIER = +_w;
    const sregex COMMENT = '#' >> *(~_n) >> _n;
    const sregex KEY = IDENTIFIER;
    const sregex SINGLE_LINE_VALUE = *(~_n);
    const sregex MULTI_LINE_VALUE = -*_;
    const sregex ENTRY =
        *(space | COMMENT) >>
        KEY >> _n >>
        (("'''" >> MULTI_LINE_VALUE >> "'''" >> *space >> _n) | SINGLE_LINE_VALUE >> _n);
    const sregex TRAILING_WS =
        *(space | COMMENT);
    const sregex REFERENCE =
        "[[" >> *space >> (s1 = IDENTIFIER) >> +space >> (s2 = IDENTIFIER) >> *space >> "]]";
    const sregex KEYEXPANSION =
        "[[" >> *space >> (s1 = IDENTIFIER) >> *space >> "]]";

    // parse input text stream
    std::string::iterator it = file_contents.begin();
    std::string::iterator end = file_contents.end();

    smatch matches;
    bool well_formed = false;
    try {
        // grab first line of file, which should be the name of this language
        well_formed = regex_search(it, end, matches, SINGLE_LINE_VALUE, regex_constants::match_continuous);
        it = end - matches.suffix().length();
        if (well_formed)
            m_language = matches.str(0);

        // match series of key-value entries to store as stringtable
        while (well_formed) {
            well_formed = regex_search(it, end, matches, ENTRY, regex_constants::match_continuous);
            it = end - matches.suffix().length();

            if (well_formed) {
                std::string key;
                for (smatch::nested_results_type::const_iterator match_it = matches.nested_results().begin();
                     match_it != matches.nested_results().end(); ++match_it)
                {
                    if (match_it->regex_id() == KEY.regex_id()) {
                        key = match_it->str();
                    } else if (match_it->regex_id() == SINGLE_LINE_VALUE.regex_id() ||
                               match_it->regex_id() == MULTI_LINE_VALUE.regex_id())
                    {
                        assert(key != "");
                        if (m_strings.find(key) == m_strings.end()) {
                            m_strings[key] = match_it->str();
                            boost::algorithm::replace_all(m_strings[key], "\\n", "\n");
                        } else {
                            Logger().errorStream() << "Duplicate string ID found: '" << key
                                                   << "' in file: '" << m_filename
                                                   << "'.  Ignoring duplicate.";
                        }
                        key = "";
                    }
                }
            }
        }

        regex_search(it, end, matches, TRAILING_WS, regex_constants::match_continuous);
        it = end - matches.suffix().length();

        well_formed = it == end;
    } catch (std::exception& e) {
        Logger().errorStream() << "Exception caught regex parsing Stringtable: " << e.what();
        std::cerr << "Exception caught regex parsing Stringtable: " << e.what() << std::endl;
        return;
    }

    if (well_formed) {
        // recursively expand keys -- replace [[KEY]] by the text resulting from expanding everything in the definition for KEY
        for (std::map<std::string, std::string>::iterator map_it = m_strings.begin();
             map_it != m_strings.end(); ++map_it)
        {
            std::size_t position = 0; // position in the definition string, past the already processed part
            smatch match;
            std::set<std::string> cyclic_reference_check;
            cyclic_reference_check.insert(map_it->first);
            while (regex_search(map_it->second.begin() + position, map_it->second.end(), match, KEYEXPANSION)) {
                position += match.position();
                if (cyclic_reference_check.find(match[1]) == cyclic_reference_check.end()) {
                    cyclic_reference_check.insert(match[1]);
                    std::map<std::string, std::string>::iterator map_lookup_it = m_strings.find(match[1]);
                    if (map_lookup_it != m_strings.end()) {
                        const std::string substitution = map_lookup_it->second;
                        map_it->second.replace(position, match.length(), substitution);
                        // replace recursively -- do not skip past substitution
                    } else {
                        Logger().errorStream() << "Unresolved key expansion: " << match[1] << " in: " << m_filename << ".";
                        position += match.length();
                    }
                } else {
                    Logger().errorStream() << "Cyclic key expansion: " << match[1] << " in: " << m_filename << ".";
                    position += match.length();
                }
            }
        }

        // nonrecursively replace references -- convert [[type REF]] to <type REF>string for REF</type>
        for (std::map<std::string, std::string>::iterator map_it = m_strings.begin();
             map_it != m_strings.end(); ++map_it)
        {
            std::size_t position = 0; // position in the definition string, past the already processed part
            smatch match;
            while (regex_search(map_it->second.begin() + position, map_it->second.end(), match, REFERENCE)) {
                position += match.position();
                std::map<std::string, std::string>::iterator map_lookup_it = m_strings.find(match[2]);
                if (map_lookup_it != m_strings.end()) {
                    const std::string substitution =
                        '<' + match[1].str() + ' ' + match[2].str() + '>' + map_lookup_it->second + "</" + match[1].str() + '>';
                    map_it->second.replace(position, match.length(), substitution);
                    position += substitution.length();
                } else {
                    Logger().errorStream() << "Unresolved reference: " << match[2] << " in: " << m_filename << ".";
                    position += match.length();
                }
            }
        }
    } else {
        Logger().errorStream() << "StringTable file \"" << m_filename << "\" is malformed around line " << std::count(file_contents.begin(), it, '\n');
    }
}
