#include "Services.h"
#include "Security.h"
#include <cmath>
using namespace std;


static Assignment loadAssignment(IAssignmentRepository& repo, int assignmentId) {
    optional<Assignment> a = repo.findById(assignmentId);
    if (!a) throw LmsError("There is no assignment with ID " + to_string(assignmentId) + ".");
    return *a;
}

static void requireText(const string& value, const string& fieldName) {
    if (value.empty()) throw LmsError(fieldName + " cannot be empty.");
}

// The department a Head of Department currently heads (0 = none). This is looked up
// every time, so a change made by the administrator applies immediately and a head who
// has been replaced loses access straight away, even if they are still logged in.
static int departmentOfHod(ICourseRepository& courses, int hodUserId) {
    for (const Department& d : courses.listDepartments())
        if (d.hodUserId == hodUserId) return d.id;
    return 0;
}

static string markRangeMessage(double maxMark) {
    return "The mark must be between 0 and " + formatNumber(maxMark) + ".";
}


unique_ptr<User> AuthService::login(const string& username, const string& password) {
    optional<Credentials> c = users.getCredentials(username);
    
    const string failMessage = "Invalid username or password.";
    if (!c) throw LmsError(failMessage);
    if (!safeEquals(hasher.hash(password, c->salt), c->passwordHash)) throw LmsError(failMessage);
    if (!c->active) throw LmsError("This account has been disabled. Please see the administrator.");
    return users.findById(c->userId);
}

void AuthService::changePassword(const User& user, const string& oldPassword, const string& newPassword) {
    optional<Credentials> c = users.getCredentials(user.getUsername());
    if (!c || !safeEquals(hasher.hash(oldPassword, c->salt), c->passwordHash))
        throw LmsError("Your current password is wrong.");
    if (newPassword.size() < 6) throw LmsError("New password must be at least 6 characters.");
    string salt = hasher.makeSalt();
    users.updatePassword(user.getId(), hasher.hash(newPassword, salt), salt);
}


int AdminService::createUser(const User& actor, Role role, const string& username,
                             const string& fullName, const string& password) {
    requirePermission(actor, Permission::MANAGE_USERS);
    if (users.getCredentials(username)) throw LmsError("That username is already taken.");
    return users.add(factory.prepareNewUser(role, username, fullName, password));
}

vector<UserRow> AdminService::listUsers(const User& actor) {
    requirePermission(actor, Permission::MANAGE_USERS);
    return users.listAll();
}

void AdminService::setUserActive(const User& actor, int userId, bool active) {
    requirePermission(actor, Permission::MANAGE_USERS);
    if (userId == actor.getId()) throw LmsError("You cannot disable your own account.");
    if (!users.findById(userId)) throw LmsError("There is no user with ID " + to_string(userId) + ".");
    users.setActive(userId, active);
}

int AdminService::createDepartment(const User& actor, const string& name) {
    requirePermission(actor, Permission::MANAGE_COURSES);
    requireText(name, "Department name");
    for (const Department& d : courses.listDepartments())
        if (d.name == name) throw LmsError("A department with that name already exists.");
    return courses.addDepartment(name);
}

void AdminService::assignHod(const User& actor, int departmentId, int userId) {
    requirePermission(actor, Permission::MANAGE_COURSES);
    unique_ptr<User> u = users.findById(userId);
    if (!u) throw LmsError("There is no user with ID " + to_string(userId) + ".");
    if (u->getRole() != Role::HOD) throw LmsError("That user is not a Head of Department.");
    bool found = false;
    for (const Department& d : courses.listDepartments()) {
        if (d.id == departmentId) found = true;
        else if (d.hodUserId == userId) throw LmsError("That person already heads another department.");
    }
    if (!found) throw LmsError("There is no department with ID " + to_string(departmentId) + ".");
    courses.setHod(departmentId, userId);
}

vector<Department> AdminService::listDepartments(const User& actor) {
    requirePermission(actor, Permission::MANAGE_COURSES);
    return courses.listDepartments();
}

int AdminService::createCourse(const User& actor, const string& code, const string& title, int departmentId) {
    requirePermission(actor, Permission::MANAGE_COURSES);
    requireText(code, "Course code");
    requireText(title, "Course title");
    bool found = false;
    for (const Department& d : courses.listDepartments())
        if (d.id == departmentId) found = true;
    if (!found) throw LmsError("There is no department with ID " + to_string(departmentId) + ".");
    for (const Course& c : courses.listCourses())
        if (c.code == code) throw LmsError("A course with that code already exists.");
    return courses.addCourse(code, title, departmentId);
}

vector<Course> AdminService::listCourses(const User& actor) {
    requirePermission(actor, Permission::MANAGE_COURSES);
    return courses.listCourses();
}

void AdminService::assignLecturer(const User& actor, int courseId, int lecturerId) {
    requirePermission(actor, Permission::MANAGE_COURSES);
    if (!courses.findCourse(courseId)) throw LmsError("There is no course with ID " + to_string(courseId) + ".");
    unique_ptr<User> u = users.findById(lecturerId);
    if (!u || u->getRole() != Role::LECTURER) throw LmsError("That user is not a lecturer.");
    courses.assignLecturer(courseId, lecturerId);
}

void AdminService::enrolStudent(const User& actor, int studentId, int courseId) {
    requirePermission(actor, Permission::MANAGE_COURSES);
    if (!courses.findCourse(courseId)) throw LmsError("There is no course with ID " + to_string(courseId) + ".");
    unique_ptr<User> u = users.findById(studentId);
    if (!u || u->getRole() != Role::STUDENT) throw LmsError("That user is not a student.");
    courses.enrol(studentId, courseId);
}


int AssignmentService::createAssignment(const User& lecturer, int courseId, const string& title,
                                        const string& description, const string& deadlineInput,
                                        double maxMark) {
    requirePermission(lecturer, Permission::CREATE_ASSIGNMENT);
    if (!courses.isLecturerOfCourse(lecturer.getId(), courseId))
        throw LmsError("You are not assigned to that course.");
    requireText(title, "Title");
    if (!isfinite(maxMark) || maxMark <= 0) throw LmsError("Maximum mark must be more than 0.");

    Assignment a;
    a.courseId = courseId;
    a.title = title;
    a.description = description;
    a.deadline = normaliseDateTime(deadlineInput);
    a.maxMark = maxMark;
    a.createdBy = lecturer.getId();
    return assignments.add(a);
}

ExtensionResult AssignmentService::grantExtension(const User& lecturer, int assignmentId, int studentId,
                                                  const string& newDeadlineInput, const string& reason) {
    requirePermission(lecturer, Permission::GRANT_EXTENSION);
    Assignment a = loadAssignment(assignments, assignmentId);
    if (!courses.isLecturerOfCourse(lecturer.getId(), a.courseId))
        throw LmsError("You are not assigned to that course.");
    if (!courses.isEnrolled(studentId, a.courseId))
        throw LmsError("That student is not enrolled in this course.");
    requireText(reason, "Reason");
    string newDeadline = normaliseDateTime(newDeadlineInput);
    if (newDeadline <= a.deadline) throw LmsError("The extension must be later than the normal deadline.");
    assignments.saveExtension(assignmentId, studentId, newDeadline, reason, lecturer.getId());

    // Work that is already in but not yet marked is judged against the new deadline, so a
    // student who handed in "late" before the extension is no longer penalised for it.
    // Marked work is left alone: changing it would silently change a mark that has been
    // audited (the Head of Department can return it for re-marking).
    ExtensionResult result;
    optional<Submission> existing = subs.find(assignmentId, studentId);
    if (!existing) return result;
    result.hasSubmission = true;
    if (existing->status != Status::SUBMITTED && existing->status != Status::LATE) return result;
    optional<string> submittedAt = subs.latestSubmittedAt(existing->id);
    if (!submittedAt) return result;
    bool late = *submittedAt > newDeadline;
    result.status = late ? Status::LATE : Status::SUBMITTED;
    result.daysLate = late ? calcDaysLate(*submittedAt, newDeadline) : 0;
    subs.updateTiming(existing->id, result.status, result.daysLate);
    result.rechecked = true;
    return result;
}

vector<Course> AssignmentService::coursesFor(const User& user) {
    switch (user.getRole()) {
        case Role::STUDENT:  return courses.coursesForStudent(user.getId());
        case Role::LECTURER: return courses.coursesForLecturer(user.getId());
        case Role::HOD: {
            int department = departmentOfHod(courses, user.getId());
            if (department == 0) return {};
            return courses.coursesForDepartment(department);
        }
        default: return {};
    }
}

vector<Assignment> AssignmentService::assignmentsOfCourse(const User& user, int courseId) {
    bool allowed = false;
    for (const Course& c : coursesFor(user))
        if (c.id == courseId) allowed = true;
    if (!allowed) throw LmsError("You do not have access to that course.");
    return assignments.listByCourse(courseId);
}

vector<UserRow> AssignmentService::studentsOfAssignment(const User& lecturer, int assignmentId) {
    requirePermission(lecturer, Permission::GRANT_EXTENSION);
    Assignment a = loadAssignment(assignments, assignmentId);
    if (!courses.isLecturerOfCourse(lecturer.getId(), a.courseId))
        throw LmsError("You are not assigned to that course.");
    return courses.studentsInCourse(a.courseId);
}

string AssignmentService::effectiveDeadline(const Assignment& assignment, int studentId) {
    optional<string> ext = assignments.findExtension(assignment.id, studentId);
    return ext ? *ext : assignment.deadline;
}

SubmitResult SubmissionService::submit(const User& student, int assignmentId, const string& content) {
    requirePermission(student, Permission::SUBMIT_WORK);
    if (content.empty()) throw LmsError("You cannot submit empty work.");
    Assignment a = loadAssignment(assignments, assignmentId);
    if (!courses.isEnrolled(student.getId(), a.courseId))
        throw LmsError("You are not enrolled in the course for this assignment.");

    // effective deadline = personal extension if there is one
    optional<string> ext = assignments.findExtension(assignmentId, student.getId());
    string deadline = ext ? *ext : a.deadline;
    string now = nowString();
    bool late = now > deadline;   // text comparison works because of the date format

    SubmitResult result;
    optional<Submission> existing = subs.find(assignmentId, student.getId());
    if (existing) {
        // resubmission rules: only unmarked work, and only before the effective deadline
        if (existing->status != Status::SUBMITTED && existing->status != Status::LATE)
            throw LmsError("This work has already been marked, so it cannot be replaced.");
        if (late) throw LmsError("The deadline has passed, so you can no longer resubmit.");
        // Being here means the work is on time now (e.g. an extension was granted), so the
        // submission is on time as well.
        result.version = subs.addVersion(existing->id, content, now, Status::SUBMITTED, 0);
        result.status = Status::SUBMITTED;
        result.resubmission = true;
        return result;
    }

    result.daysLate = late ? calcDaysLate(now, deadline) : 0;
    result.status = late ? Status::LATE : Status::SUBMITTED;   // late work is accepted, not rejected
    subs.create(assignmentId, student.getId(), result.status, result.daysLate, content, now);
    return result;
}

vector<Submission> SubmissionService::mySubmissions(const User& student) {
    requirePermission(student, Permission::SUBMIT_WORK);
    vector<Submission> list = subs.listByStudent(student.getId());
    for (Submission& s : list) {
        if (s.status != Status::RELEASED) {   // hide marks until the HoD releases them
            s.hasMark = false;
            s.rawMark = 0;
            s.feedback = "";
        }
    }
    return list;
}

void SubmissionService::requireLecturerOfAssignment(const User& lecturer, int assignmentId) {
    requirePermission(lecturer, Permission::MARK_WORK);
    Assignment a = loadAssignment(assignments, assignmentId);
    if (!courses.isLecturerOfCourse(lecturer.getId(), a.courseId))
        throw LmsError("You are not assigned to that course.");
}

vector<Submission> SubmissionService::listForAssignment(const User& lecturer, int assignmentId) {
    requireLecturerOfAssignment(lecturer, assignmentId);
    return subs.listByAssignment(assignmentId);
}

Submission SubmissionService::getSubmission(const User& lecturer, int submissionId) {
    optional<Submission> s = subs.findById(submissionId);
    if (!s) throw LmsError("There is no submission with ID " + to_string(submissionId) + ".");
    requireLecturerOfAssignment(lecturer, s->assignmentId);
    return *s;
}

string SubmissionService::viewWork(const User& lecturer, int submissionId) {
    Submission s = getSubmission(lecturer, submissionId);
    return subs.latestContent(s.id);
}

void SubmissionService::markSubmission(const User& lecturer, int submissionId, double rawMark,
                                       const string& feedback, const string& reason) {
    Submission s = getSubmission(lecturer, submissionId);   // also checks the lecturer's rights
    if (!isfinite(rawMark) || rawMark < 0 || rawMark > s.maxMark)
        throw LmsError(markRangeMessage(s.maxMark));
    if (feedback.empty()) throw LmsError("Written feedback is required.");

    string why;
    if (s.status == Status::SUBMITTED || s.status == Status::LATE) {
        why = "Initial marking";
    } else if (s.status == Status::MARKED || s.status == Status::RETURNED) {
        if (reason.empty()) throw LmsError("You must give a reason when you change a mark.");
        why = reason;
    } else {
        throw LmsError("This work is already moderated. Ask the Head of Department to return it first.");
    }
    subs.saveMark(s.id, rawMark, feedback, Status::MARKED, lecturer.getId(), false, why);
}

// ================================================================ ModerationService

int ModerationService::departmentOf(const User& hod) {
    requirePermission(hod, Permission::MODERATE_MARKS);
    int department = departmentOfHod(courses, hod.getId());
    if (department == 0) throw LmsError("You are not assigned to a department yet.");
    return department;
}

void ModerationService::checkAssignmentInDepartment(const User& hod, int assignmentId) {
    int dept = departmentOf(hod);
    Assignment a = loadAssignment(assignments, assignmentId);
    optional<Course> c = courses.findCourse(a.courseId);
    if (!c || c->departmentId != dept) throw LmsError("That assignment is not in your department.");
}

Submission ModerationService::loadOwnSubmission(const User& hod, int submissionId) {
    optional<Submission> s = subs.findById(submissionId);
    if (!s) throw LmsError("There is no submission with ID " + to_string(submissionId) + ".");
    checkAssignmentInDepartment(hod, s->assignmentId);
    return *s;
}

vector<Submission> ModerationService::listForAssignment(const User& hod, int assignmentId) {
    checkAssignmentInDepartment(hod, assignmentId);
    return subs.listByAssignment(assignmentId);
}

int ModerationService::approveAll(const User& hod, int assignmentId) {
    checkAssignmentInDepartment(hod, assignmentId);
    return subs.approveAll(assignmentId, hod.getId());
}

int ModerationService::releaseAll(const User& hod, int assignmentId) {
    checkAssignmentInDepartment(hod, assignmentId);
    // only MODERATED work is released, so nothing can skip moderation
    return subs.releaseAll(assignmentId, nowString());
}

void ModerationService::returnToLecturer(const User& hod, int submissionId, const string& reason) {
    Submission s = loadOwnSubmission(hod, submissionId);
    if (s.status != Status::MARKED && s.status != Status::MODERATED)
        throw LmsError("Only marked or moderated work can be returned.");
    if (reason.empty()) throw LmsError("You must give a reason.");
    subs.saveMark(s.id, s.rawMark, s.feedback, Status::RETURNED, hod.getId(), true,
                  "Returned to lecturer: " + reason);
}

void ModerationService::adjustMark(const User& hod, int submissionId, double newRawMark, const string& reason) {
    Submission s = loadOwnSubmission(hod, submissionId);
    if (s.status != Status::MARKED && s.status != Status::MODERATED)
        throw LmsError("Only marked or moderated work can be adjusted.");
    if (!isfinite(newRawMark) || newRawMark < 0 || newRawMark > s.maxMark)
        throw LmsError(markRangeMessage(s.maxMark));
    if (reason.empty()) throw LmsError("You must give a reason.");
    // status stays the same (MODERATED stays MODERATED)
    subs.saveMark(s.id, newRawMark, s.feedback, s.status, hod.getId(), true, "HoD adjustment: " + reason);
}

vector<MarkAudit> ModerationService::auditLog(const User& hod) {
    requirePermission(hod, Permission::VIEW_AUDIT);
    return audit.findForDepartment(departmentOf(hod));
}
