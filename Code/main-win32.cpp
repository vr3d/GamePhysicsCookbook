// Undef this if you don't want to use GLEW. 
// Only GL 1.1 will be usable without it!
// #define USE_GLEW - DEFINED IN PROJECT!

// If you don't want a console window define this

// Only change if you know what you are doing
#define MAIN_WIN_32_WINDOW_CLASS L"Win32GLSandbox"
#define ENABLE_RESIZE

// Should not need to edit below this line
// -------------------------------------------------- \\

#define WIN32_LEAN_AND_MEAN 
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <windowsx.h>

#ifdef USE_GLEW
#include <GL\glew.h>
#include <GL\wglew.h>
#endif
#include <gl\GL.h>

#include <string>
#include <iostream>
#include <cstdio>
#include "IWindow.h"
#include "matrices.h"

// I set all of these settings in the "Project Settings" of visual studio
// #pragma comment(lib, "opengl32.lib") 
// #pragma comment(lib, "glew32.lib") 
// #pragma comment(linker,"/SUBSYSTEM:CONSOLE")
// #pragma comment(linker,"/SUBSYSTEM:WINDOWS")

std::wstring MAIN_WIN32_WINDOW_NAME;
RECT windowRect;
RECT clientRect;
RECT borderRect;

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow);
bool CheckIfAlreadyRunning();
HGLRC OpenGLBindContext(HDC hdc);
void OpenGLUnbindContext(HWND hwnd, HDC hdc, HGLRC hglrc);
void UpdateFullscreen(IWindow* pWindowInstance, HWND hwnd, HDC hdc);
void SetDisplayMode(int width, int height, int bpp, int refreshRate);
int WParamToKeydef(WPARAM param, bool shiftDown);
double GetMilliseconds();

#define NO_RESIZE_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define NORMAL_STYLE (WS_VISIBLE | WS_OVERLAPPEDWINDOW)

// TODO: How do i turn on CRT memleak detection?!?!?!

// https://www.opengl.org/archives/resources/code/samples/win32_tutorial/
// https://msdn.microsoft.com/en-us/library/bb384843.aspx?f=255&MSPPError=-2147217396
// https://gist.github.com/gszauer/5718416
// http://glew.sourceforge.net/basic.html

int main(int argc, const char** argv) {
	IWindow* pWindowInstance = IWindow::GetInstance();

	pWindowInstance->GetAndResetTitleDirtyFlag();
	std::string title(pWindowInstance->GetTitle());
	MAIN_WIN32_WINDOW_NAME = std::wstring(title.begin(), title.end());

	if (CheckIfAlreadyRunning()) {
		std::cout << "Error, only one instance of application is allowed to run\n";
	}
	else {
		WinMain(GetModuleHandle(NULL), NULL, GetCommandLineA(), SW_SHOWDEFAULT);
	}

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow) {
	IWindow* pWindowInstance = IWindow::GetInstance();
	{ // In case we start with a non-console linker
		pWindowInstance->GetAndResetTitleDirtyFlag();
		std::string title(pWindowInstance->GetTitle());
		MAIN_WIN32_WINDOW_NAME = std::wstring(title.begin(), title.end());

		if (CheckIfAlreadyRunning()) {
			MessageBox(NULL, L"Only one instance of application is allowed to run", NULL, NULL);
			return FALSE;
		}
	}

	int width = pWindowInstance->GetWidth();
	int height = pWindowInstance->GetHeight();

	HINSTANCE hinstance = hInstance;

	WNDCLASSEX wndclass;
	ZeroMemory(&wndclass, sizeof(WNDCLASSEX));
	wndclass.cbSize = sizeof(WNDCLASSEX);
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = hInstance;
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wndclass.lpszMenuName = 0;
	wndclass.lpszClassName = MAIN_WIN_32_WINDOW_CLASS;
	RegisterClassEx(&wndclass);

	SetRect(&windowRect, (GetSystemMetrics(SM_CXSCREEN) / 2) - (width / 2), 
						 (GetSystemMetrics(SM_CYSCREEN) / 2) - (height / 2), 
						 (GetSystemMetrics(SM_CXSCREEN) / 2) + (width / 2), 
						 (GetSystemMetrics(SM_CYSCREEN) / 2) + (height / 2));
	AdjustWindowRectEx(&windowRect, 
#ifdef ENABLE_RESIZE
		NORMAL_STYLE
#else
		NO_RESIZE_STYLE
#endif
		, FALSE, 0);

	HWND hwnd = CreateWindowEx(0, MAIN_WIN_32_WINDOW_CLASS, MAIN_WIN32_WINDOW_NAME.c_str(),
#ifdef ENABLE_RESIZE
		NORMAL_STYLE
#else
		NO_RESIZE_STYLE
#endif
		, windowRect.left, windowRect.top,
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, 
		NULL, NULL, hInstance, szCmdLine);

	HDC hdc = GetDC(hwnd);
	HGLRC hglrc = OpenGLBindContext(hdc);

#ifdef USE_GLEW
	if (pWindowInstance->HasInt("glMajor")) {
		if (pWindowInstance->HasInt("glMinor")) {
			int major = pWindowInstance->GetInt("glMajor", 1);
			int minor = pWindowInstance->GetInt("glMinor", 1);

			std::cout << "Requesting OpenGL " << major << "." << minor << " context\n";

			GLenum err = glewInit();
			if (GLEW_OK != err) {
				std::cout << "Error initializing glew: " << glewGetErrorString(err) << "\n";
			}
			else {
				char version_string[256];
				memset(version_string, 0, sizeof(char) * 256);
				sprintf(version_string, "GL_VERSION_%d_%d", major, minor);

				if (!glewIsSupported(version_string)) {
					std::cout << "GLEW, Requested version is not supported: " << version_string << "\n";
				}
				else {
					int attribs[] =
					{
						WGL_CONTEXT_MAJOR_VERSION_ARB, major,
						WGL_CONTEXT_MINOR_VERSION_ARB, minor,
						WGL_CONTEXT_FLAGS_ARB, 0,
						0
					};

					// Needed to create 3X or higher context
					if (wglewIsSupported("WGL_ARB_create_context") == 1) {
						HGLRC glewrc = wglCreateContextAttribsARB(hdc, 0, attribs);

						wglMakeCurrent(NULL, NULL);
						wglDeleteContext(hglrc);

						wglMakeCurrent(hdc, glewrc);
						hglrc = glewrc;
					}
					else {
						std::cout << "WGL_ARB_create_context is not supported\n";
					}
				}
			}
		}
	}

	glGetError(); // Clear error flag
#endif
	int OpenGLVersion[2] = { 1, 1 };
#ifdef USE_GLEW
	glGetIntegerv(GL_MAJOR_VERSION, &OpenGLVersion[0]);
	if (glGetError() == GL_INVALID_ENUM) { // Only works with 3X
#endif
		// Provide a 2x & below fallback!
		char major_string[2] = { glGetString(GL_VERSION)[0], '\0' };
		OpenGLVersion[0] = atoi(major_string);
		char minor_string[2] = { glGetString(GL_VERSION)[2], '\0' };
		OpenGLVersion[1] = atoi(minor_string);
#ifdef USE_GLEW

	}
	else {
		glGetIntegerv(GL_MINOR_VERSION, &OpenGLVersion[1]);
	}
#endif
	pWindowInstance->SetInt("glMajor", OpenGLVersion[0]);
	pWindowInstance->SetInt("glMinor", OpenGLVersion[1]);
	std::cout << "OpenGL context: " << OpenGLVersion[0] << ", " << OpenGLVersion[1] << "\n";
	std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";
#ifdef USE_GLEW
	std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
#endif
	std::cout << "OpenGL vendor: " << glGetString(GL_VENDOR) << "\n";

	pWindowInstance->OnInitialize();

	bool fullscreen = pWindowInstance->GetFullScreen();
	if (fullscreen) {
		UpdateFullscreen(pWindowInstance, hwnd, hdc);
	}
	else {
		pWindowInstance->OnResize(width, height);
	}
	
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	GetClientRect(hwnd, &clientRect);
	GetWindowRect(hwnd, &windowRect);
	GetWindowRect(hwnd, &borderRect);

	MSG msg;
	DWORD next_game_tick = GetTickCount();
	int sleep_time = 0;
	double lastTime = GetMilliseconds();
	double fixed_millis = pWindowInstance->GetFixedFPS() / 1000.0; 
	double fixed_ellapsed = 0.0;

	while (!pWindowInstance->GetQuitFlag()) {
		// If or while? Not sure if all messages should process at once or not
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// Potentially change title
		if (pWindowInstance->GetAndResetTitleDirtyFlag()) {
			std::string title(pWindowInstance->GetTitle());
			MAIN_WIN32_WINDOW_NAME = std::wstring(title.begin(), title.end());
			SetWindowText(hwnd, MAIN_WIN32_WINDOW_NAME.c_str());
		}

		// Potentially toggle full-screen
		if (fullscreen != pWindowInstance->GetFullScreen()) {
			UpdateFullscreen(pWindowInstance, hwnd, hdc);
			fullscreen = pWindowInstance->GetFullScreen();
		}

		if (!fullscreen) {
			int windowWidth = borderRect.right - borderRect.left;
			int windowHeight = borderRect.bottom - borderRect.top;
			int clientWidth = clientRect.right - clientRect.left;
			int clientHeight = clientRect.bottom - clientRect.top;
			int borderWidth = windowWidth - clientWidth;
			int borderHeight = windowHeight - clientHeight;

			if (clientWidth != pWindowInstance->GetWidth() || clientHeight != pWindowInstance->GetHeight()) {
				SetWindowPos(hwnd, 0, windowRect.left, windowRect.top, pWindowInstance->GetWidth() + borderWidth, pWindowInstance->GetHeight() + borderHeight, SWP_NOZORDER);
			}
		}

		double time = GetMilliseconds();
		float deltaTime = float(time - lastTime) * 0.001f;
		lastTime = time;

		pWindowInstance->OnUpdate(deltaTime);

		fixed_ellapsed += deltaTime;
		while (fixed_ellapsed > fixed_millis) {
			pWindowInstance->OnFixedUpdate(fixed_millis);
			fixed_ellapsed -= fixed_millis;
		}

		pWindowInstance->OnRender();
		SwapBuffers(hdc);

		// Regulate FPS
		int SKIP_TICKS = 1000 / pWindowInstance->GetTargetFPS();
		next_game_tick += SKIP_TICKS;
		sleep_time = next_game_tick - GetTickCount();
		if (sleep_time >= 0) {
			Sleep(sleep_time);
		}
	}

	pWindowInstance->OnShutdown();
	OpenGLUnbindContext(hwnd, hdc, hglrc);

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
	IWindow* pWindowInstance = IWindow::GetInstance();
	int width, height;

	static bool shiftDown = false;
	static bool capsOn = false;

	switch (iMsg) {
	// Window Lifecycle events
	case WM_CLOSE:
		DestroyWindow(hwnd);
		pWindowInstance->Close();
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	// Handle resize event
	case WM_SIZE:
		width = LOWORD(lParam);
		height = HIWORD(lParam);
		pWindowInstance->OnResize(width, height);

		GetClientRect(hwnd, &clientRect);
		GetWindowRect(hwnd, &borderRect);
		pWindowInstance->Resize(width, height);

		break;
	// Handle Mouse Events
	case WM_MOUSEMOVE:
		pWindowInstance->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_LBUTTONDOWN:
		pWindowInstance->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		pWindowInstance->OnMouseDown(MOUSE_LEFT);
		break;
	case WM_LBUTTONUP:
		pWindowInstance->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		pWindowInstance->OnMouseUp(MOUSE_LEFT);
		break;
	case WM_RBUTTONDOWN:
		pWindowInstance->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		pWindowInstance->OnMouseDown(MOUSE_RIGHT);
		break;
	case WM_RBUTTONUP:
		pWindowInstance->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		pWindowInstance->OnMouseUp(MOUSE_RIGHT);
		break;
	case WM_MBUTTONDOWN:
		pWindowInstance->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		pWindowInstance->OnMouseDown(MOUSE_MIDDLE);
		break;
	case WM_MBUTTONUP:
		pWindowInstance->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		pWindowInstance->OnMouseUp(MOUSE_MIDDLE);
		break;
	// Handle keyboard events
	case WM_SYSKEYUP:
		break;
	case WM_SYSKEYDOWN:
		break;
	case WM_KEYDOWN:
		if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT)
			shiftDown = true;

		if (wParam == VK_CAPITAL)
			capsOn = !capsOn;

		pWindowInstance->OnKeyDown(WParamToKeydef(wParam, shiftDown ^ capsOn));
		break;
	case WM_KEYUP:
		if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT)
			shiftDown = false;

		pWindowInstance->OnKeyUp(WParamToKeydef(wParam, shiftDown ^ capsOn));
		break;
	}
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

bool CheckIfAlreadyRunning() {
	HWND hWnd = FindWindow(MAIN_WIN_32_WINDOW_CLASS, MAIN_WIN32_WINDOW_NAME.c_str());

	if (hWnd) {
		if (IsIconic(hWnd)) {
			ShowWindow(hWnd, SW_RESTORE);
		}
		SetForegroundWindow(hWnd);
		return true;
	}

	return false;
}

HGLRC OpenGLBindContext(HDC hdc) {
	PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));

	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 32;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pixelFormat = ChoosePixelFormat(hdc, &pfd);
	SetPixelFormat(hdc, pixelFormat, &pfd);

	HGLRC context = wglCreateContext(hdc);
	wglMakeCurrent(hdc, context);
	return context;
}

void OpenGLUnbindContext(HWND hwnd, HDC hdc, HGLRC hglrc) {
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hglrc);
	ReleaseDC(hwnd, hdc);
}

void UpdateFullscreen(IWindow* pWindowInstance, HWND hwnd, HDC hdc) {
	if (pWindowInstance->GetFullScreen()) {
		GetWindowRect(hwnd, &windowRect);
		SetDisplayMode(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), GetDeviceCaps(hdc, BITSPIXEL), GetDeviceCaps(hdc, VREFRESH));
		SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP);
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
		// Not 100% sure why, but setting height to GetSystemMetrics(SM_CYSCREEN) causes flickering!
		// Adding 1 to it seems to solve it. Maybe something to do with running in a scaled WM???
		SetWindowPos(hwnd, 0, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) + 1, SWP_NOZORDER);
		ShowCursor(FALSE);
	}
	else {
		SetDisplayMode(0, 0, 0, 0);
		SetWindowLongPtr(hwnd, GWL_STYLE, 
#ifdef ENABLE_RESIZE
			NORMAL_STYLE
#else
			NO_RESIZE_STYLE
#endif	
		);
		SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
		int iWindowWidth = windowRect.right - windowRect.left;
		int iWindowHeight = windowRect.bottom - windowRect.top;
		SetWindowPos(hwnd, 0, windowRect.left, windowRect.top, iWindowWidth, iWindowHeight, SWP_NOZORDER);
		ShowCursor(TRUE);
	}

	GetClientRect(hwnd, &clientRect);
	GetWindowRect(hwnd, &borderRect);
}

void SetDisplayMode(int width, int height, int bpp, int refreshRate) {
	if (width == 0 && height == 0 && bpp == 0 && refreshRate == 0) {
		ChangeDisplaySettings(NULL, 0);
		return;
	}

	DEVMODE dm;
	ZeroMemory(&dm, sizeof(DEVMODE));
	dm.dmSize = sizeof(DEVMODE);

	int i = 0;
	while (EnumDisplaySettings(NULL, i++, &dm)) {
		if (dm.dmPelsWidth == width && dm.dmPelsHeight == height &&
			dm.dmBitsPerPel == bpp && dm.dmDisplayFrequency == refreshRate) {
			if (ChangeDisplaySettings(&dm, CDS_TEST) == DISP_CHANGE_SUCCESSFUL) {
				ChangeDisplaySettings(&dm, CDS_FULLSCREEN);
				return;
			}
		}
	}
}

#define REMAP(VIR_KEY, NEW_KEY) \
	case VIR_KEY: return NEW_KEY
#define SHIFTMAP(VIR_KEY, SHIFT_DOWN, SHIFT_UP) \
	case VIR_KEY: return shiftDown? SHIFT_DOWN :  SHIFT_UP

int WParamToKeydef(WPARAM param, bool shiftDown) {
	switch (param)
	{
		REMAP(VK_PAUSE, KEY_PAUSE);
		REMAP(VK_SCROLL, KEY_SCROLL_LOCK);
		REMAP(VK_PRINT, KEY_PRINT);
		REMAP(VK_ESCAPE, KEY_ESCAPE);
		case VK_BACK: 
		case VK_F16: // CTRL + BACKSPACE
			return KEY_BACKSPACE;
		SHIFTMAP(VK_TAB, KEY_BACK_TAB, KEY_TAB);
		REMAP(VK_RETURN, KEY_RETURN);
		REMAP(VK_CAPITAL, KEY_CAPS_LOCK);
		REMAP(VK_SHIFT, KEY_SHIFT);
		REMAP(VK_LSHIFT, KEY_SHIFT);
		REMAP(VK_RSHIFT, KEY_SHIFT);
		REMAP(VK_CONTROL, KEY_CTRL);
		REMAP(VK_RCONTROL, KEY_CTRL);
		REMAP(VK_LCONTROL, KEY_CTRL);
		REMAP(VK_MENU, KEY_ALT);
		REMAP(VK_LMENU, KEY_ALT);
		REMAP(VK_RMENU, KEY_ALT);
		REMAP(VK_APPS, KEY_MENU);
		REMAP(VK_LWIN, KEY_HYPER);
		REMAP(VK_RWIN, KEY_HYPER);
		REMAP(VK_BROWSER_SEARCH, KEY_SEARCH);
		REMAP(VK_INSERT, KEY_INSERT);
		REMAP(VK_HOME, KEY_HOME);
		REMAP(VK_PRIOR, KEY_PG_UP);
		REMAP(VK_DELETE, KEY_DELETE);
		REMAP(VK_END, KEY_END);
		REMAP(VK_NEXT, KEY_PG_DOWN);
		REMAP(VK_LEFT, KEY_LEFT_ARROW);
		REMAP(VK_RIGHT, KEY_RIGHT_ARROW);
		REMAP(VK_UP, KEY_UP_ARROW);
		REMAP(VK_DOWN, KEY_DOWN_ARROW);
		REMAP(VK_NUMLOCK, KEY_NUM_LOCK);
		REMAP(VK_ADD, KEY_KP_PLUS);
		REMAP(VK_SUBTRACT, KEY_KP_MINUS);
		REMAP(VK_MULTIPLY, KEY_KP_MULTIPLY);
		REMAP(VK_DIVIDE, KEY_KP_DIVIDE);
		REMAP(VK_NUMPAD0, KEY_KP_INSERT);
		REMAP(VK_NUMPAD1, KEY_KP_END);
		REMAP(VK_NUMPAD2, KEY_KP_DOWN);
		REMAP(VK_NUMPAD3, KEY_KP_PG_DOWN);
		REMAP(VK_NUMPAD4, KEY_KP_LEFT);
		REMAP(VK_NUMPAD5, KEY_KP_FIVE);
		REMAP(VK_NUMPAD6, KEY_KP_RIGHT);
		REMAP(VK_NUMPAD7, KEY_KP_HOME);
		REMAP(VK_NUMPAD8, KEY_KP_UP);
		REMAP(VK_NUMPAD9, KEY_KP_PG_UP);
		REMAP(VK_DECIMAL, KEY_KP_DELETE);
		REMAP(VK_F1, KEY_F1);
		REMAP(VK_F2, KEY_F2);
		REMAP(VK_F3, KEY_F3);
		REMAP(VK_F4, KEY_F3);
		REMAP(VK_F5, KEY_F3);
		REMAP(VK_F6, KEY_F3);
		REMAP(VK_F7, KEY_F3);
		REMAP(VK_F8, KEY_F4);
		REMAP(VK_F9, KEY_F5);
		REMAP(VK_F10, KEY_F10);
		REMAP(VK_F11, KEY_F11);
		REMAP(VK_F12, KEY_F12);
		REMAP(VK_SPACE, KEY_SPACE);
		SHIFTMAP(0x30, KEY_RIGHT_PARENTHESIS, KEY_ZERO);
		SHIFTMAP(0x31, KEY_EXCLAM, KEY_ONE);
		SHIFTMAP(0x32, KEY_AT, KEY_TWO);
		SHIFTMAP(0x33, KEY_NUMBER, KEY_THREE);
		SHIFTMAP(0x34, KEY_DOLLAR, KEY_FOUR);
		SHIFTMAP(0x35, KEY_PERCENT, KEY_FIVE);
		SHIFTMAP(0x36, KEY_CIRCUMFLEX, KEY_SIX);
		SHIFTMAP(0x37, KEY_AMPERSAND, KEY_SEVEN);
		SHIFTMAP(0x38, KEY_ASTERISK, KEY_EIGHT);
		SHIFTMAP(0x39, KEY_LEFT_PARENTHESIS, KEY_NINE);
		SHIFTMAP(VK_OEM_PLUS, KEY_EQUAL, KEY_PLUS);
		SHIFTMAP(VK_OEM_COMMA, KEY_LESS_THAN, KEY_COMMA);
		SHIFTMAP(VK_OEM_MINUS, KEY_UNDERSCORE, KEY_MINUS);
		SHIFTMAP(VK_OEM_PERIOD, KEY_GREATER_THAN, KEY_PERIOD);
		SHIFTMAP(VK_OEM_1, KEY_COLON, KEY_SEMICOLON);
		SHIFTMAP(VK_OEM_2, KEY_QUESTION, KEY_SLASH);
		SHIFTMAP(VK_OEM_3, KEY_TILDE, KEY_GRAVE);
		SHIFTMAP(VK_OEM_4, KEY_LEFT_BRACE, KEY_LEFT_BRACKET);
		SHIFTMAP(VK_OEM_5, KEY_BAR, KEY_BACK_SLASH);
		SHIFTMAP(VK_OEM_6, KEY_RIGHT_BRACE, KEY_RIGHT_BRACKET);
		SHIFTMAP(VK_OEM_7, KEY_QUOTE, KEY_APOSTROPHE);
		SHIFTMAP(0x41, KEY_CAPITAL_A, KEY_A);
		SHIFTMAP(0x42, KEY_CAPITAL_B, KEY_B);
		SHIFTMAP(0x43, KEY_CAPITAL_C, KEY_C);
		SHIFTMAP(0x44, KEY_CAPITAL_D, KEY_D);
		SHIFTMAP(0x45, KEY_CAPITAL_E, KEY_E);
		SHIFTMAP(0x46, KEY_CAPITAL_F, KEY_F);
		SHIFTMAP(0x47, KEY_CAPITAL_G, KEY_G);
		SHIFTMAP(0x48, KEY_CAPITAL_H, KEY_H);
		SHIFTMAP(0x49, KEY_CAPITAL_I, KEY_I);
		SHIFTMAP(0x4A, KEY_CAPITAL_J, KEY_J);
		SHIFTMAP(0x4B, KEY_CAPITAL_K, KEY_K);
		SHIFTMAP(0x4C, KEY_CAPITAL_L, KEY_L);
		SHIFTMAP(0x4D, KEY_CAPITAL_M, KEY_M);
		SHIFTMAP(0x4E, KEY_CAPITAL_N, KEY_N);
		SHIFTMAP(0x4F, KEY_CAPITAL_O, KEY_O);
		SHIFTMAP(0x50, KEY_CAPITAL_P, KEY_P);
		SHIFTMAP(0x51, KEY_CAPITAL_Q, KEY_Q);
		SHIFTMAP(0x52, KEY_CAPITAL_R, KEY_R);
		SHIFTMAP(0x53, KEY_CAPITAL_S, KEY_S);
		SHIFTMAP(0x54, KEY_CAPITAL_T, KEY_T);
		SHIFTMAP(0x55, KEY_CAPITAL_U, KEY_U);
		SHIFTMAP(0x56, KEY_CAPITAL_V, KEY_V);
		SHIFTMAP(0x57, KEY_CAPITAL_W, KEY_W);
		SHIFTMAP(0x58, KEY_CAPITAL_X, KEY_X);
		SHIFTMAP(0x59, KEY_CAPITAL_Y, KEY_Y);
		SHIFTMAP(0x5A, KEY_CAPITAL_Z, KEY_Z);
	}

	return KEY_NONE;
}


double GetMilliseconds() {
	static LARGE_INTEGER s_frequency;
	static BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
	if (s_use_qpc) {
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (double)((1000LL * now.QuadPart) / s_frequency.QuadPart);
	}
	else {
		return GetTickCount();
	}
}
