#!/usr/bin/env python3
"""
End-to-end test for the School LMS.

It drives the real console program (menus, services, repositories, SQLite) the way a
user would, then inspects the database to confirm the results were stored correctly.

Usage (from the lms folder, after building):
    python3 tests/smoke_test.py            # uses ./build/lms
    python3 tests/smoke_test.py path/to/lms
"""
import os
import sqlite3
import subprocess
import sys
import tempfile
from datetime import datetime, timedelta, timezone

EXE = sys.argv[1] if len(sys.argv) > 1 else os.path.join("build", "lms")
DB = os.path.join(tempfile.mkdtemp(), "smoke.db")
failures = []
passed = 0


def check(condition, message):
    global passed
    if condition:
        passed += 1
        print("  ok   " + message)
    else:
        failures.append(message)
        print("  FAIL " + message)


def session(user, password, *menu_inputs):
    """Log in as `user`, send the menu inputs, log out and exit. Returns program output."""
    lines = ["1", user, password, *map(str, menu_inputs), "0", "0"]
    result = subprocess.run([EXE, DB], input="\n".join(lines) + "\n",
                            capture_output=True, text=True, timeout=120)
    return result.stdout


def query(sql, *params):
    con = sqlite3.connect(DB)
    try:
        return con.execute(sql, params).fetchall()
    finally:
        con.close()


def utc(delta):
    return (datetime.now(timezone.utc) + delta).strftime("%Y-%m-%d %H:%M")


def sub_id(assignment, student):
    rows = query("SELECT submission_id FROM submissions WHERE assignment_id=? AND student_id=?",
                 assignment, student)
    return rows[0][0] if rows else None


# --------------------------------------------------------------------------- 1. set-up
print("1. First run and administrator set-up")
out = session("admin", "admin123")           # creates the database and the default admin
check("Welcome, System Administrator (ADMIN)" in out, "default administrator can log in")

ADMIN_SETUP = [
    4, "Science", 4, "Arts",                                    # departments 1, 2
    1, 3, "hods", "Mr Science", "pass123",                      # user 2  (HoD)
    1, 3, "hoda", "Ms Arts", "pass123",                         # user 3  (HoD)
    1, 2, "lects", "Lecturer Science", "pass123",               # user 4
    1, 2, "lecta", "Lecturer Arts", "pass123",                  # user 5
    1, 1, "stua", "Student A", "pass123",                       # user 6
    1, 1, "stub", "Student B", "pass123",                       # user 7
    5, 1, 2, 5, 2, 3,                                           # HoDs -> departments
    6, "SCI101", "Science 101", 1, 6, "ART101", "Arts 101", 2,  # courses 1, 2
    7, 1, 4, 7, 2, 5,                                           # lecturers -> courses
    8, 6, 1, 8, 7, 1, 8, 6, 2,                                  # enrolments
    5, 1, 3,                                                    # HoD already heads another dept -> error
    5, 2, 2,                                                    # second dept's HoD again is fine (replace)
]
out = session("admin", "admin123", *ADMIN_SETUP)
check("That person already heads another department." in out, "one person cannot head two departments")
check(len(query("SELECT * FROM users")) == 7, "7 users exist")
check(len(query("SELECT * FROM enrolments")) == 3, "3 enrolments exist")
check(query("SELECT hod_user_id FROM departments ORDER BY department_id") == [(2,), (3,)],
      "heads of department stored")
out = session("admin", "admin123", 1, 2, "stua", "Dup", "pass123", 1, 2, "ab", "Short", "pass123")
check("already taken" in out and "at least 3 characters" in out, "duplicate/short usernames rejected")
out = session("admin", "admin123", 4, "Science")
check("A department with that name already exists." in out, "duplicate department rejected")

# --------------------------------------------------------------------------- 2. assignments
print("2. Lecturer creates assignments")
create = lambda title, deadline, mx: [2, 1, title, "desc", deadline, mx]
out = session("lects", "pass123",
              *create("A1 open", utc(timedelta(days=10)), 50),
              *create("A2 late 2d", utc(-timedelta(hours=36)), 10),
              *create("A3 very late", utc(-timedelta(days=5)), 20),
              *create("A4 extension", utc(-timedelta(hours=30)), 12.5),
              2, 1, "Bad", "d", "2026-02-30 10:00", 10,
              2, 1, "Bad", "d", utc(timedelta(days=1)), 0)
check(len(query("SELECT * FROM assignments")) == 4, "4 assignments created, invalid ones rejected")
check("That date does not exist." in out, "impossible date rejected")
out = session("lecta", "pass123", 2, 1, "Nope", "d", utc(timedelta(days=1)), 10)
check("You are not assigned to that course." in out, "lecturer cannot create in another course")

# --------------------------------------------------------------------------- 3. submissions
print("3. Student submissions, versions and lateness")
session("stua", "pass123", 2, 1, 2, "first draft")
out = session("stua", "pass123", 2, 1, 2, "second draft")
check("Resubmission saved (version 2)" in out, "resubmission before deadline allowed")
check(query("SELECT COUNT(*) FROM submission_versions WHERE submission_id=?", sub_id(1, 6)) == [(2,)],
      "both versions retained")
session("stua", "pass123", 2, 2, 2, "late work")
session("stua", "pass123", 2, 3, 2, "very late work")
session("stua", "pass123", 2, 4, 2, "late for extension test")
session("stub", "pass123", 2, 1, 2, "b work")
check(query("SELECT status, days_late FROM submissions WHERE submission_id=?", sub_id(2, 6)) == [("LATE", 2)],
      "36h late recorded as LATE, 2 days")
check(query("SELECT days_late FROM submissions WHERE submission_id=?", sub_id(3, 6))[0][0] >= 4,
      "5 days late recorded")
out = session("stua", "pass123", 2, 2, 2, "again")
check("deadline has passed" in out, "no resubmission after the deadline")
out = session("stua", "pass123", 2, 2, 2, "")
check("cannot submit empty work" in out, "empty submission rejected")
out = session("stub", "pass123", 2, 4, 2, "x")
check("Submitted LATE" in out, "student B can submit late work")   # sub for A4, replaced later
out = session("stub", "pass123", 2, 2, 1, "/nonexistent/file")
check("Could not open that file." in out, "missing file reported")
out = session("stua", "pass123", 1)
check("ART101" in out and "SCI101" in out, "student sees enrolled courses")
out = session("stub", "pass123", 1)
check("ART101" not in out, "student does not see courses they are not enrolled in")

# --------------------------------------------------------------------------- 4. extension
print("4. Extension granted after a late submission")
out = session("lects", "pass123", 3, 4, 6, utc(timedelta(days=2)), "illness")
check("now counts as on time" in out, "late submission re-evaluated against the extension")
check(query("SELECT status, days_late FROM submissions WHERE submission_id=?", sub_id(4, 6)) == [("SUBMITTED", 0)],
      "status and days late updated in the database")
out = session("stua", "pass123", 2, 4, 2, "replacement inside the extension")
check("Resubmission saved (version 2)" in out, "student can resubmit within the extension")
out = session("lects", "pass123", 3, 4, 7, utc(-timedelta(days=2)), "too early")
check("must be later than the normal deadline" in out, "extension must be later than normal deadline")
out = session("lects", "pass123", 3, 4, 7, utc(timedelta(days=3)), "")
check("Reason cannot be empty." in out, "extension needs a reason")

# --------------------------------------------------------------------------- 5. access control
print("5. Access control")
out = session("lecta", "pass123", 4, 1)
check("You are not assigned to that course." in out, "other lecturer cannot view submissions")
out = session("lecta", "pass123", 5, sub_id(1, 6))
check("You are not assigned to that course." in out, "other lecturer cannot mark")
out = session("hoda", "pass123", 2, 1)
check("not in your department" in out, "other department's HoD cannot review")
out = session("hoda", "pass123", 3, 1)
check("not in your department" in out, "other department's HoD cannot approve")

# --------------------------------------------------------------------------- 6. marking
print("6. Marking")
S1A, S1B, S2A, S3A, S4A = sub_id(1, 6), sub_id(1, 7), sub_id(2, 6), sub_id(3, 6), sub_id(4, 6)
out = session("lects", "pass123", 5, S4A, 12.6, "too high")
check("The mark must be between 0 and 12.5." in out, "range message shows the real maximum (12.5)")
out = session("lects", "pass123", 5, S1A, 40, "")
check("Written feedback is required." in out, "feedback is required")
session("lects", "pass123", 5, S1A, 40, "Good work", 5, S1B, 30, "Fine",
        5, S2A, 10, "On time except late", 5, S3A, 20, "Great but very late", 5, S4A, 12.5, "Perfect")
rows = dict(query("SELECT submission_id, raw_mark FROM submissions"))
check(rows[S1A] == 40 and rows[S2A] == 10 and rows[S4A] == 12.5, "marks stored")
out = session("lects", "pass123", 4, 2)
check("8.00" in out, "2 days late: 10 marks - 20% = 8.00 final")
out = session("lects", "pass123", 4, 3)
check("20.00" in out and " 0.00" in out, "more than 3 days late: final mark 0.00")
out = session("stua", "pass123", 3)
check("not available yet" in out and "Good work" not in out and "40.00" not in out,
      "student cannot see marks or feedback before release")

# --------------------------------------------------------------------------- 7. moderation
print("7. Moderation, release and audit")
out = session("hods", "pass123", 3, 1)
check("2 submission(s) moderated." in out, "HoD approves marked work")
out = session("hods", "pass123", 4, S1A, 38, "")
check("You must give a reason." in out, "mark adjustment needs a reason")
out = session("hods", "pass123", 4, S1A, 38, "moderation correction")
check("Mark adjusted" in out, "HoD adjusts a mark")
out = session("lects", "pass123", 5, S1A)
check("already moderated" in out, "lecturer blocked from marking moderated work (before typing marks)")
out = session("stua", "pass123", 3)
check("40.00" not in out and "38.00" not in out, "still hidden after moderation")
out = session("hods", "pass123", 6, 1)
check("2 mark(s) released to students." in out, "HoD releases marks")
out = session("stua", "pass123", 3)
check("Mark before penalty: 38.00" in out and "Good work" in out, "student sees released mark and feedback")
out = session("stua", "pass123", 3)
check("Perfect" not in out, "unreleased work (A4) stays hidden")
out = session("stub", "pass123", 3)
check("Mark before penalty: 30.00" in out, "other student sees own released mark")
out = session("hods", "pass123", 5, S2A, "mark looks wrong")
check("Returned to the lecturer" in out, "HoD returns work to lecturer")
out = session("lects", "pass123", 5, S2A, 9, "Rechecked", "")
check("You must give a reason when you change a mark." in out, "re-marking needs a reason")
session("lects", "pass123", 5, S2A, 9, "Rechecked", "recount")
check(query("SELECT status, raw_mark FROM submissions WHERE submission_id=?", S2A) == [("MARKED", 9.0)],
      "returned work re-marked")
out = session("hods", "pass123", 7)
check("old: 40.00  new: 38.00" in out and "moderation correction" in out and "recount" in out,
      "audit log shows old/new value, reason and who")
check(len(query("SELECT * FROM mark_audit")) >= 8, "every mark change has an audit row")
out = session("hoda", "pass123", 7)
check("No mark changes recorded yet." in out, "audit log is limited to the HoD's own department")

# --------------------------------------------------------------------------- 8. accounts
print("8. Accounts and security")
out = session("stua", "wrong")
check("Invalid username or password." in out, "wrong password rejected")
out = session("nobody", "x")
check("Invalid username or password." in out, "unknown user rejected (same message)")
pw = query("SELECT password_hash, salt FROM users WHERE username='stua'")[0]
check("pass123" not in pw[0] and len(pw[0]) == 64 and len(pw[1]) == 32, "password stored as salted hash")
check(query("SELECT COUNT(DISTINCT salt) FROM users") == [(7,)], "every user has a different salt")
session("admin", "admin123", 3, 7, 0)
out = session("stub", "pass123")
check("account has been disabled" in out, "disabled account cannot log in")
session("admin", "admin123", 3, 7, 1)
out = session("stub", "pass123")
check("Welcome, Student B" in out, "re-enabled account can log in")
out = session("admin", "admin123", 3, 1, 0)
check("cannot disable your own account" in out, "admin cannot disable themselves")
out = session("stua", "pass123", 4, "pass123", "newpass1", "different")
check("not the same" in out, "password confirmation checked")
session("stua", "pass123", 4, "pass123", "newpass1", "newpass1")
check("Welcome, Student A" in session("stua", "newpass1"), "changed password works")
check("Invalid username" in session("stua", "pass123"), "old password no longer works")

# --------------------------------------------------------------------------- 9. robustness
print("9. Robustness")
tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".bin")
tmp.write(b"abc\x00def")
tmp.close()
session("lects", "pass123", *create("A5 open", utc(timedelta(days=5)), 10))
out = session("stub", "pass123", 2, 5, 1, tmp.name)
check("Submitted on time" in out, "file with NUL bytes accepted")
size = query("SELECT length(CAST(content AS BLOB)) FROM submission_versions ORDER BY version_id DESC LIMIT 1")
check(size == [(7,)], "stored content keeps every byte (7 bytes)")
os.unlink(tmp.name)
out = session("hods", "pass123", 2, 999)
check("There is no assignment" in out, "unknown assignment ID reported, no crash")
res = subprocess.run([EXE, DB], input="1\nadmin\n", capture_output=True, text=True, timeout=30)
check(res.returncode == 0 and "Goodbye." in res.stdout, "input ending mid-way exits cleanly")
out = session("admin", "admin123", "abc", 99, 2)
check("Please enter a whole number" in out, "bad menu input re-prompts")

# --------------------------------------------------------------------------- 10. demo mode
print("10. Demo data")
demo_db = os.path.join(tempfile.mkdtemp(), "demo.db")
res = subprocess.run([EXE, "--demo", demo_db], input="0\n", capture_output=True, text=True, timeout=60)
check("Demo data added" in res.stdout, "demo data created on a fresh database")
res = subprocess.run([EXE, "--demo", demo_db], input="0\n", capture_output=True, text=True, timeout=60)
check("Demo data was not added" in res.stdout, "demo data is not added twice")
con = sqlite3.connect(demo_db)
future = con.execute("SELECT deadline > datetime('now') FROM assignments WHERE title LIKE '%open%'").fetchone()[0]
con.close()
check(future == 1, "the demo 'open' assignment really is open")

print("\n%d checks passed, %d failed" % (passed, len(failures)))
for f in failures:
    print("  FAILED:", f)
sys.exit(1 if failures else 0)
