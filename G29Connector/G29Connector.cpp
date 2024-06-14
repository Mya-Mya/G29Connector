using namespace std;

#define INITIALIZE_HTTP_RESPONSE( resp, status, reason )    \
    do                                                      \
    {                                                       \
        RtlZeroMemory( (resp), sizeof(*(resp)) );           \
        (resp)->StatusCode = (status);                      \
        (resp)->pReason = (reason);                         \
        (resp)->ReasonLength = (USHORT) strlen(reason);     \
    } while (FALSE)

#define ADD_KNOWN_HEADER(Response, HeaderId, RawValue)               \
    do                                                               \
    {                                                                \
        (Response).Headers.KnownHeaders[(HeaderId)].pRawValue =      \
                                                          (RawValue);\
        (Response).Headers.KnownHeaders[(HeaderId)].RawValueLength = \
            (USHORT) strlen(RawValue);                               \
    } while(FALSE)

#define ALLOC_MEM(cb) HeapAlloc(GetProcessHeap(), 0, (cb))

#define FREE_MEM(ptr) HeapFree(GetProcessHeap(), 0, (ptr))

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <LogitechSteeringWheelLib.h>
#include <fcntl.h>
#include<cstdlib>
#include <iostream>
#include <vector>
#include "CoutUtils.h"
#include <http.h>
#include <thread>
#include<httplib.h>
#include<chrono>
#include<time.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>


#pragma comment(lib, "ws2_32")

//#pragma comment(lib, "httpapi.lib")


const int NUM_SCALAR_INPUTS = 5;
const vector<string> SCALAR_INPUT_KEYS = { "range_rad", "steering_rad",  "throttle", "brake", "timestamp" };
vector<double> currentScalarInputs;
const int NUM_BUTTON_INPUTS = 16;
const vector<string> BUTTON_INPUT_KEYS = { "circle", "triangle", "square", "cross", "R2", "R3", "L2", "L3", "plus", "minus", "share", "options", "playstation", "return", "paddle_right", "paddle_left"};
const vector<int> BUTTON_INPUT_INDEXES = { 2,3,1,0,6,10,7,11,19,20,8,9,24,23,4,5 };
vector<int> currentButtonInputs;
mutex currentG29InputJSONMutex;
string currentG29InputJSON;

long GetCurrentEpochTime() {
	auto now = chrono::system_clock::now();
	auto epochDuration = now.time_since_epoch();
	auto epoch = chrono::duration_cast<chrono::milliseconds>(epochDuration);
	return epoch.count();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
		//case WM_MOVE:

	}
	return DefWindowProc(hWnd, msg, wp, lp);
}

BOOL InitApplication(HINSTANCE hInst) {
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"G29Connector";
	return (RegisterClass(&wc));
}
HWND InitWindow(HINSTANCE hInst) {
	HWND hWnd;
	hWnd = CreateWindowA(
		"G29Connector",
		"G29Connector",
		WS_OVERLAPPEDWINDOW,
		0, 0, 500, 200,
		NULL, NULL, hInst, NULL
	);
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);
	return hWnd;
}
void InitConsole() {
	AllocConsole();
	FILE* stream;
	freopen_s(&stream, "CONIN$", "r", stdin);
	freopen_s(&stream, "CONOUT$", "w", stderr);
	freopen_s(&stream, "CONOUT$", "w", stdout);
}

bool IsG29Connected() {
	return LogiIsModelConnected(0, LOGI_MODEL_G29);
}

const double PI = 3.1415926535;
const double DEG_TO_RAD = PI / 180.0;

void InitG29State() {
	currentScalarInputs.empty();
	for (int i = 0; i < NUM_SCALAR_INPUTS; i++)currentScalarInputs.push_back(0.0);
	currentButtonInputs.empty();
	for (int i = 0; i < NUM_BUTTON_INPUTS; i++)currentButtonInputs.push_back(0);

}

void UpdateCurrentG29InputJSON() {
	string x = "{";
	
	x += "\"scalars\":{";
	for (int i = 0; i < NUM_SCALAR_INPUTS; i++) {
		x += "\"" + SCALAR_INPUT_KEYS[i] + "\":" + to_string(currentScalarInputs[i]);
		x += ",";
	}
	x = x.substr(0, x.length() - 1);
	x += "},";
	x += "\"buttons\":{";
	for (int i = 0; i < NUM_BUTTON_INPUTS; i++) {
		string booleanText = currentButtonInputs[i] == 1 ? "true" : "false";
		x += "\"" + BUTTON_INPUT_KEYS[i] + "\":" + booleanText;
		x += ",";
	}
	x = x.substr(0, x.length() - 1);
	x += "}";

	x += "}";
	currentG29InputJSONMutex.lock();
	currentG29InputJSON = x;
	currentG29InputJSONMutex.unlock();
}

void ShowPressedButtonIdxs() {
	auto s = *LogiGetState(0);
	for (int idx = 0; idx < 128; idx++) {
		if (LogiButtonIsPressed(0, idx))cout << idx << " ";
	}
	cout << endl;
}

void UpdateG29State() {
	auto s = *LogiGetState(0);
	int range_int;
	LogiGetOperatingRange(0, range_int);

	// cout << s.lX << endl;

	double range_rad = ((double)range_int) * DEG_TO_RAD;
	double steering_rad = ((double)s.lX) / 32768.0 * range_rad * 0.5;
	double throttle = 1.0 - ((double)s.lY + 32768.0) / (32768.0 + 32767.0);
	double brake = 1.0 - ((double)s.lRz + 32768.0) / (32768.0 + 32767.0);
	double timestamp = (double)GetCurrentEpochTime();
	currentScalarInputs = { range_rad, steering_rad, throttle, brake, timestamp };

	for (int i = 0; i < NUM_BUTTON_INPUTS; i++) {
		int idx = BUTTON_INPUT_INDEXES[i];
		currentButtonInputs[i] = (int)LogiButtonIsPressed(0, idx);
	}

	UpdateCurrentG29InputJSON();
}

int MatchToInt(ssub_match m) {
	auto content_str = m.str();
	int value = atoi(content_str.c_str());
	return value;
}

void RunServer() {
	using namespace httplib;
	Server svr;
	svr.Get("/", [](const Request& req, Response& res) {
		currentG29InputJSONMutex.lock();
		res.set_content(currentG29InputJSON, "text/json");
		currentG29InputJSONMutex.unlock();
		}
	);
	svr.Get("/collisionForce", [](const Request& req, Response& res) {
		LogiPlayFrontalCollisionForce(0, 20);
		}
	);
	svr.Get(R"(/springForce/(-?\d+)/(\d+)/(\d+))", [](const Request& req, Response& res) {
		int offset = MatchToInt(req.matches[1]);
		int saturation = MatchToInt(req.matches[2]);
		int coef = MatchToInt(req.matches[3]);
		LogiPlaySpringForce(0, offset, saturation, coef);
		}
	);
	svr.Get("/stopSpringForce", [](const Request& req, Response& res) {
		LogiStopSpringForce(0);
		}
	);

	svr.listen("localhost", 3829);
}

void RunWindowLoop() {
	MSG msg;
	while (GetMessageA(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
}

void RunUpdateG29StateLoop() {
	InitG29State();
	
	int i = 0;
	while (true) {
		i++;
		Sleep(50);
		if (LogiUpdate()) {
			//ShowPressedButtonIdxs();
			UpdateG29State();
			cout << currentG29InputJSON << endl;
		}
		else {
			//LogiSteeringInitialize(TRUE);
			cout << "Reconnecting..." << endl;
		}
	}
}

void RunG29Operator(HINSTANCE hInst) {
	if (!InitApplication(hInst)) {
		MessageBoxA(NULL, "Error on InitApplication", "", MB_OK);
		return;
	}
	HWND hWnd = InitWindow(hInst);
	if (!hWnd) {
		MessageBoxA(NULL, "Error on InitWindow", "", MB_OK);
		return;
	}

	cout << "Waiting for LogiSteeringInitialize";
	while (!LogiSteeringInitialize(TRUE)) { Sleep(100); }
	cout << "->OK" << endl;

	cout << "Waiting for IsG29Connected";
	//while (!IsG29Connected()) { Sleep(100); }
	cout << "->OK" << endl;

	LogiUpdate();

	thread threadRunWindowLoop(RunWindowLoop);
	thread threadRunUpdateG29StateLoop(RunUpdateG29StateLoop);

	threadRunWindowLoop.join();
	threadRunUpdateG29StateLoop.join();

	return;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, LPSTR cmdline, int cmdshow) {
	InitConsole();
	thread threadRunG29Operator(RunG29Operator, hInst);
	thread threadRunServer(RunServer);

	threadRunG29Operator.join();
	threadRunServer.join();
	return TRUE;
}