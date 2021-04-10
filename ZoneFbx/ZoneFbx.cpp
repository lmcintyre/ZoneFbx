#include "pch.h"
#include "ZoneExporter.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

using namespace System;
using namespace Lumina::Models::Models;

int main(array<String^>^ args)
{
	auto usage = "Usage: zonefbx.exe [game_sqpack_path] [zone_path] [output_path]\n"
	               "For example, if you have the default install location, and want to export Central Shroud to your desktop,\n"
	               "zonefbx.exe \"C:\\Program Files (x86)\\SquareEnix\\FINAL FANTASY XIV - A Realm Reborn\\game\\sqpack\""
	               "ffxiv/fst_f1/fld/f1f1/level/f1f1 \"C:\\Users\\Username\\Desktop\\";

	std::printf("%d\n", args->Length);
	if (args->Length != 3)
	{
		std::printf(usage);
		exit(1);
	}

	if (!args[0]->Replace("\\", "")->EndsWith("sqpack"))
	{
		std::printf("Error: Game path must point to the sqpack folder!\n");
		std::printf(usage);
		exit(1);
	}

	if (args[1]->EndsWith("/"))
	{
		std::printf("Error: Level path must not have a trailing slash.\n");
		std::printf(usage);
		exit(1);
	}

	if (args[1]->StartsWith("bg/"))
	{
		std::printf("Error: Level path must not begin with bg/.\n");
		std::printf(usage);
		exit(1);
	}

	if (!IO::Directory::Exists(args[2]))
	{
		std::printf("Error: Export folder must exist.\n");
		std::printf(usage);
		exit(1);
	}

	if (!args[2]->EndsWith("\\"))
	{
		std::printf("Error: Export folder must have a trailing slash.\n");
		std::printf(usage);
		exit(1);
	}
	
	auto exporter = new ZoneExporter();
	exporter->export_zone(args[0], args[1], args[2]);
	
    return 0;
}
