#include <iostream>
#include <string>

#if defined(CHECK_FIND_PACKAGE_BOOST)
    #include <boost/program_options.hpp>
#endif

#include <ext_shared_lib/ext_shared_lib.h>
#include <ext_shared_lib_2.h>
#include <testing/testdefs.h>

#include "interface_lib/interface_lib.h"
#include "shared_lib/shared_lib.h"
#include "shared_lib_2/shared_lib_2.h"
#include "shared_lib_3/shared_lib_3.h"
#include "static_lib/static_lib.h"
#include "static_lib_2/static_lib_2.h"
#include "static_lib_3/static_lib_3.h"
#include "static_lib_boost/static_lib_boost.h"

#include "version.h"

#define TEST_LIB(fn, et)                                                       \
    if (fn() != et) {                                                          \
        std::cerr << "[FAIL] " << #fn << "('" << fn() << "') != '" << et << "'"\
                  << std::endl;                                                \
        rc = 1;                                                                \
    } else                                                                     \
        std::cout << "[ OK ] " << #fn << "('" << fn() << "')" << std::endl

#define TEST_CASE(var, et)                                                     \
    if (et != var) {                                                           \
        std::cerr << "[FAIL] " << #var << "('" << var << "') != '" << et << "'"\
                  << std::endl;                                                \
        rc = 1;                                                                \
    } else                                                                     \
        std::cout << "[ OK ] " << #var << std::endl


TEST(case_name_1, assert_true)
{
    ASSERT_TRUE(1 == 1) << "expected true";
}

int main(int argc, char** argv)
{
#if defined(CHECK_FIND_PACKAGE_BOOST)
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "print usage message");
    // Parse command line arguments
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
    po::notify(vm);
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }
#else
    (void)argc;
    (void)argv;
#endif

    int rc = 0;

    TEST_LIB(interface_lib_func<std::string>, "interface_lib_func");
    TEST_LIB(shared_lib_func, "shared_lib");
    TEST_LIB(static_lib_func, "static_lib_func");

    TEST_LIB(ext_shared_lib_func, "ext_shared_lib_func");
    TEST_LIB(ext_interface_lib_func, "ext_interface_lib_func");

    TEST_LIB(shared_lib_func_2, "shared_lib_2");
    TEST_LIB(static_lib_func_2, "ext_shared_lib_func_2");

    TEST_LIB(shared_lib_func_3, "interface_lib_func");
    TEST_LIB(static_lib_func_3, "interface_lib_func");

    TEST_LIB(static_lib_boost, "Tuesday 2001-Oct-09");

    TEST_CASE(PROJECT_VERSION, 101);

    RUN_ALL_TESTS();

    return rc;
}
