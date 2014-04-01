// -*- C++ -*-
#ifndef _Lexer_h_
#define _Lexer_h_

#define BOOST_SPIRIT_NO_PREDEFINED_TERMINALS

#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/spirit/home/lex/lexer/lexertl/position_token.hpp>

#include "Tokens.h"


/** \namespace parse \brief The namespace that encloses the script file lexer
    and parser. */
namespace parse {

/** The type of iterator used by the script file lexer. */
typedef std::string::const_iterator text_iterator;

/** The type of token used by the script file lexer. */
typedef boost::spirit::lex::lexertl::position_token<
    text_iterator,
    boost::mpl::vector<
        bool,
        int,
        double,
        const char*,
        std::string
    >
> token_type;

typedef boost::spirit::lex::lexertl::actor_lexer<token_type> spirit_lexer_base_type;

/** The script file lexer. */
struct lexer :
    boost::spirit::lex::lexer<spirit_lexer_base_type>
{
    static const lexer& instance();

    /** \name Comment tokens */ ///@{
    boost::spirit::lex::token_def<boost::spirit::lex::omit> inline_comment;
    boost::spirit::lex::token_def<boost::spirit::lex::omit> end_of_line_comment;
    //@}

    /** \name Tokens for common C++ types and builtins. */ ///@{
    boost::spirit::lex::token_def<bool> bool_;
    boost::spirit::lex::token_def<int> int_;
    boost::spirit::lex::token_def<double> double_;
    boost::spirit::lex::token_def<std::string> string;
    //@}

    /** \name Keyword tokens.  These should be kept in lexicographically
        sorted order, so that finding, adding, and removing tokens is a bit
        easier.  See the note above the Enum tokens section. */ ///@{
#define DECLARE_TOKEN(r, _, name) boost::spirit::lex::token_def<const char*> BOOST_PP_CAT(name, _);
    BOOST_PP_SEQ_FOR_EACH(DECLARE_TOKEN, _, TOKEN_SEQ_1)
    BOOST_PP_SEQ_FOR_EACH(DECLARE_TOKEN, _, TOKEN_SEQ_2)
    BOOST_PP_SEQ_FOR_EACH(DECLARE_TOKEN, _, TOKEN_SEQ_3)
    BOOST_PP_SEQ_FOR_EACH(DECLARE_TOKEN, _, TOKEN_SEQ_4)
    BOOST_PP_SEQ_FOR_EACH(DECLARE_TOKEN, _, TOKEN_SEQ_5)
#undef DECLARE_TOKEN
    //@}

    /** \name Error token. */ ///@{
    boost::spirit::lex::token_def<boost::spirit::lex::omit> error_token;
    //@}

    /** Returns the token_def<const char*> associated with \a name. */
    const boost::spirit::lex::token_def<const char*>& name_token(const char* name) const;

    static const char* bool_regex;
    static const char* int_regex;
    static const char* double_regex;
    static const char* string_regex;

private:
    /** Ctor. */
    lexer();

    std::map<const char*, boost::spirit::lex::token_def<const char*>*> m_name_tokens;
};

/** The type of iterator passed to the script file parser by the script file
    lexer. */
typedef lexer::iterator_type token_iterator;

typedef lexer::lexer_def lexer_def;

/** The type of the skip-parser, defined in the script file lexer, used by the
    script file parser iterator. */
typedef boost::spirit::qi::in_state_skipper<lexer_def> skipper_type;

}

namespace boost { namespace spirit { namespace traits {

    // If you want to create a token with a custom value type, you must
    // declare the conversion handler here, and define it in the .cpp file.

    // These template specializations are required by Spirit.Lex to automatically
    // convert an iterator pair to an const char* in the lexer.
    template <>
    struct assign_to_attribute_from_iterators<const char*, parse::text_iterator, void>
    { static void call(const parse::text_iterator& first, const parse::text_iterator& last, const char*& attr); };

    // HACK! This is only necessary because of a bug in Spirit in Boost
    // versions <= 1.45.
    template <>
    struct assign_to_attribute_from_iterators<bool, parse::text_iterator, void>
    { static void call(const parse::text_iterator& first, const parse::text_iterator& last, bool& attr); };

} } }

#endif
