// Repositories.h  (Data access layer).
#pragma once
#include <memory>
#include <optional>
#include <vector>
#include "Database.h"
#include "Domain.h"
using namespace std;

class IUserRepository {
public:
    virtual ~IUserRepository() = default;
    virtual int add(const NewUser& user) = 0;
    virtual optional<Credentials> getCredentials(const string& username) = 0;
    virtual unique_ptr<User> findById(int userId) = 0;
    virtual vector<UserRow> listAll() = 0;
    virtual void setActive(int userId, bool active) = 0;
    virtual void updatePassword(int userId, const string& hash, const string& salt) = 0;
    virtual int count() = 0;
};

class ICourseRepository {
public:
    virtual ~ICourseRepository() = default;
    virtual int addDepartment(const string& name) = 0;
    virtual void setHod(int departmentId, int userId) = 0;
    virtual vector<Department> listDepartments() = 0;
    virtual int addCourse(const string& code, const string& title, int departmentId) = 0;
    virtual optional<Course> findCourse(int courseId) = 0;
    virtual vector<Course> listCourses() = 0;
    virtual vector<Course> coursesForStudent(int studentId) = 0;
    virtual vector<Course> coursesForLecturer(int lecturerId) = 0;
    virtual vector<Course> coursesForDepartment(int departmentId) = 0;
    virtual void assignLecturer(int courseId, int lecturerId) = 0;
    virtual void enrol(int studentId, int courseId) = 0;
    virtual bool isLecturerOfCourse(int lecturerId, int courseId) = 0;
    virtual bool isEnrolled(int studentId, int courseId) = 0;
    virtual vector<UserRow> studentsInCourse(int courseId) = 0;
};

class IAssignmentRepository {
public:
    virtual ~IAssignmentRepository() = default;
    virtual int add(const Assignment& assignment) = 0;
    virtual optional<Assignment> findById(int assignmentId) = 0;
    virtual vector<Assignment> listByCourse(int courseId) = 0;
    virtual void saveExtension(int assignmentId, int studentId, const string& newDeadline,
                               const string& reason, int grantedBy) = 0;
    virtual optional<string> findExtension(int assignmentId, int studentId) = 0;
};

class ISubmissionRepository {
public:
    virtual ~ISubmissionRepository() = default;
    virtual optional<Submission> find(int assignmentId, int studentId) = 0;
    virtual optional<Submission> findById(int submissionId) = 0;
    virtual vector<Submission> listByAssignment(int assignmentId) = 0;
    virtual vector<Submission> listByStudent(int studentId) = 0;
    virtual void create(int assignmentId, int studentId, const string& status, int daysLate,
                        const string& content, const string& submittedAt) = 0;
    // Adds a new version AND refreshes the on-time/late status in one transaction.
    virtual int addVersion(int submissionId, const string& content, const string& submittedAt,
                           const string& status, int daysLate) = 0;
    virtual string latestContent(int submissionId) = 0;
    virtual optional<string> latestSubmittedAt(int submissionId) = 0;
    // Changes SUBMITTED/LATE timing only; work that is already marked is never touched.
    virtual void updateTiming(int submissionId, const string& status, int daysLate) = 0;
    // Changes the mark AND writes the audit row in one transaction.
    virtual void saveMark(int submissionId, double newRaw, const string& feedback,
                          const string& newStatus, int changedBy, bool byModerator,
                          const string& reason) = 0;
    virtual int approveAll(int assignmentId, int hodId) = 0;               // MARKED -> MODERATED
    virtual int releaseAll(int assignmentId, const string& when) = 0;      // MODERATED -> RELEASED
};

class IAuditRepository {
public:
    virtual ~IAuditRepository() = default;
    virtual vector<MarkAudit> findForDepartment(int departmentId) = 0;
};

// ---------------------------------------------------------------- SQLite versions
class SqliteUserRepository : public IUserRepository {
public:
    explicit SqliteUserRepository(Database& db) : db(db) {}
    int add(const NewUser& user) override;
    optional<Credentials> getCredentials(const string& username) override;
    unique_ptr<User> findById(int userId) override;
    vector<UserRow> listAll() override;
    void setActive(int userId, bool active) override;
    void updatePassword(int userId, const string& hash, const string& salt) override;
    int count() override;
private:
    Database& db;
};

class SqliteCourseRepository : public ICourseRepository {
public:
    explicit SqliteCourseRepository(Database& db) : db(db) {}
    int addDepartment(const string& name) override;
    void setHod(int departmentId, int userId) override;
    vector<Department> listDepartments() override;
    int addCourse(const string& code, const string& title, int departmentId) override;
    optional<Course> findCourse(int courseId) override;
    vector<Course> listCourses() override;
    vector<Course> coursesForStudent(int studentId) override;
    vector<Course> coursesForLecturer(int lecturerId) override;
    vector<Course> coursesForDepartment(int departmentId) override;
    void assignLecturer(int courseId, int lecturerId) override;
    void enrol(int studentId, int courseId) override;
    bool isLecturerOfCourse(int lecturerId, int courseId) override;
    bool isEnrolled(int studentId, int courseId) override;
    vector<UserRow> studentsInCourse(int courseId) override;
private:
    vector<Course> readCourses(Statement& st);
    Database& db;
};

class SqliteAssignmentRepository : public IAssignmentRepository {
public:
    explicit SqliteAssignmentRepository(Database& db) : db(db) {}
    int add(const Assignment& assignment) override;
    optional<Assignment> findById(int assignmentId) override;
    vector<Assignment> listByCourse(int courseId) override;
    void saveExtension(int assignmentId, int studentId, const string& newDeadline,
                       const string& reason, int grantedBy) override;
    optional<string> findExtension(int assignmentId, int studentId) override;
private:
    Assignment readAssignment(Statement& st);
    Database& db;
};

class SqliteSubmissionRepository : public ISubmissionRepository {
public:
    explicit SqliteSubmissionRepository(Database& db) : db(db) {}
    optional<Submission> find(int assignmentId, int studentId) override;
    optional<Submission> findById(int submissionId) override;
    vector<Submission> listByAssignment(int assignmentId) override;
    vector<Submission> listByStudent(int studentId) override;
    void create(int assignmentId, int studentId, const string& status, int daysLate,
                const string& content, const string& submittedAt) override;
    int addVersion(int submissionId, const string& content, const string& submittedAt,
                   const string& status, int daysLate) override;
    string latestContent(int submissionId) override;
    optional<string> latestSubmittedAt(int submissionId) override;
    void updateTiming(int submissionId, const string& status, int daysLate) override;
    void saveMark(int submissionId, double newRaw, const string& feedback,
                  const string& newStatus, int changedBy, bool byModerator,
                  const string& reason) override;
    int approveAll(int assignmentId, int hodId) override;
    int releaseAll(int assignmentId, const string& when) override;
private:
    Submission readSubmission(Statement& st);
    Database& db;
};

class SqliteAuditRepository : public IAuditRepository {
public:
    explicit SqliteAuditRepository(Database& db) : db(db) {}
    vector<MarkAudit> findForDepartment(int departmentId) override;
private:
    Database& db;
};
