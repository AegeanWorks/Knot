#include "paths.hpp"

namespace knot::core {

std::string qualify_path(const std::string& root, const std::string& relative,
                         bool multiple_roots, const std::string& prefix_override) {
    if (!multiple_roots) return relative;
    std::string name = prefix_override;
    if (name.empty()) {
        std::string trimmed = root;
        while (trimmed.size() > 1 &&
               (trimmed.back() == '/' || trimmed.back() == '\\')) trimmed.pop_back();
        std::size_t slash = trimmed.find_last_of("/\\");
        name = slash == std::string::npos ? trimmed : trimmed.substr(slash + 1);
    }
    if (name.empty()) return relative;
    return name + "/" + relative;
}

}  // namespace knot::core
