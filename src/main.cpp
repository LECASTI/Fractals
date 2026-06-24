#include <windows.h>
#include <GL/gl.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "palettes.h"
#include "shaders.h"
#include <commctrl.h>

// ==========================================
// CALIBRATED HARDWARE CONSTANTS
// ==========================================
const float TARGET_RESOLUTION_SCALE = 1.00f;
const int MAX_RAYMARCH_STEPS = 100;

// ==========================================
// FRACTAL GEOMETRIC & ITERATION PARAMETERS
// ==========================================
// Default constants
const int   DEFAULT_ITER_MANDELBULB = 8;
const float DEFAULT_POWER_MANDELBULB = 8.0f;
const int   DEFAULT_ITER_JULIA = 10;
const float DEFAULT_C_JULIA[4] = { -0.125f, -0.256f, 0.647f, 0.035f };
const int   DEFAULT_ITER_JERUSALEM = 5;
const float DEFAULT_SCALE_JERUSALEM = 2.5f;
const int   DEFAULT_ITER_SIERPINSKI = 10; // Default increased recursion level to 10
const float DEFAULT_SCALE_SIERPINSKI = 2.0f;
const int   DEFAULT_ITER_MENGER = 5;
const float DEFAULT_SCALE_MENGER = 3.0f;

// Mutable variables for control panel updates
int   g_iterMandelbulb = DEFAULT_ITER_MANDELBULB;
float g_mandelbulbPower = DEFAULT_POWER_MANDELBULB;
int   g_iterJulia = DEFAULT_ITER_JULIA;
float g_juliaC[4] = { DEFAULT_C_JULIA[0], DEFAULT_C_JULIA[1], DEFAULT_C_JULIA[2], DEFAULT_C_JULIA[3] };
int   g_iterJerusalem = DEFAULT_ITER_JERUSALEM;
float g_jerusalemScale = DEFAULT_SCALE_JERUSALEM;
int   g_iterSierpinski = DEFAULT_ITER_SIERPINSKI;
float g_sierpinskiScale = DEFAULT_SCALE_SIERPINSKI;
int   g_iterMenger = DEFAULT_ITER_MENGER;
float g_mengerScale = DEFAULT_SCALE_MENGER;
float g_resolutionScale = TARGET_RESOLUTION_SCALE; // Dynamically adjusted via slider
int   g_maxRaymarchSteps = MAX_RAYMARCH_STEPS;     // Dynamically adjusted via slider

// ==========================================
// CONSTANT PARAMETERS AND CONFIGURATION
// ==========================================
#define EXIT_MOUSE_THRESHOLD 10

const int DEFAULT_FONT_CHAR_COUNT = 96;
const int DEFAULT_FONT_CHAR_START = 32;
const int FONT_HEIGHT = -12;

const float TEXT_COLOR_RED = 0.45f;
const float TEXT_COLOR_GREEN = 0.45f;
const float TEXT_COLOR_BLUE = 0.47f;

const float TIME_FADE_TRANSITION = 30.0f;
const float TIME_FADE_HALF_WINDOW = 1.5f;
const float MONITOR_TIME_OFFSET_SCALE = 37.5f;

const int MAX_FRACTALS = 5;
const int MAX_PALETTES = 50;
const char* FRACTAL_NAMES[MAX_FRACTALS] = {
    "Mandelbulb",
    "Julia Quaternion",
    "Jerusalem Cube",
    "Sierpinski Pyramid",
    "Menger Sponge"
};

// Global control states (declared early)
POINT g_initialMousePos = { -1, -1 };
bool g_shouldExit = false;
bool g_isPreview = false;
bool g_isDebug = false;

// Debug control variables (declared early for use in g_controls)
bool g_paused = false;
float g_timeScale = 1.0f;
int g_overrideFractal = -1;
int g_overridePalette = -1;
int g_useAA = 1;
bool g_hideLegend = false;

enum ControlType {
    CTRL_INT,
    CTRL_FLOAT
};

struct ControlParam {
    const char* labelName;
    ControlType type;
    void* pValue;
    float minVal;
    float maxVal;
    int sliderSteps;
    HWND hwndLabel;
    HWND hwndSlider;
};

// Functions forward declarations for control parameter mapping
int ValueToSliderPos(const ControlParam& cp);
void SliderPosToValue(ControlParam& cp, int pos);
void UpdateControlLabel(const ControlParam& cp);

ControlParam g_controls[] = {
    // Universal
    { "Res Scale", CTRL_FLOAT, &g_resolutionScale, 0.1f, 1.0f, 90, nullptr, nullptr },
    { "Raymarch Steps", CTRL_INT, &g_maxRaymarchSteps, 10.0f, 150.0f, 140, nullptr, nullptr },
    { "Speed Scale", CTRL_FLOAT, &g_timeScale, 0.0f, 5.0f, 100, nullptr, nullptr },

    // Mandelbulb
    { "Mandelbulb Iterations", CTRL_INT, &g_iterMandelbulb, 1.0f, 20.0f, 19, nullptr, nullptr },
    { "Mandelbulb Power", CTRL_FLOAT, &g_mandelbulbPower, 1.0f, 20.0f, 190, nullptr, nullptr },

    // Julia
    { "Julia Iterations", CTRL_INT, &g_iterJulia, 1.0f, 20.0f, 19, nullptr, nullptr },
    { "Julia C.x", CTRL_FLOAT, &g_juliaC[0], -2.0f, 2.0f, 400, nullptr, nullptr },
    { "Julia C.y", CTRL_FLOAT, &g_juliaC[1], -2.0f, 2.0f, 400, nullptr, nullptr },
    { "Julia C.z", CTRL_FLOAT, &g_juliaC[2], -2.0f, 2.0f, 400, nullptr, nullptr },
    { "Julia C.w", CTRL_FLOAT, &g_juliaC[3], -2.0f, 2.0f, 400, nullptr, nullptr },

    // Jerusalem Cube
    { "Jerusalem Iterations", CTRL_INT, &g_iterJerusalem, 1.0f, 8.0f, 7, nullptr, nullptr },
    { "Jerusalem Scale", CTRL_FLOAT, &g_jerusalemScale, 1.5f, 3.5f, 200, nullptr, nullptr },

    // Sierpinski Pyramid
    { "Pyramid Iterations", CTRL_INT, &g_iterSierpinski, 1.0f, 15.0f, 14, nullptr, nullptr },
    { "Pyramid Scale", CTRL_FLOAT, &g_sierpinskiScale, 1.5f, 3.0f, 150, nullptr, nullptr },

    // Menger Sponge
    { "Menger Iterations", CTRL_INT, &g_iterMenger, 1.0f, 8.0f, 7, nullptr, nullptr },
    { "Menger Scale", CTRL_FLOAT, &g_mengerScale, 1.5f, 4.5f, 300, nullptr, nullptr }
};
const int NUM_CONTROLS = sizeof(g_controls) / sizeof(g_controls[0]);

int ValueToSliderPos(const ControlParam& cp) {
    if (cp.type == CTRL_INT) {
        int val = *reinterpret_cast<int*>(cp.pValue);
        return val - static_cast<int>(cp.minVal);
    } else {
        float val = *reinterpret_cast<float*>(cp.pValue);
        float ratio = (val - cp.minVal) / (cp.maxVal - cp.minVal);
        return static_cast<int>(ratio * cp.sliderSteps + 0.5f);
    }
}

void SliderPosToValue(ControlParam& cp, int pos) {
    if (cp.type == CTRL_INT) {
        int val = pos + static_cast<int>(cp.minVal);
        *reinterpret_cast<int*>(cp.pValue) = val;
    } else {
        float ratio = static_cast<float>(pos) / static_cast<float>(cp.sliderSteps);
        float val = cp.minVal + ratio * (cp.maxVal - cp.minVal);
        *reinterpret_cast<float*>(cp.pValue) = val;
    }
}

void UpdateControlLabel(const ControlParam& cp) {
    char buf[256];
    if (cp.type == CTRL_INT) {
        int val = *reinterpret_cast<int*>(cp.pValue);
        std::sprintf(buf, "%s: %d", cp.labelName, val);
    } else {
        float val = *reinterpret_cast<float*>(cp.pValue);
        std::sprintf(buf, "%s: %.3f", cp.labelName, val);
    }
    SetWindowTextA(cp.hwndLabel, buf);
}

#define IDC_RESET_SCENE 4001
#define IDC_RESET_ALL   4002
#define IDC_CHECKBOX_PAUSE     4003
#define IDC_CHECKBOX_AA        4004
#define IDC_COMBOBOX_FRACTAL   4005
#define IDC_COMBOBOX_PALETTE   4006
#define IDC_BUTTON_SAVE_CONFIG 4007

HWND g_hwndResetSceneBtn = nullptr;
HWND g_hwndResetAllBtn = nullptr;
HWND g_hwndSaveConfigBtn = nullptr;
HWND g_hwndPauseCheckbox = nullptr;
HWND g_hwndAACheckbox = nullptr;
HWND g_hwndFractalLabel = nullptr;
HWND g_hwndFractalCombobox = nullptr;
HWND g_hwndPaletteLabel = nullptr;
HWND g_hwndPaletteCombobox = nullptr;
HWND g_hwndDebugWindow = nullptr;
HFONT g_panelFont = nullptr;
bool g_panelVisible = true;
int g_currentActiveFractal = 0;

void ResetSceneVariables(int scene) {
    if (scene == 0) {
        g_iterMandelbulb = DEFAULT_ITER_MANDELBULB;
        g_mandelbulbPower = DEFAULT_POWER_MANDELBULB;
    } else if (scene == 1) {
        g_iterJulia = DEFAULT_ITER_JULIA;
        g_juliaC[0] = DEFAULT_C_JULIA[0];
        g_juliaC[1] = DEFAULT_C_JULIA[1];
        g_juliaC[2] = DEFAULT_C_JULIA[2];
        g_juliaC[3] = DEFAULT_C_JULIA[3];
    } else if (scene == 2) {
        g_iterJerusalem = DEFAULT_ITER_JERUSALEM;
        g_jerusalemScale = DEFAULT_SCALE_JERUSALEM;
    } else if (scene == 3) {
        g_iterSierpinski = DEFAULT_ITER_SIERPINSKI;
        g_sierpinskiScale = DEFAULT_SCALE_SIERPINSKI;
    } else if (scene == 4) {
        g_iterMenger = DEFAULT_ITER_MENGER;
        g_mengerScale = DEFAULT_SCALE_MENGER;
    }
}

void ResetAllVariables() {
    g_resolutionScale = TARGET_RESOLUTION_SCALE;
    g_maxRaymarchSteps = MAX_RAYMARCH_STEPS;
    g_timeScale = 1.0f;
    g_overrideFractal = -1;
    g_overridePalette = -1;
    g_paused = false;
    g_useAA = 1;
    for (int s = 0; s < 5; ++s) {
        ResetSceneVariables(s);
    }
}

void UpdateSlidersAndLabels() {
    for (int i = 0; i < NUM_CONTROLS; ++i) {
        if (g_controls[i].hwndSlider) {
            int pos = ValueToSliderPos(g_controls[i]);
            SendMessageA(g_controls[i].hwndSlider, TBM_SETPOS, TRUE, pos);
            UpdateControlLabel(g_controls[i]);
        }
    }
}

std::string GetConfigFilePath() {
    char path[MAX_PATH];
    DWORD len = GetEnvironmentVariableA("LOCALAPPDATA", path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::string dir = std::string(path) + "\\3DFractalScreensaver";
        CreateDirectoryA(dir.c_str(), nullptr);
        return dir + "\\debug_vars.cfg";
    }
    return "debug_vars.cfg";
}

void SaveConfig() {
    std::string path = GetConfigFilePath();
    std::ofstream cfg(path);
    if (cfg.is_open()) {
        for (int i = 0; i < NUM_CONTROLS; ++i) {
            if (g_controls[i].type == CTRL_INT) {
                cfg << *reinterpret_cast<int*>(g_controls[i].pValue) << "\n";
            } else {
                cfg << *reinterpret_cast<float*>(g_controls[i].pValue) << "\n";
            }
        }
        cfg << g_overrideFractal << "\n";
        cfg << g_overridePalette << "\n";
        cfg << (g_paused ? 1 : 0) << "\n";
        cfg << g_useAA << "\n";
        cfg << (g_hideLegend ? 1 : 0) << "\n";
        cfg.close();
    }
}

void LoadConfig() {
    std::string path = GetConfigFilePath();
    std::ifstream cfg(path);
    if (cfg.is_open()) {
        for (int i = 0; i < NUM_CONTROLS; ++i) {
            if (g_controls[i].type == CTRL_INT) {
                cfg >> *reinterpret_cast<int*>(g_controls[i].pValue);
            } else {
                cfg >> *reinterpret_cast<float*>(g_controls[i].pValue);
            }
        }
        if (cfg >> g_overrideFractal) {
            cfg >> g_overridePalette;
            int pausedVal;
            cfg >> pausedVal;
            g_paused = (pausedVal == 1);
            cfg >> g_useAA;
            int hideLegendVal;
            if (cfg >> hideLegendVal) {
                g_hideLegend = (hideLegendVal == 1);
            }
        }
        cfg.close();
    }
}

void UpdateControlLayout(int activeFractal, bool panelVisible) {
    if (!g_isDebug || !g_hwndDebugWindow) return;
    
    RECT rect;
    GetClientRect(g_hwndDebugWindow, &rect);
    int winW = rect.right - rect.left;
    if (winW <= 0) winW = 1920; // fallback
    
    int startX = winW - 445;
    int y = 10;
    
    int cmdShow = panelVisible ? SW_SHOW : SW_HIDE;

    // 1. Universal (first 3 controls: Res Scale, Raymarch Steps, Speed Scale)
    for (int i = 0; i < 3; ++i) {
        ShowWindow(g_controls[i].hwndLabel, cmdShow);
        ShowWindow(g_controls[i].hwndSlider, cmdShow);
        if (panelVisible) {
            SetWindowPos(g_controls[i].hwndLabel, nullptr, startX, y, 420, 18, SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowPos(g_controls[i].hwndSlider, nullptr, startX, y + 18, 420, 25, SWP_NOZORDER | SWP_NOACTIVATE);
            y += 48;
        }
    }

    // 2. Custom Settings (Scene dropdown, Palette dropdown, Pause checkbox, AA checkbox)
    if (g_hwndFractalLabel) ShowWindow(g_hwndFractalLabel, cmdShow);
    if (g_hwndFractalCombobox) ShowWindow(g_hwndFractalCombobox, cmdShow);
    if (g_hwndPaletteLabel) ShowWindow(g_hwndPaletteLabel, cmdShow);
    if (g_hwndPaletteCombobox) ShowWindow(g_hwndPaletteCombobox, cmdShow);
    if (g_hwndPauseCheckbox) ShowWindow(g_hwndPauseCheckbox, cmdShow);
    if (g_hwndAACheckbox) ShowWindow(g_hwndAACheckbox, cmdShow);

    if (panelVisible) {
        if (g_hwndFractalLabel) SetWindowPos(g_hwndFractalLabel, nullptr, startX, y, 420, 18, SWP_NOZORDER | SWP_NOACTIVATE);
        if (g_hwndFractalCombobox) SetWindowPos(g_hwndFractalCombobox, nullptr, startX, y + 18, 420, 200, SWP_NOZORDER | SWP_NOACTIVATE);
        y += 54;

        if (g_hwndPaletteLabel) SetWindowPos(g_hwndPaletteLabel, nullptr, startX, y, 420, 18, SWP_NOZORDER | SWP_NOACTIVATE);
        if (g_hwndPaletteCombobox) SetWindowPos(g_hwndPaletteCombobox, nullptr, startX, y + 18, 420, 200, SWP_NOZORDER | SWP_NOACTIVATE);
        y += 54;

        if (g_hwndPauseCheckbox) SetWindowPos(g_hwndPauseCheckbox, nullptr, startX, y, 420, 20, SWP_NOZORDER | SWP_NOACTIVATE);
        y += 24;

        if (g_hwndAACheckbox) SetWindowPos(g_hwndAACheckbox, nullptr, startX, y, 420, 20, SWP_NOZORDER | SWP_NOACTIVATE);
        y += 30;
    }
    
    // Scene specific controls
    for (int i = 3; i < NUM_CONTROLS; ++i) {
        bool shouldShow = false;
        if (panelVisible) {
            if (i >= 3 && i <= 4 && activeFractal == 0) shouldShow = true;
            else if (i >= 5 && i <= 9 && activeFractal == 1) shouldShow = true;
            else if (i >= 10 && i <= 11 && activeFractal == 2) shouldShow = true;
            else if (i >= 12 && i <= 13 && activeFractal == 3) shouldShow = true;
            else if (i >= 14 && i <= 15 && activeFractal == 4) shouldShow = true;
        }
        
        int itemShow = shouldShow ? SW_SHOW : SW_HIDE;
        ShowWindow(g_controls[i].hwndLabel, itemShow);
        ShowWindow(g_controls[i].hwndSlider, itemShow);
        
        if (shouldShow) {
            SetWindowPos(g_controls[i].hwndLabel, nullptr, startX, y, 420, 18, SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowPos(g_controls[i].hwndSlider, nullptr, startX, y + 18, 420, 25, SWP_NOZORDER | SWP_NOACTIVATE);
            y += 48;
        }
    }
    
    // Action buttons
    if (g_hwndResetSceneBtn) ShowWindow(g_hwndResetSceneBtn, cmdShow);
    if (g_hwndResetAllBtn) ShowWindow(g_hwndResetAllBtn, cmdShow);
    if (g_hwndSaveConfigBtn) ShowWindow(g_hwndSaveConfigBtn, cmdShow);

    if (panelVisible) {
        SetWindowPos(g_hwndResetSceneBtn, nullptr, startX, y + 10, 130, 30, SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(g_hwndResetAllBtn, nullptr, startX + 145, y + 10, 130, 30, SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(g_hwndSaveConfigBtn, nullptr, startX + 290, y + 10, 130, 30, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

// OpenGL dynamic extension loading defines
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER   0x8B31
#define GL_COMPILE_STATUS  0x8B81
#define GL_LINK_STATUS     0x8B82

typedef char GLchar;
typedef GLuint(WINAPI* PFNGLCREATEPROGRAMPROC)();
typedef GLuint(WINAPI* PFNGLCREATESHADERPROC)(GLenum type);
typedef void(WINAPI* PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
typedef void(WINAPI* PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void(WINAPI* PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void(WINAPI* PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void(WINAPI* PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void(WINAPI* PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);
typedef void(WINAPI* PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void(WINAPI* PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint* params);
typedef void(WINAPI* PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef GLint(WINAPI* PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar* name);
typedef void(WINAPI* PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void(WINAPI* PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void(WINAPI* PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void(WINAPI* PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void(WINAPI* PFNGLUNIFORM1IPROC)(GLint location, GLint v0);

PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
PFNGLCREATESHADERPROC glCreateShader = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
PFNGLATTACHSHADERPROC glAttachShader = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
PFNGLUNIFORM1FPROC glUniform1f = nullptr;
PFNGLUNIFORM2FPROC glUniform2f = nullptr;
PFNGLUNIFORM3FPROC glUniform3f = nullptr;
PFNGLUNIFORM4FPROC glUniform4f = nullptr;
PFNGLUNIFORM1IPROC glUniform1i = nullptr;

bool LoadGLExtensions() {
    #define LOAD_PROC(type, name) \
        name = reinterpret_cast<type>(wglGetProcAddress(#name)); \
        if (!name) return false;

    LOAD_PROC(PFNGLCREATEPROGRAMPROC, glCreateProgram);
    LOAD_PROC(PFNGLCREATESHADERPROC, glCreateShader);
    LOAD_PROC(PFNGLSHADERSOURCEPROC, glShaderSource);
    LOAD_PROC(PFNGLCOMPILESHADERPROC, glCompileShader);
    LOAD_PROC(PFNGLATTACHSHADERPROC, glAttachShader);
    LOAD_PROC(PFNGLLINKPROGRAMPROC, glLinkProgram);
    LOAD_PROC(PFNGLUSEPROGRAMPROC, glUseProgram);
    LOAD_PROC(PFNGLGETSHADERIVPROC, glGetShaderiv);
    LOAD_PROC(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog);
    LOAD_PROC(PFNGLGETPROGRAMIVPROC, glGetProgramiv);
    LOAD_PROC(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog);
    LOAD_PROC(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation);
    LOAD_PROC(PFNGLUNIFORM1FPROC, glUniform1f);
    LOAD_PROC(PFNGLUNIFORM2FPROC, glUniform2f);
    LOAD_PROC(PFNGLUNIFORM3FPROC, glUniform3f);
    LOAD_PROC(PFNGLUNIFORM4FPROC, glUniform4f);
    LOAD_PROC(PFNGLUNIFORM1IPROC, glUniform1i);

    return true;
}

// Global control states already declared early

// Debug control variables

std::ofstream logFile;

void Log(const std::string& msg) {
    if (logFile.is_open()) {
        logFile << msg << std::endl;
        logFile.flush();
    }
}

// Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            if (!g_isPreview) {
                GetCursorPos(&g_initialMousePos);
            }
            if (g_isDebug) {
                g_hwndDebugWindow = hwnd;
            }
            return 0;
        }
        case WM_SIZE: {
            if (g_isDebug && g_hwndDebugWindow) {
                UpdateControlLayout(g_currentActiveFractal, g_panelVisible);
            }
            return 0;
        }
        case WM_ACTIVATE: {
            if (g_isDebug) {
                WORD activeState = LOWORD(wParam);
                if (activeState == WA_INACTIVE) {
                    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                } else {
                    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
            }
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdcStatic, RGB(200, 200, 200)); // light gray text
            SetBkMode(hdcStatic, TRANSPARENT);
            return reinterpret_cast<INT_PTR>(GetStockObject(BLACK_BRUSH));
        }
        case WM_CTLCOLORBTN: {
            HDC hdcButton = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdcButton, RGB(255, 255, 255)); // white text
            SetBkMode(hdcButton, TRANSPARENT);
            return reinterpret_cast<INT_PTR>(GetStockObject(BLACK_BRUSH));
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            if (id == IDC_RESET_SCENE) {
                ResetSceneVariables(g_currentActiveFractal);
                UpdateSlidersAndLabels();
                if (g_hwndPauseCheckbox) SendMessageA(g_hwndPauseCheckbox, BM_SETCHECK, g_paused ? BST_CHECKED : BST_UNCHECKED, 0);
                if (g_hwndAACheckbox) SendMessageA(g_hwndAACheckbox, BM_SETCHECK, g_useAA ? BST_CHECKED : BST_UNCHECKED, 0);
                if (g_hwndFractalCombobox) SendMessageA(g_hwndFractalCombobox, CB_SETCURSEL, (g_overrideFractal != -1) ? g_overrideFractal : g_currentActiveFractal, 0);
                if (g_hwndPaletteCombobox) SendMessageA(g_hwndPaletteCombobox, CB_SETCURSEL, (g_overridePalette != -1) ? g_overridePalette : 0, 0);
                SaveConfig();
            } else if (id == IDC_RESET_ALL) {
                ResetAllVariables();
                UpdateSlidersAndLabels();
                if (g_hwndPauseCheckbox) SendMessageA(g_hwndPauseCheckbox, BM_SETCHECK, g_paused ? BST_CHECKED : BST_UNCHECKED, 0);
                if (g_hwndAACheckbox) SendMessageA(g_hwndAACheckbox, BM_SETCHECK, g_useAA ? BST_CHECKED : BST_UNCHECKED, 0);
                if (g_hwndFractalCombobox) SendMessageA(g_hwndFractalCombobox, CB_SETCURSEL, -1, 0);
                if (g_hwndPaletteCombobox) SendMessageA(g_hwndPaletteCombobox, CB_SETCURSEL, -1, 0);
                SaveConfig();
            } else if (id == IDC_BUTTON_SAVE_CONFIG) {
                SaveConfig();
                MessageBoxA(hwnd, "Configuration saved successfully to local appdata folder!", "3D Fractal Screensaver", MB_OK | MB_ICONINFORMATION);
            } else if (id == IDC_CHECKBOX_PAUSE && code == BN_CLICKED) {
                LRESULT checked = SendMessageA(g_hwndPauseCheckbox, BM_GETCHECK, 0, 0);
                g_paused = (checked == BST_CHECKED);
            } else if (id == IDC_CHECKBOX_AA && code == BN_CLICKED) {
                LRESULT checked = SendMessageA(g_hwndAACheckbox, BM_GETCHECK, 0, 0);
                g_useAA = (checked == BST_CHECKED) ? 1 : 0;
            } else if (id == IDC_COMBOBOX_FRACTAL && code == CBN_SELCHANGE) {
                int idx = SendMessageA(g_hwndFractalCombobox, CB_GETCURSEL, 0, 0);
                if (idx >= 0 && idx < MAX_FRACTALS) {
                    g_overrideFractal = idx;
                    UpdateControlLayout(idx, g_panelVisible);
                }
            } else if (id == IDC_COMBOBOX_PALETTE && code == CBN_SELCHANGE) {
                int idx = SendMessageA(g_hwndPaletteCombobox, CB_GETCURSEL, 0, 0);
                if (idx >= 0 && idx < MAX_PALETTES) {
                    g_overridePalette = idx;
                }
            }
            return 0;
        }
        case WM_HSCROLL: {
            HWND hwndTrackbar = reinterpret_cast<HWND>(lParam);
            if (hwndTrackbar) {
                for (int i = 0; i < NUM_CONTROLS; ++i) {
                    if (g_controls[i].hwndSlider == hwndTrackbar) {
                        int pos = SendMessageA(hwndTrackbar, TBM_GETPOS, 0, 0);
                        SliderPosToValue(g_controls[i], pos);
                        UpdateControlLabel(g_controls[i]);
                        SaveConfig();
                        break;
                    }
                }
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!g_isPreview && !g_isDebug) {
                POINT currentPos;
                GetCursorPos(&currentPos);
                if (g_initialMousePos.x == -1 && g_initialMousePos.y == -1) {
                    g_initialMousePos = currentPos;
                } else {
                    int dx = std::abs(currentPos.x - g_initialMousePos.x);
                    int dy = std::abs(currentPos.y - g_initialMousePos.y);
                    if (dx > EXIT_MOUSE_THRESHOLD || dy > EXIT_MOUSE_THRESHOLD) {
                        Log("Exit triggered by mouse movement: dx=" + std::to_string(dx) + ", dy=" + std::to_string(dy));
                        g_shouldExit = true;
                    }
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            if (!g_isPreview && !g_isDebug) {
                g_shouldExit = true;
            }
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (g_isDebug) {
                if (wParam == VK_ESCAPE) {
                    g_shouldExit = true;
                } else if (wParam == 'P') {
                    g_paused = !g_paused;
                    if (g_hwndPauseCheckbox) SendMessageA(g_hwndPauseCheckbox, BM_SETCHECK, g_paused ? BST_CHECKED : BST_UNCHECKED, 0);
                } else if (wParam == 'F') {
                    if (g_overrideFractal == -1) g_overrideFractal = 0;
                    else g_overrideFractal = (g_overrideFractal + 1) % MAX_FRACTALS;
                    if (g_hwndFractalCombobox) SendMessageA(g_hwndFractalCombobox, CB_SETCURSEL, g_overrideFractal, 0);
                    UpdateControlLayout(g_overrideFractal, g_panelVisible);
                } else if (wParam == 'B') {
                    if (g_overrideFractal == -1) g_overrideFractal = 0;
                    else g_overrideFractal = (g_overrideFractal + MAX_FRACTALS - 1) % MAX_FRACTALS;
                    if (g_hwndFractalCombobox) SendMessageA(g_hwndFractalCombobox, CB_SETCURSEL, g_overrideFractal, 0);
                    UpdateControlLayout(g_overrideFractal, g_panelVisible);
                } else if (wParam == 'C') {
                    if (g_overridePalette == -1) g_overridePalette = 0;
                    else g_overridePalette = (g_overridePalette + 1) % MAX_PALETTES;
                    if (g_hwndPaletteCombobox) SendMessageA(g_hwndPaletteCombobox, CB_SETCURSEL, g_overridePalette, 0);
                } else if (wParam == 'A') {
                    g_useAA = g_useAA == 1 ? 0 : 1;
                    if (g_hwndAACheckbox) SendMessageA(g_hwndAACheckbox, BM_SETCHECK, g_useAA ? BST_CHECKED : BST_UNCHECKED, 0);
                } else if (wParam == 'H') {
                    g_panelVisible = !g_panelVisible;
                    UpdateControlLayout(g_currentActiveFractal, g_panelVisible);
                } else if (wParam == VK_UP) {
                    g_timeScale += 0.2f;
                    if (g_timeScale > 5.0f) g_timeScale = 5.0f;
                    if (g_controls[2].hwndSlider) {
                        int pos = ValueToSliderPos(g_controls[2]);
                        SendMessageA(g_controls[2].hwndSlider, TBM_SETPOS, TRUE, pos);
                        UpdateControlLabel(g_controls[2]);
                    }
                } else if (wParam == VK_DOWN) {
                    g_timeScale -= 0.2f;
                    if (g_timeScale < 0.0f) g_timeScale = 0.0f;
                    if (g_controls[2].hwndSlider) {
                        int pos = ValueToSliderPos(g_controls[2]);
                        SendMessageA(g_controls[2].hwndSlider, TBM_SETPOS, TRUE, pos);
                        UpdateControlLabel(g_controls[2]);
                    }
                }
            } else {
                if (!g_isPreview) {
                    Log("Exit triggered by key press.");
                    g_shouldExit = true;
                }
            }
            return 0;
        }
        case WM_DESTROY: {
            if (g_isDebug) {
                SaveConfig();
                if (g_panelFont) {
                    DeleteObject(g_panelFont);
                    g_panelFont = nullptr;
                }
            }
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// Struct to represent a display and its associated rendering context
struct MonitorWindow {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC hrc = nullptr;
    RECT rect = {};
    GLuint shaderProgram = 0;
    GLuint fontListBase = 0;
    float timeOffset = 0.0f;
};

// Monitor enumeration callback
struct MonitorInfo {
    RECT rect;
    bool isPrimary;
};

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    auto* list = reinterpret_cast<std::vector<MonitorInfo>*>(dwData);
    MONITORINFO info;
    info.cbSize = sizeof(info);
    if (GetMonitorInfoA(hMonitor, &info)) {
        list->push_back({ info.rcMonitor, (info.dwFlags & MONITORINFOF_PRIMARY) != 0 });
    }
    return TRUE;
}

// Compile and link OpenGL shaders
GLuint BuildShader() {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, nullptr);
    glCompileShader(vs);

    GLint ok;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char infoLog[512];
        glGetShaderInfoLog(vs, 512, nullptr, infoLog);
        Log("Vertex shader compilation error: " + std::string(infoLog));
        return 0;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char infoLog[512];
        glGetShaderInfoLog(fs, 512, nullptr, infoLog);
        Log("Fragment shader compilation error: " + std::string(infoLog));
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        Log("Shader link error: " + std::string(infoLog));
        return 0;
    }

    return program;
}

// Initialize Win32 window and OpenGL context
bool InitMonitorWindow(MonitorWindow& mw, HINSTANCE hInstance, RECT rect, HWND parentHwnd = nullptr) {
    mw.rect = rect;
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    DWORD style = WS_POPUP | WS_VISIBLE;
    HWND parent = nullptr;
    if (g_isPreview && parentHwnd) {
        style = WS_CHILD | WS_VISIBLE;
        parent = parentHwnd;
    } else if (g_isDebug) {
        style = WS_POPUP | WS_CLIPCHILDREN | WS_VISIBLE;
    }

    RECT winRect = rect;
    int winW = winRect.right - winRect.left;
    int winH = winRect.bottom - winRect.top;

    mw.hwnd = CreateWindowExA(
        (g_isPreview || g_isDebug) ? 0 : WS_EX_TOPMOST,
        "FractalScreenSaverClass",
        g_isDebug ? "3D Fractal Screen Saver - Debug Controls" : "3D Fractal Screen Saver",
        style,
        winRect.left, winRect.top, winW, winH,
        parent, nullptr, hInstance, nullptr
    );

    if (!mw.hwnd) {
        Log("Failed to create window.");
        return false;
    }

    if (g_isDebug) {
        InitCommonControls();
        g_hwndDebugWindow = mw.hwnd;
        if (!g_panelFont) {
            g_panelFont = CreateFontA(FONT_HEIGHT, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
        }
        for (int i = 0; i < NUM_CONTROLS; ++i) {
            g_controls[i].hwndLabel = CreateWindowExA(
                0, "STATIC", "",
                WS_CHILD | WS_VISIBLE,
                1475, 10, 420, 18,
                mw.hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(2000 + i)),
                hInstance, nullptr
            );

            g_controls[i].hwndSlider = CreateWindowExA(
                0, "msctls_trackbar32", "",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_NOTICKS,
                1475, 28, 420, 25,
                mw.hwnd, reinterpret_cast<HMENU>(static_cast<intptr_t>(3000 + i)),
                hInstance, nullptr
            );

            SendMessageA(g_controls[i].hwndLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_panelFont), TRUE);
            SendMessageA(g_controls[i].hwndSlider, WM_SETFONT, reinterpret_cast<WPARAM>(g_panelFont), TRUE);

            SendMessageA(g_controls[i].hwndSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, g_controls[i].sliderSteps));
            int pos = ValueToSliderPos(g_controls[i]);
            SendMessageA(g_controls[i].hwndSlider, TBM_SETPOS, TRUE, pos);
            UpdateControlLabel(g_controls[i]);
        }

        // --- NEW CONTROLS ---
        g_hwndFractalLabel = CreateWindowExA(
            0, "STATIC", "Fractal Scene Selection:",
            WS_CHILD | WS_VISIBLE,
            1475, 10, 420, 18,
            mw.hwnd, nullptr, hInstance, nullptr
        );
        g_hwndFractalCombobox = CreateWindowExA(
            0, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            1475, 10, 420, 200,
            mw.hwnd, reinterpret_cast<HMENU>(IDC_COMBOBOX_FRACTAL),
            hInstance, nullptr
        );
        for (int i = 0; i < MAX_FRACTALS; ++i) {
            SendMessageA(g_hwndFractalCombobox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(FRACTAL_NAMES[i]));
        }
        int curFrac = (g_overrideFractal != -1) ? g_overrideFractal : 0;
        SendMessageA(g_hwndFractalCombobox, CB_SETCURSEL, curFrac, 0);

        g_hwndPaletteLabel = CreateWindowExA(
            0, "STATIC", "Color Palette Selection:",
            WS_CHILD | WS_VISIBLE,
            1475, 10, 420, 18,
            mw.hwnd, nullptr, hInstance, nullptr
        );
        g_hwndPaletteCombobox = CreateWindowExA(
            0, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            1475, 10, 420, 200,
            mw.hwnd, reinterpret_cast<HMENU>(IDC_COMBOBOX_PALETTE),
            hInstance, nullptr
        );
        for (int i = 0; i < MAX_PALETTES; ++i) {
            Palette p = GetPalette(i);
            char buf[128];
            std::sprintf(buf, "%d: %s", i, p.name);
            SendMessageA(g_hwndPaletteCombobox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(buf));
        }
        int curPal = (g_overridePalette != -1) ? g_overridePalette : 0;
        SendMessageA(g_hwndPaletteCombobox, CB_SETCURSEL, curPal, 0);

        g_hwndPauseCheckbox = CreateWindowExA(
            0, "BUTTON", "Pause Camera Fly-Through",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            1475, 10, 420, 20,
            mw.hwnd, reinterpret_cast<HMENU>(IDC_CHECKBOX_PAUSE),
            hInstance, nullptr
        );
        SendMessageA(g_hwndPauseCheckbox, BM_SETCHECK, g_paused ? BST_CHECKED : BST_UNCHECKED, 0);

        g_hwndAACheckbox = CreateWindowExA(
            0, "BUTTON", "Enable Antialiasing (AA)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            1475, 10, 420, 20,
            mw.hwnd, reinterpret_cast<HMENU>(IDC_CHECKBOX_AA),
            hInstance, nullptr
        );
        SendMessageA(g_hwndAACheckbox, BM_SETCHECK, g_useAA ? BST_CHECKED : BST_UNCHECKED, 0);

        // --- ACTION BUTTONS ---
        g_hwndResetSceneBtn = CreateWindowExA(
            0, "BUTTON", "Reset Scene",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1475, 500, 130, 30,
            mw.hwnd, reinterpret_cast<HMENU>(IDC_RESET_SCENE),
            hInstance, nullptr
        );
        g_hwndResetAllBtn = CreateWindowExA(
            0, "BUTTON", "Reset All",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1615, 500, 130, 30,
            mw.hwnd, reinterpret_cast<HMENU>(IDC_RESET_ALL),
            hInstance, nullptr
        );
        g_hwndSaveConfigBtn = CreateWindowExA(
            0, "BUTTON", "Save Config",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1755, 500, 130, 30,
            mw.hwnd, reinterpret_cast<HMENU>(IDC_BUTTON_SAVE_CONFIG),
            hInstance, nullptr
        );

        SendMessageA(g_hwndFractalLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_panelFont), TRUE);
        SendMessageA(g_hwndFractalCombobox, WM_SETFONT, reinterpret_cast<WPARAM>(g_panelFont), TRUE);
        SendMessageA(g_hwndPaletteLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_panelFont), TRUE);
        SendMessageA(g_hwndPaletteCombobox, WM_SETFONT, reinterpret_cast<WPARAM>(g_panelFont), TRUE);
        SendMessageA(g_hwndPauseCheckbox, WM_SETFONT, reinterpret_cast<WPARAM>(g_panelFont), TRUE);
        SendMessageA(g_hwndAACheckbox, WM_SETFONT, reinterpret_cast<WPARAM>(g_panelFont), TRUE);
        SendMessageA(g_hwndResetSceneBtn, WM_SETFONT, reinterpret_cast<WPARAM>(g_panelFont), TRUE);
        SendMessageA(g_hwndResetAllBtn, WM_SETFONT, reinterpret_cast<WPARAM>(g_panelFont), TRUE);
        SendMessageA(g_hwndSaveConfigBtn, WM_SETFONT, reinterpret_cast<WPARAM>(g_panelFont), TRUE);

        UpdateControlLayout(g_currentActiveFractal, g_panelVisible);
    }

    mw.hdc = GetDC(mw.hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    int pf = ChoosePixelFormat(mw.hdc, &pfd);
    if (!pf || !SetPixelFormat(mw.hdc, pf, &pfd)) {
        Log("Failed to set pixel format.");
        return false;
    }

    mw.hrc = wglCreateContext(mw.hdc);
    if (!mw.hrc) {
        Log("Failed to create WGL context.");
        return false;
    }

    if (!wglMakeCurrent(mw.hdc, mw.hrc)) {
        Log("Failed to bind context.");
        return false;
    }

    static bool extensionsLoaded = false;
    if (!extensionsLoaded) {
        if (!LoadGLExtensions()) {
            Log("Failed to load modern OpenGL functions.");
            return false;
        }
        extensionsLoaded = true;
    }

    mw.shaderProgram = BuildShader();
    if (!mw.shaderProgram) {
        Log("Failed to compile fractal shader program.");
        return false;
    }

    // Initialize Font bitmap lists
    HFONT font = CreateFontA(FONT_HEIGHT, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
    SelectObject(mw.hdc, font);
    mw.fontListBase = glGenLists(DEFAULT_FONT_CHAR_COUNT);
    wglUseFontBitmapsA(mw.hdc, DEFAULT_FONT_CHAR_START, DEFAULT_FONT_CHAR_COUNT, mw.fontListBase);
    DeleteObject(font);

    return true;
}

// Render text at NDC coordinates
void RenderText(float x, float y, const std::string& text, GLuint listBase) {
    glRasterPos2f(x, y);
    glListBase(listBase - DEFAULT_FONT_CHAR_START);
    glCallLists(static_cast<GLsizei>(text.length()), GL_UNSIGNED_BYTE, text.c_str());
}

enum class RunMode {
    ScreenSaver,
    Preview,
    Config
};

std::string GetOptionValue(const std::string& originalCmdLine, const std::string& optName) {
    std::string cmdLower = originalCmdLine;
    for (char& c : cmdLower) c = std::tolower(c);
    
    std::string optNameLower = optName;
    for (char& c : optNameLower) c = std::tolower(c);
    
    std::string prefixes[] = { "--" + optNameLower, "-" + optNameLower, "/" + optNameLower };
    for (const auto& pref : prefixes) {
        size_t pos = cmdLower.find(pref);
        if (pos != std::string::npos) {
            pos += pref.length();
            while (pos < originalCmdLine.length() && (originalCmdLine[pos] == ' ' || originalCmdLine[pos] == ':')) {
                pos++;
            }
            std::string val = "";
            while (pos < originalCmdLine.length() && originalCmdLine[pos] != ' ') {
                if (originalCmdLine[pos] != '"') {
                    val += originalCmdLine[pos];
                }
                pos++;
            }
            return val;
        }
    }
    return "";
}

bool HasOption(const std::string& cmdLine, const std::string& optName) {
    std::string cmdLower = cmdLine;
    for (char& c : cmdLower) c = std::tolower(c);
    
    std::string optNameLower = optName;
    for (char& c : optNameLower) c = std::tolower(c);
    
    return (cmdLower.find("/" + optNameLower) != std::string::npos) ||
           (cmdLower.find("-" + optNameLower) != std::string::npos) ||
           (cmdLower.find("--" + optNameLower) != std::string::npos);
}

void ShowConsoleHelp() {
    AllocConsole();
    FILE* fpOut;
    FILE* fpIn;
    freopen_s(&fpOut, "CONOUT$", "w", stdout);
    freopen_s(&fpIn, "CONIN$", "r", stdin);
    
    std::cout << "======================================================================\n";
    std::cout << "3D Fractal Screensaver - Command Line Help\n";
    std::cout << "======================================================================\n\n";
    std::cout << "Usage:\n";
    std::cout << "  fractals.scr [options] [overrides]\n\n";
    std::cout << "Options:\n";
    std::cout << "  /s, -s, --s              Run screensaver in fullscreen mode (default)\n";
    std::cout << "  /c, -c, --c              Run in configuration/interactive mode\n";
    std::cout << "  /p, -p, --p <HWND>       Run in preview mode parented to a child window of HWND\n";
    std::cout << "  /l, -l, --log, --verbose Force writing diagnostic logs to logs/runtime.log\n";
    std::cout << "  /hl, -hl, --hl, --hide-legend, --hidelegend\n";
    std::cout << "                           Hide visual on-screen stats legend overlay\n";
    std::cout << "  /h, -h, --help, /?, -?   Display this help message\n\n";
    std::cout << "Configuration Import/Export:\n";
    std::cout << "  /load, -load, --load <path>  Import configuration from arbitrary file path\n";
    std::cout << "  /save, -save, --save <path>  Export configuration to arbitrary file path\n\n";
    std::cout << "Parameter Overrides:\n";
    std::cout << "  /f, -f, --f <0-4>        Override starting fractal scene:\n";
    std::cout << "                           0: Mandelbulb, 1: Julia, 2: Jerusalem,\n";
    std::cout << "                           3: Sierpinski, 4: Menger\n";
    std::cout << "  /t, -t, --t <0-49>       Override starting color palette index\n";
    std::cout << "  /a, -a, --a <0|1>        Override adaptive antialiasing (0=off, 1=on)\n";
    std::cout << "  /r, -r, --r <0.1-1.0>    Override resolution scaling factor\n";
    std::cout << "  /st, -st, --st <10-150>  Override maximum raymarching step count\n";
    std::cout << "  /sp, -sp, --sp <0.0-5.0> Override camera speed scale\n\n";
    std::cout << "Press ENTER to exit help...\n";
    
    std::fflush(stdout);
    std::cin.get();
    
    if (fpOut) fclose(fpOut);
    if (fpIn) fclose(fpIn);
    FreeConsole();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::string originalCmd = lpCmdLine;

    std::string cmd = originalCmd;
    for (char& c : cmd) c = std::tolower(c);

    // Help command check
    if (HasOption(cmd, "h") || HasOption(cmd, "?") || cmd.find("help") != std::string::npos) {
        ShowConsoleHelp();
        return 0;
    }

    bool hasC = HasOption(cmd, "c");
    bool hasP = HasOption(cmd, "p");
    bool hasS = HasOption(cmd, "s");

    RunMode runMode = RunMode::ScreenSaver;
    HWND parentHwnd = nullptr;

    if (hasC) {
        runMode = RunMode::Config;
    } else if (hasP) {
        runMode = RunMode::Preview;
        std::string hwndVal = GetOptionValue(originalCmd, "p");
        if (!hwndVal.empty()) {
            parentHwnd = reinterpret_cast<HWND>(std::stoull(hwndVal));
        }
    } else if (hasS) {
        runMode = RunMode::ScreenSaver;
    } else {
        // ponytail: default to config if launched with no arguments (e.g. from Explorer context menu "Configurar")
        std::string trimmed = cmd;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n\"'"));
        trimmed.erase(trimmed.find_last_not_of(" \t\r\n\"'") + 1);
        if (trimmed.empty()) {
            runMode = RunMode::Config;
        } else {
            runMode = RunMode::ScreenSaver;
        }
    }

    // Configure logging conditional on config /C, explicit flag /L, or --verbose
    bool shouldLog = (runMode == RunMode::Config) || HasOption(cmd, "l") || HasOption(cmd, "log") || HasOption(cmd, "verbose");
    if (shouldLog) {
        CreateDirectoryA("logs", nullptr);
        logFile.open("logs/runtime.log");
        Log("Screensaver launched in logging mode. CmdLine: " + originalCmd);
    }

    // Unify config and debug state: Config mode (/C) is interactive debug mode
    g_isDebug = (runMode == RunMode::Config);

    // Both screensaver and config modes load configuration
    LoadConfig();

    // Hide legend flag overrides
    if (HasOption(cmd, "hl") || HasOption(cmd, "hide-legend") || HasOption(cmd, "hidelegend")) {
        g_hideLegend = true;
    }

    // Import configuration coordinates from custom location
    if (HasOption(cmd, "load")) {
        std::string path = GetOptionValue(originalCmd, "load");
        if (!path.empty()) {
            std::ifstream src(path);
            if (src.is_open()) {
                for (int i = 0; i < NUM_CONTROLS; ++i) {
                    if (g_controls[i].type == CTRL_INT) {
                        src >> *reinterpret_cast<int*>(g_controls[i].pValue);
                    } else {
                        src >> *reinterpret_cast<float*>(g_controls[i].pValue);
                    }
                }
                if (src >> g_overrideFractal) {
                    src >> g_overridePalette;
                    int pausedVal;
                    src >> pausedVal;
                    g_paused = (pausedVal == 1);
                    src >> g_useAA;
                }
                src.close();
                SaveConfig();
                
                AllocConsole();
                FILE* fpOut;
                freopen_s(&fpOut, "CONOUT$", "w", stdout);
                std::cout << "Configuration imported successfully from [" << path << "] to AppData.\n";
                std::cout << "Press ENTER to exit...\n";
                std::fflush(stdout);
                FILE* fpIn;
                freopen_s(&fpIn, "CONIN$", "r", stdin);
                std::cin.get();
                if (fpIn) fclose(fpIn);
                if (fpOut) fclose(fpOut);
                FreeConsole();
                return 0;
            } else {
                MessageBoxA(nullptr, ("Failed to open load source file: " + path).c_str(), "3D Fractal Screensaver Error", MB_OK | MB_ICONERROR);
                return 1;
            }
        }
    }

    // Export configuration coordinates to custom location
    if (HasOption(cmd, "save")) {
        std::string path = GetOptionValue(originalCmd, "save");
        if (!path.empty()) {
            std::ofstream dst(path);
            if (dst.is_open()) {
                for (int i = 0; i < NUM_CONTROLS; ++i) {
                    if (g_controls[i].type == CTRL_INT) {
                        dst << *reinterpret_cast<int*>(g_controls[i].pValue) << "\n";
                    } else {
                        dst << *reinterpret_cast<float*>(g_controls[i].pValue) << "\n";
                    }
                }
                dst << g_overrideFractal << "\n";
                dst << g_overridePalette << "\n";
                dst << (g_paused ? 1 : 0) << "\n";
                dst << g_useAA << "\n";
                dst.close();
                
                AllocConsole();
                FILE* fpOut;
                freopen_s(&fpOut, "CONOUT$", "w", stdout);
                std::cout << "Configuration exported successfully to [" << path << "].\n";
                std::cout << "Press ENTER to exit...\n";
                std::fflush(stdout);
                FILE* fpIn;
                freopen_s(&fpIn, "CONIN$", "r", stdin);
                std::cin.get();
                if (fpIn) fclose(fpIn);
                if (fpOut) fclose(fpOut);
                FreeConsole();
                return 0;
            } else {
                MessageBoxA(nullptr, ("Failed to create save destination file: " + path).c_str(), "3D Fractal Screensaver Error", MB_OK | MB_ICONERROR);
                return 1;
            }
        }
    }

    // Command-line flag overrides
    std::string val;
    val = GetOptionValue(originalCmd, "f");
    if (!val.empty()) g_overrideFractal = std::stoi(val);

    val = GetOptionValue(originalCmd, "t");
    if (!val.empty()) g_overridePalette = std::stoi(val);

    val = GetOptionValue(originalCmd, "a");
    if (!val.empty()) g_useAA = std::stoi(val);

    val = GetOptionValue(originalCmd, "r");
    if (!val.empty()) g_resolutionScale = std::stof(val);

    val = GetOptionValue(originalCmd, "st");
    if (!val.empty()) g_maxRaymarchSteps = std::stoi(val);

    val = GetOptionValue(originalCmd, "sp");
    if (!val.empty()) g_timeScale = std::stof(val);

    g_isPreview = (runMode == RunMode::Preview);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "FractalScreenSaverClass";
    wc.hCursor = (g_isPreview || g_isDebug) ? LoadCursor(nullptr, IDC_ARROW) : nullptr;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    if (!RegisterClassA(&wc)) {
        Log("Failed to register Window Class.");
        return 1;
    }

    // Seed RNG
    std::srand(GetTickCount());

    std::vector<MonitorWindow> windows;

    if (g_isPreview) {
        Log("Preview mode activated with parent HWND: " + std::to_string(reinterpret_cast<uintptr_t>(parentHwnd)));
        if (!parentHwnd) {
            Log("Error: Parent HWND is null in preview mode.");
            return 1;
        }
        RECT parentRect;
        GetClientRect(parentHwnd, &parentRect);
        
        MonitorWindow mw;
        mw.timeOffset = static_cast<float>(std::rand() % 1000);
        if (InitMonitorWindow(mw, hInstance, parentRect, parentHwnd)) {
            windows.push_back(mw);
        }
    } else {
        Log("Screensaver mode activated.");
        if (!g_isDebug) {
            ShowCursor(FALSE);
        }

        std::vector<MonitorInfo> monitors;
        EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));
        Log("Enumerated " + std::to_string(monitors.size()) + " monitors.");

        if (g_isDebug) {
            // Debug mode: single window of client size matching Detected Screen Size
            RECT rect;
            int screenW = GetSystemMetrics(SM_CXSCREEN);
            int screenH = GetSystemMetrics(SM_CYSCREEN);
            int winW = screenW;
            int winH = screenH + 1; // 1 pixel taller to extend off-screen at the bottom, hiding the taskbar while disabling DirectFlip
            rect.left = 0;
            rect.top = 0;
            rect.right = winW;
            rect.bottom = winH;

            MonitorWindow mw;
            mw.timeOffset = 0.0f;
            if (InitMonitorWindow(mw, hInstance, rect)) {
                windows.push_back(mw);
            }
        } else {
            for (const auto& m : monitors) {
                MonitorWindow mw;
                mw.timeOffset = static_cast<float>(std::rand() % 1000);
                if (InitMonitorWindow(mw, hInstance, m.rect)) {
                    windows.push_back(mw);
                }
            }
        }
    }

    if (windows.empty()) {
        Log("Error: No rendering windows created successfully. Exiting.");
        if (!g_isPreview && !g_isDebug) ShowCursor(TRUE);
        return 1;
    }

    Log("Entering primary message and render loop...");

    MSG msg = {};
    bool running = true;
    
    // Setup time tracking
    DWORD startTick = GetTickCount();
    DWORD lastTick = startTick;
    float timeAccumulator = static_cast<float>(std::rand() % 1000); // Random initial shared time offset

    // FPS calculation filter
    float smoothedFps = 60.0f;
    const float fpsSmoothingFilter = 0.96f;

    while (running && !g_shouldExit) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            if (g_isDebug && (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)) {
                WPARAM wp = msg.wParam;
                if (wp == VK_ESCAPE || wp == 'P' || wp == 'F' || wp == 'B' || wp == 'C' || wp == 'A' || wp == 'H' || wp == VK_UP || wp == VK_DOWN) {
                    SendMessageA(g_hwndDebugWindow, msg.message, wp, msg.lParam);
                    continue;
                }
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (!running) break;

        // Perform frame-time delta tracking
        DWORD currentTick = GetTickCount();
        float dt = static_cast<float>(currentTick - lastTick) * 0.001f;
        lastTick = currentTick;

        // Smooth FPS calculation
        if (dt > 0.0f) {
            float rawFps = 1.0f / dt;
            smoothedFps = smoothedFps * fpsSmoothingFilter + rawFps * (1.0f - fpsSmoothingFilter);
        }

        // Apply paused and speed scaling configurations
        if (!g_paused) {
            timeAccumulator += dt * g_timeScale;
        }

        // Shared global color palette indexes and transition fade
        int activeCycle = static_cast<int>(timeAccumulator / TIME_FADE_TRANSITION);
        int activePaletteIndex = (g_overridePalette != -1) ? g_overridePalette : (activeCycle % MAX_PALETTES);
        Palette activePalette = GetPalette(activePaletteIndex);

        static int lastPaletteIndex = -1;
        if (activePaletteIndex != lastPaletteIndex) {
            if (g_hwndPaletteCombobox && g_overridePalette == -1) {
                SendMessageA(g_hwndPaletteCombobox, CB_SETCURSEL, activePaletteIndex, 0);
            }
            lastPaletteIndex = activePaletteIndex;
        }

        float cycleTime = std::fmod(timeAccumulator, TIME_FADE_TRANSITION);
        float fade = 1.0f;
        if (g_overridePalette == -1) {
            if (cycleTime < TIME_FADE_HALF_WINDOW) {
                fade = cycleTime / TIME_FADE_HALF_WINDOW;
            } else if (cycleTime > (TIME_FADE_TRANSITION - TIME_FADE_HALF_WINDOW)) {
                fade = (TIME_FADE_TRANSITION - cycleTime) / TIME_FADE_HALF_WINDOW;
            }
        }

        // Render on all monitor windows with independent camera/scenes but shared palette
        for (size_t monitorIdx = 0; monitorIdx < windows.size(); ++monitorIdx) {
            auto& mw = windows[monitorIdx];
            if (!wglMakeCurrent(mw.hdc, mw.hrc)) continue;

            RECT clientRect;
            GetClientRect(mw.hwnd, &clientRect);
            int w = clientRect.right - clientRect.left;
            int h = clientRect.bottom - clientRect.top;

            // Apply calibrated resolution scale dynamically to the viewport size
            int renderW = static_cast<int>(w * g_resolutionScale);
            int renderH = static_cast<int>(h * g_resolutionScale);
            glViewport(0, 0, renderW, renderH);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Compute independent scene time/offset per monitor
            float mTime = timeAccumulator + mw.timeOffset;
            int mActiveCycle = static_cast<int>(mTime / TIME_FADE_TRANSITION);
            
            // Choose active fractal based on override or time offset
            int activeFractal = (g_overrideFractal != -1) ? g_overrideFractal : (mActiveCycle % MAX_FRACTALS);
            g_currentActiveFractal = activeFractal;
            static int lastFractal = -1;
            if (activeFractal != lastFractal) {
                UpdateControlLayout(activeFractal, g_panelVisible);
                if (g_hwndFractalCombobox && g_overrideFractal == -1) {
                    SendMessageA(g_hwndFractalCombobox, CB_SETCURSEL, activeFractal, 0);
                }
                lastFractal = activeFractal;
            }

            // Compute camera parameters based on fractal type
            float camPos[3] = { 0.0f, 0.0f, 0.0f };
            float camLookAt[3] = { 0.0f, 0.0f, 0.0f };

            if (activeFractal == 0) {
                // Mandelbulb
                float r = 1.4f + 0.2f * std::sin(mTime * 0.15f);
                float theta = mTime * 0.06f;
                float phi = mTime * 0.04f;
                camPos[0] = r * std::sin(theta) * std::cos(phi);
                camPos[1] = r * std::cos(theta);
                camPos[2] = r * std::sin(theta) * std::sin(phi);
                camLookAt[0] = 0.05f * std::sin(mTime * 0.2f);
                camLookAt[1] = 0.0f;
                camLookAt[2] = 0.05f * std::cos(mTime * 0.2f);
            } else if (activeFractal == 1) {
                // Julia Quaternion: zoom out to prevent Z-plane clipping
                float r = 2.2f + 0.4f * std::cos(mTime * 0.1f);
                float theta = mTime * 0.08f;
                camPos[0] = r * std::cos(theta);
                camPos[1] = 0.2f * std::sin(mTime * 0.08f);
                camPos[2] = r * std::sin(theta);
                camLookAt[0] = 0.0f;
                camLookAt[1] = 0.0f;
                camLookAt[2] = 0.0f;
            } else if (activeFractal == 2) {
                // Jerusalem Cube
                float r = 1.6f + 0.2f * std::sin(mTime * 0.1f);
                float theta = mTime * 0.07f;
                camPos[0] = r * std::cos(theta);
                camPos[1] = 0.3f * std::cos(mTime * 0.05f);
                camPos[2] = r * std::sin(theta);
                camLookAt[0] = 0.0f;
                camLookAt[1] = 0.0f;
                camLookAt[2] = 0.0f;
            } else if (activeFractal == 3) {
                // Sierpinski Pyramid
                float r = 1.5f + 0.2f * std::cos(mTime * 0.12f);
                float theta = mTime * 0.05f;
                camPos[0] = r * std::sin(theta);
                camPos[1] = 0.4f * std::sin(mTime * 0.08f);
                camPos[2] = r * std::cos(theta);
                camLookAt[0] = 0.0f;
                camLookAt[1] = 0.0f;
                camLookAt[2] = 0.0f;
            } else {
                // Menger Sponge
                float r = 1.5f + 0.3f * std::sin(mTime * 0.09f);
                float theta = mTime * 0.06f;
                camPos[0] = r * std::cos(theta);
                camPos[1] = 0.2f * std::sin(mTime * 0.06f);
                camPos[2] = r * std::sin(theta);
                camLookAt[0] = 0.0f;
                camLookAt[1] = 0.0f;
                camLookAt[2] = 0.0f;
            }

            // Draw raymarched fractal geometry
            glUseProgram(mw.shaderProgram);

            glUniform1f(glGetUniformLocation(mw.shaderProgram, "u_time"), mTime);
            glUniform2f(glGetUniformLocation(mw.shaderProgram, "u_resolution"), static_cast<float>(w), static_cast<float>(h));
            glUniform1i(glGetUniformLocation(mw.shaderProgram, "u_fractalType"), activeFractal);
            glUniform1i(glGetUniformLocation(mw.shaderProgram, "u_useAA"), g_useAA);
            glUniform1i(glGetUniformLocation(mw.shaderProgram, "u_maxSteps"), g_maxRaymarchSteps);

            glUniform3f(glGetUniformLocation(mw.shaderProgram, "u_cameraPos"), camPos[0], camPos[1], camPos[2]);
            glUniform3f(glGetUniformLocation(mw.shaderProgram, "u_cameraLookAt"), camLookAt[0], camLookAt[1], camLookAt[2]);

            glUniform3f(glGetUniformLocation(mw.shaderProgram, "u_paletteA"), activePalette.a[0] * fade, activePalette.a[1] * fade, activePalette.a[2] * fade);
            glUniform3f(glGetUniformLocation(mw.shaderProgram, "u_paletteB"), activePalette.b[0] * fade, activePalette.b[1] * fade, activePalette.b[2] * fade);
            glUniform3f(glGetUniformLocation(mw.shaderProgram, "u_paletteC"), activePalette.c[0], activePalette.c[1], activePalette.c[2]);
            glUniform3f(glGetUniformLocation(mw.shaderProgram, "u_paletteD"), activePalette.d[0], activePalette.d[1], activePalette.d[2]);
            glUniform3f(glGetUniformLocation(mw.shaderProgram, "u_bgColor"), activePalette.bg[0] * fade, activePalette.bg[1] * fade, activePalette.bg[2] * fade);

            // Pass dynamic parameter uniforms
            glUniform1i(glGetUniformLocation(mw.shaderProgram, "u_iterMandelbulb"), g_iterMandelbulb);
            glUniform1f(glGetUniformLocation(mw.shaderProgram, "u_mandelbulbPower"), g_mandelbulbPower);

            glUniform1i(glGetUniformLocation(mw.shaderProgram, "u_iterJulia"), g_iterJulia);
            glUniform4f(glGetUniformLocation(mw.shaderProgram, "u_juliaC"), g_juliaC[0], g_juliaC[1], g_juliaC[2], g_juliaC[3]);

            glUniform1i(glGetUniformLocation(mw.shaderProgram, "u_iterJerusalem"), g_iterJerusalem);
            glUniform1f(glGetUniformLocation(mw.shaderProgram, "u_jerusalemScale"), g_jerusalemScale);

            glUniform1i(glGetUniformLocation(mw.shaderProgram, "u_iterSierpinski"), g_iterSierpinski);
            glUniform1f(glGetUniformLocation(mw.shaderProgram, "u_sierpinskiScale"), g_sierpinskiScale);

            glUniform1i(glGetUniformLocation(mw.shaderProgram, "u_iterMenger"), g_iterMenger);
            glUniform1f(glGetUniformLocation(mw.shaderProgram, "u_mengerScale"), g_mengerScale);

            glDrawArrays(GL_TRIANGLES, 0, 3);

            // Disable shader to render overlay bitmaps
            glUseProgram(0);

            // Reset viewport to window size for GDI bitmap rendering
            glViewport(0, 0, w, h);

            if (g_isDebug && g_panelVisible) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4f(0.0f, 0.0f, 0.0f, 0.65f); // 65% opacity black overlay
                float panelStartNdc = 1.0f - 920.0f / static_cast<float>(w);
                glRectf(panelStartNdc, -1.0f, 1.0f, 1.0f);
                glDisable(GL_BLEND);
            }

            // Format on-screen legend variables
            std::string sceneStr = "Scene: " + std::string(FRACTAL_NAMES[activeFractal]) + (g_overrideFractal != -1 ? " [Manual]" : "");
            std::string paletteStr = "Palette: " + std::string(activePalette.name) + " #" + std::to_string(activePaletteIndex) + (g_overridePalette != -1 ? " [Manual]" : "");
            
            char fpsBuf[32];
            std::sprintf(fpsBuf, "FPS: %.1f", smoothedFps);
            std::string fpsStr = fpsBuf;

            char infoBuf[256];
            if (g_isDebug) {
                std::sprintf(infoBuf, "Res: %dx%d (Scale: %.2f) | Steps: %d | AA: %s | Speed: %.1fx | Controls: [P] Pause | [F/B] Scene | [C] Palette | [A] AA | [Up/Dn] Speed | [H] Panel | [Esc] Exit", 
                             w, h, g_resolutionScale, g_maxRaymarchSteps, g_useAA ? "On" : "Off", g_timeScale);
            } else {
                std::sprintf(infoBuf, "Res: %dx%d (Scale: %.2f) | Steps: %d", w, h, g_resolutionScale, g_maxRaymarchSteps);
            }
            std::string infoStr = infoBuf;

            if (!g_hideLegend) {
                // Draw unnoticeable legend using soft gray text at bottom left
                glColor3f(TEXT_COLOR_RED, TEXT_COLOR_GREEN, TEXT_COLOR_BLUE);
                
                float startX = -0.98f;
                float startY = -0.93f;
                float lineStep = 0.038f;

                RenderText(startX, startY + 3.0f * lineStep, sceneStr, mw.fontListBase);
                RenderText(startX, startY + 2.0f * lineStep, paletteStr, mw.fontListBase);
                RenderText(startX, startY + 1.0f * lineStep, fpsStr, mw.fontListBase);
                RenderText(startX, startY, infoStr, mw.fontListBase);
            }

            SwapBuffers(mw.hdc);
        }

        Sleep(10);
    }

    Log("Exiting main loop.");

    for (auto& mw : windows) {
        wglMakeCurrent(nullptr, nullptr);
        if (mw.shaderProgram) glUseProgram(0);
        if (mw.fontListBase) glDeleteLists(mw.fontListBase, DEFAULT_FONT_CHAR_COUNT);
        if (mw.hrc) wglDeleteContext(mw.hrc);
        if (mw.hdc) ReleaseDC(mw.hwnd, mw.hdc);
        if (mw.hwnd) DestroyWindow(mw.hwnd);
    }

    if (!g_isPreview && !g_isDebug) {
        ShowCursor(TRUE);
    }

    Log("Screensaver exit clean.");
    return 0;
}
