
#pragma once
#include <memory>
#include <stdexcept>
#include <string>
using namespace std;

class LmsError : public runtime_error {
public:
    explicit LmsError(const string& msg) : runtime_error(msg) {}
};

enum class Role { STUDENT, LECTURER, HOD, ADMIN };
string roleToString(Role role);          
Role roleFromString(const string& text);

enum class Permission {
    MANAGE_USERS, MANAGE_COURSES,          
    CREATE_ASSIGNMENT, GRANT_EXTENSION, MARK_WORK,   
    MODERATE_MARKS, VIEW_AUDIT,            
    SUBMIT_WORK, VIEW_OWN_COURSES          
};

class User {
public:
    User(int id, const string& username, const string& fullName)
        : userId(id), username(username), fullName(fullName) {}
    virtual ~User() = default;

    int getId() const { return userId; }
    const string& getUsername() const { return username; }
    const string& getFullName() const { return fullName; }

    virtual Role getRole() const = 0;
    virtual bool can(Permission permission) const = 0;

protected:
    int userId;
    string username;
    string fullName;
};

class Student : public User {
public:
    using User::User;
    Role getRole() const override { return Role::STUDENT; }
    bool can(Permission p) const override {
        return p == Permission::SUBMIT_WORK || p == Permission::VIEW_OWN_COURSES;
    }
};

class Lecturer : public User {
public:
    using User::User;
    Role getRole() const override { return Role::LECTURER; }
    bool can(Permission p) const override {
        return p == Permission::CREATE_ASSIGNMENT || p == Permission::GRANT_EXTENSION ||
               p == Permission::MARK_WORK || p == Permission::VIEW_OWN_COURSES;
    }
};

class HeadOfDepartment : public User {
public:
    HeadOfDepartment(int id, const string& username, const string& fullName, int departmentId)
        : User(id, username, fullName), departmentId(departmentId) {}
    Role getRole() const override { return Role::HOD; }
    bool can(Permission p) const override {
        return p == Permission::MODERATE_MARKS || p == Permission::VIEW_AUDIT ||
               p == Permission::VIEW_OWN_COURSES;
    }
    int getDepartmentId() const { return departmentId; }   // 0 = not heading a department yet
private:
    int departmentId;
};

class Administrator : public User {
public:
    using User::User;
    Role getRole() const override { return Role::ADMIN; }
    bool can(Permission p) const override {
        return p == Permission::MANAGE_USERS || p == Permission::MANAGE_COURSES;
    }
};

void requirePermission(const User& user, Permission permission);

class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;
    virtual string makeSalt() = 0;
    virtual string hash(const string& password, const string& salt) = 0;
};


struct NewUser {
    string username;
    string passwordHash;
    string salt;
    string fullName;
    Role role;
};

class UserFactory {
public:
    explicit UserFactory(IPasswordHasher& hasher) : hasher(hasher) {}

    // Checks the input and hashes the password with a new random salt.
    NewUser prepareNewUser(Role role, const string& username,
                           const string& fullName, const string& password) const;

    // Builds the right subclass for a role.
    static unique_ptr<User> createUser(Role role, int id, const string& username,
                                       const string& fullName, int departmentId = 0);

private:
    IPasswordHasher& hasher;
};

namespace Status {
const string SUBMITTED = "SUBMITTED";
const string LATE = "LATE";
const string MARKED = "MARKED";
const string RETURNED = "RETURNED";
const string MODERATED = "MODERATED";
const string RELEASED = "RELEASED";
}


class ILatePenaltyPolicy {
public:
    virtual ~ILatePenaltyPolicy() = default;
    
    virtual double apply(double rawMark, double maxMark, int daysLate) const = 0;
    virtual string describe() const = 0;
};

class TenPercentPerDayPolicy : public ILatePenaltyPolicy {
public:
    double apply(double rawMark, double maxMark, int daysLate) const override;
    string describe() const override;
};

struct Department {
    int id = 0;
    string name;
    int hodUserId = 0;   
};

struct Course {
    int id = 0;
    string code;
    string title;
    int departmentId = 0;
};

struct Assignment {
    int id = 0;
    int courseId = 0;
    string title;
    string description;
    string deadline;     // "YYYY-MM-DD HH:MM:SS" (UTC)
    double maxMark = 0;
    int createdBy = 0;
};

struct Submission {
    int id = 0;
    int assignmentId = 0;
    int studentId = 0;
    string studentName;
    string assignmentTitle;
    string status;
    bool hasMark = false;     // false until a lecturer marks it
    double rawMark = 0;       // mark before the penalty
    double maxMark = 0;
    int daysLate = 0;
    string feedback;
    int markedBy = 0;
    int moderatedBy = 0;
    string releasedAt;

    // The final mark is worked out when needed
    double finalMark(const ILatePenaltyPolicy& policy) const {
        return policy.apply(rawMark, maxMark, daysLate);
    }
};

struct MarkAudit {
    int id = 0;
    int submissionId = 0;
    string studentName;
    string assignmentTitle;
    string changedByName;
    string changedAt;
    bool hasOld = false;
    double oldValue = 0;
    double newValue = 0;
    string reason;
};

struct UserRow {           // used for listing users
    int id = 0;
    string username;
    string fullName;
    string role;
    bool active = true;
};

struct Credentials {       // used when logging in
    int userId = 0;
    string passwordHash;
    string salt;
    bool active = true;
};

string nowString();
string formatNumber(double value);              // 12.5 -> "12.5", 50 -> "50"
string normaliseDateTime(const string& input);  
int calcDaysLate(const string& submittedAt, const string& deadline);
