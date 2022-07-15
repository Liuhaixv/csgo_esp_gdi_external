#include <Windows.h>
#include<iostream>
#include <TlHelp32.h>
#include<string>
#include<cmath>
#include"signatures.hpp"

#define PI 3.14159265358979323846

/* This fixed buffer was coded by SexOffenderSally, a long time friend! Give him a <3 on discord
Make sure character set is 'Multi-Byte' in project settings! And game must be windowed fullscreen.
Updated offsets: https://github.com/frk1/hazedumper/blob/master/csgo.cs     */

using namespace offsets;

uintptr_t clientBase;
uintptr_t engineBase;
HANDLE TargetProcess;
HPEN BoxPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
RECT WBounds;
HWND EspHWND;

template<typename T> T RPM(SIZE_T address) {
	T buffer;
	ReadProcessMemory(TargetProcess, (LPCVOID)address, &buffer, sizeof(T), NULL);
	return buffer;
}

template <typename var>
bool WPM(DWORD address, var value) {
	return WriteProcessMemory(TargetProcess, (LPVOID)address, &value, sizeof(var), NULL);
}

uintptr_t GetModuleBaseAddress(DWORD tPID, const std::string_view moduleName) {
	HANDLE hmodule = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, tPID);
	MODULEENTRY32 me32;
	me32.dwSize = sizeof(me32);

	do {
		if (!(moduleName.compare(me32.szModule))) {
			CloseHandle(hmodule);
			return (uintptr_t)me32.modBaseAddr;
		}
	} while (Module32Next(hmodule, &me32));

	CloseHandle(hmodule);
	return  NULL;
}

static DWORD GetProcId(const std::string_view targetProcess) {
	DWORD procId = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 procEntry;
		procEntry.dwSize = sizeof(procEntry);
		if (Process32First(hSnap, &procEntry))
		{
			do
			{
				if (!targetProcess.compare(procEntry.szExeFile))
				{
					procId = procEntry.th32ProcessID;
					//std::cout << "found pID:" << procId << std::endl;
					//break;
				}
			} while (Process32Next(hSnap, &procEntry));
		}
	}
	CloseHandle(hSnap);

	return procId;
}

struct Vector3 {
	float x, y, z;
};

struct view_matrix_t {
	float matrix[16];
};

struct Vector3 WorldToScreen(const struct Vector3 pos, struct view_matrix_t matrix) {
	struct Vector3 out;
	float _x = matrix.matrix[0] * pos.x + matrix.matrix[1] * pos.y + matrix.matrix[2] * pos.z + matrix.matrix[3];
	float _y = matrix.matrix[4] * pos.x + matrix.matrix[5] * pos.y + matrix.matrix[6] * pos.z + matrix.matrix[7];
	out.z = matrix.matrix[12] * pos.x + matrix.matrix[13] * pos.y + matrix.matrix[14] * pos.z + matrix.matrix[15];

	_x *= 1.f / out.z;
	_y *= 1.f / out.z;

	int width = WBounds.right - WBounds.left;
	int height = WBounds.bottom + WBounds.left;

	out.x = width * .5f;
	out.y = height * .5f;

	out.x += 0.5f * _x * width + 0.5f;
	out.y -= 0.5f * _y * height + 0.5f;

	return out;
}

//�ռ������нǵļ��㹫ʽ��cos�ȣ�a* b / (| a | *| b | )
float angleOfTwoVector3(Vector3 a, Vector3 b) {
	//TODO
	float ab = a.x * b.x + a.y * b.y + a.z * b.z;
	float ab2 = sqrt(pow(a.x, 2) + pow(a.y, 2) + pow(a.z, 2))
		* sqrt(pow(b.x, 2) + pow(b.y, 2) + pow(b.z, 2));

	return acos(ab / ab2) * 180 / PI;
}

uintptr_t get_local_player() {
	DWORD clientState = RPM<DWORD>(engineBase + offsets::dwClientState);
	DWORD player_id = RPM<DWORD>(clientState + offsets::dwClientState_GetLocalPlayer);
	return  RPM<DWORD>(clientBase + offsets::dwEntityList + player_id * 0x10);
}

Vector3 getViewPosition(uintptr_t playerEnt) {
	Vector3 playerPos = RPM<Vector3>(playerEnt + offsets::m_vecOrigin);
	Vector3 eyeOffset = RPM<Vector3>(playerEnt + offsets::m_vecViewOffset);

	return Vector3(playerPos.x + eyeOffset.x,
		playerPos.y + eyeOffset.y,
		playerPos.z + eyeOffset.z);
}

Vector3 getPosition(uintptr_t playerEnt) {
	return RPM<Vector3>((playerEnt + offsets::m_vecOrigin));
}


void DrawPlayer(HDC hdc, Vector3 foot, Vector3 head, int health, uintptr_t pEnt) {
	float height = head.y - foot.y;
	float width = height / 2.4f;//72.f / 2.4f = 
	SelectObject(hdc, BoxPen);
	Rectangle(hdc, foot.x - (width / 2), foot.y, head.x + (width / 2), head.y);

	std::string health_str = std::string("health:");
	health_str += std::to_string(health);

	bool dormant = RPM<bool>(pEnt + m_bDormant);
	std::string dormant_str = std::string("dormant:");
	dormant_str += dormant ? "δ����λ��": "�Ѹ���λ��";

	auto raw_color = GetTextColor(hdc);

	SetTextColor(hdc, RGB(0, 255, 0));
	TextOutA(hdc, foot.x - (width / 2), foot.y, health_str.c_str(), health_str.size());
	TextOutA(hdc, foot.x - (width / 2), foot.y + 20, dormant_str.c_str(), dormant_str.size());

	SetTextColor(hdc, raw_color);
}

void DrawPlayer(HDC hdc, Vector3 foot, Vector3 head, int health) {
	float height = head.y - foot.y;
	float width = height / 2.4f;
	SelectObject(hdc, BoxPen);
	Rectangle(hdc, foot.x - (width / 2), foot.y, head.x + (width / 2), head.y);

	std::string health_str = std::string("health:");
	health_str += std::to_string(health);


	auto raw_color = GetTextColor(hdc);

	SetTextColor(hdc, RGB(0, 255, 0));
	TextOutA(hdc, foot.x - (width / 2), foot.y, health_str.c_str(), health_str.size());

	SetTextColor(hdc, raw_color);
}

void DrawPlayer(HDC hdc, Vector3 foot, Vector3 head) {
	float height = head.y - foot.y;
	float width = height / 2.4f;
	SelectObject(hdc, BoxPen);
	Rectangle(hdc, foot.x - (width / 2), foot.y, head.x + (width / 2), head.y);
}

void DrawPlayerScreenDistanceToCrosshair(HDC hdc, Vector3 foot, Vector3 head, uintptr_t player) {
	auto raw_color = GetTextColor(hdc);
	SetTextColor(hdc, RGB(0, 255, 0));

	Vector3 crosshair = Vector3(2560/ 2, 1440/ 2,  0);

	//����λ��
	Vector3 player_position = getPosition(player);

	Vector3 player_position_screen = WorldToScreen(player_position, RPM<view_matrix_t>(clientBase + dwViewMatrix));

	float crosshair_distance_to_player = sqrt(pow(player_position_screen.x - crosshair.x, 2) + pow(player_position_screen.y - crosshair.y, 2));
	
	std::string crosshair_distance_to_player_str = std::string("distance_to_crosshair: ") + std::to_string(crosshair_distance_to_player);

	TextOutA(hdc, foot.x, foot.y, crosshair_distance_to_player_str.c_str(), crosshair_distance_to_player_str.size());



	SetTextColor(hdc, raw_color);
}
/*
void DrawPlayerAngleToLocalPlayer(HDC hdc, Vector3 foot, Vector3 head, uintptr_t player) {
	auto raw_color = GetTextColor(hdc);
	SetTextColor(hdc, RGB(0, 255, 0));



	float angle = 0;

	//��������λ��
	Vector3 player_position = getPosition(player);
	Vector3 local_player_view_position = getViewPosition(get_local_player());
	local_player_view_position.z;

	DWORD clst = RPM<DWORD>(engineBase + offsets::dwClientState);
	//Vector3 view_angles = Vector3(0, 90, 0);
	//WPM<Vector3>(clst + offsets::dwClientState_ViewAngles, view_angles);

	Vector3 view_angles = RPM<Vector3>(clst + offsets::dwClientState_ViewAngles);


	//pitch_angle=0ʱ��ҳ���x����������糯��y��������, pitch_angle����ʱ�������y����ת����������ת
	//pitch_angle=0 => direction = (1, 0, 0)

	//x = cos(yaw) * cos(pitch)
	//y = sin(yaw) * cos(pitch)
	//z = sin(pitch)
	Vector3 dirction_of_view = Vector3(cos(view_angles.y * PI / 180) * cos(view_angles.x * PI / 180),
										sin(view_angles.y * PI / 180) * cos(view_angles.x * PI / 180),
										sin(view_angles.x * PI / 180));//TODO
	Vector3 direction_to_enemy = Vector3(player_position.x - local_player_view_position.x,
										player_position.y - local_player_view_position.y,
										player_position.z - local_player_view_position.z);//TODO

	
	std::cout << "direction to enemy:" << direction_to_enemy.x << ", " <<
		direction_to_enemy.y << ", " <<
		direction_to_enemy.z << std::endl;

	std::cout << "dirction_of_view:" << dirction_of_view.x << ", " <<
		dirction_of_view.y << ", " <<
		dirction_of_view.z << std::endl;
	std::cout << std::endl;


	//���������������н�
	//�ռ������нǵļ��㹫ʽ��cos�ȣ�a* b / (| a | *| b | )
	angle = angleOfTwoVector3(dirction_of_view, direction_to_enemy);


	std::string angle_str = ("angle: ");
	std::string yaw_str = ("yaw(y): ") + std::to_string(view_angles.y);
	std::string pitch = ("pitch(x): ") + std::to_string(view_angles.x);

	angle_str += std::to_string(angle);
	TextOutA(hdc, foot.x , foot.y, angle_str.c_str(), angle_str.size());

	TextOutA(hdc, 200, 300, yaw_str.c_str(), yaw_str.size());
	TextOutA(hdc, 200, 330, pitch.c_str(), pitch.size());


	SetTextColor(hdc, raw_color);
}*/

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC Memhdc;
		HDC hdc;
		HBITMAP Membitmap;

		int win_width = WBounds.right - WBounds.left;
		int win_height = WBounds.bottom + WBounds.left;

		hdc = BeginPaint(hwnd, &ps);
		Memhdc = CreateCompatibleDC(hdc);
		Membitmap = CreateCompatibleBitmap(hdc, win_width, win_height);
		SelectObject(Memhdc, Membitmap);
		FillRect(Memhdc, &WBounds, WHITE_BRUSH);


		view_matrix_t vm = RPM<view_matrix_t>(clientBase + dwViewMatrix);
		int localteam = RPM<int>(RPM<DWORD>(clientBase + dwEntityList) + m_iTeamNum);

		for (int i = 1; i < 64; i++) {
			uintptr_t pEnt = RPM<DWORD>(clientBase + dwEntityList + (i * 0x10));
			int team = RPM<int>(pEnt + m_iTeamNum);

			if (team != localteam) {
				int health = RPM<int>(pEnt + m_iHealth);
				Vector3 pos = RPM<Vector3>(pEnt + m_vecOrigin);
				Vector3 head; 
				head.x = pos.x; head.y = pos.y; head.z = pos.z + 72.f;
				Vector3 screenpos = WorldToScreen(pos, vm);
				Vector3 screenhead = WorldToScreen(head, vm);
				float height = screenhead.y - screenpos.y;
				float width = height / 2.4f;

				if (screenpos.z >= 0.01f && health > 0 && health < 101 && !RPM<bool>(pEnt + m_bDormant)) {
					DrawPlayer(Memhdc, screenpos, screenhead, health, pEnt);
					DrawPlayerScreenDistanceToCrosshair(Memhdc, screenpos, screenhead, pEnt);
				}
			}
		}


		BitBlt(hdc, 0, 0, win_width, win_height, Memhdc, 0, 0, SRCCOPY);
		DeleteObject(Membitmap);
		DeleteDC(Memhdc);
		DeleteDC(hdc);
		EndPaint(hwnd, &ps);
		ValidateRect(hwnd, &WBounds);
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

DWORD WorkLoop() {
	while (1) {
		InvalidateRect(EspHWND, &WBounds, true);
		Sleep(1); //16 ms * 60 fps ~ 1000 ms
	}
}

int main() {
	HWND GameHWND = FindWindowA(0, "Counter-Strike: Global Offensive - Direct3D 9");
	GetClientRect(GameHWND, &WBounds);
	auto pID = GetProcId(std::string_view("csgo.exe"));
	TargetProcess = OpenProcess(PROCESS_ALL_ACCESS, NULL, pID);
	clientBase = GetModuleBaseAddress(pID, std::string_view("client.dll"));
	engineBase = GetModuleBaseAddress(pID, std::string_view("engine.dll"));


	WNDCLASSEX WClass;
	MSG Msg;
	WClass.cbSize = sizeof(WNDCLASSEX);
	WClass.style = NULL;
	WClass.lpfnWndProc = WndProc;
	WClass.cbClsExtra = NULL;
	WClass.cbWndExtra = NULL;
	WClass.hInstance = reinterpret_cast<HINSTANCE>(GetWindowLongA(GameHWND, GWL_HINSTANCE));
	WClass.hIcon = NULL;
	WClass.hCursor = NULL;
	WClass.hbrBackground = WHITE_BRUSH;
	WClass.lpszMenuName = " ";
	WClass.lpszClassName = " ";
	WClass.hIconSm = NULL;
	RegisterClassExA(&WClass);

	HINSTANCE Hinstance = NULL;
	EspHWND = CreateWindowExA(WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED, " ", " ", WS_POPUP, WBounds.left, WBounds.top, WBounds.right - WBounds.left, WBounds.bottom + WBounds.left, NULL, NULL, Hinstance, NULL);

	SetLayeredWindowAttributes(EspHWND, RGB(255, 255, 255), 255, LWA_COLORKEY);
	ShowWindow(EspHWND, 1);
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)&WorkLoop, NULL, NULL, NULL);
	while (GetMessageA(&Msg, NULL, NULL, NULL) > 0) {
		TranslateMessage(&Msg);
		DispatchMessageA(&Msg);
		Sleep(1);
	}
	ExitThread(0);
	return 0;
}