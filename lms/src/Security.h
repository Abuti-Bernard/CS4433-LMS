// Security.h - password hashing using OpenSSL (PBKDF2-HMAC-SHA256 with a random salt)
#pragma once
#include "Domain.h"
using namespace std;

class Pbkdf2Hasher : public IPasswordHasher {
public:
    string makeSalt() override;                                    
    string hash(const string& password, const string& salt) override;  
};

bool safeEquals(const string& a, const string& b);
