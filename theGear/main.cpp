#include<iostream>
#include<fstream>
#include<exception>
#include<string>
#include<vector>
#include<iterator>

#include<Windows.h>
#include<TlHelp32.h>

int dwLocalPlayer;
int dwEntityList;
int dwForceJump;
int m_dwBoneMatrix;
int m_fFlags;
int dwViewMatrix;
int m_iTeamNum;
int m_iHealth;
int m_vecOrigin;
int m_bDormant;

int dwGlowObjectManager;
int m_iGlowIndex;

const int SCREEN_WIDTH = GetSystemMetrics(SM_CXSCREEN); const int xhairx = SCREEN_WIDTH / 2;
const int SCREEN_HEIGHT = GetSystemMetrics(SM_CYSCREEN); const int xhairy = SCREEN_HEIGHT / 2;

HWND hwnd;
DWORD procId;
HANDLE hProcess;
uintptr_t moduleBase;
HDC hdc;
int closest; //Used in a thread to save CPU usage.
std::vector<int> hexValues;

void setOffsets()
{
	m_dwBoneMatrix = hexValues[0];
	m_fFlags = hexValues[1];
	m_iGlowIndex = hexValues[2];
	m_iHealth = hexValues[3];
	m_iTeamNum = hexValues[4];
	m_vecOrigin = hexValues[5];
	dwEntityList = hexValues[6];
	dwForceJump = hexValues[7];
	dwGlowObjectManager = hexValues[8];
	dwLocalPlayer = hexValues[9];
	dwViewMatrix = hexValues[10];
	m_bDormant = hexValues[11];
}

int offsetX, offsetY;

void clense(std::string& toClense)
{
	for (std::string::iterator i = toClense.begin(); i != toClense.end(); i++)
	{
		if (*i == ';' || *i == ' ')
		{
			toClense.erase(i);
			i = toClense.begin();
			continue;
		}
	}
}

//the offsets that are needed to load
std::vector<std::string> stringsToLoad{"dwForceJump", "m_fFlags", "dwLocalPlayer", "dwEntityList", "dwViewMatrix", "m_dwBoneMatrix", "m_iTeamNum",
"m_iHealth", "m_vecOrigin", "m_bDormant", "dwGlowObjectManager", "m_iGlowIndex"};

//load generated offset file and get the updated hex values
void getNewOffsets()
{
	std::ifstream inputFile("csgo.hpp");
	std::string line;
	std::string stringToLoadUsed;

	//iterate through hpp line by line
	while (std::getline(inputFile, line))
	{
		//iterate through line in file to check for anything in stringsToLoad
		for (std::vector<std::string>::iterator s = stringsToLoad.begin(); s != stringsToLoad.end(); s++)
		{
			size_t pos = line.find(*s);
			//if found a string we need to load
			if (pos != std::string::npos)
			{
				stringToLoadUsed = *s;

				//finding the position in the line where we want to start loading, after the = sign and space
				for (size_t t = pos; t != line.size(); t++)
				{
					if (line[t] == '=')
					{
						pos = t + 1;
					}
				}
				//load the new hex value at the right position
				std::string hexConstruct;
				for (size_t t = pos; t != line.size(); t++)
				{
					hexConstruct += line[t];
				}
				//get rid of anything but the hex value like semicolens
				clense(hexConstruct);
				hexValues.push_back(strtoul(hexConstruct.c_str(), NULL, 16));
				break;
			}
		}
	}
	setOffsets();
}

uintptr_t GetModuleBaseAddress(const char* modName)
{
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	if (hSnap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry)) 
		{
			do 
			{
				if (!strcmp((const char*)modEntry.szModule, modName)) 
				{
					CloseHandle(hSnap);
					return (uintptr_t)modEntry.modBaseAddr;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}
}


struct glowStructEnemy 
{
	float red = 1.f;
	float green = 0.f;
	float blue = 0.f;
	float alpha = 1.f;
	uint8_t padding[8];
	float unknown = 1.f;
	uint8_t padding2[4];
	BYTE renderOccluded = true;
	BYTE renderUnoccluded = false;
	BYTE fullBloom = false;
}glowEnm;

struct glowStructLocal
{
	float red = 0.f;
	float green = 1.f;
	float blue = 0.f;
	float alpha = 1.f;
	uint8_t padding[8];
	float unknown = 1.f;
	uint8_t padding2[4];
	BYTE renderOccluded = true;
	BYTE renderUnoccluded = false;
	BYTE fullBloom = false;
}glowLocal;



template<typename T> T RPM(SIZE_T address)
{
	T buffer;
	ReadProcessMemory(hProcess, (LPCVOID)address, &buffer, sizeof(T), NULL);
	return buffer;
}
template<typename T> void WPM(SIZE_T address, T buffer)
{
    WriteProcessMemory(hProcess, (LPVOID)address, &buffer, sizeof(buffer), NULL);
}

class Vector3
{
public:
	float x, y, z;
	Vector3() : x(0.f), y(0.f), z(0.f) {}
	Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};


int getTeam(uintptr_t player) 
{
	return RPM<int>(player + m_iTeamNum);
}

uintptr_t GetLocalPlayer()
{
	return RPM< uintptr_t>(moduleBase + dwLocalPlayer);
}

uintptr_t GetPlayer(int index) 
{  //Each player has an index. 1-64
	return RPM< uintptr_t>(moduleBase + dwEntityList + index * 0x10); //We multiply the index by 0x10 to select the player we want in the entity list.
}

int GetPlayerHealth(uintptr_t player) 
{
	return RPM<int>(player + m_iHealth);
}

Vector3 PlayerLocation(uintptr_t player) 
{ //Stores XYZ coordinates in a Vector3.
	return RPM<Vector3>(player + m_vecOrigin);
}

bool DormantCheck(uintptr_t player)
{
	return RPM<int>(player + m_bDormant);
}

Vector3 get_head(uintptr_t player) 
{
	struct boneMatrix_t {
		byte pad3[12];
		float x;
		byte pad1[12];
		float y;
		byte pad2[12];
		float z;
	};
	uintptr_t boneBase = RPM<uintptr_t>(player + m_dwBoneMatrix);
	boneMatrix_t boneMatrix = RPM<boneMatrix_t>(boneBase + (sizeof(boneMatrix) * 8 /*8 is the boneid for head*/));
	return Vector3(boneMatrix.x, boneMatrix.y, boneMatrix.z);
}

struct view_matrix_t 
{
	float matrix[16];
} vm;

struct Vector3 WorldToScreen(const struct Vector3 pos, struct view_matrix_t matrix) 
{ //This turns 3D coordinates (ex: XYZ) int 2D coordinates (ex: XY).
	struct Vector3 out;
	float _x = matrix.matrix[0] * pos.x + matrix.matrix[1] * pos.y + matrix.matrix[2] * pos.z + matrix.matrix[3];
	float _y = matrix.matrix[4] * pos.x + matrix.matrix[5] * pos.y + matrix.matrix[6] * pos.z + matrix.matrix[7];
	out.z = matrix.matrix[12] * pos.x + matrix.matrix[13] * pos.y + matrix.matrix[14] * pos.z + matrix.matrix[15];

	_x *= 1.f / out.z;
	_y *= 1.f / out.z;

	out.x = SCREEN_WIDTH * .5f;
	out.y = SCREEN_HEIGHT * .5f;

	out.x += 0.5f * _x * SCREEN_WIDTH + 0.5f;
	out.y -= 0.5f * _y * SCREEN_HEIGHT + 0.5f;

	return out;
}

float pythag(int x1, int y1, int x2, int y2)
{
	return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

int FindClosestEnemy() 
{
	float Finish;
	int ClosestEntity = 1;
	Vector3 Calc = { 0, 0, 0 };
	float Closest = FLT_MAX;
	int localTeam = getTeam(GetLocalPlayer());
	for (int i = 1; i < 64; i++)
	{ //Loops through all the entitys in the index 1-64.
		DWORD Entity = GetPlayer(i);
		int EnmTeam = getTeam(Entity); if (EnmTeam == localTeam) continue;
		int EnmHealth = GetPlayerHealth(Entity); if (EnmHealth < 1 || EnmHealth > 100) continue;
		int Dormant = DormantCheck(Entity); if (Dormant) continue;
		Vector3 headBone = WorldToScreen(get_head(Entity), vm);
		Finish = pythag(headBone.x, headBone.y, xhairx, xhairy);
		if (Finish < Closest) {
			Closest = Finish;
			ClosestEntity = i;
		}
	}

	return ClosestEntity;
}

void FindClosestEnemyThread() 
{
	while (1) 
	{
		closest = FindClosestEnemy();
	}
}
//function to obtain crosshair offsets from user
void resetOffsets()
{
	system("CLS");
	std::cout << "Enter X offset." << std::endl;
	std::cin >> offsetX;
	std::cout << "Enter Y offset." << std::endl;
	std::cin >> offsetY;

	std::ofstream cOffsets("crosshairOffsets.txt");
	cOffsets << offsetX << "\n";
	cOffsets << offsetY;
	cOffsets.close();
}
int main() 
{
	//call hazedumper to get new offsets
	system("haze.exe");

	std::cout << std::endl << "Press the delete key to reset crosshair offsets." << std::endl;
	getNewOffsets();

	hwnd = FindWindowA(NULL, "Counter-Strike: Global Offensive");
	GetWindowThreadProcessId(hwnd, &procId);
	moduleBase = GetModuleBaseAddress("client.dll");
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, NULL, procId);
	hdc = GetDC(hwnd);
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)FindClosestEnemyThread, NULL, NULL, NULL);

	//try to load crosshair offset config, if the file is found use the config of not then ask for offsets
	std::ifstream cOffsets("crosshairOffsets.txt");
	if(!cOffsets)
		resetOffsets();
	else
	{
		std::string tempString;
		while (std::getline(cOffsets, tempString))
		{
			static int i = 0;
			if(i == 0)
				offsetX = std::stoi(tempString);
			else
			{
				offsetY = std::stoi(tempString);
				break;
			}
			i++;
		}
	}
	uintptr_t buffer;
	while (!GetAsyncKeyState(VK_END)) { //press the "end" key to end the hack
		vm = RPM<view_matrix_t>(moduleBase + dwViewMatrix);
		Vector3 closestw2shead = WorldToScreen(get_head(GetPlayer(closest)), vm);
		if (GetAsyncKeyState(VK_MENU /*alt key*/) && closestw2shead.z >= 0.001f /*onscreen check*/)
		{
			SetCursorPos(closestw2shead.x - offsetX, closestw2shead.y - offsetY); //turn off "raw input" in CSGO settings
		}

		//press the delete key to reset crosshair offsets
		if (GetAsyncKeyState(VK_DELETE))
			resetOffsets();

		//if get 'C' key use walls
		if (GetAsyncKeyState(0x43))
		{
			uintptr_t dwGlowManager = RPM<uintptr_t>(moduleBase + dwGlowObjectManager);
			int LocalTeam = RPM<int>(GetLocalPlayer() + m_iTeamNum);
			for (int i = 1; i < 32; i++) {
				uintptr_t dwEntity = RPM<uintptr_t>(moduleBase + dwEntityList + i * 0x10);
				int iGlowIndx = RPM<int>(dwEntity + m_iGlowIndex);
				int EnmHealth = RPM<int>(dwEntity + m_iHealth); if (EnmHealth < 1 || EnmHealth > 100) continue;
				int Dormant = RPM<int>(dwEntity + m_bDormant); if (Dormant) continue;
				int EntityTeam = RPM<int>(dwEntity + m_iTeamNum);

				WPM<glowStructEnemy>(dwGlowManager + (iGlowIndx * 0x38) + 0x4, glowEnm);
			}
		}
		uintptr_t localPlayer = RPM<uintptr_t>(moduleBase + dwEntityList);
		int flags = RPM<int>(localPlayer + m_fFlags);
		if (flags & 1)
			buffer = 5;
		else
			buffer = 4;
		if (GetAsyncKeyState(VK_SPACE) & 0x8000)
		{
			WPM(moduleBase + dwForceJump, buffer);
		}
	}
}
