#include <iostream>
#include "PboPatcher.hpp"
#include "Updater.hpp"

extern uint64_t GetFileHash(std::filesystem::path file);

#ifndef IS_RESTART_HELPER
int main(int argc, char* argv[]) {

	//PboPatcher patcher;

	//std::filesystem::remove("O:\\dev\\pbofolder\\test\\ace_advanced_ballistics2.pbox");
	//std::filesystem::copy("O:\\dev\\pbofolder\\test\\ace_advanced_ballistics.pbox", "O:\\dev\\pbofolder\\test\\ace_advanced_ballistics2.pbox");
	//
	//{
	//	std::ifstream inputFile("O:\\dev\\pbofolder\\test\\ace_advanced_ballistics.pbox", std::ifstream::in | std::ifstream::binary);
	//
	//	PboReader reader(inputFile);
	//	reader.readHeaders();
	//	patcher.ReadInputFile(&reader);
	//
	//	patcher.AddPatch<PatchDeleteFile>(std::filesystem::path("CfgVehicles.hpp"));
	//	patcher.ProcessPatches();
	//}
	//
	//
	//
	//
	//{
	//
	//	std::fstream outputStream("O:\\dev\\pbofolder\\test\\ace_advanced_ballistics2.pbox", std::fstream::binary | std::fstream::in | std::fstream::out);
	//
	//
	//	patcher.WriteOutputFile(outputStream);
	//}
	//
	//std::ifstream inputFile("O:\\dev\\pbofolder\\test\\ace_advanced_ballistics2.pbox", std::ifstream::in | std::ifstream::binary);
	//PboReader reader(inputFile);
	//reader.readHeaders();

	std::cout << "dllHash " << GetFileHash("x64/Release/PboExplorer.dll") << std::endl;
	std::cout << "updaterHash " << GetFileHash("x64/UpdateHelperExe/PboExplorerUpdateHelper.exe") << std::endl;




	Updater::OnStartup();


	1;

}
#endif