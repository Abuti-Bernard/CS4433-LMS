#include "Input.h"
#include <cmath>
#include <iostream>
#ifdef _WIN32
#include <conio.h>
#endif
#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif
using namespace std;

static string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

string readLine(const string& prompt) {
    cout << prompt;
    string line;
    if (!getline(cin, line)) throw InputClosed();
    return trim(line);
}

int readInt(const string& prompt, int minValue, int maxValue) {
    while (true) {
        string text = readLine(prompt);
        try {
            size_t used = 0;
            int value = stoi(text, &used);
            if (used == text.size() && value >= minValue && value <= maxValue) return value;
        } catch (const exception&) {
            // not a number (or too big), fall through and ask again
        }
        cout << "Please enter a whole number from " << minValue << " to " << maxValue << ".\n";
    }
}

double readDouble(const string& prompt) {
    while (true) {
        string text = readLine(prompt);
        try {
            size_t used = 0;
            double value = stod(text, &used);
            if (used == text.size() && isfinite(value)) return value;
        } catch (const exception&) {
        }
        cout << "Please enter a number (example: 75.5).\n";
    }
}

string readPassword(const string& prompt) {
#ifndef _WIN32
    termios oldSettings;
    bool hidden = false;
    if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &oldSettings) == 0) {
        termios newSettings = oldSettings;
        newSettings.c_lflag &= ~ECHO;          
        tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
        hidden = true;
    }
    cout << prompt;
    string password;
    bool ok = static_cast<bool>(getline(cin, password));
    if (hidden) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
        cout << "\n";
    }
    if (!ok) throw InputClosed();
    return password;
#else
    cout << prompt << flush;
    string password;
    while (true) {
        int ch = _getch();
        if (ch == '\r' || ch == '\n') {
            cout << "\n";
            break;
        }
        if (ch == 8) { // Backspace
            if (!password.empty()) password.pop_back();
        } else if (ch == 0 || ch == 224) {
            (void)_getch(); // consume the second byte of a special key
        } else if (ch >= 32 && ch <= 126) {
            password.push_back(static_cast<char>(ch));
        }
    }
    return password;
#endif
}
