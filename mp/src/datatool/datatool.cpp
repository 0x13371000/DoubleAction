#ifdef _WIN32
#include <Windows.h>
#endif

#include <string>
#include <sstream>
#include <time.h>
#include <vector>
#include <map>

#include "tier0/icommandline.h"
#include "tier0/vcrmode.h"
#include "filesystem.h"
#include "../utils/common/cmdlib.h"

#undef min
#undef max

#include "../datanetworking/math.pb.h"
#include "../datanetworking/data.pb.h"

using std::string;
using std::vector;
using std::map;

int main(int argc, const char** args)
{
	if (argc < 3)
	{
		Msg("Usage: %s [directory] [command]\n", args[0]);
		return 1;
	}

	string sDirectory = args[1];
	string sCommand = args[2];

	if (sDirectory[sDirectory.length()-1] != '\\' && sDirectory[sDirectory.length()-1] != '/')
		sDirectory.push_back('/');

	CommandLine()->CreateCmdLine( Plat_GetCommandLine() );
	CmdLib_InitFileSystem( sDirectory.c_str() );

	string sSearchPath = sDirectory + "*";

	vector<da::protobuf::GameData> apbDatas;

	FileFindHandle_t ffh;
	char const *pszFileName = g_pFullFileSystem->FindFirst( sSearchPath.c_str(), &ffh );
	while (pszFileName)
	{
		if ( pszFileName[0] == '.'  )
		{
			pszFileName = g_pFullFileSystem->FindNext( ffh );
			continue;
		}

		char ext[ 10 ];
		Q_ExtractFileExtension( pszFileName, ext, sizeof( ext ) );

		if ( Q_stricmp( ext, "pb" ) != 0 )
		{
			pszFileName = g_pFullFileSystem->FindNext( ffh );
			continue;
		}

		FileHandle_t fh = g_pFullFileSystem->Open((sDirectory + pszFileName).c_str(), "r");

		if (!fh)
		{
			pszFileName = g_pFullFileSystem->FindNext( ffh );
			continue;
		}

		int iFileSize = g_pFullFileSystem->Size(fh);

		string sBuffer;
		sBuffer.resize(iFileSize);
		g_pFullFileSystem->Read( (void*)sBuffer.data(), iFileSize, fh );

		apbDatas.push_back(da::protobuf::GameData());
		apbDatas.back().ParseFromString(sBuffer);

		pszFileName = g_pFullFileSystem->FindNext( ffh );
	}

	g_pFullFileSystem->FindClose( ffh );

	if (sCommand == "listmaps")
	{
		map<string, int> aMaps;
		for (size_t i = 0; i < apbDatas.size(); i++)
			aMaps[apbDatas[i].map_name()] += apbDatas[i].positions().position_size();

		for (auto it = aMaps.begin(); it != aMaps.end(); it++)
			Msg("%s: %d\n", it->first.c_str(), it->second);
	}

	return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, char* command_line, int show_command)
{
	int argc;
	char** argv;

	char* arg;
	int index;
	int result;

	// count the arguments

	argc = 1;
	arg = command_line;

	while (arg[0] != 0) {

		while (arg[0] != 0 && arg[0] == ' ') {
			arg++;
		}

		if (arg[0] != 0) {

			argc++;

			while (arg[0] != 0 && arg[0] != ' ') {
				arg++;
			}

		}

	}

	// tokenize the arguments

	argv = (char**)malloc(argc * sizeof(char*));

	arg = command_line;
	index = 1;

	while (arg[0] != 0) {

		while (arg[0] != 0 && arg[0] == ' ') {
			arg++;
		}

		if (arg[0] != 0) {

			argv[index] = arg;
			index++;

			while (arg[0] != 0 && arg[0] != ' ') {
				arg++;
			}

			if (arg[0] != 0) {
				arg[0] = 0;
				arg++;
			}

		}

	}

	// put the program name into argv[0]
	char filename[_MAX_PATH];

	GetModuleFileName(NULL, filename, _MAX_PATH);
	argv[0] = filename;

	// call the user specified main function

	result = main(argc, (const char**)argv);

	free(argv);
	return result;
}
#endif
