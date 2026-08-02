#pragma once

#include <string>

namespace linear::plugin {

struct LabelEditorValues {
    std::string text{"CUSTOM"};
    std::string fontFamily{};
};

// Opens a modal editor owned by the active Adobe host window. Returns true
// only when Apply is pressed; Cancel leaves values untouched.
bool showLabelEditor(LabelEditorValues& values);

} // namespace linear::plugin
