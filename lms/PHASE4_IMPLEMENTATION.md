# Phase 4: Implementation

## 1. Overview

The Learning Management System is implemented as a C++17 console application with a local SQLite database. It follows the layered design in the system design document: presentation, service, domain, data access, and security. It can run without an internet connection.

## 2. Source code organisation

| Files | Responsibility |
|---|---|
| `src/ConsoleApp.h`, `src/ConsoleApp.cpp`, `src/Input.h`, `src/Input.cpp` | Console menus and input validation |
| `src/Services.h`, `src/Services.cpp` | Authentication, role permissions, assignment, submission, marking and moderation rules |
| `src/Domain.h`, `src/Domain.cpp` | Abstract `User`, role subclasses, submission data, user factory and late-penalty strategy |
| `src/Repositories.h`, `src/Repositories.cpp` | Repository interfaces and SQLite implementations |
| `src/Database.h`, `src/Database.cpp` | SQLite connection, schema creation and prepared statements |
| `src/Security.h`, `src/Security.cpp` | Salted password hashing and safe comparison |
| `src/main.cpp` | Connects the application layers and starts the menu |

The code uses C++ standard-library types and `using namespace std;` to keep the student project readable. The main business rules are placed in services rather than being enforced only by menus.

## 3. Object-oriented design and patterns

`User` is an abstract base class. `Student`, `Lecturer`, `HeadOfDepartment`, and `Administrator` override role and permission methods, so role behaviour uses inheritance and polymorphism. `UserFactory` creates the correct user object for a stored role.

`ILatePenaltyPolicy` and `TenPercentPerDayPolicy` implement the Strategy pattern. Repository interfaces separate SQL from the services and domain classes. `UserFactory` implements the Factory pattern. These choices follow the supplied design document and keep database access out of the console presentation code.

## 4. Main functionality

- Users log in with a username and password. Disabled accounts cannot log in.
- The administrator creates users, departments and courses, assigns Heads of Department and lecturers, enrols students, and can enable or disable accounts.
- Lecturers create assignments, set deadlines and maximum marks, grant personal extensions, view submissions, mark work and provide written feedback.
- Students view enrolled courses and assignments, submit work, resubmit before the effective deadline, and view their own released marks and feedback.
- Heads of Department review marks, approve marked work, adjust marks with a reason, return work for re-marking, release approved marks and view the audit log.
- Late work is accepted. The effective deadline is the student's extension deadline when one exists. The late penalty is 10% of the maximum mark per late day for days 1–3; work more than three days late receives zero.
- Each resubmission is saved as a new row in `submission_versions`, preserving the earlier versions. The latest version is the one used for marking.
- If an extension is granted after a student has submitted but before the work is marked, the submission is re-checked against the new effective deadline and its on-time/late status and days late are updated. Marked work is never changed this way, because its mark has already been audited.
- A Head of Department's department is looked up from the `departments` table on every action rather than remembered from login, so reassigning a head takes effect immediately.
- A student cannot see the mark or feedback until the submission is released by the Head of Department.

## 5. Persistence and security

SQLite stores users, departments, courses, course lecturers, enrolments, assignments, extensions, submissions, submission versions and mark audit entries. SQL is contained in the data-access layer and uses prepared statements with bound parameters.

Passwords are not stored as plain text. The application uses OpenSSL's PBKDF2-HMAC-SHA256 with a random salt for each password. This was chosen because OpenSSL is a recognised cryptographic library and PBKDF2 slows repeated password guesses. A mark audit entry records the user, timestamp, old mark, new mark and reason. Mark updates and their audit entries are saved together.

## 6. Error handling and validation

The service layer checks permissions and ownership before allowing operations. Input helpers validate menu choices and numeric values. Services reject invalid marks, empty submissions, missing feedback, invalid deadlines and unauthorised actions. Database and application exceptions are caught at the application boundary and displayed as messages rather than being allowed to terminate the menu unexpectedly.

## 7. Build and run

### Requirements

- C++17 compiler
- CMake 3.14 or newer
- SQLite development library (3.24 or newer)
- OpenSSL development library

On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential cmake libsqlite3-dev libssl-dev
```

Build from the `lms` folder:

```bash
cmake -S . -B build
cmake --build build
```

Run the application:

```bash
./build/lms
```

On Windows, the recommended setup is MSYS2 UCRT64 with its matching GCC, CMake, SQLite and OpenSSL packages. Open the `lms_phase4` folder in VS Code from the UCRT64 environment, then use **Terminal → Run Task → LMS: Build**. The CMake configuration places the executable at `lms/build/lms.exe` for both single- and multi-configuration generators. Keep the compiler and SQLite/OpenSSL libraries from the same toolchain.

On the first run, the system creates `admin` with password `admin123`. Log in and change the password. For a demonstration database, run `./build/lms --demo`; demo users `hod1`, `lect1`, `stu1` and `stu2` use password `pass123`. Use a new or backed-up database for demonstrations.

## 8. Verification performed

The project was clean-built with CMake in C++17 mode (GCC 13.3.0, SQLite 3.45.1, OpenSSL 3.0.13) with `-Wall -Wextra` and no warnings. It was also built with AddressSanitizer and UndefinedBehaviorSanitizer, and the whole test run below was repeated with that build without any reports.

`tests/smoke_test.py` is an end-to-end test that drives the real console program as each role and then inspects the SQLite database. It currently runs 68 checks covering: first-run administrator and set-up of departments, heads, courses, lecturers and enrolments; assignment creation and date validation; submission, resubmission and retained versions; late penalties (2 days late and more than 3 days late); extensions, including an extension granted after a late submission; lecturer marking and the mark range and feedback rules; Head of Department approval, adjustment, return, release and audit log; hiding of marks until release; cross-course and cross-department access control; login, disabled accounts, salted password hashes and password change; and robustness (files containing NUL bytes, unknown IDs, bad menu input, input ending mid-way, and the demo data).

Date validation checks covered a valid leap-day date and rejected an impossible date, an invalid month, an invalid hour and trailing characters. The Windows build instructions target MSYS2 UCRT64, but a Windows machine was not available for a native Windows build test. This end-to-end test is not a substitute for the full Phase 5 unit, integration, system and acceptance test documentation; those should still be written up separately.
