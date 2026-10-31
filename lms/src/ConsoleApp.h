
#pragma once
#include "Services.h"
using namespace std;

class ConsoleApp {
public:
    ConsoleApp(AuthService& auth, AdminService& admin, AssignmentService& assignments,
               SubmissionService& submissions, ModerationService& moderation,
               const ILatePenaltyPolicy& policy)
        : auth(auth), admin(admin), assignments(assignments), submissions(submissions),
          moderation(moderation), policy(policy) {}
    void run();

private:
    void loginScreen();
    void studentMenu(const User& user);
    void lecturerMenu(const User& user);
    void hodMenu(const User& user);
    void adminMenu(const User& user);

    
    void showCoursesAndAssignments(const User& user);
    void changePassword(const User& user);
    void showSubmissionTable(const vector<Submission>& list, bool showMarks);

    
    void submitWork(const User& user);
    void showMySubmissions(const User& user);
    
    void createAssignment(const User& user);
    void grantExtension(const User& user);
    void viewSubmissions(const User& user);
    void markWork(const User& user);
    
    void reviewMarks(const User& user);
    void adjustMark(const User& user);
    void returnWork(const User& user);
    void showAudit(const User& user);

    void createUser(const User& user);
    void listUsers(const User& user);
    void toggleUser(const User& user);

    AuthService& auth;
    AdminService& admin;
    AssignmentService& assignments;
    SubmissionService& submissions;
    ModerationService& moderation;
    const ILatePenaltyPolicy& policy;
};
