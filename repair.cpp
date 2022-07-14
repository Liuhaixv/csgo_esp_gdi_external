#include <Windows.h>
#include<iostream>
#include <TlHelp32.h>
#include<string>
#include"signatures.hpp"

/* This fixed buffer was coded by SexOffenderSally, a long time friend! Give him a <3 on discord
Make sure character set is 'Multi-Byte' in project settings! And game must be windowed fullscreen.
Updated offsets: https://github.com/frk1/hazedumper/blob/master/csgo.cs     */

using namespace offsets;

uintptr_t moduleBase;
HANDLE TargetProcess;
HPEN BoxPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
RECT WBounds;
HWND EspHWND;

template<typename T> T RPM(SIZE_T address) {
	T buffer;
	ReadProcessMemory(TargetProcess, (LPCVOID)address, &buffer, sizeof(T), NULL);
	return buffer;
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


void DrawPlayer(HDC hdc, Vector3 foot, Vector3 head, int health, uintptr_t pEnt) {
	float height = head.y - foot.y;
	float width = height / 2.4f;
	SelectObject(hdc, BoxPen);
	Rectangle(hdc, foot.x - (width / 2), foot.y, head.x + (width / 2), head.y);

	std::string health_str = std::string("health:");
	health_str += std::to_string(health);

	bool dormant = RPM<bool>(pEnt + m_bDormant);
	std::string dormant_str = std::string("dormant:");
	dormant_str += dormant ? "未更新位置": "已更新位置";

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


		view_matrix_t vm = RPM<view_matrix_t>(moduleBase + dwViewMatrix);
		int localteam = RPM<int>(RPM<DWORD>(moduleBase + dwEntityList) + m_iTeamNum);

		for (int i = 1; i < 64; i++) {
			uintptr_t pEnt = RPM<DWORD>(moduleBase + dwEntityList + (i * 0x10));
			int team = RPM<int>(pEnt + m_iTeamNum);

			if (team != localteam) {
				int health = RPM<int>(pEnt + m_iHealth);
				Vector3 pos = RPM<Vector3>(pEnt + m_vecOrigin);
				Vector3 head; head.x = pos.x; head.y = pos.y; head.z = pos.z + 72.f;
				Vector3 screenpos = WorldToScreen(pos, vm);
				Vector3 screenhead = WorldToScreen(head, vm);
				float height = screenhead.y - screenpos.y;
				float width = height / 2.4f;

				if (screenpos.z >= 0.01f && health > 0 && health < 101 && !RPM<bool>(pEnt + m_bDormant)) {
					DrawPlayer(Memhdc, screenpos, screenhead, health, pEnt);
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
	moduleBase = GetModuleBaseAddress(pID, std::string_view("client.dll"));

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