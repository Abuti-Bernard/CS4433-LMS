#include "Domain.h"
#include <cctype>
#include <cmath>
#include <ctime>
#include <cstdio>
using namespace std;

#ifdef _WIN32
#define timegm _mkgmtime
#endif



string roleToString(Role role) {
    switch (role) {
        case Role::STUDENT:  return "STUDENT";
        case Role::LECTURER: return "LECTURER";
        case Role::HOD:      return "HOD";
        case Role::ADMIN:    return "ADMIN";
    }
    return "UNKNOWN";
}

Role roleFromString(const string& text) {
    if (text == "STUDENT")  return Role::STUDENT;
    if (text == "LECTURER") return Role::LECTURER;
    if (text == "HOD")      return Role::HOD;
    if (text == "ADMIN")    return Role::ADMIN;
    throw LmsError("Unknown role: " + text);
}

void requirePermission(const User& user, Permission permission) {
    if (!user.can(permission)) throw LmsError("Access denied: your role cannot do this.");
}



NewUser UserFactory::prepareNewUser(Role role, const string& username,
                                    const string& fullName, const string& password) const {
    if (username.size() < 3) throw LmsError("Username must be at least 3 characters.");
    for (char c : username)
        if (isspace(static_cast<unsigned char>(c))) throw LmsError("Username cannot contain spaces.");
    if (fullName.empty()) throw LmsError("Full name cannot be empty.");
    if (password.size() < 6) throw LmsError("Password must be at least 6 characters.");

    NewUser u;
    u.username = username;
    u.fullName = fullName;
    u.role = role;
    u.salt = hasher.makeSalt();
    u.passwordHash = hasher.hash(password, u.salt);
    return u;
}

unique_ptr<User> UserFactory::createUser(Role role, int id, const string& username,
                                         const string& fullName, int departmentId) {
    switch (role) {
        case Role::STUDENT:  return make_unique<Student>(id, username, fullName);
        case Role::LECTURER: return make_unique<Lecturer>(id, username, fullName);
        case Role::HOD:      return make_unique<HeadOfDepartment>(id, username, fullName, departmentId);
        case Role::ADMIN:    return make_unique<Administrator>(id, username, fullName);
    }
    throw LmsError("Cannot create user: bad role.");
}



double TenPercentPerDayPolicy::apply(double rawMark, double maxMark, int daysLate) const {
    if (daysLate <= 0) return rawMark;     
    if (daysLate > 3) return 0;            
    double result = rawMark - 0.10 * maxMark * daysLate;
    return result < 0 ? 0 : result;        
}

string TenPercentPerDayPolicy::describe() const {
    return "10% of the maximum mark per day late (1-3 days), zero after 3 days";
}



string formatNumber(double value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", value);
    string text = buf;
    if (text.find('.') != string::npos) {
        while (text.back() == '0') text.pop_back();
        if (text.back() == '.') text.pop_back();
    }
    return text;
}

string nowString() {
    time_t now = time(nullptr);
    tm* t = gmtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return buf;
}

static time_t parseTime(const string& text) {
    // Accept exactly YYYY-MM-DD HH:MM or YYYY-MM-DD HH:MM:SS.
    if (text.size() != 16 && text.size() != 19)
        throw LmsError("Date must look like YYYY-MM-DD HH:MM (example: 2026-11-30 17:00).");
    if (text[4] != '-' || text[7] != '-' || text[10] != ' ' ||
        text[13] != ':' || (text.size() == 19 && text[16] != ':'))
        throw LmsError("Date must look like YYYY-MM-DD HH:MM (example: 2026-11-30 17:00).");
    auto number = [&](size_t pos, size_t length) {
        int value = 0;
        for (size_t i = pos; i < pos + length; ++i) {
            if (text[i] < '0' || text[i] > '9')
                throw LmsError("That date or time is not valid.");
            value = value * 10 + (text[i] - '0');
        }
        return value;
    };
    int y = number(0, 4), mo = number(5, 2), d = number(8, 2);
    int h = number(11, 2), mi = number(14, 2);
    int sec = text.size() == 19 ? number(17, 2) : 0;
    if (y < 1 || mo < 1 || mo > 12 || d < 1 || d > 31 ||
        h > 23 || mi > 59 || sec > 59)
        throw LmsError("That date or time is not valid.");

    tm value = {};
    value.tm_year = y - 1900;
    value.tm_mon = mo - 1;
    value.tm_mday = d;
    value.tm_hour = h;
    value.tm_min = mi;
    value.tm_sec = sec;
    time_t result = timegm(&value);
    tm* back = gmtime(&result);
    if (!back || back->tm_year != y - 1900 || back->tm_mon != mo - 1 ||
        back->tm_mday != d || back->tm_hour != h || back->tm_min != mi || back->tm_sec != sec)
        throw LmsError("That date does not exist.");
    return result;
}

string normaliseDateTime(const string& input) {
    time_t t = parseTime(input);
    tm* parts = gmtime(&t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", parts);
    return buf;
}

int calcDaysLate(const string& submittedAt, const string& deadline) {
    double seconds = difftime(parseTime(submittedAt), parseTime(deadline));
    if (seconds <= 0) return 0;
    return static_cast<int>(ceil(seconds / 86400.0));   
}
