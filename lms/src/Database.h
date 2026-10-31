// Database.h - small wrapper around the SQLite C API (Data access layer)
#pragma once
#include <sqlite3.h>
#include <stdexcept>
#include <string>
using namespace std;

class DatabaseError : public runtime_error {
public:
    explicit DatabaseError(const string& msg) : runtime_error(msg) {}
};

class Database {
public:
    explicit Database(const string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* handle() const { return db; }
    void exec(const string& sql);          // run SQL that has no parameters
    long long lastInsertId() const;
    void createTables();                   // creates/validates the schema (see SCHEMA_VERSION)
    string path() const;                   // absolute path of the open database file

    static const int SCHEMA_VERSION = 1;   // stored in PRAGMA user_version

private:
    sqlite3* db = nullptr;
};


class Statement {
public:
    Statement(Database& database, const string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    
    void bindInt(int pos, long long value);
    void bindDouble(int pos, double value);
    void bindText(int pos, const string& value);
    void bindNull(int pos);

    bool step();   
    int run();     

    
    long long getInt(int col) const;
    double getDouble(int col) const;
    string getText(int col) const;
    bool isNull(int col) const;

private:
    sqlite3* db;
    sqlite3_stmt* stmt = nullptr;
};
