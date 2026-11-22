#include "paths.hpp"

namespace knot::core {

std::string qualify_path(const std::string& root, const std::string& relative,
                         bool multiple_roots) {
    if (!multiple_roots) return relative;
    std::string trimmed = root;
    while (trimmed.size() > 1 &&
           (trimmed.back() == '/' || trimmed.back() == '\\')) trimmed.pop_back();
    std::size_t slash = trimmed.find_last_of("/\\");
    std::string name = slash == std::string::npos ? trimmed : trimmed.substr(slash + 1);
    if (name.empty()) return relative;
    return name + "/" + relative;
}

}  // namespace knot::core
