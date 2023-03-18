#include <Windows.h>
#include <stdio.h>
#include <stdbool.h>

static HHOOK keyboard_hook;
static HHOOK mouse_hook;

static const UINT capslock_scanCode = 0x3a;
static const UINT capslock_vkCode = VK_CAPITAL;
static bool capslock_down = false;

#define KEYEVENT_BUF_SIZE 16
static INPUT keyevent_buf[KEYEVENT_BUF_SIZE];
static UINT keyevent_idx = 0;

static void keyevent_append(BYTE wVk, DWORD dwFlags)
{
    if (keyevent_idx < (sizeof keyevent_buf / sizeof(INPUT))) {
        keyevent_buf[keyevent_idx].ki.wVk = wVk;
        keyevent_buf[keyevent_idx].type = INPUT_KEYBOARD;
        keyevent_buf[keyevent_idx].ki.wScan = capslock_scanCode; // a trick to detect that we are the source of this key event
        keyevent_buf[keyevent_idx].ki.dwFlags = dwFlags;
        keyevent_buf[keyevent_idx].ki.time = 0;
        keyevent_buf[keyevent_idx].ki.dwExtraInfo = 0;
        keyevent_idx++;
    }
    else {
        puts("huh? keyevent buffer is full.");
    }
}

static void keyevent_reset(void)
{
    keyevent_idx = 0;
}

static void keyevent_send(void)
{
    UINT count = keyevent_idx;
    if (count > 0) {
        UINT sent = SendInput(count, keyevent_buf, sizeof(INPUT));
        if (sent != count)
            puts("huh? failed to send keyboard events.");
        keyevent_reset();
    }
}

LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
    KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;

    if (0)
        printf("nCode=%d wParam=%llx lParam=%llx vkCode=%02x scanCode=%02x\n", nCode, wParam, lParam, kb->vkCode, kb->scanCode);

    // intercept the CAPS_LOCK, take care of state and make sure nobody else can see the event
    if (kb->vkCode == capslock_vkCode) {
        capslock_down = (wParam == WM_KEYDOWN);
        return 1;
    }

    // if CAPS_LOCK is not down and/or no other key is down
    if (!capslock_down || wParam != WM_KEYDOWN)
        return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);

    // if someone is just peeking, or if this is an event triggered by us, then forward to next hook
    if (nCode != HC_ACTION || kb->scanCode == capslock_scanCode)
        return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);

    // ... at this point, CAPS_LOCK is down and some key is pressed

    keyevent_reset();

    switch (kb->vkCode) {
    case VK_ESCAPE:
        UnhookWindowsHookEx(keyboard_hook);
        puts("Unhook interceptor and exit program");
        exit(0);
    case 'N':
        keyevent_append(VK_DOWN, 0);
        keyevent_append(VK_DOWN, KEYEVENTF_KEYUP);
        break;
    case 'P':
        keyevent_append(VK_UP, 0);
        keyevent_append(VK_UP, KEYEVENTF_KEYUP);
        break;
    case 'F':
        keyevent_append(VK_RIGHT, 0);
        keyevent_append(VK_RIGHT, KEYEVENTF_KEYUP);
        break;
    case 'B':
        keyevent_append(VK_LEFT, 0);
        keyevent_append(VK_LEFT, KEYEVENTF_KEYUP);
        break;
    case 'A':
        keyevent_append(VK_HOME, 0);
        keyevent_append(VK_HOME, KEYEVENTF_KEYUP);
        break;
    case 'E':
        keyevent_append(VK_END, 0);
        keyevent_append(VK_END, KEYEVENTF_KEYUP);
        break;
    case 'J':
        keyevent_append(VK_RETURN, 0);
        keyevent_append(VK_RETURN, KEYEVENTF_KEYUP);
        break;
    case 'D':
        keyevent_append(VK_DELETE, 0);
        keyevent_append(VK_DELETE, KEYEVENTF_KEYUP);
        break;
    case 'H':
        keyevent_append(VK_BACK, 0);
        keyevent_append(VK_BACK, KEYEVENTF_KEYUP);
        break;
    case 'G':
        keyevent_append(VK_ESCAPE, 0);
        keyevent_append(VK_ESCAPE, KEYEVENTF_KEYUP);
        break;
    case 'K':
        keyevent_append(VK_LSHIFT, 0);
        keyevent_append(VK_END, 0);
        keyevent_append(VK_END, KEYEVENTF_KEYUP);
        keyevent_append(VK_LSHIFT, KEYEVENTF_KEYUP);
        keyevent_append(VK_LCONTROL, 0);
        keyevent_append('X', 0);
        keyevent_append('X', KEYEVENTF_KEYUP);
        keyevent_append(VK_LCONTROL, KEYEVENTF_KEYUP);
        break;
    case 'Y':
        keyevent_append(VK_LCONTROL, 0);
        keyevent_append('V', 0);
        keyevent_append('V', KEYEVENTF_KEYUP);
        keyevent_append(VK_LCONTROL, KEYEVENTF_KEYUP);
        break;
    case VK_RETURN:
        keyevent_append(VK_LCONTROL, 0);
        keyevent_append(VK_RETURN, 0);
        keyevent_append(VK_RETURN, KEYEVENTF_KEYUP);
        keyevent_append(VK_LCONTROL, KEYEVENTF_KEYUP);
        break;
    case 'O':
        keyevent_append(VK_LCONTROL, 0);
        keyevent_append('O', 0);
        keyevent_append('O', KEYEVENTF_KEYUP);
        keyevent_append(VK_LCONTROL, KEYEVENTF_KEYUP);
        break;
    default:
        // mute every other key while capslock_down
        break;
    }

    keyevent_send();
    return 1;
}

LRESULT CALLBACK mouse_proc(int nCode, WPARAM wParam, LPARAM lParam) {

    static HWND window_handle = NULL;
    static RECT window_rect = {0,0};
    static long old_x = 0;
    static long old_y = 0;

    if (capslock_down) {
        if (wParam == WM_LBUTTONDOWN) {
            window_handle = GetForegroundWindow();
            GetWindowRect(window_handle, &window_rect);
            MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lParam;
            old_x = p->pt.x;
            old_y = p->pt.y;
            return 1;
        }
        if (wParam == WM_LBUTTONUP) {
            window_handle = NULL;
            return 1;
        }

        if (window_handle) {
            MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lParam;
            int x = window_rect.left + (p->pt.x - old_x);
            int y = window_rect.top + (p->pt.y - old_y);
            SetWindowPos(window_handle, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
    }

    return CallNextHookEx(mouse_hook, nCode, wParam, lParam);
}

int main(void)
{
    puts("ihcl 0.96 (18mar2023)");
    puts("  CAPS_LOCK (L) will be intercepted and ignored...");
    puts("except");
    puts("  L-n down");
    puts("  L-p up");
    puts("  L-f right");
    puts("  L-b left");
    puts("  L-a line beginning");
    puts("  L-e line end");
    puts("  L-j new line");
    puts("  L-d delete forward");
    puts("  L-h delete backward");
    puts("  L-g escape");
    puts("  L-k kill/cut to end of line");
    puts("  L-y yank/paste last kill/cut");
    puts("  L-RETURN will send a C-RETURN");
    puts("  L-o will send a C-o");
    puts("  L-LBUTTONDOWN will move foreground window");
    puts("and");
    puts("  L-ESC will unhook interceptor and exit program");

    keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_proc, NULL, 0);
    mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, mouse_proc, NULL, 0);
    MSG message;
    while (GetMessage(&message, NULL, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    UnhookWindowsHookEx(mouse_hook);
    UnhookWindowsHookEx(keyboard_hook);

    return 0;
}
