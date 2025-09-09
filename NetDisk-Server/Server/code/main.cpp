#include "Server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>

// 去除string的两端空白符
std::string trim (const std::string &str);
// 读取服务器配置文件信息
std::unordered_map<std::string, std::string> readConfig(const std::string &file_path);

int main() {
  std::unordered_map<std::string, std::string> config = readConfig("server_config.cfg");
  
  // 读取服务器配置
  const char *host = config["Server.host"].c_str();
  int port = std::stoi(config["Server.port"]);
  int upPort = std::stoi(config["Server.upPort"]);
  const char *sqlUser = config["Server.sqlUser"].c_str();
  const char *sqlPwd = config["Server.sqlPwd"].c_str();
  const char *dbName = config["Server.dbName"].c_str();
  int connPoolNum = std::stoi(config["Server.connPoolNum"]);
  int sqlport = std::stoi(config["Server.sqlport"]);
  int threadNum = std::stoi(config["Server.threadNum"]);
  int logqueSize = std::stoi(config["Server.logqueSize"]);
  int timeout = std::stoi(config["Server.timeout"]);

  // 读取负载均衡器配置
  const char *EqualizerIP = config["Equalizer.EqualizerIP"].c_str();
  int EqualizerPort = std::stoi(config["Equalizer.EqualizerPort"]);
  const char *EqualizerKey = config["Equalizer.EqualizerKey"].c_str();
  const std::string servername = config["Equalizer.servername"];
  bool isConEqualizer=(config["Equalizer.IsConEualizer"]=="true");

  Server server(host, port, upPort, sqlUser, sqlPwd, dbName, connPoolNum, sqlport, threadNum, logqueSize, timeout,
                EqualizerIP, EqualizerPort, EqualizerKey, servername, isConEqualizer);
  std::cout<<"服务器启动"<<std::endl;
  
  server.start();
  return 0;
}

// 去除string的两端空白符
std::string trim (const std::string &str) {
  size_t first = str.find_first_not_of(" \t\n\r\f\v");
  if (std::string::npos == first) return "";  // 全是空白字符
  size_t last = str.find_last_not_of(" \t\n\r\f\v");
  return str.substr(first, last - first + 1);
}

std::unordered_map<std::string, std::string> readConfig(const std::string &file_path) {
  std::unordered_map<std::string, std::string> config;
  std::ifstream infile(file_path);
  std::string line;

  if (!infile.is_open()) {
    std::cerr << "无法打开配置文件：" << file_path << std::endl;
    return config;
  }

  std::string section;
  while (std::getline(infile, line)) {
    // 去掉两端的空白字符
    line = trim(line);

    // 跳过注释或空行
    if (line.empty() || line[0] == ';' || line[0] == '#') continue;

    // 处理节区 [section]
    if (line[0] == '[' && line.back() == ']') {
      section = line.substr(1, line.size() - 2) + ".";
    }
    else {
      // 处理 key = value
      std::istringstream is_line(line);
      std::string key;
      if (std::getline(is_line, key, '=')) {
        std::string value;
        if (std::getline(is_line, value)) {
          key = trim(key);
          value = trim(value);
          config[section + key] = value;
        }
      }
    }
  }

  infile.close();
  return config;
}
