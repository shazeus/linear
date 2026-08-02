#include "linear/Labels.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace linear {
namespace {

std::string formatNumber(double value, const std::string& specification) {
    std::ostringstream output;
    if (!specification.empty()) {
        const auto decimal = specification.find('.');
        if (decimal != std::string::npos) {
            const int precision =
                static_cast<int>(specification.size() - decimal - 1U);
            output << std::fixed << std::setprecision(std::clamp(precision, 0, 9));
        } else if (specification == "x" || specification == "X") {
            output << std::hex;
            if (specification == "X") {
                output << std::uppercase;
            }
            output << static_cast<std::uint64_t>(value);
            return output.str();
        }
    }
    output << value;
    return output.str();
}

} // namespace

std::string formatLabel(
    const std::string& labelTemplate, const LabelContext& context) {
    const Region& region = context.region;
    std::unordered_map<std::string, double> values{
        {"track_id", static_cast<double>(region.trackId)},
        {"id", static_cast<double>(region.componentId)},
        {"x", region.centroid.x},
        {"y", region.centroid.y},
        {"normalized_x", context.frameWidth > 0
                             ? region.centroid.x / context.frameWidth
                             : 0.0},
        {"normalized_y", context.frameHeight > 0
                             ? region.centroid.y / context.frameHeight
                             : 0.0},
        {"width", region.bounds.width},
        {"height", region.bounds.height},
        {"area", static_cast<double>(region.pixelArea)},
        {"fill_ratio", region.fillRatio},
        {"confidence", region.confidence},
        {"velocity_x", region.velocity.x},
        {"velocity_y", region.velocity.y},
        {"age", static_cast<double>(region.age)},
        {"frame", static_cast<double>(context.frameNumber)}};

    std::string result;
    result.reserve(labelTemplate.size() + 32U);
    for (std::size_t i = 0; i < labelTemplate.size();) {
        if (labelTemplate[i] == '{') {
            if (i + 1U < labelTemplate.size() && labelTemplate[i + 1U] == '{') {
                result.push_back('{');
                i += 2U;
                continue;
            }
            const std::size_t end = labelTemplate.find('}', i + 1U);
            if (end == std::string::npos) {
                throw std::invalid_argument("Unclosed label token");
            }
            const std::string token = labelTemplate.substr(i + 1U, end - i - 1U);
            const std::size_t separator = token.find(':');
            const std::string name = token.substr(0U, separator);
            const std::string specification =
                separator == std::string::npos ? std::string{}
                                               : token.substr(separator + 1U);
            const auto value = values.find(name);
            if (value == values.end()) {
                throw std::invalid_argument("Unknown label token: " + name);
            }
            result += formatNumber(value->second, specification);
            i = end + 1U;
        } else if (labelTemplate[i] == '}' && i + 1U < labelTemplate.size() &&
                   labelTemplate[i + 1U] == '}') {
            result.push_back('}');
            i += 2U;
        } else {
            result.push_back(labelTemplate[i]);
            ++i;
        }
    }
    return result;
}

} // namespace linear
