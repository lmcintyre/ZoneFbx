#include "pch.h"
#include "Util.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <msclr/marshal_cppstd.h>

std::string Util::get_std_str(System::String^ orig)
{
    return msclr::interop::marshal_as<std::string>(orig);
}

System::String^ Util::get_str_handle(std::string orig)
{
    return msclr::interop::marshal_as<System::String^>(orig);
}

std::string Util::get_zone_code(System::String^ zone_path)
{
    auto code = zone_path->Substring(zone_path->LastIndexOf("/level") - 4, 4);
    return msclr::interop::marshal_as<std::string>(code);
}

std::string Util::get_texture_folder(std::string out_path, std::string zone_code)
{
    return out_path + zone_code + "_textures\\";
}

System::String^ Util::get_texture_path(std::string out_path, std::string zone_code, System::String^ texture_path)
{
    auto tex_abs_path = texture_path->Substring(texture_path->LastIndexOf('/') + 1)->Replace(".tex", ".png");
    tex_abs_path = get_str_handle(get_texture_folder(out_path, zone_code)) + tex_abs_path;
    return tex_abs_path;
}

std::string Util::get_relative_texture_path(std::string out_path, std::string zone_code, System::String^ texture_path)
{
    auto handle = get_texture_path(out_path, zone_code, texture_path);
    auto std_str = get_std_str(handle).replace(0, out_path.length(), "");
    return std_str;
}

double Util::degrees(double radians)
{
    return 180 / M_PI * radians;
}
