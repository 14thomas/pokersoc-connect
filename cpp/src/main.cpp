#include <exception>
#include <iostream>
#include <string>

#include "domain/money/DenominationTable.h"
#include "ui/console/ConsoleApp.h"
#include "ui/console/ConsoleIO.h"

using pokersoc::domain::DenominationTable;
using pokersoc::ui::ConsoleApp;

int main() {
    std::cout << "PokerSoc Connect (C++)\n\n";

    std::string path = pokersoc::ui::ask("Session file [session.db]: ", "session.db");
    const DenominationTable table = DenominationTable::standardAustralian();

    while (true) {
        ConsoleApp::Exit exit;
        try {
            ConsoleApp app(path, table);
            exit = app.run();
        } catch (const std::exception& error) {
            std::cerr << "Error: " << error.what() << '\n';
            return 1;
        }
        if (exit.quit || pokersoc::ui::inputClosed()) {
            break;
        }
        path = exit.nextSession;
        std::cout << "\nSwitching to session '" << path << "'...\n";
    }

    std::cout << "\nGoodbye.\n";
    return 0;
}
