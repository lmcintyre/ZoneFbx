#pragma once
#include <fbxsdk.h>
#include <gcroot.h>
#include <map>
#include <unordered_map>

class ZoneExporter
{
public:
    ZoneExporter();
    ~ZoneExporter();
    bool export_zone(System::String^, System::String^, System::String^);

private:
    gcroot<Lumina::GameData^> data = nullptr;
    FbxManager* manager = nullptr;
    FbxScene* scene = nullptr; 
    std::string zone_path = "";
    std::string out_folder = "";
    std::string zone_code = "";
    std::unordered_map<unsigned long long, FbxSurfacePhong*>* material_cache = nullptr;
    std::unordered_map<std::string, FbxMesh*>* mesh_cache = nullptr;
        
    bool process_terrain();
    bool process_bg();
    void process_model(Lumina::Models::Models::Model^ model, FbxNode** node);
    bool init(System::String^);
    FbxMesh* create_mesh(Lumina::Models::Models::Mesh^, const char*);
    bool create_material(Lumina::Models::Materials::Material^ mat, FbxSurfacePhong** out);
    void extract_textures(Lumina::Models::Materials::Material^ mat);
    bool save_scene();
    void uninit();
};
