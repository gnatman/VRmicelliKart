#include <iostream>
#include <nlohmann/json.hpp>

int main() {
    try {
        nlohmann::json j;
        j["/CVars/gControllers/Port1/Buttons/1024ButtonMappingIds"] = "test";
        std::cout << j.unflatten().dump(4) << std::endl;
        std::cout << "Success!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
