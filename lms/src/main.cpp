#include <ctime>
#include <iostream>
#include "ConsoleApp.h"
#include "Security.h"
using namespace std;

static void addDemoData(AdminService& admin, AssignmentService& assignments, IUserRepository& users) {
    if (users.count() > 1) {   // only the default admin exists on a fresh database
        cout << "Demo data was not added: this database already contains users. "
                "Use a new file, e.g. './lms --demo presentation.db'.\n";
        return;
    }
    unique_ptr<User> adminUser = users.findById(1);
    if (!adminUser || adminUser->getRole() != Role::ADMIN) {
        cout << "Demo data was not added: the first user in this database is not the administrator.\n";
        return;
    }

    int dept = admin.createDepartment(*adminUser, "Science");
    int hod = admin.createUser(*adminUser, Role::HOD, "hod1", "Mr Mokoena (HoD)", "pass123");
    admin.assignHod(*adminUser, dept, hod);
    int lect = admin.createUser(*adminUser, Role::LECTURER, "lect1", "Ms Letsie", "pass123");
    int s1 = admin.createUser(*adminUser, Role::STUDENT, "stu1", "Ntsika Xaluva", "pass123");
    int s2 = admin.createUser(*adminUser, Role::STUDENT, "stu2", "Bulelani Tshabalala", "pass123");
    int course = admin.createCourse(*adminUser, "0500", "First Language English", dept);
    admin.assignLecturer(*adminUser, course, lect);
    admin.enrolStudent(*adminUser, s1, course);
    admin.enrolStudent(*adminUser, s2, course);

    unique_ptr<User> lecturer = UserFactory::createUser(Role::LECTURER, lect, "lect1", "Ms Letsie");
    // The "open" assignment is due 30 days from now so the demo works whenever it is run.
    time_t due = time(nullptr) + 30 * 24 * 60 * 60;
    char dueText[32];
    strftime(dueText, sizeof(dueText), "%Y-%m-%d %H:%M", gmtime(&due));
    assignments.createAssignment(*lecturer, course, "Assignment 1 (open)", "Write a hello world program",
                                 dueText, 50);
    assignments.createAssignment(*lecturer, course, "Assignment 2 (deadline passed)", "Loops and arrays",
                                 "2020-01-01 12:00", 50);
    cout << "Demo data added. Logins: hod1, lect1, stu1, stu2 (password pass123)\n";
}

int main(int argc, char* argv[]) {
    string dbPath = "lms.db";
    bool demo = false;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--demo") demo = true;
        else dbPath = arg;
    }

    try {
        Database db(dbPath);
        db.createTables();
        cout << "Using database: " << db.path() << "\n";

        // data access
        SqliteUserRepository users(db);
        SqliteCourseRepository courses(db);
        SqliteAssignmentRepository assignmentRepo(db);
        SqliteSubmissionRepository submissionRepo(db);
        SqliteAuditRepository auditRepo(db);

        // domain objects
        Pbkdf2Hasher hasher;
        UserFactory factory(hasher);
        TenPercentPerDayPolicy penaltyPolicy;

        // 
        if (users.count() == 0) {
            users.add(factory.prepareNewUser(Role::ADMIN, "admin", "System Administrator", "admin123"));
            cout << "First run: created administrator 'admin' with password 'admin123'.\n"
                 << "Please log in and change this password straight away.\n";
        }

        // service layer
        AuthService auth(users, hasher);
        AdminService admin(users, courses, factory);
        AssignmentService assignmentService(assignmentRepo, courses, submissionRepo);
        SubmissionService submissionService(submissionRepo, assignmentRepo, courses);
        ModerationService moderation(submissionRepo, assignmentRepo, courses, auditRepo);

        if (demo) addDemoData(admin, assignmentService, users);

        // presentation layer
        ConsoleApp app(auth, admin, assignmentService, submissionService, moderation, penaltyPolicy);
        app.run();
    } catch (const DatabaseError& e) {
        cerr << "Fatal database error: " << e.what() << "\n";
        return 1;
    } catch (const exception& e) {
        cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
