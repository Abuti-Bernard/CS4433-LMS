#pragma once
#include <vector>
#include "Domain.h"
#include "Repositories.h"
using namespace std;

class AuthService {
public:
    AuthService(IUserRepository& users, IPasswordHasher& hasher) : users(users), hasher(hasher) {}
    unique_ptr<User> login(const string& username, const string& password);
    void changePassword(const User& user, const string& oldPassword, const string& newPassword);
private:
    IUserRepository& users;
    IPasswordHasher& hasher;
};

class AdminService {
public:
    AdminService(IUserRepository& users, ICourseRepository& courses, UserFactory& factory)
        : users(users), courses(courses), factory(factory) {}
    int createUser(const User& actor, Role role, const string& username,
                   const string& fullName, const string& password);
    vector<UserRow> listUsers(const User& actor);
    void setUserActive(const User& actor, int userId, bool active);
    int createDepartment(const User& actor, const string& name);
    void assignHod(const User& actor, int departmentId, int userId);
    vector<Department> listDepartments(const User& actor);
    int createCourse(const User& actor, const string& code, const string& title, int departmentId);
    vector<Course> listCourses(const User& actor);
    void assignLecturer(const User& actor, int courseId, int lecturerId);
    void enrolStudent(const User& actor, int studentId, int courseId);
private:
    IUserRepository& users;
    ICourseRepository& courses;
    UserFactory& factory;
};

// What happened to the student's existing submission when an extension was granted.
struct ExtensionResult {
    bool hasSubmission = false;   // the student had already submitted something
    bool rechecked = false;       // ...and it was unmarked, so its timing was recalculated
    string status;                // SUBMITTED or LATE (only when rechecked)
    int daysLate = 0;             // days late against the new deadline (only when rechecked)
};

class AssignmentService {
public:
    AssignmentService(IAssignmentRepository& assignments, ICourseRepository& courses,
                      ISubmissionRepository& subs)
        : assignments(assignments), courses(courses), subs(subs) {}
    int createAssignment(const User& lecturer, int courseId, const string& title,
                         const string& description, const string& deadlineInput, double maxMark);
    ExtensionResult grantExtension(const User& lecturer, int assignmentId, int studentId,
                                   const string& newDeadlineInput, const string& reason);
    vector<Course> coursesFor(const User& user);
    vector<Assignment> assignmentsOfCourse(const User& user, int courseId);
    vector<UserRow> studentsOfAssignment(const User& lecturer, int assignmentId);
    // the student's own deadline (the extension if there is one, otherwise the normal deadline)
    string effectiveDeadline(const Assignment& assignment, int studentId);
private:
    IAssignmentRepository& assignments;
    ICourseRepository& courses;
    ISubmissionRepository& subs;
};

struct SubmitResult {
    string status;       // SUBMITTED or LATE
    int version = 1;
    int daysLate = 0;
    bool resubmission = false;
};

class SubmissionService {
public:
    SubmissionService(ISubmissionRepository& subs, IAssignmentRepository& assignments,
                      ICourseRepository& courses)
        : subs(subs), assignments(assignments), courses(courses) {}
    SubmitResult submit(const User& student, int assignmentId, const string& content);
    // Marks are hidden here (not in the menu) unless the HoD has released them.
    vector<Submission> mySubmissions(const User& student);
    vector<Submission> listForAssignment(const User& lecturer, int assignmentId);
    Submission getSubmission(const User& lecturer, int submissionId);
    string viewWork(const User& lecturer, int submissionId);
    void markSubmission(const User& lecturer, int submissionId, double rawMark,
                        const string& feedback, const string& reason);
private:
    void requireLecturerOfAssignment(const User& lecturer, int assignmentId);
    ISubmissionRepository& subs;
    IAssignmentRepository& assignments;
    ICourseRepository& courses;
};

class ModerationService {
public:
    ModerationService(ISubmissionRepository& subs, IAssignmentRepository& assignments,
                      ICourseRepository& courses, IAuditRepository& audit)
        : subs(subs), assignments(assignments), courses(courses), audit(audit) {}
    vector<Submission> listForAssignment(const User& hod, int assignmentId);
    int approveAll(const User& hod, int assignmentId);     // MARKED -> MODERATED
    int releaseAll(const User& hod, int assignmentId);     // MODERATED -> RELEASED
    void returnToLecturer(const User& hod, int submissionId, const string& reason);
    void adjustMark(const User& hod, int submissionId, double newRawMark, const string& reason);
    vector<MarkAudit> auditLog(const User& hod);
private:
    int departmentOf(const User& hod);
    void checkAssignmentInDepartment(const User& hod, int assignmentId);
    Submission loadOwnSubmission(const User& hod, int submissionId);
    ISubmissionRepository& subs;
    IAssignmentRepository& assignments;
    ICourseRepository& courses;
    IAuditRepository& audit;
};
