#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE "Freeorion parser unit/coverage tests"
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_monitor.hpp>
#include <boost/test/output/compiler_log_formatter.hpp>

#include "CommonTest.h"

// Produces an GCC-ish message prefix for various frontends that understand gcc output.
struct gcc_log_formatter : 
    public boost::unit_test::output::compiler_log_formatter
{
    void print_prefix(std::ostream& output, boost::unit_test::const_string file_name, std::size_t line)
    {
        output << file_name << ':' << line << ": error: ";
    }
};

struct boost_test_config
{
    boost_test_config()
    {
        boost::unit_test::unit_test_log.set_formatter(new gcc_log_formatter);
        boost::unit_test::unit_test_monitor.register_exception_translator<boost::spirit::qi::expectation_failure<parse::token_iterator> >(&print_expectation_failure);
    }
};

BOOST_GLOBAL_FIXTURE(boost_test_config);
