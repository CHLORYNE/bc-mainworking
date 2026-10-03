/*   NAUTITECH - Simulateur de Navigation
     Administrator password for the settings tools. See AdminLock.hpp.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "AdminLock.hpp"
#include "Utilities.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <stdint.h>
#include <vector>

#ifdef _WIN32
#include <direct.h> //_mkdir
#else
#include <sys/stat.h> //mkdir
#endif

namespace {

    //---------------------------------------------------------------------------------------------
    //SHA-256 (FIPS 180-4) and HMAC / PBKDF2 on top of it.
    //---------------------------------------------------------------------------------------------
    const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

    inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    std::string sha256(const std::string& message)
    {
        uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
        std::string m = message;
        const uint64_t bitLength = (uint64_t)message.size() * 8;
        m += (char)0x80;
        while (m.size() % 64 != 56) { m += (char)0x00; }
        for (int i = 7; i >= 0; i--) { m += (char)((bitLength >> (i * 8)) & 0xff); }

        for (size_t block = 0; block < m.size(); block += 64) {
            uint32_t w[64];
            for (int i = 0; i < 16; i++) {
                const unsigned char* p = (const unsigned char*)m.data() + block + 4 * i;
                w[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
            }
            for (int i = 16; i < 64; i++) {
                const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
                const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
            for (int i = 0; i < 64; i++) {
                const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
                const uint32_t ch = (e & f) ^ (~e & g);
                const uint32_t t1 = hh + S1 + ch + K[i] + w[i];
                const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
                const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                const uint32_t t2 = S0 + maj;
                hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
        }
        std::string digest;
        for (int i = 0; i < 8; i++) {
            for (int j = 3; j >= 0; j--) { digest += (char)((h[i] >> (j * 8)) & 0xff); }
        }
        return digest;
    }

    std::string hmacSha256(const std::string& key, const std::string& message)
    {
        std::string k = key.size() > 64 ? sha256(key) : key;
        k.resize(64, (char)0x00);
        std::string inner(64, (char)0x00), outer(64, (char)0x00);
        for (int i = 0; i < 64; i++) {
            inner[i] = (char)(k[i] ^ 0x36);
            outer[i] = (char)(k[i] ^ 0x5c);
        }
        return sha256(outer + sha256(inner + message));
    }

    std::string pbkdf2(const std::string& password, const std::string& salt, unsigned iterations, unsigned bytes)
    {
        std::string out;
        for (uint32_t blockIndex = 1; out.size() < bytes; blockIndex++) {
            std::string block = salt;
            block += (char)((blockIndex >> 24) & 0xff);
            block += (char)((blockIndex >> 16) & 0xff);
            block += (char)((blockIndex >> 8) & 0xff);
            block += (char)(blockIndex & 0xff);
            std::string u = hmacSha256(password, block);
            std::string t = u;
            for (unsigned i = 1; i < iterations; i++) {
                u = hmacSha256(password, u);
                for (size_t j = 0; j < t.size(); j++) { t[j] = (char)(t[j] ^ u[j]); }
            }
            out += t;
        }
        out.resize(bytes);
        return out;
    }

    std::string toHex(const std::string& bytes)
    {
        static const char digits[] = "0123456789abcdef";
        std::string hex;
        for (size_t i = 0; i < bytes.size(); i++) {
            const unsigned char c = (unsigned char)bytes[i];
            hex += digits[c >> 4];
            hex += digits[c & 0x0f];
        }
        return hex;
    }

    std::string fromHex(const std::string& hex)
    {
        std::string bytes;
        for (size_t i = 0; i + 1 < hex.size(); i += 2) {
            unsigned int v = 0;
            if (sscanf(hex.substr(i, 2).c_str(), "%2x", &v) != 1) { return ""; }
            bytes += (char)v;
        }
        return bytes;
    }

    //UTF-8 bytes of the typed password, so accented characters hash the same everywhere.
    std::string utf8(const std::wstring& w)
    {
        std::string s;
        for (size_t i = 0; i < w.size(); i++) {
            uint32_t c = (uint32_t)w[i];
            if (c >= 0xD800 && c <= 0xDBFF && i + 1 < w.size()) { //UTF-16 surrogate pair (Windows wchar_t)
                const uint32_t lo = (uint32_t)w[i + 1];
                if (lo >= 0xDC00 && lo <= 0xDFFF) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); i++; }
            }
            if (c < 0x80) { s += (char)c; }
            else if (c < 0x800) { s += (char)(0xC0 | (c >> 6)); s += (char)(0x80 | (c & 0x3F)); }
            else if (c < 0x10000) { s += (char)(0xE0 | (c >> 12)); s += (char)(0x80 | ((c >> 6) & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
            else { s += (char)(0xF0 | (c >> 18)); s += (char)(0x80 | ((c >> 12) & 0x3F)); s += (char)(0x80 | ((c >> 6) & 0x3F)); s += (char)(0x80 | (c & 0x3F)); }
        }
        return s;
    }

    //---------------------------------------------------------------------------------------------
    //Stored password
    //---------------------------------------------------------------------------------------------
    const unsigned ITERATIONS = 20000;

    //Built-in default password, as a hash (it is given to the administrator, not shown anywhere).
    const char* DEFAULT_SALT_HEX = "4e415554495445434841444d494e2d31";
    const char* DEFAULT_HASH_HEX = "5bb5a4357f30fd8d158215be830e6588e589a9645ba67621958c7eb4852e17a1";

    struct Stored {
        std::string salt; //bytes
        unsigned iterations;
        std::string hashHex;
    };

    std::string lockFile()
    {
        return Utilities::getUserDir() + "admin.lock";
    }

    bool readStored(Stored& s)
    {
        std::ifstream in(lockFile().c_str());
        if (!in.is_open()) { return false; }
        std::string line;
        s.iterations = 0;
        while (std::getline(in, line)) {
            line = Utilities::trim(line);
            const size_t eq = line.find('=');
            if (eq == std::string::npos) { continue; }
            const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
            if (key == "salt") { s.salt = fromHex(value); }
            if (key == "iterations") { s.iterations = (unsigned)atoi(value.c_str()); }
            if (key == "hash") { s.hashHex = value; }
        }
        return !s.salt.empty() && s.iterations > 0 && s.hashHex.size() == 64;
    }

    void makeUserFolder()
    {
        const std::string dirs[2] = { Utilities::getUserDirBase(), Utilities::getUserDir() };
        for (int d = 0; d < 2; d++) {
            std::string dir = dirs[d];
            if (dir.size() > 1 && !Utilities::pathExists(dir)) {
                dir.erase(dir.size() - 1); //no trailing slash
#ifdef _WIN32
                _mkdir(dir.c_str());
#else
                mkdir(dir.c_str(), 0755);
#endif
            }
        }
    }

    //Same-length compare that does not stop at the first difference.
    bool sameText(const std::string& a, const std::string& b)
    {
        if (a.size() != b.size()) { return false; }
        unsigned char diff = 0;
        for (size_t i = 0; i < a.size(); i++) { diff |= (unsigned char)(a[i] ^ b[i]); }
        return diff == 0;
    }
}

namespace AdminLock {

    std::string pbkdf2Hex(const std::string& password, const std::string& salt, unsigned iterations, unsigned bytes)
    {
        return toHex(pbkdf2(password, salt, iterations, bytes));
    }

    bool usingDefault()
    {
        Stored s;
        return !readStored(s);
    }

    bool check(const std::wstring& password)
    {
        Stored s;
        if (!readStored(s)) {
            s.salt = fromHex(DEFAULT_SALT_HEX);
            s.iterations = ITERATIONS;
            s.hashHex = DEFAULT_HASH_HEX;
        }
        return sameText(pbkdf2Hex(utf8(password), s.salt, s.iterations, 32), s.hashHex);
    }

    bool change(const std::wstring& newPassword)
    {
        //Fresh random salt for each password.
        std::random_device device;
        std::mt19937 mix((unsigned)device() ^ (unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::string salt;
        for (int i = 0; i < 16; i++) { salt += (char)(mix() & 0xff); }

        makeUserFolder();
        std::ofstream out(lockFile().c_str(), std::ios::trunc);
        if (!out.is_open()) { return false; }
        out << "nautitech-admin-lock-1" << "\n";
        out << "salt=" << toHex(salt) << "\n";
        out << "iterations=" << ITERATIONS << "\n";
        out << "hash=" << pbkdf2Hex(utf8(newPassword), salt, ITERATIONS, 32) << "\n";
        out.close();
        return !out.fail() && check(newPassword);
    }
}
