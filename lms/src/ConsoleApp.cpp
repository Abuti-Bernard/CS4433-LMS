#include "ConsoleApp.h"
#include <climits>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "Input.h"
using namespace std;


static string fmt(double value) {           
    ostringstream out;
    out << fixed << setprecision(2) << value;
    return out.str();
}

static void heading(const string& text) {
    cout << "\n=== " << text << " ===\n";
}

static int askId(const string& prompt) {
    return readInt(prompt, 1, INT_MAX);
}

template <typename T>
static void safely(T action) {
    try {
        action();
    } catch (const InputClosed&) {
        throw;                                   
    } catch (const LmsError& e) {
        cout << "\n[!] " << e.what() << "\n";
    } catch (const DatabaseError& e) {
        cout << "\n[!] Database problem: " << e.what() << "\n";
    } catch (const exception& e) {
        cout << "\n[!] Something went wrong: " << e.what() << "\n";
    }
}


void ConsoleApp::run() {
    try {
        while (true) {
            heading("School Learning Management System");
            cout << "1. Login\n0. Exit\n";
            int choice = readInt("Choose: ", 0, 1);
            if (choice == 0) break;
            loginScreen();
        }
    } catch (const InputClosed&) {
        cout << "\n(input ended)\n";
    }
    cout << "Goodbye.\n";
}

void ConsoleApp::loginScreen() {
    safely([&] {
        string username = readLine("Username: ");
        string password = readPassword("Password: ");
        unique_ptr<User> user = auth.login(username, password);
        cout << "\nWelcome, " << user->getFullName() << " (" << roleToString(user->getRole()) << ")\n";
        switch (user->getRole()) {
            case Role::STUDENT:  studentMenu(*user);  break;
            case Role::LECTURER: lecturerMenu(*user); break;
            case Role::HOD:      hodMenu(*user);      break;
            case Role::ADMIN:    adminMenu(*user);    break;
        }
    });
}

void ConsoleApp::changePassword(const User& user) {
    string oldPw = readPassword("Current password: ");
    string newPw = readPassword("New password (min 6 characters): ");
    string again = readPassword("Repeat new password: ");
    if (newPw != again) throw LmsError("The two new passwords are not the same.");
    auth.changePassword(user, oldPw, newPw);
    cout << "Password changed.\n";
}

void ConsoleApp::showCoursesAndAssignments(const User& user) {
    vector<Course> courses = assignments.coursesFor(user);
    if (courses.empty()) cout << "No courses to show.\n";
    for (const Course& c : courses) {
        cout << "\n" << c.code << " - " << c.title << "\n";
        vector<Assignment> list = assignments.assignmentsOfCourse(user, c.id);
        if (list.empty()) cout << "   (no assignments yet)\n";
        for (const Assignment& a : list) {
            cout << "   [ID " << a.id << "] " << a.title << "  (max mark " << fmt(a.maxMark) << ")\n";
            string deadline = a.deadline;
            if (user.getRole() == Role::STUDENT) deadline = assignments.effectiveDeadline(a, user.getId());
            cout << "        Deadline: " << deadline << " UTC";
            if (deadline != a.deadline) cout << "  (your extension; normal deadline " << a.deadline << ")";
            cout << "\n";
        }
    }
}

void ConsoleApp::showSubmissionTable(const vector<Submission>& list, bool showMarks) {
    if (list.empty()) {
        cout << "Nothing to show.\n";
        return;
    }
    cout << left << setw(6) << "ID" << setw(22) << "Student" << setw(22) << "Assignment"
         << setw(11) << "Status" << setw(6) << "Late";
    if (showMarks) cout << setw(9) << "Raw" << setw(9) << "Final";
    cout << "\n" << string(showMarks ? 85 : 67, '-') << "\n";
    for (const Submission& s : list) {
        cout << left << setw(6) << s.id << setw(22) << s.studentName.substr(0, 20)
             << setw(22) << s.assignmentTitle.substr(0, 20) << setw(11) << s.status
             << setw(6) << s.daysLate;
        if (showMarks) {
            if (s.hasMark) cout << setw(9) << fmt(s.rawMark) << setw(9) << fmt(s.finalMark(policy));
            else cout << setw(9) << "-" << setw(9) << "-";
        }
        cout << "\n";
    }
}


void ConsoleApp::studentMenu(const User& user) {
    while (true) {
        heading("Student menu");
        cout << "1. My courses and assignments\n2. Submit or resubmit work\n"
             << "3. My submissions and marks\n4. Change password\n0. Logout\n";
        int choice = readInt("Choose: ", 0, 4);
        if (choice == 0) return;
        safely([&] {
            if (choice == 1) showCoursesAndAssignments(user);
            else if (choice == 2) submitWork(user);
            else if (choice == 3) showMySubmissions(user);
            else changePassword(user);
        });
    }
}

void ConsoleApp::submitWork(const User& user) {
    showCoursesAndAssignments(user);
    int assignmentId = askId("\nAssignment ID to submit (see list above): ");
    cout << "1. Submit a file\n2. Type my answer\n";
    int how = readInt("Choose: ", 1, 2);
    string content;
    if (how == 1) {
        string path = readLine("File path: ");
        ifstream file(path);
        if (!file) throw LmsError("Could not open that file.");
        stringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
    } else {
        content = readLine("Type your answer: ");
    }
    SubmitResult r = submissions.submit(user, assignmentId, content);
    if (r.resubmission)
        cout << "Resubmission saved (version " << r.version << "). The latest version will be marked.\n";
    else if (r.status == Status::LATE)
        cout << "Submitted LATE by " << r.daysLate << " day(s). A late penalty will be applied when it is marked.\n";
    else
        cout << "Submitted on time. You may resubmit until the deadline.\n";
}

void ConsoleApp::showMySubmissions(const User& user) {
    vector<Submission> list = submissions.mySubmissions(user);
    if (list.empty()) {
        cout << "You have not submitted anything yet.\n";
        return;
    }
    for (const Submission& s : list) {
        cout << "\n" << s.assignmentTitle << "  - status: " << s.status;
        if (s.daysLate > 0) cout << "  (" << s.daysLate << " day(s) late)";
        cout << "\n";
        if (s.status == Status::RELEASED) {
            cout << "   Mark before penalty: " << fmt(s.rawMark) << " / " << fmt(s.maxMark) << "\n";
            cout << "   Final mark:          " << fmt(s.finalMark(policy)) << " / " << fmt(s.maxMark) << "\n";
            cout << "   Feedback: " << s.feedback << "\n";
        } else {
            cout << "   Your mark is not available yet.\n";
        }
    }
}


void ConsoleApp::lecturerMenu(const User& user) {
    while (true) {
        heading("Lecturer menu");
        cout << "1. My courses and assignments\n2. Create assignment\n3. Grant extension to a student\n"
             << "4. View submissions for an assignment\n5. Mark a submission\n"
             << "6. Change password\n0. Logout\n";
        int choice = readInt("Choose: ", 0, 6);
        if (choice == 0) return;
        safely([&] {
            if (choice == 1) showCoursesAndAssignments(user);
            else if (choice == 2) createAssignment(user);
            else if (choice == 3) grantExtension(user);
            else if (choice == 4) viewSubmissions(user);
            else if (choice == 5) markWork(user);
            else changePassword(user);
        });
    }
}

void ConsoleApp::createAssignment(const User& user) {
    for (const Course& c : assignments.coursesFor(user))
        cout << "[ID " << c.id << "] " << c.code << " - " << c.title << "\n";
    int courseId = askId("Course ID: ");
    string title = readLine("Title: ");
    string description = readLine("Description: ");
    string deadline = readLine("Deadline (YYYY-MM-DD HH:MM, UTC): ");
    double maxMark = readDouble("Maximum mark: ");
    int id = assignments.createAssignment(user, courseId, title, description, deadline, maxMark);
    cout << "Assignment created with ID " << id << ".\n";
}

void ConsoleApp::grantExtension(const User& user) {
    int assignmentId = askId("Assignment ID: ");
    cout << "Students in this course:\n";
    for (const UserRow& s : assignments.studentsOfAssignment(user, assignmentId))
        cout << "  [ID " << s.id << "] " << s.fullName << " (" << s.username << ")\n";
    int studentId = askId("Student ID: ");
    string newDeadline = readLine("New deadline for this student (YYYY-MM-DD HH:MM, UTC): ");
    string reason = readLine("Reason: ");
    ExtensionResult r = assignments.grantExtension(user, assignmentId, studentId, newDeadline, reason);
    cout << "Extension saved. Late penalties for this student now count from the new deadline.\n";
    if (r.rechecked) {
        if (r.status == Status::LATE)
            cout << "Their existing submission is still late: " << r.daysLate << " day(s) after the new deadline.\n";
        else
            cout << "Their existing submission now counts as on time.\n";
    } else if (r.hasSubmission) {
        cout << "Their submission has already been marked, so its late status was not changed. "
                "Ask the Head of Department to return it if it needs to be re-marked.\n";
    }
}

void ConsoleApp::viewSubmissions(const User& user) {
    int assignmentId = askId("Assignment ID: ");
    showSubmissionTable(submissions.listForAssignment(user, assignmentId), true);
}

void ConsoleApp::markWork(const User& user) {
    int submissionId = askId("Submission ID: ");
    Submission s = submissions.getSubmission(user, submissionId);
    if (s.status == Status::MODERATED || s.status == Status::RELEASED)
        throw LmsError("This work is already " + string(s.status == Status::RELEASED ? "released" : "moderated") +
                       ". Ask the Head of Department to return it first.");
    cout << "\nStudent: " << s.studentName << "   Status: " << s.status
         << "   Days late: " << s.daysLate << "\nWork submitted:\n----\n"
         << submissions.viewWork(user, submissionId) << "\n----\n";
    if (s.hasMark) cout << "Current mark: " << fmt(s.rawMark) << " (feedback: " << s.feedback << ")\n";

    double mark = readDouble("Mark out of " + fmt(s.maxMark) + " (before penalty): ");
    string feedback = readLine("Written feedback: ");
    string reason;
    if (s.hasMark) reason = readLine("Reason for changing the mark: ");
    submissions.markSubmission(user, submissionId, mark, feedback, reason);

    Submission after = submissions.getSubmission(user, submissionId);
    cout << "Mark saved. Final mark after late penalty: " << fmt(after.finalMark(policy)) << "\n"
         << "(The student cannot see it until the Head of Department releases it.)\n";
}


void ConsoleApp::hodMenu(const User& user) {
    while (true) {
        heading("Head of Department menu");
        cout << "1. Department courses and assignments\n2. Review marks for an assignment\n"
             << "3. Approve all marked work for an assignment\n4. Adjust a mark\n"
             << "5. Return a submission to the lecturer\n6. Release approved marks to students\n"
             << "7. View mark audit log\n8. Change password\n0. Logout\n";
        int choice = readInt("Choose: ", 0, 8);
        if (choice == 0) return;
        safely([&] {
            if (choice == 1) showCoursesAndAssignments(user);
            else if (choice == 2) reviewMarks(user);
            else if (choice == 3) {
                int n = moderation.approveAll(user, askId("Assignment ID: "));
                cout << n << " submission(s) moderated.\n";
            } else if (choice == 4) adjustMark(user);
            else if (choice == 5) returnWork(user);
            else if (choice == 6) {
                int n = moderation.releaseAll(user, askId("Assignment ID: "));
                cout << n << " mark(s) released to students.\n";
            } else if (choice == 7) showAudit(user);
            else changePassword(user);
        });
    }
}

void ConsoleApp::reviewMarks(const User& user) {
    int assignmentId = askId("Assignment ID: ");
    vector<Submission> list = moderation.listForAssignment(user, assignmentId);
    showSubmissionTable(list, true);
    for (const Submission& s : list)
        if (s.hasMark) cout << "  Feedback for " << s.studentName << ": " << s.feedback << "\n";
}

void ConsoleApp::adjustMark(const User& user) {
    int submissionId = askId("Submission ID: ");
    double mark = readDouble("New mark (before penalty): ");
    string reason = readLine("Reason: ");
    moderation.adjustMark(user, submissionId, mark, reason);
    cout << "Mark adjusted and recorded in the audit log.\n";
}

void ConsoleApp::returnWork(const User& user) {
    int submissionId = askId("Submission ID: ");
    string reason = readLine("Reason: ");
    moderation.returnToLecturer(user, submissionId, reason);
    cout << "Returned to the lecturer for re-marking.\n";
}

void ConsoleApp::showAudit(const User& user) {
    vector<MarkAudit> log = moderation.auditLog(user);
    if (log.empty()) cout << "No mark changes recorded yet.\n";
    for (const MarkAudit& m : log) {
        cout << "\n#" << m.id << "  " << m.changedAt << "  by " << m.changedByName << "\n"
             << "   " << m.studentName << " / " << m.assignmentTitle << "\n"
             << "   old: " << (m.hasOld ? fmt(m.oldValue) : string("(none)")) << "  new: " << fmt(m.newValue) << "\n"
             << "   reason: " << m.reason << "\n";
    }
}


void ConsoleApp::adminMenu(const User& user) {
    while (true) {
        heading("Administrator menu");
        cout << "1. Create user\n2. List users\n3. Enable / disable a user\n"
             << "4. Create department\n5. Set head of a department\n6. Create course\n"
             << "7. Assign lecturer to course\n8. Enrol student in course\n"
             << "9. List departments and courses\n10. Change password\n0. Logout\n";
        int choice = readInt("Choose: ", 0, 10);
        if (choice == 0) return;
        safely([&] {
            if (choice == 1) createUser(user);
            else if (choice == 2) listUsers(user);
            else if (choice == 3) toggleUser(user);
            else if (choice == 4) {
                int id = admin.createDepartment(user, readLine("Department name: "));
                cout << "Department created with ID " << id << ".\n";
            } else if (choice == 5) {
                int dept = askId("Department ID: ");
                admin.assignHod(user, dept, askId("User ID of the HoD: "));
                cout << "Head of department set.\n";
            } else if (choice == 6) {
                string code = readLine("Course code: ");
                string title = readLine("Course title: ");
                int id = admin.createCourse(user, code, title, askId("Department ID: "));
                cout << "Course created with ID " << id << ".\n";
            } else if (choice == 7) {
                int course = askId("Course ID: ");
                admin.assignLecturer(user, course, askId("Lecturer user ID: "));
                cout << "Lecturer assigned.\n";
            } else if (choice == 8) {
                int student = askId("Student user ID: ");
                admin.enrolStudent(user, student, askId("Course ID: "));
                cout << "Student enrolled.\n";
            } else if (choice == 9) {
                for (const Department& d : admin.listDepartments(user))
                    cout << "Department [ID " << d.id << "] " << d.name
                         << (d.hodUserId ? "  (HoD user " + to_string(d.hodUserId) + ")" : "  (no HoD yet)") << "\n";
                for (const Course& c : admin.listCourses(user))
                    cout << "Course     [ID " << c.id << "] " << c.code << " - " << c.title
                         << "  (department " << c.departmentId << ")\n";
            } else changePassword(user);
        });
    }
}

void ConsoleApp::createUser(const User& user) {
    cout << "Role: 1. Student  2. Lecturer  3. Head of Department  4. Administrator\n";
    int r = readInt("Choose: ", 1, 4);
    Role roles[] = {Role::STUDENT, Role::LECTURER, Role::HOD, Role::ADMIN};
    string username = readLine("Username: ");
    string fullName = readLine("Full name: ");
    string password = readPassword("Temporary password (min 6 characters): ");
    int id = admin.createUser(user, roles[r - 1], username, fullName, password);
    cout << "User created with ID " << id << ".\n";
}

void ConsoleApp::listUsers(const User& user) {
    cout << left << setw(6) << "ID" << setw(16) << "Username" << setw(26) << "Name"
         << setw(10) << "Role" << "Active\n" << string(64, '-') << "\n";
    for (const UserRow& u : admin.listUsers(user))
        cout << left << setw(6) << u.id << setw(16) << u.username << setw(26) << u.fullName.substr(0, 24)
             << setw(10) << u.role << (u.active ? "yes" : "NO") << "\n";
}

void ConsoleApp::toggleUser(const User& user) {
    int id = askId("User ID: ");
    int on = readInt("1 = enable, 0 = disable: ", 0, 1);
    admin.setUserActive(user, id, on == 1);
    cout << "Account updated.\n";
}
