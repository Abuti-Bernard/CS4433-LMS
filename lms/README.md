# School Learning Management System (CS4433 Phase 4)

A C++17 console-based School Learning Management System using a local SQLite database. It follows the layered design in the supplied System Design Document and works offline.

## Source layout

| Layer | Files |
|---|---|
| Presentation | `src/ConsoleApp.*`, `src/Input.*` |
| Service | `src/Services.*` |
| Domain | `src/Domain.*` |
| Data access | `src/Repositories.*`, `src/Database.*` |
| Security | `src/Security.*` |

## Requirements

- C++17 compiler
- CMake 3.14+
- SQLite development library (3.24 or newer)
- OpenSSL development library
- Python 3 (optional, only to run the automated test)

Ubuntu/Debian setup:

```bash
sudo apt update
sudo apt install build-essential cmake libsqlite3-dev libssl-dev
```

## Build and run

Run these commands from this `lms` folder:

```bash
cmake -S . -B build
cmake --build build
./build/lms
```

For Windows, the simplest option for this project is the MSYS2 UCRT64 environment. Open the **UCRT64** terminal and install the tools and libraries:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-openssl
```

Open the parent `lms_phase4` folder in VS Code from that UCRT64 environment, then select **Terminal → Run Task → LMS: Build**. The supplied VS Code task uses the Ninja generator so it uses the UCRT64 compiler found on that environment's PATH. You can also build directly from the `lms` folder with `cmake -S . -B build -G Ninja` followed by `cmake --build build`. The executable is placed at `lms/build/lms.exe` for both MinGW/Ninja and Visual Studio generators. Run it from the `lms` folder so the database is created there. Do not mix libraries from different toolchains (for example, MSVC libraries with the MSYS2 UCRT64 compiler).

**Do not build by pressing the Run/Build button on `main.cpp`** (or the "C/C++: g++ build active file" task): that compiles only one file and the linker reports many `undefined reference` errors. Use the CMake task above, or from the UCRT64 shell in the `lms` folder run `./build.sh` (which compiles every file in `src/`).

The application uses `lms.db` in the current working directory by default and prints the full path of the database it opened at start-up. To use another database, pass its path as an argument. The schema is versioned (`PRAGMA user_version`); a file that is not an LMS database, is corrupt, or comes from an incompatible version is rejected with a clear message instead of failing mid-session.

## First login and demo data

On a new database, the application creates username `admin` with password `admin123`. Change the password after logging in.

Run `./build/lms --demo presentation.db` to create a separate demonstration database with sample data (on Windows, use `./build/lms.exe --demo presentation.db`). Demo accounts are `hod1`, `lect1`, `stu1` and `stu2`; their password is `pass123`. Demo data is only added when the selected database contains just the initial administrator. Using a separate file keeps an existing `lms.db` untouched.

Dates are entered as `YYYY-MM-DD HH:MM` in UTC.

## Implemented rules

- Late work is accepted and recorded as late.
- The effective deadline is the student's personal extension deadline, if one exists.
- For 1–3 days late, the penalty is 10% of the maximum mark per day. More than 3 days late receives a final mark of zero.
- Resubmission is allowed only before the effective deadline and before marking. Every version is retained; the latest version is marked.
- If an extension is granted after a student has already handed in unmarked work, that work is re-checked against the new deadline (so it can stop being late). Work that has already been marked is not changed.
- Marks and feedback are hidden from students until the Head of Department releases them.
- Mark changes are recorded in the audit log with who changed them, when, old and new values, and the reason.
- A Head of Department can only see and moderate work in the department they currently head; this is checked on every action, so reassigning a department takes effect immediately.
- Passwords use salted PBKDF2-HMAC-SHA256 hashes via OpenSSL. SQL statements use bound parameters in the data-access layer.

## Automated test

`tests/smoke_test.py` drives the real console program through every role (administrator set-up, assignments, submissions, late penalties, extensions, marking, moderation, release, audit log, access control, accounts) and then checks the SQLite database. After building:

```bash
python3 tests/smoke_test.py            # or: ctest --test-dir build --output-on-failure
```

On Windows use `python tests/smoke_test.py build/lms.exe`.

See `PHASE4_IMPLEMENTATION.md` for the implementation write-up.
