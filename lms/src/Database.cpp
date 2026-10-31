#include "Database.h"
using namespace std;

Database::Database(const string& path) {
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        string msg = db ? sqlite3_errmsg(db) : "out of memory";
        sqlite3_close(db);
        db = nullptr;
        throw DatabaseError("Cannot open database '" + path + "': " + msg);
    }
    // SQLite waits briefly if another process (or another LMS window) is writing.
    sqlite3_busy_timeout(db, 5000);
    try {
        exec("PRAGMA foreign_keys = ON;");   // SQLite ignores foreign keys unless this is on
    } catch (...) {
        sqlite3_close(db);                   // the destructor will not run if the constructor throws
        db = nullptr;
        throw;
    }
}

Database::~Database() {
    if (db) sqlite3_close(db);
}

void Database::exec(const string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw DatabaseError(msg);
    }
}

long long Database::lastInsertId() const {
    return sqlite3_last_insert_rowid(db);
}

string Database::path() const {
    const char* name = sqlite3_db_filename(db, "main");
    return (name && *name) ? string(name) : string("(in-memory)");
}

void Database::createTables() {
    // Give a clear message for a file that is not a SQLite database, is corrupt,
    // or is read-only, instead of a bare "file is not a database".
    try {
        exec("PRAGMA schema_version;");
        exec("SELECT count(*) FROM sqlite_master;");
    } catch (const DatabaseError& e) {
        throw DatabaseError("'" + path() + "' cannot be used as an LMS database (" + e.what() +
                            "). Delete it or pass a different file name.");
    }

    // Refuse databases written by a newer version rather than corrupting them.
    int version = 0;
    {
        Statement s(*this, "PRAGMA user_version;");
        if (s.step()) version = static_cast<int>(s.getInt(0));
    }
    if (version > SCHEMA_VERSION)
        throw DatabaseError("'" + path() + "' was created by a newer version of this program (schema " +
                            to_string(version) + ").");

    exec(R"(
CREATE TABLE IF NOT EXISTS users (
    user_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    salt          TEXT NOT NULL,
    role          TEXT NOT NULL CHECK (role IN ('STUDENT','LECTURER','HOD','ADMIN')),
    full_name     TEXT NOT NULL,
    is_active     INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1))
);
CREATE TABLE IF NOT EXISTS departments (
    department_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name          TEXT NOT NULL UNIQUE,
    hod_user_id   INTEGER UNIQUE REFERENCES users(user_id)
);
CREATE TABLE IF NOT EXISTS courses (
    course_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    code          TEXT NOT NULL UNIQUE,
    title         TEXT NOT NULL,
    department_id INTEGER NOT NULL REFERENCES departments(department_id)
);
CREATE TABLE IF NOT EXISTS course_lecturers (
    course_id   INTEGER NOT NULL REFERENCES courses(course_id) ON DELETE CASCADE,
    lecturer_id INTEGER NOT NULL REFERENCES users(user_id),
    PRIMARY KEY (course_id, lecturer_id)
);
CREATE TABLE IF NOT EXISTS enrolments (
    student_id  INTEGER NOT NULL REFERENCES users(user_id),
    course_id   INTEGER NOT NULL REFERENCES courses(course_id) ON DELETE CASCADE,
    enrolled_on TEXT NOT NULL DEFAULT (datetime('now')),
    PRIMARY KEY (student_id, course_id)
);
CREATE TABLE IF NOT EXISTS assignments (
    assignment_id INTEGER PRIMARY KEY AUTOINCREMENT,
    course_id     INTEGER NOT NULL REFERENCES courses(course_id),
    title         TEXT NOT NULL,
    description   TEXT,
    deadline      TEXT NOT NULL,
    max_mark      REAL NOT NULL CHECK (max_mark > 0),
    created_by    INTEGER NOT NULL REFERENCES users(user_id)
);
CREATE TABLE IF NOT EXISTS extensions (
    extension_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    assignment_id INTEGER NOT NULL REFERENCES assignments(assignment_id),
    student_id    INTEGER NOT NULL REFERENCES users(user_id),
    new_deadline  TEXT NOT NULL,
    reason        TEXT NOT NULL,
    granted_by    INTEGER NOT NULL REFERENCES users(user_id),
    granted_at    TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE (assignment_id, student_id)
);
CREATE TABLE IF NOT EXISTS submissions (
    submission_id INTEGER PRIMARY KEY AUTOINCREMENT,
    assignment_id INTEGER NOT NULL REFERENCES assignments(assignment_id),
    student_id    INTEGER NOT NULL REFERENCES users(user_id),
    status        TEXT NOT NULL CHECK (status IN
                  ('SUBMITTED','LATE','MARKED','RETURNED','MODERATED','RELEASED')),
    raw_mark      REAL CHECK (raw_mark IS NULL OR raw_mark >= 0),
    days_late     INTEGER NOT NULL DEFAULT 0 CHECK (days_late >= 0),
    feedback      TEXT,
    marked_by     INTEGER REFERENCES users(user_id),
    moderated_by  INTEGER REFERENCES users(user_id),
    released_at   TEXT,
    UNIQUE (assignment_id, student_id)
);
CREATE TABLE IF NOT EXISTS submission_versions (
    version_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    submission_id INTEGER NOT NULL REFERENCES submissions(submission_id) ON DELETE CASCADE,
    version_no    INTEGER NOT NULL CHECK (version_no >= 1),
    content       TEXT NOT NULL,
    submitted_at  TEXT NOT NULL,
    UNIQUE (submission_id, version_no)
);
CREATE TABLE IF NOT EXISTS mark_audit (
    audit_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    submission_id INTEGER NOT NULL REFERENCES submissions(submission_id),
    changed_by    INTEGER NOT NULL REFERENCES users(user_id),
    changed_at    TEXT NOT NULL DEFAULT (datetime('now')),
    old_value     REAL,
    new_value     REAL NOT NULL,
    reason        TEXT NOT NULL CHECK (length(trim(reason)) > 0)
);
CREATE INDEX IF NOT EXISTS idx_sub_assignment ON submissions(assignment_id);
CREATE INDEX IF NOT EXISTS idx_sub_student    ON submissions(student_id, status);
CREATE INDEX IF NOT EXISTS idx_audit_sub      ON mark_audit(submission_id);
CREATE INDEX IF NOT EXISTS idx_enrol_course   ON enrolments(course_id);
)");

    // Existing tables must have the columns this version expects; otherwise later
    // queries would fail with "no such column" halfway through a session.
    const char* required[][2] = {
        {"users", "password_hash"}, {"users", "salt"}, {"departments", "hod_user_id"},
        {"submissions", "days_late"}, {"submissions", "moderated_by"},
        {"submission_versions", "version_no"}, {"mark_audit", "reason"},
    };
    for (auto& rc : required) {
        Statement s(*this, string("SELECT count(*) FROM pragma_table_info('") + rc[0] +
                           "') WHERE name = '" + rc[1] + "';");
        if (s.step() && s.getInt(0) == 0)
            throw DatabaseError("'" + path() + "' is from an older, incompatible version (table '" +
                                rc[0] + "' lacks column '" + rc[1] +
                                "'). Back it up and delete it to start fresh.");
    }

    if (version < SCHEMA_VERSION) exec("PRAGMA user_version = " + to_string(SCHEMA_VERSION) + ";");
}



Statement::Statement(Database& database, const string& sql) : db(database.handle()) {
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        throw DatabaseError(sqlite3_errmsg(db));
}

Statement::~Statement() {
    sqlite3_finalize(stmt);
}

void Statement::bindInt(int pos, long long value) {
    if (sqlite3_bind_int64(stmt, pos, value) != SQLITE_OK) throw DatabaseError(sqlite3_errmsg(db));
}
void Statement::bindDouble(int pos, double value) {
    if (sqlite3_bind_double(stmt, pos, value) != SQLITE_OK) throw DatabaseError(sqlite3_errmsg(db));
}
void Statement::bindText(int pos, const string& value) {
    // SQLITE_TRANSIENT makes SQLite copy the text, so our string can go away safely
    // The explicit length keeps text that contains NUL bytes intact (e.g. uploaded files).
    if (sqlite3_bind_text(stmt, pos, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK)
        throw DatabaseError(sqlite3_errmsg(db));
}
void Statement::bindNull(int pos) {
    if (sqlite3_bind_null(stmt, pos) != SQLITE_OK) throw DatabaseError(sqlite3_errmsg(db));
}

bool Statement::step() {
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw DatabaseError(sqlite3_errmsg(db));
}

int Statement::run() {
    step();
    return sqlite3_changes(db);
}

long long Statement::getInt(int col) const { return sqlite3_column_int64(stmt, col); }
double Statement::getDouble(int col) const { return sqlite3_column_double(stmt, col); }
bool Statement::isNull(int col) const { return sqlite3_column_type(stmt, col) == SQLITE_NULL; }

string Statement::getText(int col) const {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    if (!text) return string();
    return string(reinterpret_cast<const char*>(text), static_cast<size_t>(sqlite3_column_bytes(stmt, col)));
}
