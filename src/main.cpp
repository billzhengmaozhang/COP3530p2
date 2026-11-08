#include "minidb/Cli.h"
#include <exception>
#include <iostream>

int main() {
    try {
        minidb::Database database;
        minidb::Cli cli(database, std::cin, std::cout);
        return cli.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }
}
