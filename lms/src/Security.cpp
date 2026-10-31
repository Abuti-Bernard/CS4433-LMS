#include "Security.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
using namespace std;

static const int ITERATIONS = 100000;   

static string toHex(const unsigned char* data, size_t length) {
    const char* digits = "0123456789abcdef";
    string out;
    for (size_t i = 0; i < length; i++) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 0x0F];
    }
    return out;
}

string Pbkdf2Hasher::makeSalt() {
    unsigned char salt[16];
    if (RAND_bytes(salt, sizeof(salt)) != 1) throw LmsError("Could not create a random salt.");
    return toHex(salt, sizeof(salt));
}

string Pbkdf2Hasher::hash(const string& password, const string& salt) {
    unsigned char out[32];
    int ok = PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                               reinterpret_cast<const unsigned char*>(salt.c_str()),
                               static_cast<int>(salt.size()), ITERATIONS, EVP_sha256(),
                               sizeof(out), out);
    if (ok != 1) throw LmsError("Password hashing failed.");
    return toHex(out, sizeof(out));
}

bool safeEquals(const string& a, const string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); i++) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}
