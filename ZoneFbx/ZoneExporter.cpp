#include "pch.h"
#include "ZoneExporter.h"

#include <algorithm>
#include <ostream>
#include <msclr/marshal_cppstd.h>

#include "Util.h"

bool ZoneExporter::export_zone(System::String^ game_path, System::String^ zone_path, System::String^ output_path)
{
    if (manager || scene)
        uninit();

    this->zone_path = Util::get_std_str(zone_path);
    this->out_folder = Util::get_std_str(output_path);
    this->zone_code = Util::get_zone_code(zone_path);
    this->material_cache = new std::unordered_map<unsigned long long, FbxSurfacePhong*>();
    this->mesh_cache = new std::unordered_map<std::string, FbxMesh*>();

    std::printf("Initializing...\n");
    auto init_result = init(game_path);
    if (!init_result)
    {
        std::printf("Error occurred during ZoneExporter initialization.\n");
        return false;
    }

    std::printf("Processing models and textures...\n");
    std::printf("Processing zone terrain.\n");
    if (!process_terrain())
    {
        std::printf("Failed to process zone terrain.\n");
        return false;
    }

    std::printf("Processing bg.lgb...\n");
    if (!process_bg())
    {
        std::printf("Failed to process zone bg.\n");
        return false;
    }

    std::printf("Saving scene...\n");
    if (!save_scene())
    {
        std::printf("Failed to save scene.\n");
        return false;
    }

    std::printf("Saved scene...\n");

    return true;
}

bool ZoneExporter::process_terrain()
{
    auto terrain_path = zone_path.substr(0, zone_path.rfind("/level"));
    terrain_path = "bg/" + terrain_path + "/bgplate/";
    auto terafile_path = terrain_path + "terrain.tera";

    if (!data->FileExists(Util::get_str_handle(terafile_path)))
        return true;
    
    auto terafile = data->GetFile<Lumina::Data::Files::TeraFile^>(Util::get_str_handle(terafile_path));

    if (terafile == nullptr)
        return false;

    FbxNode* terrain_node = FbxNode::Create(manager, "terrain");

    for (int i = 0; i < terafile->PlateCount; i++)
    {
        FbxNode* plate_node = FbxNode::Create(manager, (std::string("bgplate_") + std::to_string(i)).c_str());
        auto pos = terafile->GetPlatePosition(i);
        plate_node->LclTranslation.Set(FbxVectorTemplate3<double>(pos.X, 0, pos.Y));
        auto plate_model = gcnew Lumina::Models::Models::Model(data,
                                                               Util::get_str_handle(terrain_path) + System::String::Format("{0:D4}.mdl", i),
                                                               Lumina::Models::Models::Model::ModelLod::High,
                                                               1);
        process_model(plate_model, &plate_node);
        
        terrain_node->AddChild(plate_node);
    }

    scene->GetRootNode()->AddChild(terrain_node);
    return true;
}

System::String^ ZoneExporter::get_eobj_sgb_path(System::UInt32 eobj_id)
{
    if (eobj_sgb_paths->Count == 0)
    {
        auto eobj_sheet = data->Excel->GetSheetRaw("EObj");
        auto exported_sg_sheet = data->Excel->GetSheetRaw("ExportedSG");

        auto exported_sg_paths = gcnew System::Collections::Generic::Dictionary<System::UInt16, System::String^>();

        for each (Lumina::Excel::RowParser^ row in exported_sg_sheet->EnumerateRowParsers())
            if (!exported_sg_paths->ContainsKey(row->Row))
                exported_sg_paths->Add(row->Row, row->ReadColumn<System::String^>(0));

        for each (Lumina::Excel::RowParser^ row in eobj_sheet->EnumerateRowParsers())
        {
            auto link = row->ReadColumn<System::UInt16>(11);
            System::String^ out;
            if (exported_sg_paths->TryGetValue(link, out))
                if (!eobj_sgb_paths->ContainsKey(row->Row))
                    eobj_sgb_paths->Add(row->Row, out);
        }
    }
    System::String^ ret;
    eobj_sgb_paths->TryGetValue(eobj_id, ret);
    return ret;
}

System::String^ ZoneExporter::get_eobj_name(System::UInt32 eobj_id)
{
    if (eobj_names->Count == 0)
    {
        auto eobj_name_sheet = data->Excel->GetSheetRaw("EObjName");

        for each (Lumina::Excel::RowParser ^ row in eobj_name_sheet->EnumerateRowParsers())
            if (!eobj_names->ContainsKey(row->Row))
                eobj_names->Add(row->Row, row->ReadColumn<System::String^>(0));
    }
    System::String^ ret;
    eobj_names->TryGetValue(eobj_id, ret);
    return ret;
}

void ZoneExporter::process_layer(Lumina::Data::Parsing::Layer::LayerCommon::Layer^ layer, FbxNode* parent_node)
{
    auto layer_node = parent_node ? parent_node : FbxNode::Create(scene, Util::get_std_str(layer->Name).c_str());
    
    for (int j = 0; j < layer->InstanceObjects->Length; j++)
    {
        auto object = % layer->InstanceObjects[j];
        auto object_node = FbxNode::Create(scene, Util::get_std_str(object->Name).c_str());

        object_node->LclTranslation.Set(FbxDouble3(object->Transform.Translation.X, object->Transform.Translation.Y, object->Transform.Translation.Z));
        object_node->LclRotation.Set(FbxDouble3(Util::degrees(object->Transform.Rotation.X),
            Util::degrees(object->Transform.Rotation.Y),
            Util::degrees(object->Transform.Rotation.Z)));
        object_node->LclScaling.Set(FbxDouble3(object->Transform.Scale.X, object->Transform.Scale.Y, object->Transform.Scale.Z));

        if (object->AssetType == Lumina::Data::Parsing::Layer::LayerEntryType::BG)
        {
            auto instance_object = static_cast<Lumina::Data::Parsing::Layer::LayerCommon::BGInstanceObject^>(object->Object);
            auto object_path = instance_object->AssetPath;
            auto model = gcnew Lumina::Models::Models::Model(data, object_path, Lumina::Models::Models::Model::ModelLod::High, 1);

            auto model_node = FbxNode::Create(scene, Util::get_std_str(object_path->Substring(object_path->LastIndexOf('/') + 1)).c_str());
            // model_node->LclTranslation.Set(FbxDouble3(object->Transform.Translation.X, object->Transform.Translation.Y, object->Transform.Translation.Z));
            // model_node->LclRotation.Set(FbxDouble3(object->Transform.Rotation.X, object->Transform.Rotation.Y, object->Transform.Rotation.Z));
            // model_node->LclScaling.Set(FbxDouble3(object->Transform.Scale.X, object->Transform.Scale.Y, object->Transform.Scale.Z));

            process_model(model, &model_node);

            object_node->AddChild(model_node);
            layer_node->AddChild(object_node);
        }
        else if (object->AssetType == Lumina::Data::Parsing::Layer::LayerEntryType::LayLight)
        {
            auto light_object = static_cast<Lumina::Data::Parsing::Layer::LayerCommon::LightInstanceObject^>(object->Object);

            auto light_node = FbxLight::Create(object_node, Util::get_std_str(object->Name + "_light").c_str());
            switch (light_object->LightType)
            {
            case Lumina::Data::Parsing::Layer::LightType::Directional:
                light_node->LightType = FbxLight::EType::eDirectional;
                break;
            case Lumina::Data::Parsing::Layer::LightType::Point:
                light_node->LightType = FbxLight::EType::ePoint;
                break;
            case Lumina::Data::Parsing::Layer::LightType::Spot:
                light_node->LightType = FbxLight::EType::eSpot;
                break;
            case Lumina::Data::Parsing::Layer::LightType::Plane:
                light_node->LightType = FbxLight::EType::eArea;
                light_node->AreaLightShape = FbxLight::EAreaLightShape::eRectangle;
            default:
                light_node->LightType = FbxLight::EType::ePoint;
                break;
            }
            light_node->Intensity.Set((double)light_object->DiffuseColorHDRI.Intensity * 100.0);
            light_node->CastLight.Set(true);
            light_node->CastShadows.Set(light_object->BGShadowEnabled != 0);
            light_node->Color.Set({ light_object->DiffuseColorHDRI.Red / 255.f, light_object->DiffuseColorHDRI.Green / 255.f, light_object->DiffuseColorHDRI.Blue / 255.f });
            light_node->DecayStart.Set((double)light_object->RangeRate + 25.);
            light_node->DecayType.Set(FbxLight::EDecayType::eCubic);
            light_node->EnableFarAttenuation.Set(true);
            light_node->EnableNearAttenuation.Set(true);
            light_node->InnerAngle.Set(0.0);
            light_node->OuterAngle.Set((double)light_object->ConeDegree + 0.01f); // shitty div by 0 in blender

            object_node->LclRotation.Set(FbxDouble3(
                object_node->LclRotation.Get().mData[0] - 90.,
                object_node->LclRotation.Get().mData[1],
                object_node->LclRotation.Get().mData[2]
            ));

            object_node->SetNodeAttribute(light_node);
            layer_node->AddChild(object_node);
            //System::Console::WriteLine("Cone degree {0}, Attenuation {1} RangeRate {2}", light_object->ConeDegree, light_object->Attenuation, light_object->RangeRate);
        }
        else if (object->AssetType == Lumina::Data::Parsing::Layer::LayerEntryType::VFX)
        {
            auto vfx_object = static_cast<Lumina::Data::Parsing::Layer::LayerCommon::VFXInstanceObject^>(object->Object);

            auto light_node = FbxLight::Create(object_node, Util::get_std_str(object->Name + "_vfx").c_str());
            light_node->NearAttenuationStart.Set((double)vfx_object->FadeNearStart);
            light_node->NearAttenuationEnd.Set((double)vfx_object->FadeNearEnd);
            light_node->FarAttenuationStart.Set((double)vfx_object->FadeFarStart);
            light_node->FarAttenuationEnd.Set((double)vfx_object->FadeFarEnd);
            light_node->Color.Set({ vfx_object->Color.Red / 255.f, vfx_object->Color.Green / 255.f, vfx_object->Color.Blue / 255.f });
            light_node->CastLight.Set(vfx_object->IsAutoPlay != 0);
            light_node->DecayStart.Set(25.f + vfx_object->SoftParticleFadeRange);

            object_node->LclRotation.Set(FbxDouble3(
                object_node->LclRotation.Get().mData[0] - 90.,
                object_node->LclRotation.Get().mData[1],
                object_node->LclRotation.Get().mData[2]
            ));

            object_node->SetNodeAttribute(light_node);
            layer_node->AddChild(object_node);
        }
        else if (object->AssetType == Lumina::Data::Parsing::Layer::LayerEntryType::EventObject)
        {
            auto event_object = static_cast<Lumina::Data::Parsing::Layer::LayerCommon::EventInstanceObject^>(object->Object);
            auto shared_path = get_eobj_sgb_path(event_object->ParentData.BaseId);

            // read the eobj name from exd
            object_node->SetName(Util::get_std_str(get_eobj_name(event_object->ParentData.BaseId)).c_str());

            if (shared_path != nullptr)
            {
                auto shared_file = data->GetFile<Lumina::Data::Files::SgbFile^>(shared_path);
                if (shared_file)
                {
                    for (auto k = 0; k < shared_file->LayerGroups->Length; ++k)
                    {
                        auto group = shared_file->LayerGroups[k];
                        for (auto l = 0; l < group.Layers->Length; ++l)
                        {
                            auto sgbLayer = group.Layers[l];
                            process_layer(sgbLayer, object_node);
                        }
                    }
                }
            }
            layer_node->AddChild(object_node);
        }
        else if (object->AssetType == Lumina::Data::Parsing::Layer::LayerEntryType::SharedGroup)
        {
            auto shared_object = static_cast<Lumina::Data::Parsing::Layer::LayerCommon::SharedGroupInstanceObject^>(object->Object);
            auto shared_path = shared_object->AssetPath;
            auto shared_file = data->GetFile<Lumina::Data::Files::SgbFile^>(shared_path);

            for (auto k = 0; k < shared_file->LayerGroups->Length; ++k)
            {
                auto group = shared_file->LayerGroups[k];
                for (auto l = 0; l < group.Layers->Length; ++l)
                {
                    auto sgbLayer = group.Layers[l];
                    process_layer(sgbLayer, object_node);
                }
            }
            layer_node->AddChild(object_node);
            System::Console::WriteLine(shared_path);
        }
    }
    scene->GetRootNode()->AddChild(layer_node);
}

bool ZoneExporter::process_bg()
{
    auto bg_path = "bg/" + Util::get_str_handle(zone_path.substr(0, zone_path.length() - 5)) + "level/";
    std::vector<std::string> paths = { "bg.lgb", "planmap.lgb", "planevent.lgb" };
    
    for (auto path : paths)
    {
        auto bg = data->GetFile<Lumina::Data::Files::LgbFile^>(bg_path + gcnew System::String(path.c_str()));

        if (bg == nullptr)
            return false;

        for (int i = 0; i < bg->Layers->Length; i++)
        {
            auto layer = bg->Layers[i];
            auto layer_node = FbxNode::Create(scene, Util::get_std_str(layer.Name).c_str());
            process_layer(layer, layer_node);
        }
    }
    return true;
}

void ZoneExporter::process_model(Lumina::Models::Models::Model^ model, FbxNode** node)
{
    auto path = model->File->FilePath->Path;
    path = path->Substring(path->LastIndexOf('/') + 1);

    for (int j = 0; j < model->Meshes->Length; j++)
    {
        const auto mesh_name = Util::get_std_str(path + "_") + std::to_string(j);
        FbxNode* mesh_node = FbxNode::Create(manager, mesh_name.c_str());

        auto result = mesh_cache->find(mesh_name);
        FbxMesh* mesh;
        if (result != mesh_cache->end())
        {
            mesh = result->second;
        } else {
            mesh = create_mesh(model->Meshes[j], mesh_name.c_str());
            FbxSurfacePhong* material;
            create_material(model->Meshes[j]->Material, &material);
            mesh_node->AddMaterial(material);
            mesh_cache->insert({mesh_name, mesh});
        }
        
        mesh_node->SetNodeAttribute(mesh);
        (*node)->AddChild(mesh_node);
    }
}

FbxMesh* ZoneExporter::create_mesh(Lumina::Models::Models::Mesh^ game_mesh, const char* mesh_name)
{
    FbxMesh* mesh = FbxMesh::Create(scene, mesh_name);
    mesh->InitControlPoints(game_mesh->Vertices->Length);
    mesh->InitNormals(game_mesh->Vertices->Length);

    FbxGeometryElementVertexColor* colorElement = mesh->CreateElementVertexColor();
    colorElement->SetMappingMode(FbxLayerElement::EMappingMode::eByControlPoint);

    FbxGeometryElementUV* uvElement1 = mesh->CreateElementUV("uv1");
    uvElement1->SetMappingMode(FbxLayerElement::EMappingMode::eByControlPoint);

    FbxGeometryElementUV* uvElement2 = mesh->CreateElementUV("uv2");
    uvElement2->SetMappingMode(FbxLayerElement::EMappingMode::eByControlPoint);

    FbxGeometryElementTangent* tangentElem1 = mesh->CreateElementTangent();
    tangentElem1->SetMappingMode(FbxLayerElement::EMappingMode::eByControlPoint);

    FbxGeometryElementTangent* tangentElem2 = mesh->CreateElementTangent();
    tangentElem2->SetMappingMode(FbxLayerElement::EMappingMode::eByControlPoint);

    for (int i = 0; i < game_mesh->Vertices->Length; i++)
    {
        FbxVector4 pos, norm, uv, tangent1, tangent2;
        FbxColor color;
        if (game_mesh->Vertices[i].Position.HasValue) {}
            pos = FbxVector4(game_mesh->Vertices[i].Position.Value.X,
                             game_mesh->Vertices[i].Position.Value.Y,
                             game_mesh->Vertices[i].Position.Value.Z,
                             game_mesh->Vertices[i].Position.Value.W);

        if (game_mesh->Vertices[i].Normal.HasValue)
            norm = FbxVector4(game_mesh->Vertices[i].Normal.Value.X,
                              game_mesh->Vertices[i].Normal.Value.Y,
                              game_mesh->Vertices[i].Normal.Value.Z,
                              0);
        // if (game_mesh->Vertices[i].UV.HasValue)
        //     uv = FbxVector4(game_mesh->Vertices[i].UV.Value.X,
        //                             game_mesh->Vertices[i].UV.Value.Y,
        //                             game_mesh->Vertices[i].UV.Value.Z,
        //                             game_mesh->Vertices[i].UV.Value.W);

        if (game_mesh->Vertices[i].Color.HasValue)
            color = FbxColor(game_mesh->Vertices[i].Color.Value.X,
                             game_mesh->Vertices[i].Color.Value.Y,
                             game_mesh->Vertices[i].Color.Value.Z,
                             game_mesh->Vertices[i].Color.Value.W);

        if (game_mesh->Vertices[i].Tangent1.HasValue)
            tangent1 = FbxVector4(game_mesh->Vertices[i].Tangent1.Value.X,
                                  game_mesh->Vertices[i].Tangent1.Value.Y,
                                  game_mesh->Vertices[i].Tangent1.Value.Z,
                                  game_mesh->Vertices[i].Tangent1.Value.W);
        if (game_mesh->Vertices[i].Tangent2.HasValue)
            tangent2 = FbxVector4(game_mesh->Vertices[i].Tangent2.Value.X,
                                  game_mesh->Vertices[i].Tangent2.Value.Y,
                                  game_mesh->Vertices[i].Tangent2.Value.Z,
                                  game_mesh->Vertices[i].Tangent2.Value.W);

        if (pos && norm)
            mesh->SetControlPointAt(pos, norm, i);

        if (game_mesh->Vertices[i].UV.HasValue)
        {
            uvElement1->GetDirectArray().Add(FbxVector2(game_mesh->Vertices[i].UV.Value.X, game_mesh->Vertices[i].UV.Value.Y * -1));
            uvElement2->GetDirectArray().Add(FbxVector2(game_mesh->Vertices[i].UV.Value.Z, game_mesh->Vertices[i].UV.Value.W * -1));
        }

        // Color
        colorElement->GetDirectArray().Add(color);

        // Tangents
        tangentElem1->GetDirectArray().Add(tangent1);
        tangentElem2->GetDirectArray().Add(tangent2);
    }

    for (int i = 0; i < game_mesh->Indices->Length; i += 3)
    {
        mesh->BeginPolygon();
        mesh->AddPolygon(game_mesh->Indices[i]);
        mesh->AddPolygon(game_mesh->Indices[i + 1]);
        mesh->AddPolygon(game_mesh->Indices[i + 2]);
        mesh->EndPolygon();
    }

    return mesh;
}

bool ZoneExporter::create_material(Lumina::Models::Materials::Material^ mat, FbxSurfacePhong** out)
{
    auto mat_path = mat->MaterialPath;
    auto material_name = mat_path->Substring(mat_path->LastIndexOf('/') + 1);
    auto std_material_name = Util::get_std_str(material_name);

    const auto hash = mat->File->FilePath->IndexHash;
    auto result = material_cache->find(hash);
    if (result != material_cache->end())
    {
        *out = result->second;
        return true;
    }
    extract_textures(mat);
    *out = FbxSurfacePhong::Create(scene, std_material_name.c_str());

    (*out)->AmbientFactor.Set(1.);
    (*out)->DiffuseFactor.Set(1.);
    (*out)->SpecularFactor.Set(0.3);
    (*out)->BumpFactor.Set(0.3);
    (*out)->EmissiveFactor.Set(1.0);

    (*out)->ShadingModel.Set("Phong");

    std::map<std::string, std::vector<FbxTexture*>> textures;
    auto fbx_textures = std::map<std::string, FbxPropertyT<FbxDouble3>*>
    {
        {"ambient", &(*out)->Ambient},
        {"diffuse", &(*out)->Diffuse},
        {"specular", &(*out)->Specular},
        {"normal", &(*out)->NormalMap},
        {"emissive", &(*out)->Emissive}
    };

    for (int i = 0; i < mat->Textures->Length; i++)
    {
        if (mat->Textures[i]->TexturePath->Contains("dummy"))
            continue;

        auto usage_name = mat->Textures[i]->TextureUsageSimple.ToString();

        auto texture = FbxFileTexture::Create(scene, Util::get_std_str(usage_name).c_str());
        auto rel = Util::get_relative_texture_path(out_folder, zone_code, mat->Textures[i]->TexturePath);
        texture->SetFileName(rel.c_str());
        texture->SetMappingType(FbxTexture::eUV);
        texture->SetTextureUse(FbxTexture::eStandard);
        texture->SetMaterialUse(FbxFileTexture::eModelMaterial);
        texture->SetSwapUV(false);
        texture->SetTranslation(0.0, 0.0);
        texture->SetScale(1.0, 1.0);
        texture->SetRotation(0.0, 0.0);

        // We are ignoring the 2nd texture that they use for blending // LIES LIES LIES
        switch (mat->Textures[i]->TextureUsageSimple)
        {
            case Lumina::Models::Materials::Texture::Usage::Diffuse:  textures["diffuse"].push_back(texture); break;
            case Lumina::Models::Materials::Texture::Usage::Specular: textures["specular"].push_back(texture); break;
            case Lumina::Models::Materials::Texture::Usage::Normal:   textures["normal"].push_back(texture); break;
            default: textures["ambient"].push_back(texture);      break;
        }
    }

    // thanks azurerain1
    for (auto& slot : textures)
    {
        if (/*slot.second.size() > 1*/ false)
        {
            auto layered_texture = FbxLayeredTexture::Create(scene, (std_material_name + "_layered_texture_group").c_str());
            for (int i = 0; i < slot.second.size(); ++i)
            {
                layered_texture->ConnectSrcObject(slot.second[i]);
                layered_texture->SetTextureBlendMode(i, FbxLayeredTexture::EBlendMode::eAdditive);
                layered_texture->SetTextureAlpha(i, 0.5);
            }
            fbx_textures[slot.first]->ConnectSrcObject(layered_texture);
        }
        else if (!slot.second.empty())
        {
            fbx_textures[slot.first]->ConnectSrcObject(slot.second[0]);
        }
    }


    material_cache->insert({hash, *out});
    return true;
}

void ZoneExporter::extract_textures(Lumina::Models::Materials::Material^ mat)
{
    // I wish this was a C# function
    for (int i = 0; i < mat->Textures->Length; i++)
    {
        auto tex_path = Util::get_texture_path(out_folder, zone_code, mat->Textures[i]->TexturePath);

        if (System::IO::File::Exists(tex_path) || tex_path->Contains("dummy"))
            continue;

        Lumina::Data::Files::TexFile^ texfile;
        try { texfile = mat->Textures[i]->GetTextureNc(data); } catch (System::Exception^ exception) { continue; }
        
        // end me
        unsigned char* arr = new unsigned char[texfile->ImageData->Length];
        for (int i = 0; i < texfile->ImageData->Length; i++)
            arr[i] = texfile->ImageData[i];

        System::Drawing::Image^ texture = gcnew System::Drawing::Bitmap(texfile->Header.Width,
                                                                        texfile->Header.Height,
                                                                        texfile->Header.Width * 4,
                                                                        System::Drawing::Imaging::PixelFormat::Format32bppArgb,
                                                                        System::IntPtr(arr));
        System::IO::Directory::CreateDirectory(System::IO::Path::GetDirectoryName(tex_path));
        texture->Save(tex_path, System::Drawing::Imaging::ImageFormat::Png);
        delete[] arr;
    }
}

bool ZoneExporter::save_scene()
{
    auto exporter = FbxExporter::Create(manager, "exporter");
    auto out_fbx = out_folder + zone_code + ".fbx";

    if (!exporter->Initialize(out_fbx.c_str(), -1, manager->GetIOSettings()))
        return false;

    const auto result = exporter->Export(scene);
    auto test = exporter->GetStatus();
    exporter->Destroy();
    return result;
}

bool ZoneExporter::init(System::String^ game_path)
{
    data = gcnew Lumina::GameData(game_path, gcnew Lumina::LuminaOptions());
    data->Options->PanicOnSheetChecksumMismatch = false; // probably haraam
    eobj_sgb_paths = gcnew System::Collections::Generic::Dictionary<System::UInt32, System::String^>();
    eobj_names = gcnew System::Collections::Generic::Dictionary<System::UInt32, System::String^>();

    auto name = zone_path.substr(zone_path.rfind("/level") - 4, 4);

    manager = FbxManager::Create();
    if (!manager)
        return false;

    scene = FbxScene::Create(manager, name.c_str());
    if (!scene)
        return false;

    FbxIOSettings* io = FbxIOSettings::Create(manager, "IOSRoot");
    manager->SetIOSettings(io);

    (*manager->GetIOSettings()).SetBoolProp(EXP_FBX_MATERIAL, true);
    (*manager->GetIOSettings()).SetBoolProp(EXP_FBX_TEXTURE, true);
    (*manager->GetIOSettings()).SetBoolProp(EXP_FBX_EMBEDDED, false);
    (*manager->GetIOSettings()).SetBoolProp(EXP_FBX_SHAPE, true);
    (*manager->GetIOSettings()).SetBoolProp(EXP_FBX_GOBO, true);
    (*manager->GetIOSettings()).SetBoolProp(EXP_FBX_ANIMATION, false);
    (*manager->GetIOSettings()).SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);
    
    scene->GetGlobalSettings().SetSystemUnit(FbxSystemUnit::m);
    return true;
}

void ZoneExporter::uninit()
{
    data = nullptr;
    if (manager)
        manager->Destroy();
    zone_path = "";
    out_folder = "";
    zone_code = "";
    delete material_cache;
    delete mesh_cache;
}

ZoneExporter::ZoneExporter()
{
    // We don't want to restrict a ZoneExporter object to a single export,
    // so any initialization code is contained in export_zone
}

ZoneExporter::~ZoneExporter()
{
    uninit();
}
