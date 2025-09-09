#include "AbstractTool.h"


std::string AbstractTool::getSuffix(const std::string &file_name) {
  if (file_name.empty()) {
    return "";
  }

  size_t pos = file_name.rfind('.');
  if (pos != std::string::npos && pos != file_name.size() - 1) {
    return file_name.substr(pos + 1);
  }
  return "other";
}
