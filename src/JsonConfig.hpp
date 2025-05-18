#ifndef __JSON_CONFIG_HPP__
#define __JSON_CONFIG_HPP__

#include <exception>
#include <fstream>
#include <raylib.h>
#include "nlohmann/json.hpp"

class JsonConfig {
    std::string _filename;
    nlohmann::json _defaults;
    nlohmann::json _data;
    public:
    JsonConfig(std::string filename, nlohmann::json defaults) {
        _filename = filename;
        _defaults = defaults;
        _data = _defaults;
        load();
    }
    JsonConfig(std::string filename) {
        _filename = filename;
        load();
    }
    bool load() {
        std::ifstream fd(_filename);
        if (fd.is_open()) {
            try {
                fd >> _data;
            } catch (nlohmann::detail::exception err) {
                TraceLog(LOG_WARNING, "Failed to load json from file %s: %s", _filename.c_str(), err.what());
                return false;
            } catch (std::exception err) {
                TraceLog(LOG_WARNING, "Failed to load json from file %s: %s", _filename.c_str(), err.what());
                return false;
            }
            fd.close();
            return true;
        }
        return false;
    }
    bool save() {
        std::ofstream fd(_filename);
        if (fd.is_open()) {
            try {
                fd << _data;
            } catch (std::exception err) {
                TraceLog(LOG_WARNING, "Failed to save json to file %s: %s", _filename.c_str(), err.what());
                return false;
            }
            fd.close();
            return true;
        }
        return false;
    }
    template<class T>
    T get(std::string key) {
        try {
            return _data[key].get<T>();
        } catch (nlohmann::detail::exception err) {
            TraceLog(LOG_WARNING, "Failed to get key %s: %s", key.c_str(), err.what());
            return T();
        }
    }
    template<class T>
    void set(std::string key, T value) {
        try {
            _data[key] = value;
        } catch (nlohmann::detail::exception err) {
            TraceLog(LOG_WARNING, "Failed to set key %s: %s", key.c_str(), err.what());
        }
    }
    bool contains(std::string key) {
        return _data.contains(key);
    }
    nlohmann::json& operator[](std::string key) {
        return _data[key];
    }

};

#endif