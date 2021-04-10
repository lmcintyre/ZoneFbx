#pragma once
#include <string>
#include <fbxsdk.h>

class Util
{
public:
    static std::string get_std_str(System::String^ orig);
    static System::String^ get_str_handle(std::string orig);
    static std::string get_zone_code(System::String^ zone_path);
    static std::string get_texture_folder(std::string out_path, std::string zone_code);
    static System::String^ get_texture_path(std::string out_path, std::string zone_code, System::String^ texture_path);
    static std::string get_relative_texture_path(std::string out_path, std::string zone_code, System::String^ texture_path);
    static double degrees(double radians);
    static std::string get_texture_path(std::string out_path, System::String^ texture_path);
};
