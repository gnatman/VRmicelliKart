#include <iostream>
#include <nlohmann/json.hpp>

int main() {
    try {
        nlohmann::json j;
        j["/A/1024X"] = 1;
        j["/A/B"] = 2;
        std::cout << j.unflatten().dump(4) << std::endl;
        
        nlohmann::json j2;
        j2["/B/123"] = 1;
        j2["/B/C"] = 2;
        std::cout << "Trying conflict..." << std::endl;
        std::cout << j2.unflatten().dump(4) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
