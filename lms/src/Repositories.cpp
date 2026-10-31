#include "Repositories.h"
using namespace std;


int SqliteUserRepository::add(const NewUser& user) {
    Statement st(db, "INSERT INTO users (username, password_hash, salt, role, full_name) "
                     "VALUES (?, ?, ?, ?, ?)");
    st.bindText(1, user.username);
    st.bindText(2, user.passwordHash);
    st.bindText(3, user.salt);
    st.bindText(4, roleToString(user.role));
    st.bindText(5, user.fullName);
    st.run();
    return static_cast<int>(db.lastInsertId());
}

optional<Credentials> SqliteUserRepository::getCredentials(const string& username) {
    Statement st(db, "SELECT user_id, password_hash, salt, is_active FROM users WHERE username = ?");
    st.bindText(1, username);
    if (!st.step()) return nullopt;
    Credentials c;
    c.userId = static_cast<int>(st.getInt(0));
    c.passwordHash = st.getText(1);
    c.salt = st.getText(2);
    c.active = st.getInt(3) == 1;
    return c;
}

unique_ptr<User> SqliteUserRepository::findById(int userId) {
    string username, fullName, roleText;
    {
        Statement st(db, "SELECT username, full_name, role FROM users WHERE user_id = ?");
        st.bindInt(1, userId);
        if (!st.step()) return nullptr;
        username = st.getText(0);
        fullName = st.getText(1);
        roleText = st.getText(2);
    }
    Role role = roleFromString(roleText);
    int departmentId = 0;
    if (role == Role::HOD) {   // the HoD's department is stored in the departments table
        Statement st(db, "SELECT department_id FROM departments WHERE hod_user_id = ?");
        st.bindInt(1, userId);
        if (st.step()) departmentId = static_cast<int>(st.getInt(0));
    }
    return UserFactory::createUser(role, userId, username, fullName, departmentId);
}

vector<UserRow> SqliteUserRepository::listAll() {
    vector<UserRow> rows;
    Statement st(db, "SELECT user_id, username, full_name, role, is_active FROM users ORDER BY user_id");
    while (st.step()) {
        UserRow r;
        r.id = static_cast<int>(st.getInt(0));
        r.username = st.getText(1);
        r.fullName = st.getText(2);
        r.role = st.getText(3);
        r.active = st.getInt(4) == 1;
        rows.push_back(r);
    }
    return rows;
}

void SqliteUserRepository::setActive(int userId, bool active) {
    Statement st(db, "UPDATE users SET is_active = ? WHERE user_id = ?");
    st.bindInt(1, active ? 1 : 0);
    st.bindInt(2, userId);
    st.run();
}

void SqliteUserRepository::updatePassword(int userId, const string& hash, const string& salt) {
    Statement st(db, "UPDATE users SET password_hash = ?, salt = ? WHERE user_id = ?");
    st.bindText(1, hash);
    st.bindText(2, salt);
    st.bindInt(3, userId);
    st.run();
}

int SqliteUserRepository::count() {
    Statement st(db, "SELECT COUNT(*) FROM users");
    st.step();
    return static_cast<int>(st.getInt(0));
}


int SqliteCourseRepository::addDepartment(const string& name) {
    Statement st(db, "INSERT INTO departments (name) VALUES (?)");
    st.bindText(1, name);
    st.run();
    return static_cast<int>(db.lastInsertId());
}

void SqliteCourseRepository::setHod(int departmentId, int userId) {
    Statement st(db, "UPDATE departments SET hod_user_id = ? WHERE department_id = ?");
    st.bindInt(1, userId);
    st.bindInt(2, departmentId);
    st.run();
}

vector<Department> SqliteCourseRepository::listDepartments() {
    vector<Department> list;
    Statement st(db, "SELECT department_id, name, hod_user_id FROM departments ORDER BY department_id");
    while (st.step()) {
        Department d;
        d.id = static_cast<int>(st.getInt(0));
        d.name = st.getText(1);
        d.hodUserId = st.isNull(2) ? 0 : static_cast<int>(st.getInt(2));
        list.push_back(d);
    }
    return list;
}

int SqliteCourseRepository::addCourse(const string& code, const string& title, int departmentId) {
    Statement st(db, "INSERT INTO courses (code, title, department_id) VALUES (?, ?, ?)");
    st.bindText(1, code);
    st.bindText(2, title);
    st.bindInt(3, departmentId);
    st.run();
    return static_cast<int>(db.lastInsertId());
}

vector<Course> SqliteCourseRepository::readCourses(Statement& st) {
    vector<Course> list;
    while (st.step()) {
        Course c;
        c.id = static_cast<int>(st.getInt(0));
        c.code = st.getText(1);
        c.title = st.getText(2);
        c.departmentId = static_cast<int>(st.getInt(3));
        list.push_back(c);
    }
    return list;
}

optional<Course> SqliteCourseRepository::findCourse(int courseId) {
    Statement st(db, "SELECT course_id, code, title, department_id FROM courses WHERE course_id = ?");
    st.bindInt(1, courseId);
    vector<Course> list = readCourses(st);
    if (list.empty()) return nullopt;
    return list[0];
}

vector<Course> SqliteCourseRepository::listCourses() {
    Statement st(db, "SELECT course_id, code, title, department_id FROM courses ORDER BY code");
    return readCourses(st);
}

vector<Course> SqliteCourseRepository::coursesForStudent(int studentId) {
    Statement st(db, "SELECT c.course_id, c.code, c.title, c.department_id FROM courses c "
                     "JOIN enrolments e ON e.course_id = c.course_id "
                     "WHERE e.student_id = ? ORDER BY c.code");
    st.bindInt(1, studentId);
    return readCourses(st);
}

vector<Course> SqliteCourseRepository::coursesForLecturer(int lecturerId) {
    Statement st(db, "SELECT c.course_id, c.code, c.title, c.department_id FROM courses c "
                     "JOIN course_lecturers cl ON cl.course_id = c.course_id "
                     "WHERE cl.lecturer_id = ? ORDER BY c.code");
    st.bindInt(1, lecturerId);
    return readCourses(st);
}

vector<Course> SqliteCourseRepository::coursesForDepartment(int departmentId) {
    Statement st(db, "SELECT course_id, code, title, department_id FROM courses "
                     "WHERE department_id = ? ORDER BY code");
    st.bindInt(1, departmentId);
    return readCourses(st);
}

void SqliteCourseRepository::assignLecturer(int courseId, int lecturerId) {
    Statement st(db, "INSERT OR IGNORE INTO course_lecturers (course_id, lecturer_id) VALUES (?, ?)");
    st.bindInt(1, courseId);
    st.bindInt(2, lecturerId);
    st.run();
}

void SqliteCourseRepository::enrol(int studentId, int courseId) {
    Statement st(db, "INSERT OR IGNORE INTO enrolments (student_id, course_id) VALUES (?, ?)");
    st.bindInt(1, studentId);
    st.bindInt(2, courseId);
    st.run();
}

bool SqliteCourseRepository::isLecturerOfCourse(int lecturerId, int courseId) {
    Statement st(db, "SELECT 1 FROM course_lecturers WHERE lecturer_id = ? AND course_id = ?");
    st.bindInt(1, lecturerId);
    st.bindInt(2, courseId);
    return st.step();
}

bool SqliteCourseRepository::isEnrolled(int studentId, int courseId) {
    Statement st(db, "SELECT 1 FROM enrolments WHERE student_id = ? AND course_id = ?");
    st.bindInt(1, studentId);
    st.bindInt(2, courseId);
    return st.step();
}

vector<UserRow> SqliteCourseRepository::studentsInCourse(int courseId) {
    vector<UserRow> rows;
    Statement st(db, "SELECT u.user_id, u.username, u.full_name, u.role, u.is_active FROM users u "
                     "JOIN enrolments e ON e.student_id = u.user_id "
                     "WHERE e.course_id = ? ORDER BY u.full_name");
    st.bindInt(1, courseId);
    while (st.step()) {
        UserRow r;
        r.id = static_cast<int>(st.getInt(0));
        r.username = st.getText(1);
        r.fullName = st.getText(2);
        r.role = st.getText(3);
        r.active = st.getInt(4) == 1;
        rows.push_back(r);
    }
    return rows;
}


Assignment SqliteAssignmentRepository::readAssignment(Statement& st) {
    Assignment a;
    a.id = static_cast<int>(st.getInt(0));
    a.courseId = static_cast<int>(st.getInt(1));
    a.title = st.getText(2);
    a.description = st.getText(3);
    a.deadline = st.getText(4);
    a.maxMark = st.getDouble(5);
    a.createdBy = static_cast<int>(st.getInt(6));
    return a;
}

int SqliteAssignmentRepository::add(const Assignment& a) {
    Statement st(db, "INSERT INTO assignments (course_id, title, description, deadline, max_mark, created_by) "
                     "VALUES (?, ?, ?, ?, ?, ?)");
    st.bindInt(1, a.courseId);
    st.bindText(2, a.title);
    st.bindText(3, a.description);
    st.bindText(4, a.deadline);
    st.bindDouble(5, a.maxMark);
    st.bindInt(6, a.createdBy);
    st.run();
    return static_cast<int>(db.lastInsertId());
}

optional<Assignment> SqliteAssignmentRepository::findById(int assignmentId) {
    Statement st(db, "SELECT assignment_id, course_id, title, description, deadline, max_mark, created_by "
                     "FROM assignments WHERE assignment_id = ?");
    st.bindInt(1, assignmentId);
    if (!st.step()) return nullopt;
    return readAssignment(st);
}

vector<Assignment> SqliteAssignmentRepository::listByCourse(int courseId) {
    vector<Assignment> list;
    Statement st(db, "SELECT assignment_id, course_id, title, description, deadline, max_mark, created_by "
                     "FROM assignments WHERE course_id = ? ORDER BY deadline");
    st.bindInt(1, courseId);
    while (st.step()) list.push_back(readAssignment(st));
    return list;
}

void SqliteAssignmentRepository::saveExtension(int assignmentId, int studentId, const string& newDeadline,
                                               const string& reason, int grantedBy) {
    // one extension per student per assignment: a second one replaces the first
    Statement st(db, "INSERT INTO extensions (assignment_id, student_id, new_deadline, reason, granted_by) "
                     "VALUES (?, ?, ?, ?, ?) "
                     "ON CONFLICT(assignment_id, student_id) DO UPDATE SET "
                     "new_deadline = excluded.new_deadline, reason = excluded.reason, "
                     "granted_by = excluded.granted_by, granted_at = datetime('now')");
    st.bindInt(1, assignmentId);
    st.bindInt(2, studentId);
    st.bindText(3, newDeadline);
    st.bindText(4, reason);
    st.bindInt(5, grantedBy);
    st.run();
}

optional<string> SqliteAssignmentRepository::findExtension(int assignmentId, int studentId) {
    Statement st(db, "SELECT new_deadline FROM extensions WHERE assignment_id = ? AND student_id = ?");
    st.bindInt(1, assignmentId);
    st.bindInt(2, studentId);
    if (!st.step()) return nullopt;
    return st.getText(0);
}


static const string SUBMISSION_SELECT =
    "SELECT s.submission_id, s.assignment_id, s.student_id, u.full_name, a.title, s.status, "
    "s.raw_mark, a.max_mark, s.days_late, s.feedback, s.marked_by, s.moderated_by, s.released_at "
    "FROM submissions s "
    "JOIN assignments a ON a.assignment_id = s.assignment_id "
    "JOIN users u ON u.user_id = s.student_id ";

Submission SqliteSubmissionRepository::readSubmission(Statement& st) {
    Submission s;
    s.id = static_cast<int>(st.getInt(0));
    s.assignmentId = static_cast<int>(st.getInt(1));
    s.studentId = static_cast<int>(st.getInt(2));
    s.studentName = st.getText(3);
    s.assignmentTitle = st.getText(4);
    s.status = st.getText(5);
    s.hasMark = !st.isNull(6);
    s.rawMark = s.hasMark ? st.getDouble(6) : 0;
    s.maxMark = st.getDouble(7);
    s.daysLate = static_cast<int>(st.getInt(8));
    s.feedback = st.getText(9);
    s.markedBy = st.isNull(10) ? 0 : static_cast<int>(st.getInt(10));
    s.moderatedBy = st.isNull(11) ? 0 : static_cast<int>(st.getInt(11));
    s.releasedAt = st.getText(12);
    return s;
}

optional<Submission> SqliteSubmissionRepository::find(int assignmentId, int studentId) {
    Statement st(db, SUBMISSION_SELECT + "WHERE s.assignment_id = ? AND s.student_id = ?");
    st.bindInt(1, assignmentId);
    st.bindInt(2, studentId);
    if (!st.step()) return nullopt;
    return readSubmission(st);
}

optional<Submission> SqliteSubmissionRepository::findById(int submissionId) {
    Statement st(db, SUBMISSION_SELECT + "WHERE s.submission_id = ?");
    st.bindInt(1, submissionId);
    if (!st.step()) return nullopt;
    return readSubmission(st);
}

vector<Submission> SqliteSubmissionRepository::listByAssignment(int assignmentId) {
    vector<Submission> list;
    Statement st(db, SUBMISSION_SELECT + "WHERE s.assignment_id = ? ORDER BY u.full_name");
    st.bindInt(1, assignmentId);
    while (st.step()) list.push_back(readSubmission(st));
    return list;
}

vector<Submission> SqliteSubmissionRepository::listByStudent(int studentId) {
    vector<Submission> list;
    Statement st(db, SUBMISSION_SELECT + "WHERE s.student_id = ? ORDER BY s.submission_id");
    st.bindInt(1, studentId);
    while (st.step()) list.push_back(readSubmission(st));
    return list;
}

void SqliteSubmissionRepository::create(int assignmentId, int studentId, const string& status,
                                        int daysLate, const string& content, const string& submittedAt) {
    db.exec("BEGIN");
    try {
        {
            Statement st(db, "INSERT INTO submissions (assignment_id, student_id, status, days_late) "
                             "VALUES (?, ?, ?, ?)");
            st.bindInt(1, assignmentId);
            st.bindInt(2, studentId);
            st.bindText(3, status);
            st.bindInt(4, daysLate);
            st.run();
        }
        int submissionId = static_cast<int>(db.lastInsertId());
        {
            Statement st(db, "INSERT INTO submission_versions (submission_id, version_no, content, submitted_at) "
                             "VALUES (?, 1, ?, ?)");
            st.bindInt(1, submissionId);
            st.bindText(2, content);
            st.bindText(3, submittedAt);
            st.run();
        }
        db.exec("COMMIT");
    } catch (...) {
        try { db.exec("ROLLBACK"); } catch (...) { /* keep the original error */ }   // undo both inserts
        throw;
    }
}

int SqliteSubmissionRepository::addVersion(int submissionId, const string& content,
                                           const string& submittedAt, const string& status, int daysLate) {
    db.exec("BEGIN");
    try {
        int versionNo = 0;
        {
            Statement st(db, "SELECT COALESCE(MAX(version_no), 0) + 1 FROM submission_versions "
                             "WHERE submission_id = ?");
            st.bindInt(1, submissionId);
            st.step();
            versionNo = static_cast<int>(st.getInt(0));
        }
        {
            Statement st(db, "INSERT INTO submission_versions (submission_id, version_no, content, submitted_at) "
                             "VALUES (?, ?, ?, ?)");
            st.bindInt(1, submissionId);
            st.bindInt(2, versionNo);
            st.bindText(3, content);
            st.bindText(4, submittedAt);
            st.run();
        }
        {
            Statement st(db, "UPDATE submissions SET status = ?, days_late = ? "
                             "WHERE submission_id = ? AND status IN ('SUBMITTED','LATE')");
            st.bindText(1, status);
            st.bindInt(2, daysLate);
            st.bindInt(3, submissionId);
            if (st.run() != 1) throw DatabaseError("This work can no longer be replaced.");
        }
        db.exec("COMMIT");
        return versionNo;
    } catch (...) {
        try { db.exec("ROLLBACK"); } catch (...) { /* keep the original error */ }
        throw;
    }
}

string SqliteSubmissionRepository::latestContent(int submissionId) {
    Statement st(db, "SELECT content FROM submission_versions WHERE submission_id = ? "
                     "ORDER BY version_no DESC LIMIT 1");
    st.bindInt(1, submissionId);
    if (!st.step()) return "";
    return st.getText(0);
}

optional<string> SqliteSubmissionRepository::latestSubmittedAt(int submissionId) {
    Statement st(db, "SELECT submitted_at FROM submission_versions WHERE submission_id = ? "
                     "ORDER BY version_no DESC LIMIT 1");
    st.bindInt(1, submissionId);
    if (!st.step()) return nullopt;
    return st.getText(0);
}

void SqliteSubmissionRepository::updateTiming(int submissionId, const string& status, int daysLate) {
    Statement st(db, "UPDATE submissions SET status = ?, days_late = ? "
                     "WHERE submission_id = ? AND status IN ('SUBMITTED','LATE')");
    st.bindText(1, status);
    st.bindInt(2, daysLate);
    st.bindInt(3, submissionId);
    st.run();
}

void SqliteSubmissionRepository::saveMark(int submissionId, double newRaw, const string& feedback,
                                          const string& newStatus, int changedBy, bool byModerator,
                                          const string& reason) {
    // Read the previous mark, update the submission and write the audit row
    // in one transaction so the audit always describes the exact change.
    string who = byModerator ? "moderated_by" : "marked_by";
    db.exec("BEGIN");
    try {
        bool hadOld = false;
        double oldValue = 0;
        {
            Statement st(db, "SELECT raw_mark FROM submissions WHERE submission_id = ?");
            st.bindInt(1, submissionId);
            if (!st.step()) throw DatabaseError("Cannot update a submission that does not exist.");
            if (!st.isNull(0)) {
                hadOld = true;
                oldValue = st.getDouble(0);
            }
        }
        {
            Statement st(db, "UPDATE submissions SET raw_mark = ?, feedback = ?, status = ?, " + who +
                             " = ? WHERE submission_id = ?");
            st.bindDouble(1, newRaw);
            st.bindText(2, feedback);
            st.bindText(3, newStatus);
            st.bindInt(4, changedBy);
            st.bindInt(5, submissionId);
            if (st.run() != 1) throw DatabaseError("The submission could not be updated.");
        }
        {
            Statement st(db, "INSERT INTO mark_audit (submission_id, changed_by, changed_at, old_value, new_value, reason) "
                             "VALUES (?, ?, datetime('now'), ?, ?, ?)");
            st.bindInt(1, submissionId);
            st.bindInt(2, changedBy);
            if (hadOld) st.bindDouble(3, oldValue); else st.bindNull(3);
            st.bindDouble(4, newRaw);
            st.bindText(5, reason);
            st.run();
        }
        db.exec("COMMIT");
    } catch (...) {
        try { db.exec("ROLLBACK"); } catch (...) { /* preserve the original error */ }
        throw;
    }
}

int SqliteSubmissionRepository::approveAll(int assignmentId, int hodId) {
    Statement st(db, "UPDATE submissions SET status = 'MODERATED', moderated_by = ? "
                     "WHERE assignment_id = ? AND status = 'MARKED'");
    st.bindInt(1, hodId);
    st.bindInt(2, assignmentId);
    return st.run();
}

int SqliteSubmissionRepository::releaseAll(int assignmentId, const string& when) {
    Statement st(db, "UPDATE submissions SET status = 'RELEASED', released_at = ? "
                     "WHERE assignment_id = ? AND status = 'MODERATED'");
    st.bindText(1, when);
    st.bindInt(2, assignmentId);
    return st.run();
}

// ---------------------------------------------------------------- audit

vector<MarkAudit> SqliteAuditRepository::findForDepartment(int departmentId) {
    vector<MarkAudit> list;
    Statement st(db,
        "SELECT m.audit_id, m.submission_id, stu.full_name, a.title, changer.full_name, "
        "m.changed_at, m.old_value, m.new_value, m.reason "
        "FROM mark_audit m "
        "JOIN submissions s ON s.submission_id = m.submission_id "
        "JOIN assignments a ON a.assignment_id = s.assignment_id "
        "JOIN courses c ON c.course_id = a.course_id "
        "JOIN users stu ON stu.user_id = s.student_id "
        "JOIN users changer ON changer.user_id = m.changed_by "
        "WHERE c.department_id = ? ORDER BY m.audit_id");
    st.bindInt(1, departmentId);
    while (st.step()) {
        MarkAudit m;
        m.id = static_cast<int>(st.getInt(0));
        m.submissionId = static_cast<int>(st.getInt(1));
        m.studentName = st.getText(2);
        m.assignmentTitle = st.getText(3);
        m.changedByName = st.getText(4);
        m.changedAt = st.getText(5);
        m.hasOld = !st.isNull(6);
        m.oldValue = m.hasOld ? st.getDouble(6) : 0;
        m.newValue = st.getDouble(7);
        m.reason = st.getText(8);
        list.push_back(m);
    }
    return list;
}
